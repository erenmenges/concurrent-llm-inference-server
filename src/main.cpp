#include "engine.hpp"


#include <chrono>
#include <functional>
#include <cmath>
#include <thread>
#include <cstdint>
#include <random>
#include <cstdio>
#include <vector>
#include <string>

constexpr int kNCtx = 4096; // total kv cache capacity
constexpr int kNSeqMax = 8; // max sequences in a batch

constexpr std::size_t kClients = 16;
constexpr std::size_t kPerClient = 8;
constexpr double kMeanGapMs = 2000.0;

const std::vector<std::string> kPrompts = {
    "Explain what a mutex protects, in two sentences.",
    "Compare Windows and MacOS, in four sentences?",
    "What is the most popular park in NYC? Answer with one sentence.",
    "Give me a draft email saying I can't go to class, in three sentences."
};

std::string chat_wrap(const std::string& user) {
    // qwen 3 chat format
    // by adding an empty thinking block, we skip thinking 
    return "<|im_start|>user\n" + user + "<|im_end|>\n<|im_start|>assistant\n<think>\n\n</think>\n\n";
}

void client(Engine& engine, std::size_t client_id, std::uint32_t seed, std::vector<Response>& out) {
    std::mt19937 rng(seed);

    std::exponential_distribution<double> gap(1.0/kMeanGapMs);
    std::uniform_real_distribution<double> log_len(std::log(32.0), std::log(512.0));

    std::vector<std::future<Response>> futures;
    futures.reserve(kPerClient);

    for (std::size_t i = 0; i < kPerClient; i++) {
        std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(gap(rng)));
        Request req;
        req.id = client_id * 1000 + i;
        req.prompt = chat_wrap(kPrompts[i % kPrompts.size()]); // so it's like req 1 and req 5 gets the 1st prompt 
        req.max_tokens = static_cast<int>(std::exp(log_len(rng)));
        futures.push_back(engine.submit(req));
    }

    for (std::future<Response>& f : futures) {
        out.push_back(f.get());
    }
}

void run(LlamaModel& model, const char* name, Policy policy) {
    Engine engine(model, policy);

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
    int total_tokens = 0;

    for (const Response& r : all) {
        const double latency = r.finish_ms - r.submit_ms;
        sum_ms += latency;
        if (latency > max_ms) {max_ms = latency;}
        total_tokens += r.n_generated;
    }

    std::printf("%-11s  n=%2zu  mean=%8.1fms  max=%8.1fms  tokens=%5d  kv_idle=%4.1f%%\n",
            name, all.size(), 
            sum_ms / static_cast<double>(all.size()), 
            max_ms, total_tokens, 
            100.0 * static_cast<double>(engine.idle_kv_steps()) / static_cast<double>(engine.total_kv_steps())
        );
}


int main (int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <model.gguf>\n", argv[0]); // warn that model should be passed in the cli
        return 1;
    }


    LlamaModel model(argv[1], kNCtx, kNSeqMax);

    run(model, "Static", Policy::Static);
    run(model, "Continuous", Policy::Continuous);
    return 0;
}