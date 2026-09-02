#pragma once

#include "request.hpp"
#include "llama.h"

#include <string>
#include <vector>


class LlamaModel {
public:
    LlamaModel(const std::string& gguf_path, int n_ctx, int n_seq_max, int n_threads);
    ~LlamaModel();

    LlamaModel(const LlamaModel&) = delete; //disable copy constructor
    LlamaModel& operator=(const LlamaModel&) = delete; // disable copy assignment

    std::vector<TokenID> tokenize(const std::string& text, bool parse_special) const;
    std::string detokenize(const std::vector<TokenID>& tokens) const;
    bool is_eos(TokenID token) const;

    TokenID prefill (Sequence& seq);
    std::vector<TokenID> step(std::vector<Sequence>& batch);
    void release(int seq_id);

    int n_ctx() const {return n_ctx_;}
    int n_seq_max() const {return n_seq_max_;}

private:
    llama_model* model_ = nullptr;
    llama_context* ctx_ = nullptr;
    const llama_vocab* vocab_ = nullptr;
    llama_sampler* sampler_ = nullptr;

    llama_batch batch_{};
    int n_ctx_ = 0;
    int n_seq_max_ = 0;
};