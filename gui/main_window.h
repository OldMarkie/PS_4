// main_window.hpp
#pragma once
#include <QtWidgets/QMainWindow>
#include <Widgets/QListWidget>
#include <Widgets/QProgressBar>
#include <Widgets/QPushButton>
#include "ocr_service.grpc.pb.h"
#include <grpcpp/grpcpp.h>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onUploadClicked();
    void startNewBatch();

private:
    void processCurrentBatch();

    QListWidget* listWidget;
    QProgressBar* progressBar;
    QPushButton* uploadBtn;

    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<ocr::OCRService::Stub> stub;

    struct Batch {
        std::vector<std::string> files;
        std::atomic<int> completed{ 0 };
        int total = 0;
    };
    Batch currentBatch;
    std::mutex batchMutex;
};