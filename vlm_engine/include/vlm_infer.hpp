#pragma once

#include <string>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <functional>

namespace vlm_engine {

struct KVCacheEntry {
    std::vector<float> key_states;
    std::vector<float> value_states;
    size_t sequence_length = 0;
};

class KVCacheManager {
public:
    KVCacheManager(size_t max_batch_size = 8, size_t num_layers = 32);
    
    void UpdateCache(size_t batch_idx, size_t layer_idx, const std::vector<float>& new_keys, const std::vector<float>& new_values);
    KVCacheEntry GetCache(size_t batch_idx, size_t layer_idx) const;
    void ClearCache(size_t batch_idx);

private:
    size_t max_batch_size_;
    size_t num_layers_;
    // batch_idx -> layer_idx -> cache entry
    std::vector<std::vector<KVCacheEntry>> caches_;
    mutable std::shared_mutex mutex_;
};

struct ModelConfig {
    std::string model_path;
    size_t hidden_size = 4096;
    size_t num_heads = 32;
    size_t num_layers = 32;
    float temperature = 0.7f;
    float top_p = 0.9f;
};

class VLMModel {
public:
    VLMModel(const ModelConfig& config);
    
    bool LoadModel();
    bool IsLoaded() const;

    std::string RunImageDescription(const std::vector<uint8_t>& image_bytes, const std::string& prompt);
    
    void RunStreamingInference(const std::string& prompt, 
                               const std::vector<std::string>& history,
                               const std::vector<uint8_t>& image_bytes,
                               std::function<void(const std::string& token, bool done)> callback);

private:
    ModelConfig config_;
    bool is_loaded_ = false;
    std::unique_ptr<KVCacheManager> kv_cache_;
    mutable std::shared_mutex model_mutex_;
};

} // namespace vlm_engine
