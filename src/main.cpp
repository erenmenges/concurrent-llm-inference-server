#include "fake_model.hpp"

#include <iostream>
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
}