// main_window.cpp (most important parts)
void MainWindow::onUploadClicked() {
    QStringList files = QFileDialog::getOpenFileNames(this, "Select Images",
        "", "Images (*.png *.jpg *.jpeg *.bmp *.tif)");
    if (files.isEmpty()) return;

    std::lock_guard<std::mutex> lock(batchMutex);
    if (currentBatch.total > 0 && currentBatch.completed < currentBatch.total) {
        // Still processing previous batch  just add to current batch
        currentBatch.files.insert(currentBatch.files.end(), files.begin(), files.end());
        currentBatch.total += files.size();
    }
    else {
        // Start brand new batch
        startNewBatch();
        currentBatch.files = files.toStdVector();
        currentBatch.total = files.size();
        currentBatch.completed = 0;
        listWidget->clear();
    }

    progressBar->setMaximum(currentBatch.total);
    progressBar->setValue(currentBatch.completed);

    for (const auto& f : files)
        listWidget->addItem(new QListWidgetItem(QIcon(":/icons/wait.png"), QFileInfo(f).fileName()));

    // Fire and forget – processing happens in background
    std::thread(&MainWindow::processCurrentBatch, this).detach();
}

void MainWindow::processCurrentBatch() {
    auto reader_writer = stub->ProcessImages(&context);

    // Send all images
    for (const auto& path : currentBatch.files) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            ocr::ImageData req;
            req.set_filename(QFileInfo(path).fileName().toStdString());
            req.set_content(file.readAll().toStdString());
            reader_writer->Write(req);
        }
    }
    reader_writer->WritesDone();

    // Receive results as they come (out-of-order is fine)
    ocr::OCRResult res;
    while (reader_writer->Read(&res)) {
        QString fname = QString::fromStdString(res.filename());
        QString text = QString::fromStdString(res.extracted_text());

        // Update UI in main thread
        QMetaObject::invokeMethod(this, [this, fname, text]() {
            for (int i = 0; i < listWidget->count(); ++i) {
                QListWidgetItem* item = listWidget->item(i);
                if (item->text() == fname) {
                    item->setIcon(QIcon(":/icons/done.png"));
                    item->setToolTip(text);
                    break;
                }
            }
            progressBar->setValue(++currentBatch.completed);

            // When batch finishes  allow new batch to clear old results
            if (currentBatch.completed == currentBatch.total) {
                // ready for completely new batch
            }
            }
    }, Qt::QueuedConnection);
}

grpc::Status status = reader_writer->Finish();
if (!status.ok()) {
    QMetaObject::invokeMethod(this, [status]() {
        QMessageBox::warning(this, "Error", QString::fromStdString(status.error_message()));
        });
}
}