#include "bm25.hpp"
#include <mutex>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace search_core {

BM25Index::BM25Index(double k1, double b) : k1_(k1), b_(b) {}

std::vector<std::string> BM25Index::Tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::string token;
    for (char ch : text) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            token += std::tolower(static_cast<unsigned char>(ch));
        } else {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        }
    }
    if (!token.empty()) {
        tokens.push_back(token);
    }
    return tokens;
}

void BM25Index::AddDocument(const std::string& url, const std::string& title, 
                           const std::string& raw_text, const std::vector<std::string>& image_urls) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    size_t doc_id = documents_.size();
    documents_.push_back({url, title, raw_text, image_urls});
    
    std::vector<std::string> tokens = Tokenize(title + " " + raw_text);
    doc_lengths_.push_back(tokens.size());
    total_terms_across_docs_ += tokens.size();
    avg_doc_length_ = static_cast<double>(total_terms_across_docs_) / documents_.size();
    
    std::unordered_map<std::string, uint32_t> term_counts;
    for (const auto& token : tokens) {
        term_counts[token]++;
    }
    
    for (const auto& [term, count] : term_counts) {
        inverted_index_[term].push_back({doc_id, count});
    }
}

std::vector<SearchResultInternal> BM25Index::Search(const std::string& query, int limit, int offset) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::string> query_tokens = Tokenize(query);
    if (query_tokens.empty() || documents_.empty()) {
        return {};
    }
    
    std::unordered_map<size_t, double> doc_scores;
    size_t N = documents_.size();
    
    for (const auto& term : query_tokens) {
        auto it = inverted_index_.find(term);
        if (it == inverted_index_.end()) {
            continue;
        }
        
        const auto& postings = it->second;
        size_t n_q = postings.size(); // number of docs containing term
        
        // Compute IDF
        double idf = std::log(1.0 + (N - n_q + 0.5) / (n_q + 0.5));
        if (idf < 0) idf = 0.0001; // Avoid negative IDFs for extremely common terms
        
        for (const auto& [doc_id, tf] : postings) {
            double doc_len = doc_lengths_[doc_id];
            double numerator = tf * (k1_ + 1.0);
            double denominator = tf + k1_ * (1.0 - b_ + b_ * (doc_len / avg_doc_length_));
            doc_scores[doc_id] += idf * (numerator / denominator);
        }
    }
    
    std::vector<std::pair<size_t, double>> ranked_docs(doc_scores.begin(), doc_scores.end());
    std::sort(ranked_docs.begin(), ranked_docs.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });
    
    std::vector<SearchResultInternal> results;
    int start = std::min(static_cast<int>(ranked_docs.size()), offset);
    int end = std::min(static_cast<int>(ranked_docs.size()), offset + limit);
    
    for (int i = start; i < end; ++i) {
        size_t doc_id = ranked_docs[i].first;
        double score = ranked_docs[i].second;
        const auto& doc = documents_[doc_id];
        
        results.push_back({
            doc.url,
            doc.title,
            GenerateSnippet(doc.raw_text, query_tokens),
            static_cast<float>(score),
            doc.image_urls
        });
    }
    
    return results;
}

size_t BM25Index::GetDocCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return documents_.size();
}

std::string BM25Index::GenerateSnippet(const std::string& text, const std::vector<std::string>& query_tokens) const {
    if (text.empty()) return "";
    
    // Find the sentence containing the most query tokens
    std::vector<std::string> sentences;
    std::string sentence;
    std::istringstream stream(text);
    
    // Simple sentence splitter
    char ch;
    while (stream.get(ch)) {
        sentence += ch;
        if (ch == '.' || ch == '?' || ch == '!') {
            // Trim leading whitespace
            size_t first = sentence.find_first_not_of(" \t\n\r");
            if (first != std::string::npos) {
                sentence = sentence.substr(first);
            }
            if (!sentence.empty()) {
                sentences.push_back(sentence);
            }
            sentence.clear();
        }
    }
    if (!sentence.empty()) {
        size_t first = sentence.find_first_not_of(" \t\n\r");
        if (first != std::string::npos) {
            sentence = sentence.substr(first);
        }
        if (!sentence.empty()) {
            sentences.push_back(sentence);
        }
    }
    
    if (sentences.empty()) {
        return text.substr(0, std::min(text.size(), size_t(160))) + "...";
    }
    
    size_t best_sentence_idx = 0;
    size_t max_matches = 0;
    
    for (size_t i = 0; i < sentences.size(); ++i) {
        std::vector<std::string> tokens = Tokenize(sentences[i]);
        size_t matches = 0;
        for (const auto& token : tokens) {
            if (std::find(query_tokens.begin(), query_tokens.end(), token) != query_tokens.end()) {
                matches++;
            }
        }
        if (matches > max_matches) {
            max_matches = matches;
            best_sentence_idx = i;
        }
    }
    
    std::string snippet = sentences[best_sentence_idx];
    if (best_sentence_idx + 1 < sentences.size()) {
        snippet += " " + sentences[best_sentence_idx + 1];
    }
    if (snippet.size() > 200) {
        snippet = snippet.substr(0, 197) + "...";
    }
    return snippet;
}

} // namespace search_core
