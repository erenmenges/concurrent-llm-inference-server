#include "fake_model.hpp"

#include <chrono>
#include <thread>

FakeModel::FakeModel(std::uint32_t seed) : rng_(seed) {}

std::vector<TokenID> FakeModel::step(const std::vector<Sequence>& batch) {
    const double latency_ms = kBaseLatencyMs + kPerSeqLatencyMs * static_cast<double>(batch.size());
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(latency_ms));
    std::vector<TokenID> next;
    next.reserve(batch.size());

    std::uniform_real_distribution<double> coin(0.0, 1.0);
    std::uniform_int_distribution<TokenID> pick(1, 5000);
    for(const Sequence& seq : batch) {
        if (seq.output.size() >= 4 && coin(rng_) < 0.15) {
            next.push_back(kEosToken);
        } else {
            next.push_back(pick(rng_));
        }
    }
    return next;
}