#include "engine.hpp"

#include <utility>

Engine::Engine(FakeModel& model, std::size_t max_batch_size, Policy policy) : 
                model_(model),
                max_batch_size_(max_batch_size),
                policy_(policy),
                t0_(std::chrono::steady_clock::now()),
                worker_([this] { loop(); }) 
                {}

Engine::~Engine() {
    stop();
}

double Engine::now_ms() const {
    std::chrono::duration<double, std::milli> d = std::chrono::steady_clock::now() - t0_;
    return d.count();
}

std::future<Response> Engine::submit(const Request& req) {
    Sequence seq;
    seq.id = req.id;
    seq.prompt = req.prompt;
    seq.max_new_tokens = req.max_tokens;
    seq.submit_ms = now_ms();
    std::future<Response> fut = seq.promise.get_future();
    {
        std::lock_guard<std::mutex> lock(mu_);
        queue_.push_back(std::move(seq));
    }
    work_ready_.notify_one();
    return fut;
}


void Engine::stop() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (stopping_) {
            return;
        }
        stopping_ = true;
    }
    work_ready_.notify_all();
    worker_.join();
}

void Engine::loop() {
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(mu_);

            // worker sleeps if it doesn't have to do anything
            work_ready_.wait(lock, [this] {
                return stopping_ || !queue_.empty() || !batch_.empty();
            });

            if (stopping_ && queue_.empty() && batch_.empty()) {
                return;
            }

            const bool may_admit = (policy_ == Policy::Continuous) || batch_.empty();
            while (may_admit && batch_.size() < max_batch_size_ && !queue_.empty()) {
                batch_.push_back(std::move(queue_.front()));
                queue_.pop_front();
            }
        }
        step_once();
    }

}


void Engine::step_once() {
    total_slot_steps_ += static_cast<long long>(max_batch_size_);
    wasted_slot_steps_ += static_cast<long long>(max_batch_size_) - static_cast<long long>(batch_.size());

    const std::vector<TokenID> next = model_.step(batch_);
    const double finish_ms = now_ms();

    for (std::size_t i = 0; i < batch_.size(); i++) {
        batch_[i].output.push_back(next[i]);
        if (static_cast<int>(batch_[i].output.size()) >= batch_[i].max_new_tokens) {
            batch_[i].finished = true;
            batch_[i].finish_ms = finish_ms;
        }
    }

    for (std::size_t i = 0; i < batch_.size(); ) {
        if (!batch_[i].finished) {
            i += 1;
            continue;
        }
        Sequence& seq = batch_[i];
        Response resp;
        resp.id = seq.id;
        resp.output = std::move(seq.output);
        resp.submit_ms = seq.submit_ms;
        resp.finish_ms = seq.finish_ms;
        seq.promise.set_value(std::move(resp));
        
        batch_.erase(batch_.begin() + static_cast<std::ptrdiff_t>(i));
    }
}





