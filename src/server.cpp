#include "engine.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdio>
#include <future>
#include <stdexcept>
#include <string>
#include <cstddef>

constexpr int kNCtx = 4096;
constexpr int kNSeqMax = 8;
constexpr int kNThreads = 8;
constexpr int kDefaultMaxTokens = 64;

constexpr int kPort = 8080;
constexpr std::size_t kHttpThreads = 8;
constexpr std::size_t kHttpThreadsMax = 32;

using json = nlohmann::json;

std::string chat_wrap(const std::string& user) {
    // qwen 3 chat format
    // by adding an empty thinking block, we skip thinking 
    return "<|im_start|>user\n" + user + "<|im_end|>\n<|im_start|>assistant\n<think>\n\n</think>\n\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <model.gguf>, optional: [static|continuous]\n", argv[0]); // warn that model should be passed in the cli
        return 1;
    }
    Policy policy = Policy::Continuous;
    if (argc >= 3) {
        const std::string policy_arg = argv[2];
        if (policy_arg == "static") {
            policy = Policy::Static;
        } else if (policy_arg != "continuous") {
            std::fprintf(stderr, "unrecognized policy argument: %s\n", argv[2]);
            return 1;
        }
    }



    LlamaModel model(argv[1], kNCtx, kNSeqMax, kNThreads);
    Engine engine(model, policy);
    std::atomic<std::size_t> next_id{0};

    httplib::Server svr;
    svr.new_task_queue = [] {return new httplib::ThreadPool(kHttpThreads, kHttpThreadsMax);};

    svr.Post("/generate", [&engine, &next_id](const httplib::Request& http_req, httplib::Response& http_res) {
        try {
            const json body = json::parse(http_req.body);
            Request req;
            req.id = next_id.fetch_add(1); // fetch_add returns the value then adds
            req.prompt = chat_wrap(body.at("prompt").get<std::string>());
            req.max_tokens = body.value("max_tokens", kDefaultMaxTokens);
            if (req.max_tokens < 1) {
                throw std::invalid_argument("max tokens should be >=1");
            }

            std::future<Response> fut = engine.submit(req);
            const Response resp = fut.get();
            std::fprintf(stderr, "req %zu    gen=%d   took=%.0f ms\n", resp.id, resp.n_generated, resp.finish_ms - resp.submit_ms);

            json out;
            out["id"] = resp.id;
            out["output"] = resp.output;
            out["n_generated"] = resp.n_generated;

            http_res.set_content(out.dump(-1, ' ', false, json::error_handler_t::replace), "application/json");
        } catch (const std::exception& e) {
            http_res.status = 400;
            json err;
            err["error"] = e.what();
            http_res.set_content(err.dump(), "application/json");
        } 
    });

    if (!svr.bind_to_port("0.0.0.0", kPort)) {
        std::fprintf(stderr, "could not bind to port %d, maybe already in use\n", kPort);
        return 1;
    }

    std::fprintf(stderr, "listening on http://0.0.0.0:%d  with policy=%s\n", kPort, policy == Policy::Static ? "static" : "continuous");
    svr.listen_after_bind();
    return 0;
}