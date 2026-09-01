#pragma once

#include "llama_model.hpp"
#include "request.hpp"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <vector>


enum class Policy {Static, Continuous};

class Engine {
public: 
    Engine(LlamaModel& model, Policy policy);
    ~Engine();

    std::future<Response> submit(const Request& req);
    void stop();

    long long total_kv_steps() const { return total_kv_steps_; }
    long long idle_kv_steps() const { return idle_kv_steps_; }

private:
    void loop();
    void step_once();
    double now_ms() const;
    bool check_finished(Sequence& seq, double t);
    void retire(Sequence& seq);

    LlamaModel& model_;
    Policy policy_;
    std::chrono::steady_clock::time_point t0_;

    std::mutex mu_;
    std::condition_variable work_ready_;
    std::deque<Sequence> queue_;
    bool stopping_ = false;

    std::vector<Sequence> batch_;
    std::vector<int> free_seq_ids_;
    int kv_committed_ = 0;
    long long total_kv_steps_ = 0;
    long long idle_kv_steps_ = 0;

    std::thread worker_;
};
