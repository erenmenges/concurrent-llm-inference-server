#include "fake_model.hpp"

#include <cstdio>
#include <chrono>
#include <vector>


void run_static_batch(FakeModel& model, std::vector<Sequence>& batch) {
    while(true) {
        const std::vector<TokenID> next = model.step(batch);
        bool any_active = false;
        for (std::size_t i = 0; i < batch.size(); i++) {
            Sequence& seq = batch[i];
            if (seq.finished){continue;}
            if (next[i] == kEosToken) {
                seq.finished = true;
            } else {
                seq.output.push_back(next[i]);
                seq.finished = static_cast<int>(seq.output.size()) >= seq.max_new_tokens;
            }
            if (!seq.finished) {any_active = true;}
        }
        if (!any_active) {break;}
    }
}


int main() {
    FakeModel model(42);

    std::vector<Sequence> batch;
    for (std::size_t i = 0; i < 8; ++i) {
        Sequence s;
        s.max_new_tokens = 16;
        batch.push_back(s);
    }

    const auto t0 = std::chrono::steady_clock::now();
    run_static_batch(model, batch);
    const auto t1 = std::chrono::steady_clock::now();

    const std::chrono::duration<double, std::milli> elapsed = t1 - t0;

    int total_tokens = 0;
    for (const Sequence& s : batch) {
        total_tokens += static_cast<int>(s.output.size());
    }

    std::printf("batch=%zu tokens=%d wall=%.1fms throughput=%.0f tok/s\n",
                batch.size(), 
                total_tokens, 
                elapsed.count(),
                total_tokens / (elapsed.count() / 1000.0));
    
    return 0;
}