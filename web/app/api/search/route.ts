import { NextRequest, NextResponse } from "next/server";
import * as grpc from "@grpc/grpc-js";

// Import generated stubs
import { SearchServiceClient } from "../../../lib/pb/search";
import { InferenceServiceClient } from "../../../lib/pb/inference";

// Create C++ gRPC clients (connections are managed by grpc-js)
const searchCoreAddr = process.env.SEARCH_CORE_ADDR || "localhost:50051";
const vlmEngineAddr = process.env.VLM_ENGINE_ADDR || "localhost:50052";

const searchClient = new SearchServiceClient(
  searchCoreAddr,
  grpc.credentials.createInsecure(),
  { "grpc.keepalive_time_ms": 120000 }
);

const vlmClient = new InferenceServiceClient(
  vlmEngineAddr,
  grpc.credentials.createInsecure(),
  { "grpc.keepalive_time_ms": 120000 }
);

export async function POST(req: NextRequest) {
  try {
    const { query, mode, image, history = [] } = await req.json();

    if (mode === "text" || mode === "vector") {
      // 1. Text/Vector Search via C++ Search Core
      const enableVectorSearch = mode === "vector";
      
      const searchRequest = {
        query: query || "",
        limit: 10,
        offset: 0,
        enableVectorSearch,
      };

      return new Promise<NextResponse>((resolve) => {
        searchClient.search(searchRequest, (error, response) => {
          if (error) {
            console.error("[BFF] gRPC Search Error:", error);
            resolve(
              NextResponse.json(
                { success: false, error: error.message },
                { status: 500 }
              )
            );
            return;
          }

          resolve(
            NextResponse.json({
              success: true,
              results: response.results || [],
              totalResults: response.totalResults ? response.totalResults.toString() : "0",
              latencyMs: response.latencyMs || 0,
            })
          );
        });
      });
    } else if (mode === "vlm") {
      // 2. Multimodal Streaming Chat via C++ VLM Engine
      let imageBuffer = Buffer.alloc(0);
      
      if (image) {
        // Strip data prefix if present (e.g. data:image/jpeg;base64,)
        const base64Data = image.replace(/^data:image\/\w+;base64,/, "");
        imageBuffer = Buffer.from(base64Data, "base64");
      }

      const chatRequest = {
        prompt: query || "Describe this image",
        history: history || [],
        imageData: imageBuffer,
      };

      // Create readable stream for streaming tokens to Next.js Client
      const encoder = new TextEncoder();
      const call = vlmClient.chat(chatRequest);

      const stream = new ReadableStream({
        start(controller) {
          call.on("data", (response) => {
            const chunk = JSON.stringify({
              token: response.token,
              done: response.done,
            });
            // Send SSE or newline-separated JSON chunks
            controller.enqueue(encoder.encode(chunk + "\n"));
          });

          call.on("end", () => {
            controller.close();
          });

          call.on("error", (error) => {
            console.error("[BFF] VLM Streaming error:", error);
            controller.error(error);
          });
        },
        cancel() {
          call.cancel();
        },
      });

      return new Response(stream, {
        headers: {
          "Content-Type": "application/x-ndjson",
          "Cache-Control": "no-cache",
          "Connection": "keep-alive",
        },
      });
    } else {
      return NextResponse.json(
        { success: false, error: "Invalid mode specified" },
        { status: 400 }
      );
    }
  } catch (err: any) {
    console.error("[BFF] POST Handler Error:", err);
    return NextResponse.json(
      { success: false, error: err.message },
      { status: 500 }
    );
  }
}
