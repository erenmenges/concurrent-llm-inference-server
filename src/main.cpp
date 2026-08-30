#include "engine.hpp"

#include <cmath>
#include <cstdint>
#include <random>
#include <cstdio>
#include <chrono>
#include <vector>

std::vector<Request> make_workload(std::size_t count, int min_len, int max_len, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> log_len(std::log(static_cast<double>(min_len)), std::log(static_cast<double>(max_len)));

    std::vector<Request> requests;
    requests.reserve(count);

    for (std::size_t i = 0; i < count; i++) {
        Request req;
        req.id = i;
        req.max_tokens = static_cast<int>(std::exp(log_len(rng)));
        requests.push_back(req);
    }
    return requests;
}

void report(const char* name, const RunResult& result, double wall_ms) {
    int total_tokens = 0;
    for (const Response& resp : result.responses) {
        total_tokens += static_cast<int>(resp.output.size());
    }
    std::printf("%-10s   wall=%7.0fms   tok=%6d   thr=%6.0f tok/s   slots=%7lld   wasted=%7lld (%2.0f%%)\n",
            name, wall_ms, total_tokens, total_tokens / (wall_ms / 1000.0),
            result.total_slot_steps, result.wasted_slot_steps,
            100.0 * static_cast<double>(result.wasted_slot_steps)
                    / static_cast<double>(result.total_slot_steps));
}


int main() {
    FakeModel model(42);

    const std::vector<Request> requests = make_workload(40, 8, 256, 42);
    const std::size_t cap = 8;

    const auto s0 = std::chrono::steady_clock::now();
    const RunResult stat = run_static_batch(model, requests, cap);
    const std::chrono::duration<double, std::milli> s_ms = std::chrono::steady_clock::now() - s0;

    const auto c0 = std::chrono::steady_clock::now();
    const RunResult cont = run_continuous_batch(model, requests, cap);
    const std::chrono::duration<double, std::milli> c_ms = std::chrono::steady_clock::now() - c0;

    report("static", stat, s_ms.count());
    report("continuous", cont, c_ms.count());


    for (std::size_t i = 0; i < requests.size(); i++) {
        std::printf("id=%2zu   len=%4d   static=%5.0fms   cont=%5.0fms\n",
                    i, requests[i].max_tokens, stat.responses[i].finish_ms, cont.responses[i].finish_ms);
    }

    return 0;


}