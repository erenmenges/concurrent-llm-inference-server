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
    const RunResult result = run_static_batch(model, requests, 4);
    const auto t1 = std::chrono::steady_clock::now();

    const std::chrono::duration<double, std::milli> elapsed = t1 - t0;

    int total_tokens = 0;
    for (const Response& resp : result.responses) {
        total_tokens += static_cast<int>(resp.output.size());
    }

    for (std::size_t i = 0; i < result.responses.size(); i++){
        std::printf("req=%zu tokens=%zu finish=%.1fms\n",
            i, result.responses[i].output.size(), result.responses[i].finish_ms);
    }

    std::printf("slot_steps=%lld wasted=%lld %.0f%%\n",
                result.total_slot_steps,
                result.wasted_slot_steps,
                100.0 * static_cast<double>(result.wasted_slot_steps) / static_cast<double>(result.total_slot_steps));

    std::printf("requests=%zu tokens=%d wall=%.1fms throughput=%.0f tok/s\n",
                requests.size(),
                total_tokens,
                elapsed.count(),
                total_tokens / (elapsed.count() / 1000.0));

    return 0;


}