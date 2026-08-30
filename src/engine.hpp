#pragma once

#include "fake_model.hpp"
#include "request.hpp"

#include <vector>

std::vector<Response> run_static_batch(FakeModel& model, const std::vector<Request>& requests);

