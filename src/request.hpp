#pragma once

#include "fake_model.hpp"

#include <vector>

struct Request {
    std::size_t id = 0;
    std::vector<TokenID> prompt{};
    int max_tokens = 16;
};

struct Response {
    std::size_t id = 0;
    std::vector<TokenID> output{};
    double finish_ms = 0.0;
};