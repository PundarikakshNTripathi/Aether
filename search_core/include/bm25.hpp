#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>

namespace search_core {

struct Document {
    std::string url;
    std::string title;
    std::string raw_text;
    std::vector<std::string> image_urls;
};

struct SearchResultInternal {
    std::string url;
    std::string title;
    std::string snippet;
    float score;
    std::vector<std::string> image_urls;
};

class BM25Index {
public:
    BM25Index(double k1 = 1.2, double b = 0.75);

    void AddDocument(const std::string& url, const std::string& title, 
                     const std::string& raw_text, const std::vector<std::string>& image_urls);
    
    std::vector<SearchResultInternal> Search(const std::string& query, int limit = 10, int offset = 0);

    size_t GetDocCount() const;

private:
    std::vector<std::string> Tokenize(const std::string& text) const;
    std::string GenerateSnippet(const std::string& text, const std::vector<std::string>& query_tokens) const;

    double k1_;
    double b_;
    
    mutable std::shared_mutex mutex_;
    
    std::vector<Document> documents_;
    std::unordered_map<std::string, std::vector<std::pair<size_t, uint32_t>>> inverted_index_; // term -> list of {doc_idx, tf}
    std::vector<size_t> doc_lengths_;
    double avg_doc_length_ = 0.0;
    size_t total_terms_across_docs_ = 0;
};

} // namespace search_core
