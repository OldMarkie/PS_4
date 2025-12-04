#include "clientwindow.h"
#include <QVBoxLayout>
#include <QFileDialog>
#include <QPixmap>
#include <QLabel>

ClientWindow::ClientWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("Distributed OCR Client");
    resize(1000, 800);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    // ----- Upload Button -----
    uploadBtn = new QPushButton("Upload Images");
    uploadBtn->setFixedHeight(40);
    uploadBtn->setStyleSheet(
        "QPushButton { background-color: #444; color: white; border-radius: 4px; }"
        "QPushButton:hover { background-color: #555; }"
    );
    layout->addWidget(uploadBtn, 0, Qt::AlignHCenter);

    // Click event
    connect(uploadBtn, &QPushButton::clicked, this, [this]() {
        QStringList files = QFileDialog::getOpenFileNames(
            this, "Select Images", "", "Images (*.png *.jpg *.jpeg *.bmp)"
        );
        if (!files.isEmpty())
            emit imagesChosen(files);
        });

    // ----- Progress Bar -----
    progressBar = new QProgressBar();
    progressBar->setTextVisible(false);
    progressBar->setFixedHeight(12);
    progressBar->setStyleSheet(
        "QProgressBar { background-color: #222; border: 1px solid #333; }"
        "QProgressBar::chunk { background-color: #2f7cff; }"
    );
    layout->addWidget(progressBar);

    // ----- Scroll Area -----
    scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("background-color: #2b2b2b;");

    scrollWidget = new QWidget();
    scrollWidget->setStyleSheet("background-color: #2b2b2b;");
    grid = new QGridLayout(scrollWidget);
    grid->setSpacing(20);
    grid->setContentsMargins(20, 20, 20, 20);

    scrollArea->setWidget(scrollWidget);
    layout->addWidget(scrollArea);
}

void ClientWindow::addImageAndText(const QString& imgPath, const QString& ocrText)
{
    // Load scaled image
    QPixmap pix(imgPath);
    QPixmap scaled = pix.scaled(220, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QLabel* img = new QLabel();
    img->setPixmap(scaled);
    img->setAlignment(Qt::AlignCenter);
    img->setStyleSheet("background: white; padding: 4px;");

    QLabel* txt = new QLabel(ocrText);
    txt->setStyleSheet("color: white; font-size: 12px;");
    txt->setAlignment(Qt::AlignLeft);

    QWidget* container = new QWidget();
    QVBoxLayout* v = new QVBoxLayout(container);
    v->setSpacing(4);
    v->addWidget(img);
    v->addWidget(txt);

    grid->addWidget(container, row, col);

    col++;
    if (col == 4) { // 4 columns like screenshot
        col = 0;
        row++;
    }
}

void ClientWindow::updateProgress(int value)
{
    progressBar->setValue(value);
}
