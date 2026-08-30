#include "engine.hpp"

#include <cstdio>
#include <chrono>
#include <vector>


int main() {
    FakeModel model(42);

    const std::vector<int> lengths = {4, 6, 8, 12, 16, 24, 32, 64};

    std::vector<Request> requests;

    for (int len : lengths) {
        Request req;
        req.max_tokens = len;
        requests.push_back(req);
    }


    const auto t0 = std::chrono::steady_clock::now();
    const std::vector<Response> responses = run_static_batch(model, requests);
    const auto t1 = std::chrono::steady_clock::now();

    const std::chrono::duration<double, std::milli> elapsed = t1 - t0;

    int total_tokens = 0;
    for (const Response& resp : responses) {
        total_tokens += static_cast<int>(resp.output.size());
    }

    std::printf("requests=%zu tokens=%d wall=%.1fms throughput=%.0f tok/s\n",
                requests.size(),
                total_tokens,
                elapsed.count(),
                total_tokens / (elapsed.count() / 1000.0));

    return 0;


}