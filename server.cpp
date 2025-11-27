#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "service.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

class DistributedAIService final : public ps4::DistributedAI::Service {
public:
    Status SendTask(ServerContext* context, const ps4::TaskRequest* request,
        ps4::TaskResponse* response) override {
        std::cout << "Received: " << request->task() << std::endl;

        response->set_result("Server processed: " + request->task());
        return Status::OK;
    }
};

void RunServer() {
    std::string address("0.0.0.0:50051");
    DistributedAIService service;

    ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server running at " << address << std::endl;

    server->Wait();
}

int main() {
    RunServer();
    return 0;
}
