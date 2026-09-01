#include "engine.hpp"

#include <utility>

// constructor
Engine::Engine(LlamaModel& model, Policy policy) : model_(model),policy_(policy), t0_(std::chrono::steady_clock::now()),worker_ () {
    free_seq_ids_.reserve(static_cast<std::size_t>(model_.n_seq_max()));
    for (int i = model_.n_seq_max(); i >= 0; i--) {
        free_seq_ids_.push_back(i);
    }
    worker_ = std::thread([this] {loop();});
}

// destructor
Engine::~Engine() {stop();}

double Engine::now_ms() const {
    std::chrono::duration<double, std::milli> d = std::chrono::steady_clock::now() - t0_;
    return d.count();
}

std::future<Response> Engine::submit(const Request& req) {
    Sequence seq;
    seq.id = req.id;
    seq.prompt = model_.tokenize(req.prompt, true);
    seq.max_new_tokens = req.max_tokens;
    seq.submit_ms = now_ms();

    const int need = static_cast<int>(seq.prompt.size()) + seq.max_new_tokens;
    if (need > model_.n_ctx()) {
        throw std::runtime_error("prompt too big to fit in our kv cache, ever.");
    }

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
        if (stopping_) {return;}
        stopping_ = true;
    }
    work_ready_.notify_all();
    worker_.join();
}

void Engine::loop() {
    for (;;) {
        std::vector<Sequence> admitted;
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
            while (may_admit && !queue_.empty() && !free_seq_ids_.empty()) {  // !free_seq_ids_.empty() means only admit if there is a slot for a seq
                Sequence& head = queue_.front();  // we make it a seq reference first because we need to look inside for kv capacity calculation
                const int need = static_cast<int>(head.prompt.size()) + head.max_new_tokens;
                if (kv_committed_ + need > model_.n_ctx()) {break;}  // block until there's room for this seq. head of line blocking.

                head.seq_id = free_seq_ids_.back();
                free_seq_ids_.pop_back();
                head.kv_reserved = need;
                kv_committed_ += need;

                admitted.push_back(std::move(head));
                queue_.pop_front();
            }
        }
        for(Sequence& seq : admitted) {
            seq.output.push_back(model_.prefill(seq));
            if (check_finished(seq, now_ms())) {
                retire(seq);
            } else {
                batch_.push_back(std::move(seq));
            }
        }

        if (!batch_.empty()) {
            step_once();
        }
    }

}

bool Engine::check_finished(Sequence& seq, double t) {
    const bool hit_limit = static_cast<int>(seq.output.size()) >= seq.max_new_tokens;
    const bool hit_eos = model_.is_eos(seq.output.back());
    if (!hit_limit && !hit_eos) {return false;}  //demorgan's law for (hit_limit || hit_eos)

    seq.finished = true;
    seq.finish_ms = t;
    return true;
}

void Engine::retire(Sequence& seq) {
    Response resp;
    resp.id = seq.id;
    resp.output = model_.detokenize(seq.output);
    resp.n_generated = static_cast<int>(seq.output.size());
    resp.submit_ms = seq.submit_ms;
    resp.finish_ms = seq.finish_ms;

    model_.release(seq.seq_id);
    free_seq_ids_.push_back(seq.seq_id);
    kv_committed_ -= seq.kv_reserved;

    seq.promise.set_value(std::move(resp));
}



void Engine::step_once() {
    total_kv_steps_ += model_.n_ctx();
    idle_kv_steps_ += model_.n_ctx() - kv_committed_;

    const std::vector<TokenID> next = model_.step(batch_);
    const double finish_ms = now_ms();

    for (std::size_t i = 0; i < batch_.size(); i++) {
        batch_[i].output.push_back(next[i]);
        check_finished(batch_[i], finish_ms);
    }

    for (std::size_t i = 0; i < batch_.size(); ) {
        if (!batch_[i].finished) {
            i += 1;
            continue;
        }
        retire(batch_[i]);        
        batch_.erase(batch_.begin() + static_cast<std::ptrdiff_t>(i));
    }
}





