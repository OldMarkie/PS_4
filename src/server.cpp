#include <grpcpp/grpcpp.h>
#include "ocr_service.grpc.pb.h"
#include "ocr_worker.hpp"
#include <thread>
#include <queue>
#include <mutex>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using ocr::ImageData;
using ocr::OCRResult;
using ocr::OCRService;

class OCRServiceImpl final : public OCRService::Service {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> taskQueue_;
    std::mutex queueMtx_;
    std::condition_variable cv_;
    std::atomic<bool> shutdown_{ false };

public:
    OCRServiceImpl() {
        unsigned int n = std::thread::hardware_concurrency();
        for (unsigned i = 0; i < n; ++i) {
            workers_.emplace_back(&OCRServiceImpl::workerLoop, this);
        }
    }

    ~OCRServiceImpl() {
        shutdown_ = true;
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

    grpc::Status ProcessImages(ServerContext* context,
        ServerReaderWriter<OCRResult, ImageData>* stream) override {
        ImageData img;
        std::vector<std::future<std::pair<std::string, std::string>>> futures;

        // Read all images from client first
        while (stream->Read(&img)) {
            std::string data(img.content().begin(), img.content().end());
            auto fut = std::async(std::packaged_task<std::pair<std::string, std::string>() >> ([=, data = std::move(data)]() mutable {
                static thread_local OCRWorker worker;
                std::string text = worker.process(img.filename(), reinterpret_cast<const std::byte*>(data.data()), data.size());
                return std::make_pair(img.filename(), text);
                }));
            futures.push_back(fut.get_future());

            {
                std::lock_guard<std::mutex> lk(queueMtx_);
                taskQueue_.push([fut = std::move(fut)]() mutable { fut.get(); });
            }
            cv_.notify_one();
        }

        // As soon as a result is ready  send it immediately
        for (auto& fut : futures) {
            auto [fname, text] = fut.get();
            OCRResult res;
            res.set_filename(fname);
            res.set_extracted_text(text);
            res.set_success(true);
            stream->Write(&res);
        }
        return grpc::Status::OK;
    }

private:
    void workerLoop() {
        while (!shutdown_) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lk(queueMtx_);
                cv_.wait(lk, [&] { return shutdown_ || !taskQueue_.empty(); });
                if (taskQueue_.empty()) continue;
                task = std::move(taskQueue_.front());
                taskQueue_.pop();
            }
            task();
        }
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    OCRServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "OCR Server listening on " << server_address << std::endl;
    server->Wait();
}

int main() {
    RunServer();
    return 0;
}