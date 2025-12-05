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
    // STEP 1: Try to UPDATE existing entry first
    for (int i = 0; i < grid->count(); ++i) {
        QLayoutItem* item = grid->itemAt(i);
        if (!item || !item->widget()) continue;

        QWidget* container = item->widget();
        QVBoxLayout* v = qobject_cast<QVBoxLayout*>(container->layout());
        if (v && v->count() >= 2) {
            QLabel* textLabel = qobject_cast<QLabel*>(v->itemAt(1)->widget());
            if (textLabel && textLabel->property("imagePath").toString() == imgPath) {
                textLabel->setText(ocrText);  // UPDATE SUCCESS!
                return;
            }
        }
    }

    // STEP 2: If not found  add new one
    QPixmap pix(imgPath);
    if (pix.isNull()) return;

    QPixmap scaled = pix.scaled(220, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QLabel* imgLabel = new QLabel();
    imgLabel->setPixmap(scaled);
    imgLabel->setAlignment(Qt::AlignCenter);
    imgLabel->setStyleSheet("background: white; padding: 6px; border-radius: 8px;");

    QLabel* textLabel = new QLabel(ocrText);
    textLabel->setStyleSheet("color: #00ff00; font-size: 14px; font-weight: bold;");
    textLabel->setWordWrap(true);
    textLabel->setAlignment(Qt::AlignCenter);
    textLabel->setProperty("imagePath", imgPath);  // CRITICAL

    QWidget* container = new QWidget();
    QVBoxLayout* v = new QVBoxLayout(container);
    v->setSpacing(8);
    v->addWidget(imgLabel);
    v->addWidget(textLabel);

    grid->addWidget(container, row, col++);
    if (col >= 4) { col = 0; row++; }
}

void ClientWindow::updateProgress(int value)
{
    progressBar->setValue(value);
}

void ClientWindow::clearGrid()
{
    QLayoutItem* child;
    while ((child = grid->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    row = col = 0;
}

void ClientWindow::updateResultText(const QString& imagePath, const QString& newText)
{
    // Find all labels in the grid and update the one matching this path
    for (int i = 0; i < grid->count(); ++i) {
        QLayoutItem* item = grid->itemAt(i);
        if (item && item->widget()) {
            QWidget* container = item->widget();
            QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(container->layout());
            if (layout && layout->count() >= 2) {
                QLabel* textLabel = qobject_cast<QLabel*>(layout->itemAt(1)->widget());
                if (textLabel && textLabel->property("imagePath").toString() == imagePath) {
                    textLabel->setText(newText);
                    return;
                }
            }
        }
    }
}

