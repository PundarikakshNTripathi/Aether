#include "vlm_infer.hpp"
#include <mutex>

namespace vlm_engine {

KVCacheManager::KVCacheManager(size_t max_batch_size, size_t num_layers) 
    : max_batch_size_(max_batch_size), num_layers_(num_layers) {
    caches_.resize(max_batch_size_);
    for (size_t i = 0; i < max_batch_size_; ++i) {
        caches_[i].resize(num_layers_);
    }
}

void KVCacheManager::UpdateCache(size_t batch_idx, size_t layer_idx, 
                                 const std::vector<float>& new_keys, 
                                 const std::vector<float>& new_values) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (batch_idx >= max_batch_size_ || layer_idx >= num_layers_) return;
    
    auto& entry = caches_[batch_idx][layer_idx];
    entry.key_states.insert(entry.key_states.end(), new_keys.begin(), new_keys.end());
    entry.value_states.insert(entry.value_states.end(), new_values.begin(), new_values.end());
    entry.sequence_length++;
}

KVCacheEntry KVCacheManager::GetCache(size_t batch_idx, size_t layer_idx) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (batch_idx >= max_batch_size_ || layer_idx >= num_layers_) return {};
    return caches_[batch_idx][layer_idx];
}

void KVCacheManager::ClearCache(size_t batch_idx) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (batch_idx >= max_batch_size_) return;
    for (size_t i = 0; i < num_layers_; ++i) {
        caches_[batch_idx][i].key_states.clear();
        caches_[batch_idx][i].value_states.clear();
        caches_[batch_idx][i].sequence_length = 0;
    }
}

} // namespace vlm_engine
