/**
 * @file DecoderTypes.h
 * @brief Common types and structures for the VideoDecoder library.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace videodecoder {

/**
 * @enum BackendType
 * @brief Specifies the underlying multimedia decoding backend.
 */
enum class BackendType {
    FFMPEG, ///< Use FFmpeg backend for file demuxing and frame decoding
    GSTREAMER ///< Use GStreamer backend utilizing pipeline playbin/decodebin
};

/**
 * @struct FrameInfo
 * @brief Contains raw frame data and metadata of a decoded video frame.
 */
struct FrameInfo {
    const uint8_t* data = nullptr; ///< Pointer to the raw video frame buffer (packed RGB24 format)
    int width = 0; ///< Width of the video frame in pixels
    int height = 0; ///< Height of the video frame in pixels
    size_t size = 0; ///< Total size of the frame buffer in bytes (width * height * 3)
    double timestamp = 0.0; ///< Presentation timestamp (PTS) of the frame in seconds
};

} // namespace videodecoder
