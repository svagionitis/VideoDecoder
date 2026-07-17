/**
 * @file FFmpegDecoder.h
 * @brief Header for the FFmpeg-based video decoder backend.
 */

#pragma once

#include "IVideoDecoder.h"
#include <memory>
#include <string>
#include <vector>

// Forward declarations and extern C declarations for FFmpeg headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace videodecoder {

/**
 * @struct AVFormatContextDeleter
 * @brief Custom deleter for AVFormatContext.
 */
struct AVFormatContextDeleter {
    void operator()(AVFormatContext* ptr) const
    {
        if (ptr) {
            avformat_close_input(&ptr);
        }
    }
};
using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;

/**
 * @struct AVCodecContextDeleter
 * @brief Custom deleter for AVCodecContext.
 */
struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ptr) const
    {
        if (ptr) {
            avcodec_free_context(&ptr);
        }
    }
};
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;

/**
 * @struct AVFrameDeleter
 * @brief Custom deleter for AVFrame.
 */
struct AVFrameDeleter {
    void operator()(AVFrame* ptr) const
    {
        if (ptr) {
            av_frame_free(&ptr);
        }
    }
};
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

/**
 * @struct AVPacketDeleter
 * @brief Custom deleter for AVPacket.
 */
struct AVPacketDeleter {
    void operator()(AVPacket* ptr) const
    {
        if (ptr) {
            av_packet_free(&ptr);
        }
    }
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

/**
 * @struct SwsContextDeleter
 * @brief Custom deleter for SwsContext.
 */
struct SwsContextDeleter {
    void operator()(SwsContext* ptr) const
    {
        if (ptr) {
            sws_freeContext(ptr);
        }
    }
};
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;

/**
 * @class FFmpegDecoder
 * @brief Concrete implementation of IVideoDecoder using FFmpeg APIs.
 */
class FFmpegDecoder : public IVideoDecoder {
public:
    /**
     * @brief Constructor.
     */
    FFmpegDecoder();

    /**
     * @brief Destructor. Closes resources.
     */
    ~FFmpegDecoder() override;

    bool initialize(std::string_view filePath) override;
    bool decodeNextFrame() override;
    FrameInfo getRawFrameData() const override;
    VideoMetadata getVideoMetadata() const override;
    DecoderPerformanceStats getPerformanceStats() const override;
    void close() override;

private:
    AVFormatContextPtr m_formatCtx; ///< Format context for container demuxing
    AVCodecContextPtr m_codecCtx; ///< Codec context for video decoding
    AVFramePtr m_rawFrame; ///< Raw decoded frame (e.g., YUV format)
    AVPacketPtr m_packet; ///< Packet read from demuxer
    SwsContextPtr m_swsCtx; ///< Software scaling context for YUV->RGB conversion

    int m_videoStreamIndex = -1; ///< Stream index of the selected video track
    std::vector<uint8_t> m_rgbBuffer; ///< Reusable buffer holding the converted RGB24 pixels
    int m_width = 0; ///< Cached frame width
    int m_height = 0; ///< Cached frame height
    double m_timestamp = 0.0; ///< Presentation timestamp (PTS) in seconds
    double m_frameRate = 0.0; ///< Video frame rate (FPS)
    double m_duration = 0.0; ///< Video duration in seconds
    std::string m_codecName; ///< Video codec name
    double m_initTimeMs = 0.0; ///< Initialization time in milliseconds
    double m_lastDecodeTimeMs = 0.0; ///< Processing latency of the last decoded frame in milliseconds
    double m_totalDecodeTimeMs = 0.0; ///< Cumulative processing latency of all frames in milliseconds
    uint64_t m_decodedFramesCount = 0; ///< Cumulative count of successfully decoded frames
    bool m_isInitialized = false; ///< Status flag
    bool m_reachedEof = false; ///< End of file flag

    /**
     * @brief Helper to allocate scaling context and destination buffer.
     * @param width Frame width.
     * @param height Frame height.
     * @param srcFormat Input pixel format.
     * @return true if successful, false otherwise.
     */
    bool allocateBufferAndSws(int width, int height, AVPixelFormat srcFormat);
};

} // namespace videodecoder
