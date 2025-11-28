#include <grpcpp/grpcpp.h>
#include "service.grpc.pb.h"

#include <opencv2/opencv.hpp>
#include <tesseract/baseapi.h>

#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <cstdlib> // for _putenv

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

// ---------------------------
// Thread-safe OCR task queue
// ---------------------------
struct OCRTask {
    std::string image_path;
    ps4::TaskResponse* response;
    ServerContext* context;
};

std::queue<OCRTask> ocr_task_queue;
std::mutex queue_mutex;
std::condition_variable queue_cv;

// ---------------------------
// gRPC Service Implementation
// ---------------------------
class DistributedAIService final : public ps4::DistributedAI::Service {
public:
    Status SendTask(ServerContext* context, const ps4::TaskRequest* request,
        ps4::TaskResponse* response) override
    {
        OCRTask task{ request->task(), response, context };
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            ocr_task_queue.push(task);
        }
        queue_cv.notify_one();
        return Status::OK;
    }
};

// ---------------------------
// OCR Worker Thread
// ---------------------------
void ocr_worker() {
    // Set TESSDATA_PREFIX so Tesseract can find the traineddata files
    _putenv("TESSDATA_PREFIX=D:/Tools/vcpkg/installed/x64-windows/share/tessdata/");

    tesseract::TessBaseAPI ocr;
    if (ocr.Init(nullptr, "eng") != 0) {
        std::cerr << "Failed to initialize Tesseract." << std::endl;
        return;
    }

    while (true) {
        OCRTask task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [] { return !ocr_task_queue.empty(); });
            task = ocr_task_queue.front();
            ocr_task_queue.pop();
        }

        // Load image using OpenCV
        cv::Mat image = cv::imread(task.image_path, cv::IMREAD_COLOR);
        if (image.empty()) {
            task.response->set_result("Failed to open image: " + task.image_path);
            continue;
        }

        // Perform OCR
        ocr.SetImage(image.data, image.cols, image.rows, image.channels(), image.step);
        std::string extracted_text = ocr.GetUTF8Text();

        task.response->set_result(extracted_text);
        std::cout << "Processed image: " << task.image_path << std::endl;
    }
}

// ---------------------------
// Server Runner
// ---------------------------
void RunServer(const std::string& address = "0.0.0.0:50051") {
    DistributedAIService service;

    ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    // Start OCR worker threads
    const int num_threads = 4;
    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back(ocr_worker);
    }

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server running at " << address << std::endl;

    server->Wait();

    for (auto& t : workers) t.join();
}

// ---------------------------
// Main
// ---------------------------
int main() {
    RunServer();
    return 0;
}
