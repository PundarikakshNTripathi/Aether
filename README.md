# Aether

A distributed, multimodal web search engine built for the AI era. 

Aether replaces traditional inverted-index search by combining a high-concurrency Go web crawler, a C++ HNSW vector retrieval database, and a dedicated C++ Vision-Language Model (VLM) engine for deep semantic contextualization.

## Architecture
* **Frontend:** Next.js / React
* **Crawler Node:** Go + Redis
* **Search Core:** C++20 (BM25 + HNSW Graph)
* **VLM Engine:** C++ (Dedicated Inference Service)
* **Communication:** gRPC / Protocol Buffers

## Quick Start (Docker)
The easiest way to spin up the entire Aether microservice cluster is via Docker Compose.

```bash
git clone [https://github.com/yourusername/Aether.git](https://github.com/yourusername/Aether.git)
cd Aether

# 1. Generate the gRPC stubs for all languages
make proto

# 2. Build and spin up the cluster
docker-compose up --build
```
The search UI will be available at http://localhost:3000.
