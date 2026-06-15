#include "vlm_infer.hpp"
#include <mutex>
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>

namespace vlm_engine {

VLMModel::VLMModel(const ModelConfig& config) : config_(config) {
    kv_cache_ = std::make_unique<KVCacheManager>(8, config_.num_layers);
}

bool VLMModel::LoadModel() {
    std::unique_lock<std::shared_mutex> lock(model_mutex_);
    std::cout << "[VLMModel] Loading weights from " << config_.model_path << "..." << std::endl;
    // Simulate loading weights into GPU/VRAM
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    is_loaded_ = true;
    std::cout << "[VLMModel] Model successfully loaded into memory." << std::endl;
    return true;
}

bool VLMModel::IsLoaded() const {
    std::shared_lock<std::shared_mutex> lock(model_mutex_);
    return is_loaded_;
}

std::string VLMModel::RunImageDescription(const std::vector<uint8_t>& image_bytes, const std::string& prompt) {
    if (!IsLoaded()) {
        return "Error: VLM model is not loaded in memory.";
    }

    std::string format = "unknown binary format";
    if (image_bytes.size() >= 3 && image_bytes[0] == 0xFF && image_bytes[1] == 0xD8 && image_bytes[2] == 0xFF) {
        format = "JPEG image";
    } else if (image_bytes.size() >= 4 && image_bytes[0] == 0x89 && image_bytes[1] == 0x50 && image_bytes[2] == 0x4E && image_bytes[3] == 0x47) {
        format = "PNG image";
    } else if (image_bytes.size() >= 4 && image_bytes[0] == 'G' && image_bytes[1] == 'I' && image_bytes[2] == 'F' && image_bytes[3] == '8') {
        format = "GIF image";
    }

    std::stringstream ss;
    ss << "This is a high-resolution " << format << " (" << (image_bytes.size() / 1024) << " KB). ";
    
    // Generate description based on prompt heuristics
    std::string prompt_lower = prompt;
    std::transform(prompt_lower.begin(), prompt_lower.end(), prompt_lower.begin(), ::tolower);

    if (prompt_lower.find("weather") != std::string::npos || prompt_lower.find("sky") != std::string::npos) {
        ss << "The scene captures a clear sky with soft afternoon clouds, casting gentle light on the environment.";
    } else if (prompt_lower.find("code") != std::string::npos || prompt_lower.find("programming") != std::string::npos) {
        ss << "The image contains lines of code on a dark-themed text editor, showing a modular C++ implementation of a database system.";
    } else if (prompt_lower.find("chart") != std::string::npos || prompt_lower.find("graph") != std::string::npos) {
        ss << "A data dashboard displaying performance benchmarks, indicating high query throughput and low latency spikes.";
    } else {
        ss << "The visual context shows a modern web workspace. In the center, there is a representation of a distributed cluster, symbolizing scalability, high availability, and real-time data sync.";
    }

    return ss.str();
}

void VLMModel::RunStreamingInference(const std::string& prompt, 
                                     const std::vector<std::string>& history,
                                     const std::vector<uint8_t>& image_bytes,
                                     std::function<void(const std::string& token, bool done)> callback) {
    if (!IsLoaded()) {
        callback("Error: VLM model is not loaded.", true);
        return;
    }

    // Prepare context
    std::string prompt_lower = prompt;
    std::transform(prompt_lower.begin(), prompt_lower.end(), prompt_lower.begin(), ::tolower);

    std::stringstream response_stream;
    
    // Add image details to response if image is provided
    if (!image_bytes.empty()) {
        response_stream << "[Vision Context Analysis] " << RunImageDescription(image_bytes, prompt) << "\n\n";
    }

    if (prompt_lower.find("hello") != std::string::npos || prompt_lower.find("hi") != std::string::npos) {
        response_stream << "Hello! I am the Aether VLM engine. I can help analyze search contexts, interpret images, or discuss the database schema. How can I assist you today?";
    } else if (prompt_lower.find("architecture") != std::string::npos || prompt_lower.find("aether") != std::string::npos) {
        response_stream << "Aether is designed as a modular search system. The components are:\n"
                        << "1. Go Crawler: Continuously scrapes and parses web pages, storing frontiers in Redis.\n"
                        << "2. C++ Search Core: Uses BM25 for text indexes and HNSW Graphs for vector embeddings.\n"
                        << "3. C++ VLM Engine: Serves deep multimodal and visual understanding queries.\n"
                        << "This hybrid design allows millisecond-level similarity searches over millions of vectors.";
    } else if (prompt_lower.find("hnsw") != std::string::npos || prompt_lower.find("vector") != std::string::npos) {
        response_stream << "HNSW (Hierarchical Navigable Small World) structures vectors into hierarchical layers of skip-lists. "
                        << "The top layer has longer connections for fast coarse routing, and the bottom layer has detailed local connections. "
                        << "This avoids the O(N) linear scan of vector databases, giving O(log N) search times.";
    } else {
        response_stream << "I have received your query: \"" << prompt << "\". "
                        << "Based on Aether's contextual vector space, this query maps to high-relevance topics. "
                        << "We can drill down on the search relevance, optimize HNSW graph parameters, or look at crawler concurrency patterns.";
    }

    std::string full_response = response_stream.str();
    std::stringstream word_stream(full_response);
    std::string word;
    
    // Simulate token-by-token generation with brief sleep intervals
    while (word_stream >> word) {
        callback(word + " ", false);
        std::this_thread::sleep_for(std::chrono::milliseconds(35));
    }
    callback("", true);
}

} // namespace vlm_engine
