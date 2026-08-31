#include "engine.hpp"


#include <functional>
#include <thread>
#include <cmath>
#include <cstdint>
#include <random>
#include <cstdio>
#include <chrono>
#include <vector>

constexpr std::size_t kClients = 4;
constexpr std::size_t kPerClient = 60;
constexpr double kMeanGapMs = 15.0;
constexpr std::size_t kCap = 8;

void client(Engine& engine, std::size_t client_id, std::uint32_t seed, std::vector<Response>& out) {
    std::mt19937 rng(seed);

    std::exponential_distribution<double> gap(1.0/kMeanGapMs);
    std::uniform_real_distribution<double> log_len(std::log(8.0), std::log(256.0));

    std::vector<std::future<Response>> futures;
    futures.reserve(kPerClient);

    for (std::size_t i = 0; i < kPerClient; i++) {
        std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(gap(rng)));
        Request req;
        req.id = client_id * 1000 + i;
        req.max_tokens = static_cast<int>(std::exp(log_len(rng)));
        futures.push_back(engine.submit(req));
    }

    for (std::future<Response>& f : futures) {
        out.push_back(f.get());
    }
}

void run(const char* name, Policy policy) {
    FakeModel model(42);
    Engine engine(model, kCap, policy);

    std::vector<std::vector<Response>> results(kClients);
    std::vector<std::thread> clients;
    clients.reserve(kClients);

    for(std::size_t i = 0; i < kClients; i++) {
        clients.emplace_back(client, std::ref(engine), i, static_cast<std::uint32_t>(100 + i), std::ref(results[i]));
    }

    for (std::thread& t : clients) {
        t.join();
    }
    engine.stop();

    std::vector<Response> all;
    for (std::vector<Response>& per_client : results) {
        for (Response& r : per_client) {
            all.push_back(std::move(r));
        }
    }

    double sum_ms = 0.0;
    double max_ms = 0.0;

    for (const Response& r : all) {
        const double latency = r.finish_ms - r.submit_ms;
        sum_ms += latency;
        if (latency > max_ms) {max_ms = latency;}
    }

    std::printf("%-11s   n=%3zu   mean=%7.1fms    max=%7.1fms    slots=%6lld    idle=%6lld\n",
                name, all.size(), sum_ms / static_cast<double>(all.size()), max_ms,
                engine.total_slot_steps(), engine.wasted_slot_steps());
}


int main () {
    run("Static", Policy::Static);
    run("Continuous", Policy::Continuous);
    return 0;
}