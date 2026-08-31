#pragma once

#include "fake_model.hpp"
#include "request.hpp"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <vector>
#include <cstddef>

struct RunResult {
    std::vector<Response> responses{};
    long long total_slot_steps = 0;
    long long wasted_slot_steps = 0;
};

enum class Policy {Static, Continuous};

class Engine {
public: 
    Engine(FakeModel& model, std::size_t max_batch_size_, Policy policy);
    ~Engine();

    std::future<Response> submit(const Request& req);
    void stop();

    long long total_slot_steps() const { return total_slot_steps_; }
    long long wasted_slot_steps() const { return wasted_slot_steps_; }

private:
    void loop();
    void step_once();
    double now_ms() const;

    FakeModel& model_;
    std::size_t max_batch_size_;
    Policy policy_;
    std::chrono::steady_clock::time_point t0_;

    std::mutex mu_;
    std::condition_variable work_ready_;
    std::deque<Sequence> queue_;
    bool stopping_ = false;

    std::vector<Sequence> batch_;
    long long total_slot_steps_ = 0;
    long long wasted_slot_steps_ = 0;

    std::thread worker_;
};


RunResult run_batch(FakeModel& model, const std::vector<Request>& requests, std::size_t max_batch_size, Policy policy);
