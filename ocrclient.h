#pragma once
#include <QObject>
#include <grpcpp/grpcpp.h>
#include "service.grpc.pb.h"
#include <atomic>
#include <memory>
#include <mutex>

class OCRClient : public QObject {
    Q_OBJECT
public:
    explicit OCRClient(QObject* parent = nullptr);
    void sendImages(const QStringList& paths);

signals:
    void progressUpdated(int percent);
    void resultReady(const QString& imagePath, const QString& result);
    void batchCleared();

private:
    void emitResult(const QString& path, const QString& result);

    std::unique_ptr<ps4::DistributedAI::Stub> stub_;
    std::atomic<int> batchProcessed{ 0 };
    int batchTotal = 0;
    std::mutex batchMutex;
};