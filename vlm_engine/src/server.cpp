#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <thread>

#include <grpcpp/grpcpp.h>
#include "inference.grpc.pb.h"
#include "vlm_infer.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;

using inference::InferenceService;
using inference::DescribeImageRequest;
using inference::DescribeImageResponse;
using inference::ChatRequest;
using inference::ChatResponse;

namespace vlm_engine {

class InferenceServiceImpl final : public InferenceService::Service {
public:
    InferenceServiceImpl() {
        ModelConfig config;
        config.model_path = "/models/vlm-mini-8b.bin";
        model_ = std::make_unique<VLMModel>(config);
        model_->LoadModel();
    }

    Status DescribeImage(ServerContext* context, const DescribeImageRequest* request, DescribeImageResponse* response) override {
        std::cout << "[VLM Engine] DescribeImage request received. Image size: " << request->image_data().size() << " bytes." << std::endl;
        
        std::vector<uint8_t> img_bytes(request->image_data().begin(), request->image_data().end());
        std::string description = model_->RunImageDescription(img_bytes, request->prompt());
        
        response->set_description(description);
        return Status::OK;
    }

    Status Chat(ServerContext* context, const ChatRequest* request, ServerWriter<ChatResponse>* writer) override {
        std::cout << "[VLM Engine] Chat request received: \"" << request->prompt() << "\"" << std::endl;
        
        std::vector<uint8_t> img_bytes(request->image_data().begin(), request->image_data().end());
        std::vector<std::string> history(request->history().begin(), request->history().end());

        // Lock/Mutex and state is thread safe inside VLMModel
        model_->RunStreamingInference(
            request->prompt(),
            history,
            img_bytes,
            [writer, context](const std::string& token, bool done) {
                // If the client canceled the RPC, stop generating
                if (context->IsCancelled()) {
                    return;
                }
                ChatResponse response;
                response.set_token(token);
                response.set_done(done);
                writer->Write(response);
            }
        );

        return Status::OK;
    }

private:
    std::unique_ptr<VLMModel> model_;
};

} // namespace vlm_engine

int main(int argc, char** argv) {
    std::string server_address("0.0.0.0:50052");
    vlm_engine::InferenceServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Aether VLM Engine gRPC Server listening on " << server_address << std::endl;
    server->Wait();

    return 0;
}
