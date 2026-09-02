#include <httplib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;

const std::vector<std::string> kPrompts = {
    "Explain what a mutex protects, in two sentences.",
    "Compare Windows and MacOS, in four sentences.",
    "What is the most popular park in NYC? Answer with one sentence.",
    "Give me a draft email saying I can't go to class, in three sentences."
};

struct Sample {
    double latency_ms = 0.0;
    int n_generated;
};

double percentile(const std::vector<double>& sorted, double p) {
    // simpler than it looks: multiply p with size and take the ceiling. then subtract.
    const std::size_t idx = static_cast<std::size_t>(std::ceil(p * static_cast<double>(sorted.size()))) - 1;  
    return sorted[idx];
}

void client(const std::string& host, int port, std::size_t n_requests, int max_tokens, std::vector<Sample>& out) {
    httplib::Client cli(host, port);
    cli.set_read_timeout(600, 0);
    for(std::size_t i = 0; i < n_requests; i++) {
        json body;
        body["prompt"] = kPrompts[i % kPrompts.size()];
        body["max_tokens"] = max_tokens;

        const auto t0 = std::chrono::steady_clock::now();
        httplib::Result res = cli.Post("/generate", body.dump(), "application/json");
        const auto t1 = std::chrono::steady_clock::now();

        if (!res) {
            std::fprintf(stderr, "request error: %s\n", httplib::to_string(res.error()).c_str());
            continue;
        }

        if (res->status != 200) {
            std::fprintf(stderr, "HTTP %d: %s\n", res->status, res->body.c_str());
            continue;
        }

        Sample s;
        s.latency_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        s.n_generated = json::parse(res->body).at("n_generated").get<int>();
        out.push_back(s);
    }
}


int main(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(stderr, "usage: %s [host] [port] [concurrency] [requests_per_client] [max_tokens]\n", argv[0]);
        return 1;
    }

    const std::string host = argv[1];
    const int port = std::atoi(argv[2]);
    const std::size_t concurrency = static_cast<std::size_t>(std::atoi(argv[3]));
    const std::size_t per_client = static_cast<std::size_t>(std::atoi(argv[4]));
    const int max_tokens = std::atoi(argv[5]);

    std::vector<std::vector<Sample>> results(concurrency);
    std::vector<std::thread> threads;
    threads.reserve(concurrency);

    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < concurrency; i++) {
        threads.emplace_back(client, host, port, per_client, max_tokens, std::ref(results[i]));
    }

    for (std::size_t i = 0; i < concurrency; i++) {
        threads[i].join();
    }

    const double wall_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    std::vector<double> latencies;
    long long total_tokens = 0;
    for (const std::vector<Sample>& per_client_samples : results) {
        for (const Sample& s : per_client_samples) {
            latencies.push_back(s.latency_ms);
            total_tokens += s.n_generated;
        }
    }

    if (latencies.empty()) {
        std::fprintf(stderr, "no successful requests :(\n");
        return 1;
    }

    std::sort(latencies.begin(), latencies.end());
    const std::size_t n = latencies.size();
    std::printf("conc=%2zu  ok=%4zu  failed=%2zu  wall=%6.1fs  req/s=%5.2f  tok/s=%6.1f  tok/req=%5.1f  p50=%6.0fms  p95=%6.0fms  p99=%6.0fms  max=%6.0fms\n",
                concurrency, n,
                concurrency * per_client - n,
                wall_s,
                static_cast<double>(n) / wall_s, 
                static_cast<double>(total_tokens) / wall_s,
                static_cast<double>(total_tokens) / static_cast<double>(n),
                percentile(latencies, 0.50), percentile(latencies, 0.95), percentile(latencies, 0.99), percentile(latencies, 1.0));
    return 0;




}