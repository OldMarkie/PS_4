#include "client.h"

#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QFileDialog>


OCRClient::OCRClient(QObject* parent) : QObject(parent) {
    auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
    stub_ = ps4::DistributedAI::NewStub(channel);
}

void OCRClient::sendImages(const QStringList& paths) {
    int total = paths.size();
    auto processed = std::make_shared<std::atomic<int>>(0);

    for (const auto& path : paths) {
        // copy path and shared state into the thread
        std::string pathStd = path.toStdString();
        auto processedCopy = processed;

        std::thread([this, pathStd, total, processedCopy]() {
            ps4::TaskRequest req;
            req.set_task(pathStd);
            ps4::TaskResponse resp;
            grpc::ClientContext ctx;

            grpc::Status status = stub_->SendTask(&ctx, req, &resp);

            QString text = status.ok()
                ? QString::fromStdString(resp.result())
                : QString::fromStdString(std::string("RPC failed: ") + status.error_message());

            // update counters
            int done = ++(*processedCopy);
            int progress = (done * 100) / total;

            // Schedule signal emission on this QObject's thread (queued)
            QMetaObject::invokeMethod(
                this,
                [this, text, progress]() {
                    // This lambda runs in the thread that 'this' (OCRClient) lives in.
                    emit ocrResultReady(text);
                    emit progressUpdated(progress);
                },
                Qt::QueuedConnection
            );

            }).detach();
    }
}


int main(int argc, char* argv[]) {
    QApplication a(argc, argv);

    QWidget window;
    QVBoxLayout layout(&window);

    QPushButton uploadBtn("Upload Images");
    QProgressBar progressBar;
    QTextEdit resultView;

    layout.addWidget(&uploadBtn);
    layout.addWidget(&progressBar);
    layout.addWidget(&resultView);

    OCRClient client;

    QObject::connect(&uploadBtn, &QPushButton::clicked, [&]() {
        QStringList files = QFileDialog::getOpenFileNames(&window, "Select Images");
        if (!files.isEmpty()) {
            client.sendImages(files);
        }
        });

    QObject::connect(&client, &OCRClient::progressUpdated, [&](int value) {
        progressBar.setValue(value);
        });

    QObject::connect(&client, &OCRClient::ocrResultReady, [&](const QString& text) {
        resultView.append(text + "\n--------------------\n");
        });

    window.show();
    return a.exec();
}