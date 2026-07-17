/**
 * @file DecoderTypes.h
 * @brief Common types and structures for the VideoDecoder library.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

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
 * @enum PixelFormat
 * @brief Specifies the output pixel format of decoded frames.
 */
enum class PixelFormat {
    RGB24, ///< 24-bit packed RGB format (3 bytes per pixel: R-G-B)
    BGR24 ///< 24-bit packed BGR format (3 bytes per pixel: B-G-R)
};

/**
 * @enum DeviceType
 * @brief Specifies the hardware device type for decoding acceleration.
 */
enum class DeviceType {
    CPU, ///< Standard software decoding on the CPU
    CUDA, ///< NVIDIA CUDA / NVDEC hardware acceleration
    VAAPI, ///< VA-API (Intel/AMD on Linux)
    D3D11VA ///< Direct3D11 Video Acceleration (Windows)
};

/**
 * @struct FrameInfo
 * @brief Contains raw frame data and metadata of a decoded video frame.
 */
struct FrameInfo {
    const uint8_t* data = nullptr; ///< Pointer to the raw video frame buffer (packed format)
    int width = 0; ///< Width of the video frame in pixels
    int height = 0; ///< Height of the video frame in pixels
    size_t size = 0; ///< Total size of the frame buffer in bytes (width * height * 3)
    double timestamp = 0.0; ///< Presentation timestamp (PTS) of the frame in seconds
    double decodeTimeMs = 0.0; ///< Active processing time of this frame in milliseconds
    PixelFormat format = PixelFormat::RGB24; ///< Pixel format of this frame buffer
};

/**
 * @struct VideoMetadata
 * @brief Contains static information about the loaded video stream.
 */
struct VideoMetadata {
    int width = 0; ///< Width of the video in pixels
    int height = 0; ///< Height of the video in pixels
    double frameRate = 0.0; ///< Average frame rate in frames per second (FPS)
    double duration = 0.0; ///< Total duration of the video in seconds (0.0 if unknown or live stream)
    std::string codecName; ///< Name of the video codec (e.g. "h264", "hevc")
    PixelFormat format = PixelFormat::RGB24; ///< The selected output format of the decoder
    DeviceType deviceType = DeviceType::CPU; ///< The hardware device type used by the decoder
};

/**
 * @struct DecoderPerformanceStats
 * @brief Holds runtime performance and latency metrics of the video decoder.
 */
struct DecoderPerformanceStats {
    double initializationTimeMs = 0.0; ///< Duration of the initialization stage in milliseconds
    double averageDecodeTimeMs = 0.0; ///< Rolling average active decode time in milliseconds
    uint64_t totalDecodedFrames = 0; ///< Total number of frames successfully decoded
};

} // namespace videodecoder
