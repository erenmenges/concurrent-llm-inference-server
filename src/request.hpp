#pragma once

#include <cstddef>
#include <cstdint>
#include <future>
#include <vector>

using TokenID = std::int32_t;
constexpr TokenID kEosToken = 0;


struct Request {
    std::size_t id = 0;
    std::vector<TokenID> prompt{};
    int max_tokens = 16;
};

struct Response {
    std::size_t id = 0;
    std::vector<TokenID> output{};
    double submit_ms = 0.0;
    double finish_ms = 0.0;
};

struct Sequence {
    std::size_t id = 0;
    std::vector<TokenID> prompt{};
    int max_new_tokens = 16;
    double submit_ms = 0.0;

    std::vector<TokenID> output{};
    bool finished = false;
    double finish_ms = 0.0;

    std::promise<Response> promise{};
};