#include <grpcpp/grpcpp.h>
#include "service.grpc.pb.h"

#include <opencv2/opencv.hpp>
#include <tesseract/baseapi.h>

#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <vector>
#include <string>
#include <cstdlib>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

// ---------------------------
// Thread-safe OCR task queue
// ---------------------------
struct OCRTask {
    std::string image_path;
    std::shared_ptr<ps4::TaskResponse> response; // use shared_ptr
};


std::queue<OCRTask> ocr_task_queue;
std::mutex queue_mutex;
std::condition_variable queue_cv;

// ---------------------------
// gRPC Service Implementation
// ---------------------------
class DistributedAIService final : public ps4::DistributedAI::Service {
public:
    Status SendTask(ServerContext* context,
        const ps4::TaskRequest* request,
        ps4::TaskResponse* response) override
    {
        try {
            cv::Mat image = cv::imread(request->task(), cv::IMREAD_COLOR);
            if (image.empty()) {
                response->set_result("Failed to open image: " + request->task());
                return Status::OK;
            }

            cv::Mat aligned;
            image.convertTo(aligned, CV_8UC3);
            if (!aligned.isContinuous()) aligned = aligned.clone();

            tesseract::TessBaseAPI ocr;
            if (ocr.Init(nullptr, "eng") != 0) {
                response->set_result("Failed to initialize Tesseract");
                return Status::OK;
            }

            ocr.SetImage(aligned.data, aligned.cols, aligned.rows, aligned.channels(), static_cast<int>(aligned.step));
            char* raw_text = ocr.GetUTF8Text();
            std::string extracted = raw_text ? raw_text : "";
            delete[] raw_text;

            response->set_result(extracted);
            return Status::OK;
        }
        catch (const std::exception& e) {
            response->set_result("OCR Worker exception: " + std::string(e.what()));
            return Status::OK;
        }
    }



};

// ---------------------------
// OCR Worker Thread
// ---------------------------
void ocr_worker() {
    tesseract::TessBaseAPI ocr;
    if (ocr.Init(nullptr, "eng") != 0) {
        std::cerr << "[Worker] Failed to initialize Tesseract OCR." << std::endl;
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

        try {
            cv::Mat image = cv::imread(task.image_path, cv::IMREAD_COLOR);
            if (image.empty()) {
                task.response->set_result("Failed to open image: " + task.image_path);
                continue;
            }

            cv::Mat aligned;
            image.convertTo(aligned, CV_8UC3);
            if (!aligned.isContinuous()) aligned = aligned.clone();

            ocr.SetImage(aligned.data, aligned.cols, aligned.rows, aligned.channels(), static_cast<int>(aligned.step));
            char* raw_text = ocr.GetUTF8Text();
            std::string extracted = raw_text ? raw_text : "";
            delete[] raw_text;

            task.response->set_result(extracted);
            std::cout << "[Worker] Processed: " << task.image_path << std::endl;
        }
        catch (const std::exception& e) {
            task.response->set_result("Worker error: " + std::string(e.what()));
        }
        catch (...) {
            task.response->set_result("Worker encountered unknown error.");
        }
    }
}

// ---------------------------
// Server Runner
// ---------------------------
void RunServer() {
    DistributedAIService service;

    ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[Server] Running at 0.0.0.0:50051\n";

    // Start a few background worker threads
    const int WORKER_COUNT = 4;
    for (int i = 0; i < WORKER_COUNT; ++i) {
        std::thread(ocr_worker).detach();
    }

    server->Wait();
}

// ---------------------------
// Main
// ---------------------------
int main() {
    _putenv("TESSDATA_PREFIX=D:/Tools/vcpkg/installed/x64-windows/share/tessdata/");

    try {
        RunServer();
    }
    catch (const std::exception& e) {
        std::cerr << "[Fatal] Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (...) {
        std::cerr << "[Fatal] Unknown exception." << std::endl;
        return EXIT_FAILURE;
    }
    return 0;
}

