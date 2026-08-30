#include "engine.hpp"

#include <chrono>
#include <cstdio>
#include <utility>

RunResult run_static_batch(FakeModel& model, const std::vector<Request>& requests, std::size_t max_batch_size) {
    const auto t0 = std::chrono::steady_clock::now();

    RunResult result;
    result.responses.reserve(requests.size());

    std::size_t next_request = 0;

    while (next_request < requests.size()) {
        std::vector<Sequence> batch;

        // fill batch slots with requests
        while (batch.size() < max_batch_size && next_request < requests.size()) {
            Sequence seq;
            seq.id = requests[next_request].id;
            seq.prompt = requests[next_request].prompt;
            seq.max_new_tokens = requests[next_request].max_tokens;
            batch.push_back(seq);
            next_request += 1;
        }

        // process the batch statically
        while (true) {
            int active = 0;
            for (const Sequence& seq : batch) {
                if (!seq.finished) {active += 1;}
            }
            if (active == 0) {break;}
            
            result.total_slot_steps += static_cast<long long>(batch.size());
            result.wasted_slot_steps += static_cast<long long>(batch.size()) - active;

            const std::vector<TokenID> next = model.step(batch);
            const std::chrono::duration<double, std::milli> now = std::chrono::steady_clock::now() - t0;

            // append generated token to each sequence in batch
            for (std::size_t i = 0; i < batch.size(); i++) {
                Sequence& seq = batch[i];
                if (seq.finished) {continue;}

                if (next[i] == kEosToken) {seq.finished = true;}
                else {
                    seq.output.push_back(next[i]);
                    seq.finished = static_cast<int>(seq.output.size()) >= seq.max_new_tokens;
                }

                if (seq.finished) {seq.finish_ms = now.count();}
            }
        }

        for (Sequence& seq : batch) {
            Response resp;
            resp.id = seq.id;
            resp.output = std::move(seq.output);
            resp.finish_ms = seq.finish_ms;
            result.responses.push_back(std::move(resp));
        }
    }
    return result;
}

RunResult run_continuous_batch(FakeModel& model, const std::vector<Request>& requests, std::size_t max_batch_size){
    const auto t0 = std::chrono::steady_clock::now();
    RunResult result;
    result.responses.resize(requests.size());

    std::vector<Sequence> batch;
    std::size_t next_request = 0;

    while(next_request < requests.size() || !batch.empty()) {
        while (batch.size() < max_batch_size && next_request < requests.size()) {
            Sequence seq;
            seq.id = requests[next_request].id;
            seq.prompt = requests[next_request].prompt;
            seq.max_new_tokens = requests[next_request].max_tokens;
            batch.push_back(seq);
            next_request += 1;
        }
        result.total_slot_steps += static_cast<long long>(max_batch_size);
        result.wasted_slot_steps += static_cast<long long>(max_batch_size) - static_cast<long long>(batch.size());

        const std::vector<TokenID> next = model.step(batch);
        const std::chrono::duration<double, std::milli> now = std::chrono::steady_clock::now() - t0;

        for (std::size_t i = 0; i < batch.size(); i++) {
            Sequence& seq = batch[i];
            seq.output.push_back(next[i]);
            if(static_cast<int>(seq.output.size()) >= seq.max_new_tokens) {
                seq.finished = true;
                seq.finish_ms = now.count();
            }
        }
        for (std::size_t i = 0; i < batch.size(); ) {
            if (!batch[i].finished) {
                i += 1;
                continue;
            }
            Response& resp = result.responses[batch[i].id];
            resp.id = batch[i].id;
            resp.output = std::move(batch[i].output);
            resp.finish_ms = batch[i].finish_ms;
            batch.erase(batch.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
    return result;
}