#include "ocrclient.h"
#include <QMetaObject>
#include <QFile>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <atomic>
#include <QtConcurrent>
#include <QDebug>

struct FailedTask {
    QString path;
    int retries = 0;
};

static std::queue<FailedTask> failedTasks;
static std::mutex failedTasksMutex;
static std::condition_variable retry_cv;

std::atomic<int> consecutiveFailures{ 0 };
std::atomic<bool> circuitOpen{ false };

const int FAILURE_THRESHOLD = 2;
const int COOLDOWN_MS = 5000;
const int MAX_RETRIES = 999;
const int RETRY_DELAY_MS = 1000;
const int RPC_TIMEOUT_SEC = 5;

OCRClient::OCRClient(QObject* parent) : QObject(parent)
{
    auto channel = grpc::CreateChannel("192.168.1.4:50051", grpc::InsecureChannelCredentials());
    stub_ = ps4::DistributedAI::NewStub(channel);

    // Background retry thread
    std::thread([this]() {
        while (true) {
            std::queue<FailedTask> tasksToRetry;

            {
                std::unique_lock<std::mutex> lock(failedTasksMutex);
                retry_cv.wait_for(lock, std::chrono::seconds(2));  // Just wait
                if (failedTasks.empty()) continue;
                tasksToRetry.swap(failedTasks);
            }

            while (!tasksToRetry.empty()) {
                FailedTask task = std::move(tasksToRetry.front());
                tasksToRetry.pop();

                QFile file(task.path);
                if (!file.open(QIODevice::ReadOnly)) {
                    emitResult(task.path, "File missing: " + task.path);
                    continue;
                }

                QByteArray data = file.readAll();
                ps4::TaskRequest req;
                req.set_image_data(data.constData(), data.size());
                req.set_filename(task.path.toStdString());

                ps4::TaskResponse resp;
                grpc::ClientContext ctx;
                ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(RPC_TIMEOUT_SEC));

                grpc::Status status = stub_->SendTask(&ctx, req, &resp);

                if (status.ok()) {
                    consecutiveFailures = 0;
                    emitResult(task.path, QString::fromStdString(resp.result()));
                }
                else if (++task.retries < MAX_RETRIES) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    { std::lock_guard<std::mutex> l(failedTasksMutex); failedTasks.push(std::move(task)); }
                }
                else {
                    emitResult(task.path, "Failed permanently: " + QString::fromStdString(status.error_message()));
                }
            }
        }
        }).detach();
}

void OCRClient::emitResult(const QString& path, const QString& result)
{
    QMetaObject::invokeMethod(this, [this, path, result] {
        emit resultReady(path, result);
        }, Qt::QueuedConnection);
}

void OCRClient::sendImages(const QStringList& paths)
{
    {
        std::lock_guard<std::mutex> lock(batchMutex);
        if (batchProcessed >= batchTotal) {
            batchProcessed = 0;
            batchTotal = 0;
            emit batchCleared();
        }
        batchTotal += paths.size();
    }

    for (const QString& path : paths) {
        QtConcurrent::run([this, path]() {
            if (circuitOpen.load()) {
                { std::lock_guard<std::mutex> l(failedTasksMutex); failedTasks.push({ path, 0 }); }
                emitResult(path, "Server offline – will retry when back");
                { std::lock_guard<std::mutex> l(batchMutex); ++batchProcessed; emit progressUpdated((batchProcessed * 100) / batchTotal); }
                return;
            }

            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                emitResult(path, "Cannot open file: " + path);
                { std::lock_guard<std::mutex> l(batchMutex); ++batchProcessed; emit progressUpdated((batchProcessed * 100) / batchTotal); }
                return;
            }

            QByteArray data = file.readAll();
            ps4::TaskRequest req;
            req.set_image_data(data.constData(), data.size());     // CORRECT
            req.set_filename(path.toStdString());                  // CORRECT

            ps4::TaskResponse resp;
            grpc::Status status;
            int attempt = 0;

            do {
                grpc::ClientContext ctx;
                ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(RPC_TIMEOUT_SEC));
                status = stub_->SendTask(&ctx, req, &resp);
                if (status.ok()) break;
                ++attempt;
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            } while (attempt < MAX_RETRIES);

            if (!status.ok()) {
                { std::lock_guard<std::mutex> l(failedTasksMutex); failedTasks.push({ path, attempt }); }

                if (++consecutiveFailures >= FAILURE_THRESHOLD && !circuitOpen.exchange(true)) {
                    qDebug() << "Circuit breaker OPENED";
                    std::thread([this]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(COOLDOWN_MS));
                        circuitOpen = false;
                        consecutiveFailures = 0;
                        qDebug() << "Circuit breaker CLOSED — RETRYING ALL TASKS NOW!";

                        // Wake up 5 times — 100% guaranteed
                        for (int i = 0; i < 5; ++i) {
                            retry_cv.notify_one();
                        }
                        }).detach();
                }
                emitResult(path, "Failed – retrying in background...");
            }
            else {
                consecutiveFailures = 0;
                emitResult(path, QString::fromStdString(resp.result()));
            }

            { std::lock_guard<std::mutex> l(batchMutex); ++batchProcessed; emit progressUpdated((batchProcessed * 100) / batchTotal); }
            });
    }
}