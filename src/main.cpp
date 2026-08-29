#include "fake_model.hpp"

#include <iostream>
#include <chrono>
#include <vector>


void generate(FakeModel& model, Sequence& seq) {
    while (!seq.finished) {
        const TokenID next = model.step(seq);
        if (next == kEosToken) {
            seq.finished = true;
        }
        else {
            seq.output.push_back(next);
            seq.finished = static_cast<int>(seq.output.size()) >= seq.max_new_tokens;
        }
    }
}

int main() {
    FakeModel model(31);

    std::vector<Sequence> requests = {
        Sequence{.prompt = {10, 11, 12}, .max_new_tokens = 16},
        Sequence{.prompt = {7}, .max_new_tokens = 8},
        Sequence{.prompt = {42, 43}, .max_new_tokens = 24}
    };

    const auto start = std::chrono::steady_clock::now();
    for (Sequence& seq : requests) {
        generate(model, seq);
    }
    const auto end = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < requests.size(); i++) {
        std::cout << "seq" << i 
                  << " prompt=" << requests[i].prompt.size()
                  << " generated=" << requests[i].output.size() << "\n";
    }

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "total wall time: " << ms << "ms/n";
    return 0;
}