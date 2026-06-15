#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <shared_mutex>
#include <random>

namespace search_core {

struct HNSWNode {
    size_t id;
    std::string url;
    std::string title;
    std::vector<float> vector;
    // Each level has a list of neighbor node IDs
    std::vector<std::vector<size_t>> neighbors;
};

class HNSWGraph {
public:
    HNSWGraph(size_t dim = 128, size_t M = 16, size_t ef_construction = 64, size_t ef_search = 32);

    void Insert(const std::string& url, const std::string& title, const std::vector<float>& vec);
    
    struct SearchResult {
        std::string url;
        std::string title;
        float score; // Cosine similarity
    };

    std::vector<SearchResult> Search(const std::vector<float>& query_vec, size_t k) const;

    size_t GetNodeCount() const;

private:
    float CosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) const;
    float L2Distance(const std::vector<float>& a, const std::vector<float>& b) const;
    
    size_t GetRandomLevel();
    
    std::vector<size_t> SearchLayer(const std::vector<float>& query_vec, 
                                    const std::vector<size_t>& enter_nodes, 
                                    size_t ef, size_t level) const;

    size_t dim_;
    size_t M_; // Max number of connections per node per layer
    size_t M0_; // Max connections for layer 0
    size_t ef_construction_;
    size_t ef_search_;
    double mL_; // Level normalization factor
    
    int max_level_ = -1;
    size_t enter_node_id_ = 0;
    bool has_enter_node_ = false;

    std::vector<HNSWNode> nodes_;
    std::unordered_map<std::string, size_t> url_to_node_id_;
    
    mutable std::shared_mutex mutex_;
    std::default_random_engine rng_;
    std::uniform_real_distribution<double> uniform_dist_;
};

} // namespace search_core
