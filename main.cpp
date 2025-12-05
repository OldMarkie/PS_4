#include "clientwindow.h"
#include "ocrclient.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    ClientWindow window;
    OCRClient client;

    // Existing connections
    QObject::connect(&window, &ClientWindow::imagesChosen,
        &client, &OCRClient::sendImages);

    QObject::connect(&client, &OCRClient::resultReady,
        &window, &ClientWindow::addImageAndText);

    QObject::connect(&client, &OCRClient::progressUpdated,
        &window, &ClientWindow::updateProgress);

    QObject::connect(&client, &OCRClient::batchCleared, &window, &ClientWindow::clearGrid);


    window.show();
    return app.exec();
}
