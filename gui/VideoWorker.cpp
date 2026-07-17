/**
 * @file VideoWorker.cpp
 * @brief Implementation of the VideoWorker background decoding thread.
 */

#include "VideoWorker.h"
#include "DecoderFactory.h"
#include <chrono>
#include <glog/logging.h>

VideoWorker::VideoWorker(QObject* parent)
    : QThread(parent)
{
}

VideoWorker::~VideoWorker()
{
    stopPlayback();
}

void VideoWorker::setVideoSource(const std::string& filePath, videodecoder::BackendType backend)
{
    QMutexLocker locker(&m_mutex);
    m_filePath = filePath;
    m_backend = backend;
}

void VideoWorker::play()
{
    QMutexLocker locker(&m_mutex);
    if (!isRunning()) {
        m_stop = false;
        m_pause = false;
        start();
    } else if (m_pause) {
        m_pause = false;
        m_condition.wakeAll();
    }
}

void VideoWorker::pause()
{
    QMutexLocker locker(&m_mutex);
    m_pause = true;
}

void VideoWorker::stopPlayback()
{
    {
        QMutexLocker locker(&m_mutex);
        m_stop = true;
        m_pause = false;
        m_condition.wakeAll();
    }
    wait(); // Wait for the run thread to exit completely
}

void VideoWorker::run()
{
    std::string path;
    videodecoder::BackendType backendType;

    {
        QMutexLocker locker(&m_mutex);
        path = m_filePath;
        backendType = m_backend;
        m_stop = false;
        m_pause = false;
    }

    LOG(INFO) << "GUI VideoWorker: Starting decoding thread using backend: "
              << (backendType == videodecoder::BackendType::FFMPEG ? "FFmpeg" : "GStreamer");

    auto decoder = videodecoder::DecoderFactory::create(backendType);
    if (!decoder) {
        emit errorOccurred("Failed to create decoder backend.");
        return;
    }

    if (!decoder->initialize(path)) {
        emit errorOccurred(QString("Failed to initialize decoder for: %1").arg(QString::fromStdString(path)));
        return;
    }

    double lastPts = -1.0;
    auto lastFrameTime = std::chrono::high_resolution_clock::now();

    while (true) {
        // Check state control flags
        {
            QMutexLocker locker(&m_mutex);
            if (m_stop) {
                break;
            }
            while (m_pause) {
                m_condition.wait(&m_mutex);
                lastPts = -1.0; // Reset pacer timing on resume
                if (m_stop) {
                    break;
                }
            }
            if (m_stop) {
                break;
            }
        }

        // Decode next frame
        bool frameDecodedSuccessfully = decoder->decodeNextFrame();
        if (!frameDecodedSuccessfully) {
            // Reached End of Stream or encountered error
            LOG(INFO) << "GUI VideoWorker: Reached end of video or decoding error.";
            break;
        }

        // Retrieve raw frame pointer
        auto frame = decoder->getRawFrameData();
        if (frame.data && frame.width > 0 && frame.height > 0) {
            // Pack frame data into QByteArray (copies the pixels for thread-safe UI queuing)
            QByteArray rgbData(reinterpret_cast<const char*>(frame.data), static_cast<int>(frame.size));
            emit frameDecoded(rgbData, frame.width, frame.height, frame.timestamp);

            // Compute frame pacing delay using PTS to match native file speed
            if (lastPts >= 0.0) {
                double ptsDiff = frame.timestamp - lastPts;
                // Sanity check: keep pacing between 1ms and 5 seconds
                if (ptsDiff > 0.001 && ptsDiff < 5.0) {
                    auto now = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double> timeSinceLastFrame = now - lastFrameTime;
                    double sleepRequired = ptsDiff - timeSinceLastFrame.count();

                    if (sleepRequired > 0.001) {
                        QThread::msleep(static_cast<unsigned long>(sleepRequired * 1000));
                    }
                }
            }
            lastPts = frame.timestamp;
            lastFrameTime = std::chrono::high_resolution_clock::now();
        }
    }

    decoder->close();
    LOG(INFO) << "GUI VideoWorker: Thread run finished.";
    emit finishedDecoding();
}
