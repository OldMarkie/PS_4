#pragma once
#include <QObject>
#include <grpcpp/grpcpp.h>
#include "service.grpc.pb.h"

#include <QString>
#include <QStringList>
#include <thread>

class OCRClient : public QObject {
    Q_OBJECT
public:
    OCRClient(QObject* parent = nullptr);

    void sendImages(const QStringList& paths);

signals:
    void progressUpdated(int percent);
    void ocrResultReady(const QString& result);

private:
    std::unique_ptr<ps4::DistributedAI::Stub> stub_;
};
