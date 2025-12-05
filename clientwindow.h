#pragma once
#include <QWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QStringList>

class ClientWindow : public QWidget {
    Q_OBJECT

public:
    explicit ClientWindow(QWidget* parent = nullptr);
    void clearGrid();

signals:
    void imagesChosen(const QStringList& files);

public slots:
    void addImageAndText(const QString& imgPath, const QString& ocrText);
    void updateProgress(int value);

private:
    QPushButton* uploadBtn;
    QProgressBar* progressBar;

    QScrollArea* scrollArea;
    QWidget* scrollWidget;
    QGridLayout* grid;

    int row = 0, col = 0;
};
