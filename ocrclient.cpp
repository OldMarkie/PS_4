#include "ocrclient.h"
#include <QMetaObject>

OCRClient::OCRClient(QObject* parent)
    : QObject(parent)
{
    auto channel = grpc::CreateChannel(
        "localhost:50051", grpc::InsecureChannelCredentials()
    );
    stub_ = ps4::DistributedAI::NewStub(channel);
}

void OCRClient::sendImages(const QStringList& paths)
{
    int total = paths.size();
    auto processed = std::make_shared<std::atomic<int>>(0);

    for (const QString& path : paths)
    {
        std::string pathStd = path.toStdString();
        auto processedCopy = processed;

        std::thread([this, path, pathStd, total, processedCopy]() {
            ps4::TaskRequest req;
            req.set_task(pathStd);

            ps4::TaskResponse resp;
            grpc::ClientContext ctx;
            grpc::Status status = stub_->SendTask(&ctx, req, &resp);

            QString result = status.ok()
                ? QString::fromStdString(resp.result())
                : "RPC Error: " + QString::fromStdString(status.error_message());

            int done = ++(*processedCopy);
            int progress = (done * 100) / total;

            QMetaObject::invokeMethod(
                this,
                [this, path, result, progress]() {
                    emit resultReady(path, result);
                    emit progressUpdated(progress);
                },
                Qt::QueuedConnection
            );

            }).detach();
    }
}
