/**
 * @file VideoWorker.h
 * @brief Background thread worker to decode video frames asynchronously.
 */

#pragma once

#include "DecoderTypes.h"
#include <QByteArray>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <string>

/**
 * @class VideoWorker
 * @brief Background decoder worker. Runs a dedicated thread to call the decoder library.
 */
class VideoWorker : public QThread {
    Q_OBJECT
public:
    /**
     * @brief Constructor.
     * @param parent Optional Qt parent.
     */
    explicit VideoWorker(QObject* parent = nullptr);

    /**
     * @brief Destructor. Safely stops the thread.
     */
    ~VideoWorker() override;

    /**
     * @brief Sets the input source file and selected backend.
     * @param filePath Path to the video.
     * @param backend Decoder backend.
     */
    void setVideoSource(const std::string& filePath, videodecoder::BackendType backend);

    /**
     * @brief Resumes or starts the playback loop.
     */
    void play();

    /**
     * @brief Pauses the playback loop.
     */
    void pause();

    /**
     * @brief Stops and terminates the playback.
     */
    void stopPlayback();

signals:
    /**
     * @brief Emitted when a video frame is successfully decoded.
     * @param rgbData Converted RGB24 pixel buffer wrapped in QByteArray.
     * @param width Frame width in pixels.
     * @param height Frame height in pixels.
     * @param pts Presentation timestamp in seconds.
     */
    void frameDecoded(const QByteArray& rgbData, int width, int height, double pts);

    /**
     * @brief Emitted when the decoder reaches the end of the video or stops.
     */
    void finishedDecoding();

    /**
     * @brief Emitted when a fatal decoding error occurs.
     * @param message Text describing the error.
     */
    void errorOccurred(const QString& message);

protected:
    /**
     * @brief The thread execution loop.
     */
    void run() override;

private:
    std::string m_filePath; ///< Input file path
    videodecoder::BackendType m_backend = videodecoder::BackendType::FFMPEG; ///< Backend selection

    QMutex m_mutex; ///< Protects status flags
    QWaitCondition m_condition; ///< Synchronizes pausing/resuming
    bool m_stop = false; ///< Termination flag
    bool m_pause = false; ///< Paused state flag
};
