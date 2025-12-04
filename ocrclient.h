#pragma once
#include <QObject>
#include <grpcpp/grpcpp.h>
#include "service.grpc.pb.h"
#include <atomic>
#include <memory>

class OCRClient : public QObject {
    Q_OBJECT

public:
    explicit OCRClient(QObject* parent = nullptr);
    void sendImages(const QStringList& paths);

signals:
    void progressUpdated(int percent);
    void resultReady(const QString& imagePath, const QString& result);

private:
    std::unique_ptr<ps4::DistributedAI::Stub> stub_;
};
