#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>

struct PendingResult {
    std::string filename;
    std::string text;
    bool ready = false;
    std::mutex mtx;
    std::condition_variable cv;
};