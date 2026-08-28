#include "fake_model.hpp"

#include <chrono>
#include <thread>

FakeModel::FakeModel(std::uint32_t seed) : rng_(seed) {};

TokenID FakeModel::step(const Sequence& seq) {
    std::this_thread::sleep_for((std::chrono::milliseconds(5)));
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    if (seq.output.size() >= 4 && coin(rng_) < 0.15) {
        return kEosToken;
    }

    std::uniform_int_distribution<TokenID> pick(1, 5000);
    return pick(rng_);
};