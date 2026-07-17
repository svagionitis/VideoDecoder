/**
 * @file main.cpp
 * @brief Entry point for the VideoDecoder Qt GUI Client.
 */

#include "MainWindow.h"
#include <QApplication>
#include <glog/logging.h>

int main(int argc, char* argv[])
{
    // Initialize Google Logging
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1; // Stream logs to stdout/stderr

    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    int ret = app.exec();

    google::ShutdownGoogleLogging();
    return ret;
}
