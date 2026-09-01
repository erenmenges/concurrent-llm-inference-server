#pragma once

#include <cstddef>
#include <cstdint>
#include <future>
#include <vector>
#include <string>


using TokenID = std::int32_t;


struct Request {
    std::size_t id = 0;
    std::string prompt;
    int max_tokens = 16;
};

struct Response {
    std::size_t id = 0;
    std::string output;
    int n_generated = 0;
    double submit_ms = 0.0;
    double finish_ms = 0.0;
};

struct Sequence {
    std::size_t id = 0;
    std::vector<TokenID> prompt{};
    int max_new_tokens = 16;
    double submit_ms = 0.0;

    int seq_id = -1; // for llama cpp kv cache
    int n_past = 0; // kv cache size
    int kv_reserved = 0; // max amount of tokens this seq will occupy

    std::vector<TokenID> output{};
    bool finished = false;
    double finish_ms = 0.0;

    std::promise<Response> promise{};
};