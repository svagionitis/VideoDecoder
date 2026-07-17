/**
 * @file IVideoDecoder.h
 * @brief Definition of the abstract IVideoDecoder interface.
 */

#pragma once

#include "DecoderTypes.h"
#include "Visibility.h"
#include <string_view>

namespace videodecoder {

/**
 * @class IVideoDecoder
 * @brief Abstract interface defining the lifecycle and operations of a video decoder.
 *
 * Clients instantiate concrete implementations using the DecoderFactory. All operations
 * follow RAII rules for memory management and thread safety.
 */
class VIDEODECODER_API IVideoDecoder {
public:
    /**
     * @brief Virtual destructor to ensure clean cleanup of subclasses.
     */
    virtual ~IVideoDecoder() = default;

    /**
     * @brief Initializes the video decoder for the specified file.
     *
     * Opens the video container, parses headers, selects the best video stream, and sets up
     * the decoder pipeline (FFmpeg codecs or GStreamer parse/sink elements).
     *
     * @param filePath Absolute path or valid file path to the video source.
     * @param format The requested output format for decoded frames (defaults to RGB24).
     * @return true if initialization is successful and stream is ready to decode, false otherwise.
     * @throws std::runtime_error or other standard exceptions on critical failure (e.g. backend out of memory).
     * @note If initialized successfully, you must call close() or rely on destruction to release handles.
     */
    virtual bool initialize(std::string_view filePath, PixelFormat format = PixelFormat::RGB24) = 0;

    /**
     * @brief Decodes the next available frame in the video stream.
     *
     * Reads packets from the video file, sends them to the decoder, and extracts the next
     * decoded frame. Converts the frame color space internally to RGB24.
     *
     * @return true if a new frame was successfully decoded and cached; false if the end of
     *         stream (EOS) was reached or an error occurred.
     * @note This call is synchronous and blocking. The decoded frame contents will be cached internally
     *       and accessible via getRawFrameData() until the next call to decodeNextFrame() or close().
     */
    virtual bool decodeNextFrame() = 0;

    /**
     * @brief Retrieves the metadata and raw buffer pointer for the current decoded frame.
     *
     * @return FrameInfo structure containing frame buffer address, width, height, and timestamp.
     * @note The data pointer returned in FrameInfo is owned by the decoder backend and is only valid
     *       until the next call to decodeNextFrame() or close().
     */
    virtual FrameInfo getRawFrameData() const = 0;

    /**
     * @brief Retrieves the static metadata and statistics of the loaded video.
     *
     * @return VideoMetadata structure containing resolution, frame rate, duration, and codec details.
     */
    virtual VideoMetadata getVideoMetadata() const = 0;

    /**
     * @brief Retrieves the runtime performance and latency statistics of the decoder.
     *
     * @return DecoderPerformanceStats structure containing initialization and average decode latencies.
     */
    virtual DecoderPerformanceStats getPerformanceStats() const = 0;

    /**
     * @brief Seeks to a specific timestamp in the video stream.
     *
     * @param timeInSeconds The target timestamp in seconds.
     * @return true if the seek operation succeeded, false otherwise.
     * @note Seeking is typically keyframe-accurate (seeks to the nearest keyframe at or before the timestamp)
     *       and flushes internal decoder buffers. Seeking has no effect and returns false for live streams.
     */
    virtual bool seek(double timeInSeconds) = 0;

    /**
     * @brief Closes the video file and releases all allocated decoder resources.
     */
    virtual void close() = 0;
};

} // namespace videodecoder
