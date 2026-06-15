#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>

#include <grpcpp/grpcpp.h>
#include "crawler.grpc.pb.h"
#include "search.grpc.pb.h"

#include "bm25.hpp"
#include "hnsw.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using crawler::CrawlerService;
using crawler::IndexPageRequest;
using crawler::IndexPageResponse;

using search::SearchService;
using search::SearchRequest;
using search::SearchResponse;
using search::SearchResult;

namespace search_core {

// Deterministic hashing vector embedding generator for text (128-dimensional)
std::vector<float> GetTextEmbedding(const std::string& text) {
    std::vector<float> vec(128, 0.0f);
    std::string token;
    for (char ch : text) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            token += std::tolower(static_cast<unsigned char>(ch));
        } else {
            if (!token.empty()) {
                size_t hash = std::hash<std::string>{}(token);
                size_t dim = hash % 128;
                vec[dim] += 1.0f;
                token.clear();
            }
        }
    }
    if (!token.empty()) {
        size_t hash = std::hash<std::string>{}(token);
        size_t dim = hash % 128;
        vec[dim] += 1.0f;
    }
    
    // Normalize vector to unit length (L2 norm)
    float sum_sq = 0.0f;
    for (float val : vec) sum_sq += val * val;
    if (sum_sq > 0.0f) {
        float norm = std::sqrt(sum_sq);
        for (float& val : vec) val /= norm;
    }
    return vec;
}

class AetherSearchCoreServiceImpl final : public CrawlerService::Service, public SearchService::Service {
public:
    AetherSearchCoreServiceImpl() 
        : bm25_index_(1.2, 0.75), hnsw_graph_(128, 16, 64, 32) {}

    // CrawlerService Implementations
    Status IndexPage(ServerContext* context, const IndexPageRequest* request, IndexPageResponse* response) override {
        try {
            std::vector<std::string> img_urls;
            for (const auto& img : request->images()) {
                img_urls.push_back(img.url());
            }

            // 1. Index in BM25 text index
            bm25_index_.AddDocument(request->url(), request->title(), request->raw_text(), img_urls);

            // 2. Generate text embedding vector
            std::vector<float> vec = GetTextEmbedding(request->title() + " " + request->raw_text());

            // 3. Index in HNSW Vector Graph
            hnsw_graph_.Insert(request->url(), request->title(), vec);

            // Keep track of document details locally for fast vector search retrieval
            {
                std::unique_lock<std::shared_mutex> lock(doc_details_mutex_);
                doc_details_[request->url()] = {
                    request->title(),
                    request->raw_text().substr(0, std::min(request->raw_text().size(), size_t(200))),
                    img_urls
                };
            }

            std::cout << "[CrawlerService] Successfully indexed url: " << request->url() << std::endl;
            response->set_success(true);
        } catch (const std::exception& e) {
            std::cerr << "[CrawlerService] Error indexing url: " << request->url() << " - " << e.what() << std::endl;
            response->set_success(false);
            response->set_error_message(e.what());
        }
        return Status::OK;
    }

    // SearchService Implementations
    Status Search(ServerContext* context, const SearchRequest* request, SearchResponse* response) override {
        auto start_time = std::chrono::high_resolution_clock::now();

        std::cout << "[SearchService] Search query received: \"" << request->query() 
                  << "\" (Vector search=" << (request->enable_vector_search() ? "true" : "false") << ")" << std::endl;

        std::vector<SearchResultInternal> final_results;

        if (request->enable_vector_search()) {
            // Vector similarity search
            std::vector<float> query_vec = GetTextEmbedding(request->query());
            auto graph_results = hnsw_graph_.Search(query_vec, request->limit());

            std::shared_lock<std::shared_mutex> lock(doc_details_mutex_);
            for (const auto& res : graph_results) {
                SearchResultInternal sres;
                sres.url = res.url;
                sres.title = res.title;
                sres.score = res.score;
                
                auto detail_it = doc_details_.find(res.url);
                if (detail_it != doc_details_.end()) {
                    sres.snippet = detail_it->second.snippet;
                    sres.image_urls = detail_it->second.image_urls;
                } else {
                    sres.snippet = "Match found via vector search.";
                }
                final_results.push_back(sres);
            }
        } else {
            // Keyword BM25 search
            final_results = bm25_index_.Search(request->query(), request->limit(), request->offset());
        }

        // Populate gRPC response
        for (const auto& item : final_results) {
            auto* proto_res = response->add_results();
            proto_res->set_url(item.url);
            proto_res->set_title(item.title);
            proto_res->set_snippet(item.snippet);
            proto_res->set_score(item.score);
            for (const auto& img_url : item.image_urls) {
                proto_res->add_image_urls(img_url);
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float, std::milli> duration = end_time - start_time;

        response->set_total_results(final_results.size());
        response->set_latency_ms(duration.count());

        std::cout << "[SearchService] Returned " << final_results.size() << " results in " << duration.count() << " ms." << std::endl;
        return Status::OK;
    }

private:
    struct DocDetail {
        std::string title;
        std::string snippet;
        std::vector<std::string> image_urls;
    };

    BM25Index bm25_index_;
    HNSWGraph hnsw_graph_;
    
    std::unordered_map<std::string, DocDetail> doc_details_;
    std::shared_mutex doc_details_mutex_;
};

} // namespace search_core

int main(int argc, char** argv) {
    std::string server_address("0.0.0.0:50051");
    search_core::AetherSearchCoreServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    
    // Register the service classes as implementation for both gRPC service definitions
    builder.RegisterService(static_cast<CrawlerService::Service*>(&service));
    builder.RegisterService(static_cast<SearchService::Service*>(&service));

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Aether Search Core gRPC Server listening on " << server_address << std::endl;
    server->Wait();

    return 0;
}
