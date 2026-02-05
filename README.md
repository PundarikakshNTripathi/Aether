# Aether

**Aether** is a hybrid search engine built to deconstruct the architecture of modern information retrieval systems. It implements a full indexing and retrieval pipeline from scratch, combining classical keyword search with modern neural capabilities.

It is designed to bridge the gap between traditional search algorithms (ranking based on term frequency) and semantic understanding (retrieval based on meaning).

## Architecture
The system consists of two distinct subsystems:
1.  **The Indexer (Crawler & Processor):** * Ingests raw documents.
    * Tokenizes and normalizes text.
    * Builds a forward index and an efficient Inverted Index (Hash Map based).
    * Generates vector embeddings for semantic chunks.
    
2.  **The Retriever (Query Engine):**
    * **Keyword Path:** Executes boolean and ranked retrieval using BM25/TF-IDF scoring.
    * **Semantic Path:** Performs Cosine Similarity search on vector space.
    * **Re-ranking:** Agreggates results to return the most contextually relevant documents.

## Key Features
* **From-Scratch Indexing:** No high-level search libraries (like Elasticsearch); the data structures for the inverted index are custom-built.
* **Ranking Algorithms:** Manual implementation of BM25 for probabilistic relevance scoring.
* **Hybrid Search:** Capable of retrieving documents that match the *intent* of a query even if exact keywords are missing.

## Usage
```python
from aether.index import Indexer
from aether.search import Engine

# Build the index
idx = Indexer(corpus_path="./data")
idx.build()

# Query
engine = Engine(idx)
results = engine.search("distributed consensus mechanisms", mode="hybrid")
```
