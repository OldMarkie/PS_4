#include <grpcpp/grpcpp.h>
#include "service.grpc.pb.h"
#include <iostream>
#include <memory>
#include <string>

int main() {
    // 1. Create a channel to the server
    auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());

    // 2. Create a stub for the DistributedAI service
    std::unique_ptr<ps4::DistributedAI::Stub> stub = ps4::DistributedAI::NewStub(channel);

    // 3. Prepare the request
    ps4::TaskRequest request;
    request.set_task("Do AI processing");

    // 4. Prepare the response object and context
    ps4::TaskResponse response;
    grpc::ClientContext context;

    // 5. Make the RPC call
    grpc::Status status = stub->SendTask(&context, request, &response);

    // 6. Check the result
    if (status.ok()) {
        std::cout << "Server response: " << response.result() << std::endl;
    }
    else {
        std::cout << "RPC failed: " << status.error_message() << std::endl;
    }

    return 0;
}


std::shared_ptr<grpc::Channel> createChannel() {
    while (true) {
        auto ch = grpc::CreateChannel("server_ip:50051", grpc::InsecureChannelCredentials());
        if (ch->WaitForConnected(gpr_time_add(gpr_now(GPR_CLOCK_REALTIME),
            gpr_time_from_seconds(5, GPR_TIMESPAN)))) {
            return ch;
        }
        std::cerr << "Cannot connect to server, retrying in 3s...\n";
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}