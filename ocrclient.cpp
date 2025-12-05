#include "ocrclient.h"
#include <QMetaObject>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <atomic>
#include <QtConcurrent/QtConcurrent>

struct FailedTask {
    QString path;
    int retries;
};

static std::queue<FailedTask> failedTasks;
static std::mutex failedTasksMutex;

std::atomic<int> consecutiveFailures{ 0 };
std::atomic<bool> circuitOpen{ false };

const int FAILURE_THRESHOLD = 5;
const int COOLDOWN_MS = 10000;
const int MAX_RETRIES = 10;
const int RETRY_DELAY_MS = 5000;
const int RPC_TIMEOUT_SEC = 5;

OCRClient::OCRClient(QObject* parent)
    : QObject(parent)
{
    auto channel = grpc::CreateChannel(
        "192.168.1.4:50051", grpc::InsecureChannelCredentials()
    );
    stub_ = ps4::DistributedAI::NewStub(channel);

    // Start background thread to retry failed tasks
    std::thread([this]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::queue<FailedTask> tasksToRetry;

            {
                std::lock_guard<std::mutex> lock(failedTasksMutex);
                std::swap(tasksToRetry, failedTasks);
            }

            while (!tasksToRetry.empty()) {
                FailedTask task = tasksToRetry.front();
                tasksToRetry.pop();

                ps4::TaskRequest req;

                QFile file(task.path);
                if (!file.open(QIODevice::ReadOnly)) {
                    QString result = "Failed to open image: " + task.path;
                    QMetaObject::invokeMethod(
                        this,
                        [this, task, result]() { emit resultReady(task.path, result); },
                        Qt::QueuedConnection
                    );
                    continue; // not return, because we are inside a loop
                }


                // Read bytes
                QByteArray data = file.readAll();
                req.set_image_data(std::string(data.begin(), data.end()));
                req.set_filename(task.path.toStdString());


                ps4::TaskResponse resp;
                grpc::ClientContext ctx;
                ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(RPC_TIMEOUT_SEC));
                grpc::Status status = stub_->SendTask(&ctx, req, &resp);

                if (status.ok()) {
                    QString result = QString::fromStdString(resp.result());
                    QMetaObject::invokeMethod(
                        this,
                        [this, task, result]() { emit resultReady(task.path, result); },
                        Qt::QueuedConnection
                    );
                }
                else if (++task.retries < MAX_RETRIES) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                    std::lock_guard<std::mutex> lock(failedTasksMutex);
                    failedTasks.push(task); // re-queue
                }
                else {
                    QString result = "RPC Failed after retries: " + QString::fromStdString(status.error_message());
                    QMetaObject::invokeMethod(
                        this,
                        [this, task, result]() { emit resultReady(task.path, result); },
                        Qt::QueuedConnection
                    );
                }
            }
        }
        }).detach();
}

void OCRClient::sendImages(const QStringList& paths)
{
    int total = paths.size();
    auto processed = std::make_shared<std::atomic<int>>(0);

    for (QString path : paths) {
        QtConcurrent::run([this, path, total, processed]() {

            if (circuitOpen) {
                QString result = "Service unavailable (circuit breaker open)";
                QMetaObject::invokeMethod(
                    this,
                    [this, path, result]() { emit resultReady(path, result); },
                    Qt::QueuedConnection
                );
                return;
            }

            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                QString result = "Failed to open image: " + path;
                QMetaObject::invokeMethod(this, [this, path, result]() {
                    emit resultReady(path, result);
                    }, Qt::QueuedConnection);
                return;
            }

            QByteArray data = file.readAll();

            ps4::TaskRequest req;
            req.set_image_data(std::string(data.begin(), data.end()));
            req.set_filename(path.toStdString());

            ps4::TaskResponse resp;
            grpc::Status status;
            int attempt = 0;

            while (attempt < MAX_RETRIES) {
                grpc::ClientContext ctx;
                ctx.set_deadline(std::chrono::system_clock::now() +
                    std::chrono::seconds(RPC_TIMEOUT_SEC));

                status = stub_->SendTask(&ctx, req, &resp);
                if (status.ok()) break;

                ++attempt;
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            }

            if (!status.ok()) {
                std::lock_guard<std::mutex> lock(failedTasksMutex);
                failedTasks.push({ path, attempt });

                if (++consecutiveFailures >= FAILURE_THRESHOLD && !circuitOpen) {
                    circuitOpen = true;
                    std::thread([=]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(COOLDOWN_MS));
                        consecutiveFailures = 0;
                        circuitOpen = false;
                        }).detach();
                }
            }
            else {
                consecutiveFailures = 0; // reset on success
            }

            QString result = status.ok()
                ? QString::fromStdString(resp.result())
                : "RPC Failed after retries";

            int done = ++(*processed);
            int progress = (done * 100) / total;

            QMetaObject::invokeMethod(this, [this, path, result, progress]() {
                emit resultReady(path, result);
                emit progressUpdated(progress);
                }, Qt::QueuedConnection);
            });
    }
}


