#pragma once

#include "fake_model.hpp"
#include "request.hpp"

#include <vector>
#include <cstddef>

struct RunResult {
    std::vector<Response> responses{};
    long long total_slot_steps = 0;
    long long wasted_slot_steps = 0;
};

RunResult run_static_batch(FakeModel& model, const std::vector<Request>& requests, std::size_t max_batch_size);
