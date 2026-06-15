"use client";

import React, { useState, useRef, useEffect } from "react";
import { 
  Search, 
  Image as ImageIcon, 
  Zap, 
  Sparkles, 
  Cpu, 
  FileText, 
  Globe, 
  Send, 
  Trash2,
  HelpCircle,
  Code
} from "lucide-react";

interface SearchResult {
  url: string;
  title: string;
  snippet: string;
  score: number;
  imageUrls: string[];
}

interface ChatMessage {
  role: "user" | "vlm";
  text: string;
  image?: string;
}

export default function Home() {
  const [query, setQuery] = useState("");
  const [mode, setMode] = useState<"text" | "vector" | "vlm">("text");
  const [image, setImage] = useState<string | null>(null);
  const [results, setResults] = useState<SearchResult[]>([]);
  const [chatHistory, setChatHistory] = useState<ChatMessage[]>([]);
  const [loading, setLoading] = useState(false);
  const [latency, setLatency] = useState<number | null>(null);
  const [totalResults, setTotalResults] = useState<string | null>(null);
  const [vlmInput, setVlmInput] = useState("");
  
  const fileInputRef = useRef<HTMLInputElement>(null);
  const chatEndRef = useRef<HTMLDivElement>(null);

  // Auto scroll chat to bottom when VLM streams text
  useEffect(() => {
    chatEndRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [chatHistory]);

  const handleImageUpload = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (file) {
      const reader = new FileReader();
      reader.onloadend = () => {
        setImage(reader.result as string);
      };
      reader.readAsDataURL(file);
    }
  };

  const clearImage = () => {
    setImage(null);
    if (fileInputRef.current) {
      fileInputRef.current.value = "";
    }
  };

  const runSearch = async (searchQuery: string, searchMode: "text" | "vector") => {
    if (!searchQuery.trim()) return;
    setLoading(true);
    setResults([]);
    setLatency(null);
    setTotalResults(null);

    try {
      const response = await fetch("/api/search", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ query: searchQuery, mode: searchMode }),
      });

      const data = await response.json();
      if (data.success) {
        setResults(data.results);
        setLatency(data.latencyMs);
        setTotalResults(data.totalResults);
      } else {
        console.error("Search failed:", data.error);
      }
    } catch (err) {
      console.error("Search error:", err);
    } finally {
      setLoading(false);
    }
  };

  const runVLMChat = async (e: React.FormEvent) => {
    e.preventDefault();
    const prompt = vlmInput.trim();
    if (!prompt && !image) return;

    setVlmInput("");
    setLoading(true);

    const userMessage: ChatMessage = {
      role: "user",
      text: prompt || "Analyze this image",
      image: image || undefined,
    };

    const newHistory = [...chatHistory, userMessage];
    setChatHistory(newHistory);

    // Add empty placeholder message for VLM stream response
    const vlmPlaceholderIndex = newHistory.length;
    setChatHistory(prev => [...prev, { role: "vlm", text: "" }]);

    try {
      // Build VLM prompt history
      const vlmHistory = chatHistory
        .filter(m => m.role === "user")
        .map(m => m.text)
        .slice(-5); // Keep last 5 user queries as context

      const response = await fetch("/api/search", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          query: prompt,
          mode: "vlm",
          image: image,
          history: vlmHistory,
        }),
      });

      if (!response.body) {
        throw new Error("ReadableStream not supported on VLM response");
      }

      const reader = response.body.getReader();
      const decoder = new TextDecoder();
      let streamedResponse = "";

      while (true) {
        const { value, done } = await reader.read();
        if (done) break;

        const textChunk = decoder.decode(value);
        // Clean JSON lines chunks
        const lines = textChunk.split("\n").filter(l => l.trim());
        
        for (const line of lines) {
          try {
            const data = JSON.parse(line);
            if (data.token) {
              streamedResponse += data.token;
              setChatHistory(prev => {
                const updated = [...prev];
                if (updated[vlmPlaceholderIndex]) {
                  updated[vlmPlaceholderIndex] = {
                    role: "vlm",
                    text: streamedResponse,
                  };
                }
                return updated;
              });
            }
          } catch (jsonErr) {
            // Partial JSON buffer, wait for next chunk
          }
        }
      }
    } catch (err) {
      console.error("VLM Chat error:", err);
      setChatHistory(prev => {
        const updated = [...prev];
        if (updated[vlmPlaceholderIndex]) {
          updated[vlmPlaceholderIndex] = {
            role: "vlm",
            text: "Error connecting to VLM server. Check Docker status.",
          };
        }
        return updated;
      });
    } finally {
      setLoading(false);
      clearImage();
    }
  };

  const handleSearchSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (mode === "vlm") {
      runVLMChat(e);
    } else {
      runSearch(query, mode);
    }
  };

  const handlePresetClick = (presetQuery: string, presetMode: "text" | "vector" | "vlm") => {
    setMode(presetMode);
    if (presetMode === "vlm") {
      setVlmInput(presetQuery);
    } else {
      setQuery(presetQuery);
      runSearch(presetQuery, presetMode);
    }
  };

  return (
    <div style={{ minHeight: "100vh", position: "relative" }}>
      <div className="bg-mesh" />
      
      {/* Top Navigation Bar */}
      <header style={styles.header}>
        <div style={styles.logoContainer}>
          <div style={styles.logoBadge}>Æ</div>
          <span style={styles.logoText}>AETHER</span>
          <span style={styles.badge}>Multimodal v1.0</span>
        </div>
        <div style={styles.headerStats}>
          <div style={styles.statItem}>
            <Globe size={14} color="#6366f1" />
            <span>Crawler: Active</span>
          </div>
          <div style={styles.statItem}>
            <Cpu size={14} color="#10b981" />
            <span>Search Core: Online</span>
          </div>
        </div>
      </header>

      {/* Main Body Layout */}
      <main style={styles.mainContainer}>
        {/* Search Control Board */}
        <section style={styles.controlBoard}>
          <h1 style={styles.mainTitle}>
            Discover the Web, <span style={styles.gradientText}>Contextually.</span>
          </h1>
          <p style={styles.subtitle}>
            Aether replaces indices with hyper-concurrency and deep multimodal vector routing.
          </p>

          {/* Mode Selector Panel */}
          <div style={styles.modeSelector}>
            <button 
              className={`toggle-button ${mode === "text" ? "active" : ""}`}
              onClick={() => { setMode("text"); setResults([]); }}
            >
              <FileText size={14} style={{ marginRight: 6 }} />
              Traditional (BM25)
            </button>
            <button 
              className={`toggle-button ${mode === "vector" ? "active" : ""}`}
              onClick={() => { setMode("vector"); setResults([]); }}
            >
              <Zap size={14} style={{ marginRight: 6 }} />
              Vector (HNSW Graph)
            </button>
            <button 
              className={`toggle-button ${mode === "vlm" ? "active" : ""}`}
              onClick={() => { setMode("vlm"); setResults([]); }}
            >
              <Sparkles size={14} style={{ marginRight: 6 }} />
              Multimodal VLM
            </button>
          </div>

          {/* Search Inputs */}
          {mode !== "vlm" ? (
            <form onSubmit={handleSearchSubmit} style={{ width: "100%", display: "flex", justifyContent: "center" }}>
              <div className="search-container">
                <Search size={18} color="#9ca3af" />
                <input 
                  type="text" 
                  value={query}
                  onChange={(e) => setQuery(e.target.value)}
                  placeholder={
                    mode === "text" 
                      ? "Search indexed web pages via BM25 keyword matching..." 
                      : "Search pages semantically via HNSW vector graph matching..."
                  }
                  className="search-input"
                />
                <button type="submit" className="btn-primary" disabled={loading}>
                  {loading ? "Routing..." : "Search"}
                </button>
              </div>
            </form>
          ) : (
            <div style={styles.vlmInputBox} className="glass-panel">
              {/* Image Uploader Interface */}
              <div style={styles.imageUploadArea}>
                {!image ? (
                  <button 
                    type="button"
                    onClick={() => fileInputRef.current?.click()}
                    style={styles.uploadBtn}
                  >
                    <ImageIcon size={16} />
                    <span>Upload Image context</span>
                  </button>
                ) : (
                  <div style={styles.imagePreviewContainer}>
                    <img src={image} alt="uploaded preview" style={styles.imagePreview} />
                    <button type="button" onClick={clearImage} style={styles.removeImageBtn}>
                      <Trash2 size={12} />
                    </button>
                  </div>
                )}
                <input 
                  type="file" 
                  ref={fileInputRef} 
                  onChange={handleImageUpload} 
                  accept="image/*" 
                  style={{ display: "none" }}
                />
              </div>

              {/* Chat Input Area */}
              <form onSubmit={handleSearchSubmit} style={styles.vlmForm}>
                <input 
                  type="text" 
                  value={vlmInput}
                  onChange={(e) => setVlmInput(e.target.value)}
                  placeholder="Ask the Vision-Language Model about the image, or chat..."
                  style={styles.vlmTextInput}
                />
                <button type="submit" style={styles.vlmSendBtn} disabled={loading && !vlmInput}>
                  <Send size={14} />
                </button>
              </form>
            </div>
          )}
        </section>

        {/* Results & Chat Visualizer */}
        <section style={styles.contentVisualizer}>
          {loading && results.length === 0 && mode !== "vlm" && (
            <div style={styles.loadingContainer}>
              {[1, 2, 3].map((i) => (
                <div key={i} className="glass-panel shimmer" style={styles.skeletonCard} />
              ))}
            </div>
          )}

          {/* Search Latency Stat Panel */}
          {latency !== null && (
            <div style={styles.latencyBar}>
              <Cpu size={14} color="#8b5cf6" />
              <span>
                Search latency: <strong>{latency.toFixed(2)} ms</strong> | Retrieved <strong>{totalResults}</strong> documents
              </span>
            </div>
          )}

          {/* BM25 / Vector Search Results Grid */}
          {mode !== "vlm" && results.length > 0 && (
            <div style={styles.resultsList}>
              {results.map((item, index) => (
                <article key={index} className="glass-panel" style={styles.resultCard}>
                  <a href={item.url} target="_blank" rel="noreferrer" style={styles.resultTitleLink}>
                    <h3 style={styles.resultTitle}>{item.title || item.url}</h3>
                  </a>
                  <p style={styles.resultUrl}>{item.url}</p>
                  <p style={styles.resultSnippet}>{item.snippet}</p>
                  
                  {item.imageUrls && item.imageUrls.length > 0 && (
                    <div style={styles.resultImages}>
                      {item.imageUrls.map((imgUrl, imgIdx) => (
                        <div key={imgIdx} style={styles.resultImageWrapper}>
                          <img src={imgUrl} alt="crawled thumbnail" style={styles.resultImage} onError={(e)=>{(e.target as HTMLElement).style.display='none'}} />
                        </div>
                      ))}
                    </div>
                  )}

                  <div style={styles.resultFooter}>
                    <span style={styles.resultScore}>
                      Match Score: <strong>{item.score.toFixed(4)}</strong>
                    </span>
                  </div>
                </article>
              ))}
            </div>
          )}

          {/* VLM Chat Interface */}
          {mode === "vlm" && (
            <div className="glass-panel" style={styles.chatContainer}>
              <div style={styles.chatHeader}>
                <div style={styles.chatTitleGroup}>
                  <Sparkles size={16} color="#8b5cf6" />
                  <span style={styles.chatTitle}>Multimodal VLM Assistant</span>
                </div>
                <button 
                  onClick={() => setChatHistory([])}
                  style={styles.clearChatBtn}
                  title="Clear Conversation"
                >
                  <Trash2 size={14} />
                  <span>Reset</span>
                </button>
              </div>

              <div style={styles.chatMessagesArea}>
                {chatHistory.length === 0 ? (
                  <div style={styles.chatEmptyState}>
                    <HelpCircle size={32} color="#4b5563" style={{ marginBottom: 12 }} />
                    <p>Start a conversation. Upload an image to analyze it with the visual language model, or ask architecture questions.</p>
                  </div>
                ) : (
                  chatHistory.map((msg, idx) => (
                    <div 
                      key={idx} 
                      style={{
                        ...styles.chatBubble,
                        alignSelf: msg.role === "user" ? "flex-end" : "flex-start",
                        background: msg.role === "user" ? "rgba(99, 102, 241, 0.15)" : "rgba(255, 255, 255, 0.03)",
                        borderColor: msg.role === "user" ? "rgba(99, 102, 241, 0.3)" : "rgba(255, 255, 255, 0.06)",
                      }}
                    >
                      {msg.image && (
                        <div style={styles.chatBubbleImageWrapper}>
                          <img src={msg.image} alt="User upload context" style={styles.chatBubbleImage} />
                        </div>
                      )}
                      <p style={{ whiteSpace: "pre-wrap", fontSize: "0.95rem" }}>
                        {msg.text || (
                          <span style={styles.typingIndicator}>Thinking...</span>
                        )}
                      </p>
                    </div>
                  ))
                )}
                <div ref={chatEndRef} />
              </div>
            </div>
          )}

          {/* Preset Prompts suggestions */}
          {!loading && results.length === 0 && chatHistory.length === 0 && (
            <div style={styles.presetsGrid}>
              <h4 style={styles.presetsTitle}>Or explore these indexing & search queries:</h4>
              <div style={styles.presetButtons}>
                <button 
                  style={styles.presetBtn}
                  onClick={() => handlePresetClick("Explain HNSW vector similarity routing layers", "vector")}
                >
                  <Zap size={12} color="#3b82f6" />
                  <span>Explain HNSW layers (Vector Search)</span>
                </button>
                <button 
                  style={styles.presetBtn}
                  onClick={() => handlePresetClick("Describe the Aether multimodal search cluster layout", "text")}
                >
                  <FileText size={12} color="#10b981" />
                  <span>Search engine architecture (BM25)</span>
                </button>
                <button 
                  style={styles.presetBtn}
                  onClick={() => handlePresetClick("How does the VLM Engine custom KV cache save memory during token generation?", "vlm")}
                >
                  <Sparkles size={12} color="#8b5cf6" />
                  <span>Ask about KV caching (VLM Chat)</span>
                </button>
              </div>
            </div>
          )}
        </section>
      </main>

      {/* Footer */}
      <footer style={styles.footer}>
        <p>Aether Distributed Multimodal Search Engine • Coded with C++20, Go, & Next.js</p>
      </footer>
    </div>
  );
}

const styles = {
  header: {
    display: "flex",
    justifyContent: "space-between",
    alignItems: "center",
    padding: "20px 40px",
    borderBottom: "1px solid rgba(255, 255, 255, 0.05)",
    background: "rgba(10, 11, 16, 0.7)",
    backdropFilter: "blur(8px)",
    position: "sticky" as "sticky",
    top: 0,
    zIndex: 100,
  },
  logoContainer: {
    display: "flex",
    alignItems: "center",
    gap: "10px",
  },
  logoBadge: {
    background: "linear-gradient(135deg, #3b82f6 0%, #8b5cf6 100%)",
    color: "white",
    width: "28px",
    height: "28px",
    borderRadius: "8px",
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
    fontWeight: "bold",
    fontFamily: "var(--font-family-display)",
    fontSize: "1.1rem",
  },
  logoText: {
    fontFamily: "var(--font-family-display)",
    fontWeight: "700",
    fontSize: "1.2rem",
    letterSpacing: "0.1em",
  },
  badge: {
    fontSize: "0.75rem",
    background: "rgba(99, 102, 241, 0.15)",
    color: "#8b5cf6",
    padding: "2px 8px",
    borderRadius: "99px",
    border: "1px solid rgba(99, 102, 241, 0.3)",
    fontWeight: 500,
  },
  headerStats: {
    display: "flex",
    gap: "20px",
  },
  statItem: {
    display: "flex",
    alignItems: "center",
    gap: "6px",
    fontSize: "0.8rem",
    color: "#9ca3af",
  },
  mainContainer: {
    maxWidth: "1100px",
    margin: "0 auto",
    padding: "60px 20px 100px 20px",
    display: "flex",
    flexDirection: "column" as "column",
    gap: "40px",
  },
  controlBoard: {
    display: "flex",
    flexDirection: "column" as "column",
    alignItems: "center",
    textAlign: "center" as "center",
    gap: "24px",
  },
  mainTitle: {
    fontSize: "3rem",
    fontWeight: "800",
    lineHeight: "1.15",
  },
  gradientText: {
    background: "linear-gradient(90deg, #3b82f6 0%, #8b5cf6 100%)",
    WebkitBackgroundClip: "text",
    WebkitTextFillColor: "transparent",
  },
  subtitle: {
    fontSize: "1.1rem",
    color: "#9ca3af",
    maxWidth: "600px",
  },
  modeSelector: {
    display: "inline-flex",
    background: "rgba(255, 255, 255, 0.03)",
    border: "1px solid rgba(255, 255, 255, 0.05)",
    padding: "4px",
    borderRadius: "99px",
  },
  vlmInputBox: {
    width: "100%",
    maxWidth: "680px",
    padding: "16px",
    display: "flex",
    flexDirection: "column" as "column",
    gap: "14px",
    alignItems: "stretch",
  },
  imageUploadArea: {
    display: "flex",
    alignItems: "center",
  },
  uploadBtn: {
    display: "flex",
    alignItems: "center",
    gap: "8px",
    background: "rgba(255, 255, 255, 0.04)",
    border: "1px dashed rgba(255, 255, 255, 0.15)",
    borderRadius: "8px",
    padding: "8px 16px",
    color: "#d1d5db",
    cursor: "pointer",
    fontSize: "0.85rem",
    transition: "all 0.2s",
  },
  imagePreviewContainer: {
    position: "relative" as "relative",
    borderRadius: "8px",
    overflow: "hidden",
    border: "1px solid rgba(255, 255, 255, 0.15)",
  },
  imagePreview: {
    height: "64px",
    width: "auto",
    display: "block",
  },
  removeImageBtn: {
    position: "absolute" as "absolute",
    top: 2,
    right: 2,
    background: "rgba(0, 0, 0, 0.7)",
    border: "none",
    color: "#ef4444",
    borderRadius: "50%",
    width: "18px",
    height: "18px",
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
    cursor: "pointer",
  },
  vlmForm: {
    display: "flex",
    gap: "8px",
    width: "100%",
  },
  vlmTextInput: {
    flex: 1,
    background: "rgba(0, 0, 0, 0.2)",
    border: "1px solid rgba(255, 255, 255, 0.08)",
    borderRadius: "8px",
    padding: "10px 14px",
    color: "#f3f4f6",
    fontSize: "0.95rem",
    outline: "none",
  },
  vlmSendBtn: {
    background: "linear-gradient(135deg, #3b82f6 0%, #8b5cf6 100%)",
    color: "white",
    border: "none",
    borderRadius: "8px",
    width: "40px",
    cursor: "pointer",
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
  },
  contentVisualizer: {
    width: "100%",
  },
  loadingContainer: {
    display: "flex",
    flexDirection: "column" as "column",
    gap: "20px",
  },
  skeletonCard: {
    height: "120px",
    borderRadius: "16px",
  },
  latencyBar: {
    display: "inline-flex",
    alignItems: "center",
    gap: "8px",
    background: "rgba(139, 92, 246, 0.08)",
    border: "1px solid rgba(139, 92, 246, 0.2)",
    borderRadius: "8px",
    padding: "6px 14px",
    fontSize: "0.85rem",
    color: "#a78bfa",
    marginBottom: "20px",
  },
  resultsList: {
    display: "flex",
    flexDirection: "column" as "column",
    gap: "20px",
  },
  resultCard: {
    padding: "24px",
    display: "flex",
    flexDirection: "column" as "column",
    gap: "10px",
  },
  resultTitleLink: {
    textDecoration: "none",
    color: "inherit",
    alignSelf: "flex-start",
  },
  resultTitle: {
    fontSize: "1.25rem",
    fontWeight: "600",
    color: "#60a5fa",
    transition: "color 0.2s",
  },
  resultUrl: {
    fontSize: "0.8rem",
    color: "#10b981",
    wordBreak: "break-all" as "break-all",
  },
  resultSnippet: {
    fontSize: "0.95rem",
    color: "#d1d5db",
  },
  resultImages: {
    display: "flex",
    gap: "10px",
    flexWrap: "wrap" as "wrap",
    marginTop: "8px",
  },
  resultImageWrapper: {
    borderRadius: "6px",
    overflow: "hidden",
    border: "1px solid rgba(255, 255, 255, 0.1)",
  },
  resultImage: {
    height: "60px",
    display: "block",
  },
  resultFooter: {
    borderTop: "1px solid rgba(255, 255, 255, 0.05)",
    paddingTop: "12px",
    marginTop: "6px",
  },
  resultScore: {
    fontSize: "0.8rem",
    color: "#9ca3af",
  },
  chatContainer: {
    display: "flex",
    flexDirection: "column" as "column",
    height: "460px",
    overflow: "hidden",
  },
  chatHeader: {
    display: "flex",
    justifyContent: "space-between",
    alignItems: "center",
    padding: "16px 20px",
    borderBottom: "1px solid rgba(255, 255, 255, 0.06)",
  },
  chatTitleGroup: {
    display: "flex",
    alignItems: "center",
    gap: "8px",
  },
  chatTitle: {
    fontWeight: "600",
    fontSize: "0.95rem",
  },
  clearChatBtn: {
    display: "flex",
    alignItems: "center",
    gap: "6px",
    background: "transparent",
    border: "none",
    color: "#9ca3af",
    cursor: "pointer",
    fontSize: "0.8rem",
  },
  chatMessagesArea: {
    flex: 1,
    overflowY: "auto" as "auto",
    padding: "20px",
    display: "flex",
    flexDirection: "column" as "column",
    gap: "16px",
  },
  chatEmptyState: {
    display: "flex",
    flexDirection: "column" as "column",
    alignItems: "center",
    justifyContent: "center",
    textAlign: "center" as "center",
    height: "100%",
    color: "#6b7280",
    fontSize: "0.9rem",
    padding: "0 40px",
  },
  chatBubble: {
    maxWidth: "80%",
    padding: "12px 16px",
    borderRadius: "12px",
    border: "1px solid",
  },
  chatBubbleImageWrapper: {
    borderRadius: "8px",
    overflow: "hidden",
    marginBottom: "10px",
    border: "1px solid rgba(255, 255, 255, 0.1)",
  },
  chatBubbleImage: {
    maxWidth: "240px",
    maxHeight: "180px",
    display: "block",
  },
  typingIndicator: {
    color: "#9ca3af",
    fontStyle: "italic",
  },
  presetsGrid: {
    marginTop: "20px",
    textAlign: "center" as "center",
  },
  presetsTitle: {
    fontSize: "0.85rem",
    color: "#4b5563",
    marginBottom: "12px",
  },
  presetButtons: {
    display: "flex",
    flexWrap: "wrap" as "wrap",
    justifyContent: "center",
    gap: "10px",
  },
  presetBtn: {
    display: "flex",
    alignItems: "center",
    gap: "8px",
    background: "rgba(255, 255, 255, 0.03)",
    border: "1px solid rgba(255, 255, 255, 0.06)",
    borderRadius: "99px",
    padding: "6px 14px",
    color: "#9ca3af",
    cursor: "pointer",
    fontSize: "0.8rem",
    transition: "all 0.2s",
  },
  footer: {
    borderTop: "1px solid rgba(255, 255, 255, 0.05)",
    padding: "30px",
    textAlign: "center" as "center",
    fontSize: "0.8rem",
    color: "#4b5563",
    position: "absolute" as "absolute",
    bottom: 0,
    width: "100%",
  },
};
