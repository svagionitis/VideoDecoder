/**
 * @file MainWindow.cpp
 * @brief Implementation of the MainWindow class.
 */

#include "MainWindow.h"
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <glog/logging.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Video Decoder Client (Qt)");
    resize(800, 600);

    // Setup main widget container
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // Top control panel: File pathway and backend selectors
    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel("File:"));
    m_filePathEdit = new QLineEdit();
    m_filePathEdit->setPlaceholderText("Select a video file to play...");
    topLayout->addWidget(m_filePathEdit);

    m_browseButton = new QPushButton("Browse...");
    topLayout->addWidget(m_browseButton);

    topLayout->addWidget(new QLabel("Backend:"));
    m_backendCombo = new QComboBox();
    m_backendCombo->addItem("FFmpeg", static_cast<int>(videodecoder::BackendType::FFMPEG));
    m_backendCombo->addItem("GStreamer", static_cast<int>(videodecoder::BackendType::GSTREAMER));
    topLayout->addWidget(m_backendCombo);

    mainLayout->addLayout(topLayout);

    // Custom frame rendering canvas
    m_videoWidget = new VideoWidget();
    mainLayout->addWidget(m_videoWidget, 1); // Give rendering widget maximum stretch priority

    // Bottom playback panels
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_playButton = new QPushButton("Play");
    m_pauseButton = new QPushButton("Pause");
    m_stopButton = new QPushButton("Stop");
    bottomLayout->addWidget(m_playButton);
    bottomLayout->addWidget(m_pauseButton);
    bottomLayout->addWidget(m_stopButton);

    m_statusLabel = new QLabel("Status: Stopped");
    bottomLayout->addWidget(m_statusLabel, 1);

    mainLayout->addLayout(bottomLayout);

    // Setup button interactions
    connect(m_browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopClicked);

    // Initialize worker thread
    m_worker = new VideoWorker(this);
    connect(m_worker, &VideoWorker::frameDecoded, this, &MainWindow::onFrameDecoded);
    connect(m_worker, &VideoWorker::finishedDecoding, this, &MainWindow::onFinishedDecoding);
    connect(m_worker, &VideoWorker::errorOccurred, this, &MainWindow::onErrorOccurred);

    // Enforce initial button availability
    m_pauseButton->setEnabled(false);
    m_stopButton->setEnabled(false);
}

MainWindow::~MainWindow()
{
    // Ensuring background thread is cleanly stopped on close
    m_worker->stopPlayback();
}

void MainWindow::onBrowseClicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, "Open Video File", QString(), "Video Files (*.mp4 *.mkv *.avi *.mov *.y4m);;All Files (*)");
    if (!fileName.isEmpty()) {
        m_filePathEdit->setText(fileName);
    }
}

void MainWindow::onPlayClicked()
{
    QString filePath = m_filePathEdit->text().trimmed();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "Input Required", "Please select a valid video file.");
        return;
    }

    videodecoder::BackendType backend = static_cast<videodecoder::BackendType>(m_backendCombo->currentData().toInt());

    // Reset status counters
    m_frameCount = 0;

    // Load file and start background loop
    m_worker->setVideoSource(filePath.toStdString(), backend);
    m_worker->play();

    // Toggle widgets states
    m_playButton->setEnabled(false);
    m_pauseButton->setEnabled(true);
    m_stopButton->setEnabled(true);
    m_backendCombo->setEnabled(false);
    m_filePathEdit->setEnabled(false);
    m_browseButton->setEnabled(false);
    m_statusLabel->setText("Status: Playing");
}

void MainWindow::onPauseClicked()
{
    m_worker->pause();

    m_playButton->setEnabled(true);
    m_pauseButton->setEnabled(false);
    m_statusLabel->setText("Status: Paused");
}

void MainWindow::onStopClicked()
{
    m_worker->stopPlayback();
    m_videoWidget->clearFrame();
    m_frameCount = 0;

    // Restore widget states
    m_playButton->setEnabled(true);
    m_pauseButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    m_backendCombo->setEnabled(true);
    m_filePathEdit->setEnabled(true);
    m_browseButton->setEnabled(true);
    m_statusLabel->setText("Status: Stopped");
}

void MainWindow::onFrameDecoded(const QByteArray& rgbData, int width, int height, double pts)
{
    m_frameCount++;
    m_videoWidget->updateFrame(rgbData, width, height);
    m_statusLabel->setText(QString("Frame: %1 | PTS: %2s | Resolution: %3x%4")
                               .arg(m_frameCount)
                               .arg(pts, 0, 'f', 2)
                               .arg(width)
                               .arg(height));
}

void MainWindow::onFinishedDecoding()
{
    // Reset buttons when playback finishes natively (e.g. EOF)
    m_playButton->setEnabled(true);
    m_pauseButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    m_backendCombo->setEnabled(true);
    m_filePathEdit->setEnabled(true);
    m_browseButton->setEnabled(true);
    m_statusLabel->setText(QString("Playback Finished. Total Frames: %1").arg(m_frameCount));
}

void MainWindow::onErrorOccurred(const QString& message)
{
    QMessageBox::critical(this, "Decoding Error", message);
    onStopClicked();
}
