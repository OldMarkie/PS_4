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
    int processed = 0;

    for (const auto& path : paths) {
        std::thread([this, path, total, &processed]() {
            ps4::TaskRequest req;
            req.set_task(path.toStdString());
            ps4::TaskResponse resp;
            grpc::ClientContext ctx;

            grpc::Status status = stub_->SendTask(&ctx, req, &resp);
            if (status.ok()) {
                emit ocrResultReady(QString::fromStdString(resp.result()));
            }
            else {
                emit ocrResultReady("RPC failed: " + QString::fromStdString(status.error_message()));
            }

            processed++;
            emit progressUpdated((processed * 100) / total);
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