#include "engine.hpp"

#include <cstdio>
#include <utility>

std::vector<Response> run_static_batch(FakeModel& model, const std::vector<Request>& requests) {
    std::vector<Sequence> batch;
    batch.reserve(requests.size());

    for (const Request& req : requests) {
        Sequence seq;
        seq.prompt = req.prompt;
        seq.max_new_tokens = req.max_tokens;
        batch.push_back(seq);
    }
    int step_index = 0;
    while (true) {
        int active = 0;
        for (const Sequence& seq : batch) {
            if (!seq.finished) {active += 1;}
        }
        if (active == 0) {break;}

        std::printf("step=%d slots=%zu active=%d\n",
                    step_index, batch.size(), active);

        const std::vector<TokenID> next = model.step(batch);
        for (std::size_t i = 0; i < batch.size(); i++) {
            Sequence& seq = batch[i];
            if (seq.finished) { continue; }
            if (next[i] == kEosToken) {
                seq.finished = true;
            } else {
                seq.output.push_back(next[i]);
                seq.finished = static_cast<int>(seq.output.size()) >= seq.max_new_tokens;
            }
        }
        step_index += 1;
    }

    std::vector<Response> responses;
    responses.reserve(batch.size());

    for (Sequence& seq : batch) {
        Response resp;
        resp.output = std::move(seq.output);
        responses.push_back(std::move(resp));
    }

    return responses;
}