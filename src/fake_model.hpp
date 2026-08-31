#pragma once

#include "request.hpp"

#include <cstdint>
#include <random>
#include <vector>


inline constexpr double kBaseLatencyMs = 5.0;
inline constexpr double kPerSeqLatencyMs = 0.2;


class FakeModel {
public:
    explicit FakeModel(std::uint32_t seed);
    std::vector<TokenID> step(const std::vector<Sequence>& batch);
private:
    std::mt19937 rng_;
};