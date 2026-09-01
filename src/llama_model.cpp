#include "llama_model.hpp"

#include <stdexcept>
#include <string>

namespace {
    void batch_clear(llama_batch& b) {
        b.n_tokens = 0;
    }

    void batch_add(llama_batch& b, TokenID tok, int pos, int seq_id, bool want_logits) {
        const int i = b.n_tokens;
        b.token[i] = tok;
        b.pos[i] = static_cast<llama_pos>(pos);

        b.n_seq_id[i] = 1;
        b.seq_id[i][0] = static_cast<llama_seq_id>(seq_id);
        b.logits[i] = want_logits ? 1 : 0;  // ternary conditional in cpp
        b.n_tokens += 1;
    }
} // namespace closed


LlamaModel::LlamaModel(const std::string& gguf_path, int n_ctx, int n_seq_max) : n_ctx_(n_ctx), n_seq_max_(n_seq_max) {
        llama_backend_init();

        // llama first initializes a default struct then assigns the real values to be C compatible
        llama_model_params mparams = llama_model_default_params();
        model_ = llama_model_load_from_file(gguf_path.c_str(), mparams);
        if (model_ == nullptr) {
            throw std::runtime_error("failed to load model: " + gguf_path);
        }

        vocab_ = llama_model_get_vocab(model_);

        // context stuff
        llama_context_params cparams = llama_context_default_params();
        cparams.n_ctx = static_cast<uint32_t>(n_ctx);
        cparams.n_seq_max = static_cast<uint32_t>(n_seq_max);
        cparams.kv_unified = true;
        

        // llama_decode() stuff
        cparams.n_batch = static_cast<uint32_t>(n_ctx);
        cparams.n_ubatch = static_cast<uint32_t>(n_ctx);

        ctx_ = llama_init_from_model(model_, cparams);
        if (ctx_ == nullptr) {
            llama_model_free(model_);
            throw std::runtime_error("failed to create llama context");
        }

        n_ctx_ = static_cast<int>(llama_n_ctx(ctx_));
        n_seq_max_ = static_cast<int>(llama_n_seq_max(ctx_));

        sampler_ = llama_sampler_init_greedy();
        batch_ = llama_batch_init(static_cast<int32_t>(n_ctx), 0, 1);
}

LlamaModel::~LlamaModel() {
    llama_batch_free(batch_);
    llama_sampler_free(sampler_);
    llama_free(ctx_);
    llama_model_free(model_);
    llama_backend_free();
}

std::vector<TokenID> LlamaModel::tokenize(const std::string& text, bool parse_special) const {
    const int n_max = text.size() + 8;  //  +8 is us reserving space for special tokens
    std::vector<TokenID> tokens(n_max);

    const int n = llama_tokenize(vocab_, text.c_str(), static_cast<int>(text.size()), tokens.data(), n_max, true, parse_special);
    if (n < 0) {throw std::runtime_error("tokenize: buffer too small");}
    tokens.resize(static_cast<std::size_t>(n));
    return tokens;
}

std::string LlamaModel::detokenize(const std::vector<TokenID>& tokens) const {
    if (tokens.empty()) {return {};}

    std::string text(tokens.size() * 8, '\0');

    int n = llama_detokenize(vocab_, tokens.data(), static_cast<int>(tokens.size()), text.data(), static_cast<int>(text.size()), false, false);
    if (n < 0) {  // our 8 bytes is a guess. if a token is more than that, retry.
        text.resize(static_cast<std::size_t>(-n));
        n = llama_detokenize(vocab_, tokens.data(), static_cast<int>(tokens.size()), text.data(), static_cast<int>(text.size()), false, false);
    }

    text.resize(static_cast<std::size_t>(n));
    return text;
}

bool LlamaModel::is_eos(TokenID token) const {
    return llama_vocab_is_eog(vocab_, token);
}

void LlamaModel::release(int seq_id) {
    llama_memory_seq_rm(llama_get_memory(ctx_), seq_id, -1, -1);
}

TokenID LlamaModel::prefill(Sequence& seq) {
    batch_clear(batch_);
    for (std::size_t i = 0; i < seq.prompt.size(); i++) {
        batch_add(batch_, seq.prompt[i], static_cast<int>(i), seq.seq_id, false);
    }

    batch_.logits[batch_.n_tokens - 1] = 1;
    if (llama_decode(ctx_, batch_) != 0) {throw std::runtime_error("prefill failed");}

    seq.n_past = static_cast<int>(seq.prompt.size());
    return llama_sampler_sample(sampler_, ctx_, -1);
}

std::vector<TokenID> LlamaModel::step(std::vector<Sequence>& engine_batch) {
    batch_clear(batch_);
    for (Sequence& seq : engine_batch) {
        batch_add(batch_, seq.output.back(), seq.n_past, seq.seq_id, true);
    }
    
    if (llama_decode(ctx_, batch_) != 0) {throw std::runtime_error("step: llama_decode failed");}

    std::vector<TokenID> next;
    next.reserve(engine_batch.size());

    for(std::size_t i = 0; i < engine_batch.size(); i++) {
        next.push_back(llama_sampler_sample(sampler_, ctx_, static_cast<int>(i)));
        engine_batch[i].n_past += 1;
    }

    return next;
}