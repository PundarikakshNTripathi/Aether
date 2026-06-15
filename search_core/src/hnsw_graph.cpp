#include "hnsw.hpp"
#include <mutex>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace search_core {

HNSWGraph::HNSWGraph(size_t dim, size_t M, size_t ef_construction, size_t ef_search)
    : dim_(dim), M_(M), M0_(2 * M), ef_construction_(ef_construction), ef_search_(ef_search) {
    mL_ = 1.0 / std::log(static_cast<double>(M_));
    rng_.seed(42); // deterministic seed for reproducibility
    uniform_dist_ = std::uniform_real_distribution<double>(0.0, 1.0);
}

float HNSWGraph::CosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) const {
    if (a.size() != b.size() || a.empty()) return 0.0f;
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    if (norm_a == 0.0f || norm_b == 0.0f) return 0.0f;
    return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

float HNSWGraph::L2Distance(const std::vector<float>& a, const std::vector<float>& b) const {
    float dist = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        float diff = a[i] - b[i];
        dist += diff * diff;
    }
    return dist;
}

size_t HNSWGraph::GetRandomLevel() {
    double r = uniform_dist_(rng_);
    if (r == 0.0) r = 0.0000001; // Avoid log(0)
    return static_cast<size_t>(-std::log(r) * mL_);
}

std::vector<size_t> HNSWGraph::SearchLayer(const std::vector<float>& query_vec, 
                                           const std::vector<size_t>& enter_nodes, 
                                           size_t ef, size_t level) const {
    std::unordered_set<size_t> visited(enter_nodes.begin(), enter_nodes.end());
    
    // Max heap to keep the best ef results (furthest node is at top)
    auto cmp_far = [this, &query_vec](size_t left, size_t right) {
        return this->L2Distance(this->nodes_[left].vector, query_vec) < 
               this->L2Distance(this->nodes_[right].vector, query_vec);
    };
    std::priority_queue<size_t, std::vector<size_t>, decltype(cmp_far)> W(cmp_far);
    
    // Min heap for candidates to explore (closest node is at top)
    auto cmp_close = [this, &query_vec](size_t left, size_t right) {
        return this->L2Distance(this->nodes_[left].vector, query_vec) > 
               this->L2Distance(this->nodes_[right].vector, query_vec);
    };
    std::priority_queue<size_t, std::vector<size_t>, decltype(cmp_close)> candidates(cmp_close);
    
    for (size_t en : enter_nodes) {
        W.push(en);
        candidates.push(en);
    }
    
    while (!candidates.empty()) {
        size_t curr = candidates.top();
        candidates.pop();
        
        float curr_dist = L2Distance(nodes_[curr].vector, query_vec);
        float furthest_dist = L2Distance(nodes_[W.top()].vector, query_vec);
        
        if (curr_dist > furthest_dist) {
            break;
        }
        
        const auto& neighbors = nodes_[curr].neighbors[level];
        for (size_t neighbor : neighbors) {
            if (visited.find(neighbor) == visited.end()) {
                visited.insert(neighbor);
                
                float neighbor_dist = L2Distance(nodes_[neighbor].vector, query_vec);
                float w_top_dist = L2Distance(nodes_[W.top()].vector, query_vec);
                
                if (neighbor_dist < w_top_dist || W.size() < ef) {
                    candidates.push(neighbor);
                    W.push(neighbor);
                    if (W.size() > ef) {
                        W.pop();
                    }
                }
            }
        }
    }
    
    std::vector<size_t> results;
    while (!W.empty()) {
        results.push_back(W.top());
        W.pop();
    }
    return results;
}

void HNSWGraph::Insert(const std::string& url, const std::string& title, const std::vector<float>& vec) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // Check if URL is already indexed. If so, update vector.
    auto it = url_to_node_id_.find(url);
    if (it != url_to_node_id_.end()) {
        nodes_[it->second].vector = vec;
        return;
    }
    
    size_t new_id = nodes_.size();
    size_t insert_level = GetRandomLevel();
    
    HNSWNode new_node;
    new_node.id = new_id;
    new_node.url = url;
    new_node.title = title;
    new_node.vector = vec;
    new_node.neighbors.resize(insert_level + 1);
    
    nodes_.push_back(new_node);
    url_to_node_id_[url] = new_id;
    
    if (!has_enter_node_) {
        enter_node_id_ = new_id;
        max_level_ = static_cast<int>(insert_level);
        has_enter_node_ = true;
        return;
    }
    
    size_t curr_enter_node = enter_node_id_;
    std::vector<size_t> enter_nodes = {curr_enter_node};
    
    // 1. Search from the top level down to insert_level + 1
    int l = max_level_;
    for (; l > static_cast<int>(insert_level); --l) {
        enter_nodes = SearchLayer(vec, enter_nodes, 1, l);
    }
    
    // 2. Search and insert from min(insert_level, max_level) down to 0
    for (; l >= 0; --l) {
        enter_nodes = SearchLayer(vec, enter_nodes, ef_construction_, l);
        
        // Find nearest neighbors to connect
        std::vector<std::pair<float, size_t>> neighbors_with_dist;
        for (size_t cand : enter_nodes) {
            if (cand != new_id) {
                neighbors_with_dist.push_back({L2Distance(nodes_[cand].vector, vec), cand});
            }
        }
        std::sort(neighbors_with_dist.begin(), neighbors_with_dist.end());
        
        size_t current_M = (l == 0) ? M0_ : M_;
        size_t num_to_connect = std::min(current_M, neighbors_with_dist.size());
        
        for (size_t i = 0; i < num_to_connect; ++i) {
            size_t neighbor_id = neighbors_with_dist[i].second;
            
            // Connect new_node -> neighbor
            nodes_[new_id].neighbors[l].push_back(neighbor_id);
            // Connect neighbor -> new_node
            nodes_[neighbor_id].neighbors[l].push_back(new_id);
            
            // Shrink connections of the neighbor if needed
            if (nodes_[neighbor_id].neighbors[l].size() > current_M) {
                std::vector<std::pair<float, size_t>> shrink_candidates;
                for (size_t n : nodes_[neighbor_id].neighbors[l]) {
                    shrink_candidates.push_back({L2Distance(nodes_[neighbor_id].vector, nodes_[n].vector), n});
                }
                std::sort(shrink_candidates.begin(), shrink_candidates.end());
                nodes_[neighbor_id].neighbors[l].clear();
                for (size_t j = 0; j < current_M; ++j) {
                    nodes_[neighbor_id].neighbors[l].push_back(shrink_candidates[j].second);
                }
            }
        }
    }
    
    if (static_cast<int>(insert_level) > max_level_) {
        max_level_ = static_cast<int>(insert_level);
        enter_node_id_ = new_id;
    }
}

std::vector<HNSWGraph::SearchResult> HNSWGraph::Search(const std::vector<float>& query_vec, size_t k) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (!has_enter_node_ || nodes_.empty()) {
        return {};
    }
    
    std::vector<size_t> enter_nodes = {enter_node_id_};
    
    // Walk down to level 1
    for (int l = max_level_; l >= 1; --l) {
        enter_nodes = SearchLayer(query_vec, enter_nodes, 1, l);
    }
    
    // Search level 0 with ef_search
    std::vector<size_t> raw_results = SearchLayer(query_vec, enter_nodes, ef_search_, 0);
    
    std::vector<std::pair<float, size_t>> ranked_results;
    for (size_t id : raw_results) {
        ranked_results.push_back({CosineSimilarity(nodes_[id].vector, query_vec), id});
    }
    std::sort(ranked_results.begin(), ranked_results.end(), [](const auto& a, const auto& b) {
        return a.first > b.first; // highest similarity first
    });
    
    std::vector<SearchResult> search_results;
    size_t count = std::min(k, ranked_results.size());
    for (size_t i = 0; i < count; ++i) {
        size_t id = ranked_results[i].second;
        search_results.push_back({
            nodes_[id].url,
            nodes_[id].title,
            ranked_results[i].first
        });
    }
    
    return search_results;
}

size_t HNSWGraph::GetNodeCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return nodes_.size();
}

} // namespace search_core
