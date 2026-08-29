#pragma once

#include <cstdint>
#include <random>
#include <vector>

using TokenID = std::int32_t;

constexpr TokenID kEosToken = 0;

struct Sequence {
    std::vector<TokenID> prompt{};
    std::vector<TokenID> output{};
    int max_new_tokens = 16;
    bool finished = false;
};


class FakeModel {
public:
    explicit FakeModel(std::uint32_t seed);
    TokenID step (const Sequence& seq);
private:
    std::mt19937 rng_;
};