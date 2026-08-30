#pragma once

#include "fake_model.hpp"

#include <vector>

struct Request {
    std::vector<TokenID> prompt{};
    int max_tokens = 16;
};

struct Response {
    std::vector<TokenID> output{};
    double finish_ms = 0.0;
};