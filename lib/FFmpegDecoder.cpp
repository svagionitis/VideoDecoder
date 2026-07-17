/**
 * @file FFmpegDecoder.cpp
 * @brief Implementation of the FFmpeg-based video decoder.
 */

#include "FFmpegDecoder.h"
#include <chrono>
#include <glog/logging.h>

namespace videodecoder {

FFmpegDecoder::FFmpegDecoder()
{
    // Note: in modern FFmpeg, av_register_all() is deprecated and no longer needed.
}

FFmpegDecoder::~FFmpegDecoder()
{
    close();
}

bool FFmpegDecoder::initialize(std::string_view filePath, PixelFormat format)
{
    close();
    auto start = std::chrono::high_resolution_clock::now();
    m_outputFormat = format;

    AVFormatContext* formatCtxRaw = nullptr;
    std::string pathStr(filePath);

    // Open input stream
    int ret = avformat_open_input(&formatCtxRaw, pathStr.c_str(), nullptr, nullptr);
    if (ret < 0) {
        LOG(ERROR) << "FFmpeg: Failed to open video file: " << filePath << " (error: " << ret << ")";
        return false;
    }
    m_formatCtx.reset(formatCtxRaw);

    // Retrieve stream information
    ret = avformat_find_stream_info(m_formatCtx.get(), nullptr);
    if (ret < 0) {
        LOG(ERROR) << "FFmpeg: Failed to find stream info for: " << filePath << " (error: " << ret << ")";
        return false;
    }

    // Find the best video stream
    AVCodec* codec = nullptr;
    ret = av_find_best_stream(m_formatCtx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (ret < 0 || !codec) {
        LOG(ERROR) << "FFmpeg: Failed to find valid video stream in: " << filePath;
        return false;
    }
    m_videoStreamIndex = ret;

    // Allocate codec context
    AVCodecContext* codecCtxRaw = avcodec_alloc_context3(codec);
    if (!codecCtxRaw) {
        LOG(ERROR) << "FFmpeg: Failed to allocate codec context";
        return false;
    }
    m_codecCtx.reset(codecCtxRaw);

    // Copy parameters to context
    ret = avcodec_parameters_to_context(m_codecCtx.get(), m_formatCtx->streams[m_videoStreamIndex]->codecpar);
    if (ret < 0) {
        LOG(ERROR) << "FFmpeg: Failed to copy parameters to context (error: " << ret << ")";
        return false;
    }

    // Open decoder codec
    ret = avcodec_open2(m_codecCtx.get(), codec, nullptr);
    if (ret < 0) {
        LOG(ERROR) << "FFmpeg: Failed to open codec (error: " << ret << ")";
        return false;
    }

    // Allocate resources
    m_rawFrame.reset(av_frame_alloc());
    m_packet.reset(av_packet_alloc());
    if (!m_rawFrame || !m_packet) {
        LOG(ERROR) << "FFmpeg: Failed to allocate frames or packets";
        return false;
    }

    m_width = m_codecCtx->width;
    m_height = m_codecCtx->height;

    // Get average frame rate
    AVStream* stream = m_formatCtx->streams[m_videoStreamIndex];
    AVRational fps_rational = av_guess_frame_rate(m_formatCtx.get(), stream, nullptr);
    if (fps_rational.den > 0) {
        m_frameRate = av_q2d(fps_rational);
    } else {
        m_frameRate = 0.0;
    }

    // Get video duration
    if (m_formatCtx->duration != AV_NOPTS_VALUE) {
        m_duration = static_cast<double>(m_formatCtx->duration) / AV_TIME_BASE;
    } else {
        m_duration = 0.0;
    }

    // Get codec name
    m_codecName = codec->name ? codec->name : "unknown";

    m_reachedEof = false;
    m_isInitialized = true;

    auto end = std::chrono::high_resolution_clock::now();
    m_initTimeMs = std::chrono::duration<double, std::milli>(end - start).count();

    LOG(INFO) << "FFmpeg: Successfully initialized for file: " << filePath << " | Resolution: " << m_width << "x"
              << m_height << " | Frame Rate: " << m_frameRate << " FPS | Duration: " << m_duration
              << "s | Codec: " << m_codecName << " | Init Time: " << m_initTimeMs << " ms";
    return true;
}

bool FFmpegDecoder::decodeNextFrame()
{
    if (!m_isInitialized) {
        LOG(ERROR) << "FFmpeg: Cannot decode. Decoder not initialized.";
        return false;
    }

    auto decodeStart = std::chrono::high_resolution_clock::now();
    double totalReadTimeMs = 0.0;

    while (true) {
        // Try to receive a decoded frame from the codec
        int ret = avcodec_receive_frame(m_codecCtx.get(), m_rawFrame.get());
        if (ret == 0) {
            // Frame successfully decoded. Prepare output buffer.
            if (!allocateBufferAndSws(
                    m_rawFrame->width, m_rawFrame->height, static_cast<AVPixelFormat>(m_rawFrame->format))) {
                return false;
            }

            // Convert to RGB24
            uint8_t* dstData[4] = { m_rgbBuffer.data(), nullptr, nullptr, nullptr };
            int dstLinesize[4] = { m_width * 3, 0, 0, 0 };

            sws_scale(m_swsCtx.get(), m_rawFrame->data, m_rawFrame->linesize, 0, m_height, dstData, dstLinesize);

            // Compute Presentation Timestamp (PTS)
            double pts = 0.0;
            if (m_rawFrame->best_effort_timestamp != AV_NOPTS_VALUE) {
                pts = m_rawFrame->best_effort_timestamp * av_q2d(m_formatCtx->streams[m_videoStreamIndex]->time_base);
            }
            m_timestamp = pts;

            // Measure end time and save latency statistics
            auto decodeEnd = std::chrono::high_resolution_clock::now();
            double totalTimeMs = std::chrono::duration<double, std::milli>(decodeEnd - decodeStart).count();
            m_lastDecodeTimeMs = totalTimeMs - totalReadTimeMs;
            if (m_lastDecodeTimeMs < 0.0) {
                m_lastDecodeTimeMs = 0.0;
            }
            m_totalDecodeTimeMs += m_lastDecodeTimeMs;
            m_decodedFramesCount++;

            return true;
        } else if (ret == AVERROR(EAGAIN)) {
            // Decoder needs more input packets
            if (m_reachedEof) {
                // If we've already reached EOF and flushed, EAGAIN means we are out of frames.
                return false;
            }

            auto readStart = std::chrono::high_resolution_clock::now();
            int readRet = av_read_frame(m_formatCtx.get(), m_packet.get());
            auto readEnd = std::chrono::high_resolution_clock::now();
            totalReadTimeMs += std::chrono::duration<double, std::milli>(readEnd - readStart).count();

            if (readRet >= 0) {
                if (m_packet->stream_index == m_videoStreamIndex) {
                    int sendRet = avcodec_send_packet(m_codecCtx.get(), m_packet.get());
                    av_packet_unref(m_packet.get());
                    if (sendRet < 0) {
                        LOG(ERROR) << "FFmpeg: Error sending packet to decoder (error: " << sendRet << ")";
                        return false;
                    }
                } else {
                    av_packet_unref(m_packet.get());
                }
            } else if (readRet == AVERROR_EOF) {
                // Reach the end of file. Send flush packet (null) to drain decoder.
                m_reachedEof = true;
                int sendRet = avcodec_send_packet(m_codecCtx.get(), nullptr);
                if (sendRet < 0 && sendRet != AVERROR_EOF) {
                    LOG(ERROR) << "FFmpeg: Error sending EOF flush packet (error: " << sendRet << ")";
                    return false;
                }
            } else {
                LOG(ERROR) << "FFmpeg: Error reading packet from stream (error: " << readRet << ")";
                return false;
            }
        } else if (ret == AVERROR_EOF) {
            // Codec is fully drained
            VLOG(1) << "FFmpeg: Codec fully drained (EOF).";
            return false;
        } else {
            LOG(ERROR) << "FFmpeg: Error during frame decoding (error: " << ret << ")";
            return false;
        }
    }
}

FrameInfo FFmpegDecoder::getRawFrameData() const
{
    if (!m_isInitialized || m_rgbBuffer.empty()) {
        return FrameInfo {};
    }
    FrameInfo info;
    info.data = m_rgbBuffer.data();
    info.width = m_width;
    info.height = m_height;
    info.size = m_rgbBuffer.size();
    info.timestamp = m_timestamp;
    info.decodeTimeMs = m_lastDecodeTimeMs;
    info.format = m_outputFormat;
    return info;
}

void FFmpegDecoder::close()
{
    m_formatCtx.reset();
    m_codecCtx.reset();
    m_rawFrame.reset();
    m_packet.reset();
    m_swsCtx.reset();

    m_videoStreamIndex = -1;
    m_rgbBuffer.clear();
    m_width = 0;
    m_height = 0;
    m_timestamp = 0.0;
    m_frameRate = 0.0;
    m_duration = 0.0;
    m_codecName.clear();
    m_outputFormat = PixelFormat::RGB24;
    m_initTimeMs = 0.0;
    m_lastDecodeTimeMs = 0.0;
    m_totalDecodeTimeMs = 0.0;
    m_decodedFramesCount = 0;
    m_isInitialized = false;
    m_reachedEof = false;
}

VideoMetadata FFmpegDecoder::getVideoMetadata() const
{
    VideoMetadata meta;
    meta.width = m_width;
    meta.height = m_height;
    meta.frameRate = m_frameRate;
    meta.duration = m_duration;
    meta.codecName = m_codecName;
    meta.format = m_outputFormat;
    return meta;
}

DecoderPerformanceStats FFmpegDecoder::getPerformanceStats() const
{
    DecoderPerformanceStats stats;
    stats.initializationTimeMs = m_initTimeMs;
    stats.totalDecodedFrames = m_decodedFramesCount;
    if (m_decodedFramesCount > 0) {
        stats.averageDecodeTimeMs = m_totalDecodeTimeMs / static_cast<double>(m_decodedFramesCount);
    } else {
        stats.averageDecodeTimeMs = 0.0;
    }
    return stats;
}

bool FFmpegDecoder::seek(double timeInSeconds)
{
    if (!m_isInitialized || !m_formatCtx || m_videoStreamIndex < 0) {
        LOG(ERROR) << "FFmpeg: Cannot seek. Decoder not initialized.";
        return false;
    }

    // Check if the stream is a live stream (e.g. RTSP or duration <= 0)
    bool isLive = (m_duration <= 0.0);
    if (m_formatCtx->iformat && m_formatCtx->iformat->name) {
        std::string formatName(m_formatCtx->iformat->name);
        if (formatName.find("rtsp") != std::string::npos || formatName.find("sdp") != std::string::npos) {
            isLive = true;
        }
    }

    if (isLive) {
        LOG(WARNING) << "FFmpeg: Seeking is unsupported for live network streams.";
        return false;
    }

    if (timeInSeconds < 0.0 || timeInSeconds > m_duration) {
        LOG(ERROR) << "FFmpeg: Seek target " << timeInSeconds << "s is out of bounds [0, " << m_duration << "]";
        return false;
    }

    AVStream* stream = m_formatCtx->streams[m_videoStreamIndex];
    int64_t targetPts = static_cast<int64_t>(timeInSeconds / av_q2d(stream->time_base));

    // Seek to keyframe at or before target timestamp
    int ret = av_seek_frame(m_formatCtx.get(), m_videoStreamIndex, targetPts, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        LOG(ERROR) << "FFmpeg: Seek failed to target time " << timeInSeconds << "s (error: " << ret << ")";
        return false;
    }

    // Flush internal codec buffers to prevent residual decoding artifacts from old frames
    avcodec_flush_buffers(m_codecCtx.get());

    // Clean up cached packet state
    if (m_packet) {
        av_packet_unref(m_packet.get());
    }

    m_reachedEof = false;
    m_timestamp = timeInSeconds; // update cached timestamp representation

    VLOG(1) << "FFmpeg: Successfully seeked to timestamp " << timeInSeconds << "s";
    return true;
}

bool FFmpegDecoder::allocateBufferAndSws(int width, int height, AVPixelFormat srcFormat)
{
    if (width <= 0 || height <= 0) {
        LOG(ERROR) << "FFmpeg: Invalid dimensions " << width << "x" << height;
        return false;
    }

    if (width != m_width || height != m_height || !m_swsCtx) {
        m_width = width;
        m_height = height;
        m_rgbBuffer.resize(m_width * m_height * 3);

        AVPixelFormat dstFormat = (m_outputFormat == PixelFormat::BGR24) ? AV_PIX_FMT_BGR24 : AV_PIX_FMT_RGB24;
        m_swsCtx.reset(sws_getContext(
            m_width, m_height, srcFormat, m_width, m_height, dstFormat, SWS_BILINEAR, nullptr, nullptr, nullptr));

        if (!m_swsCtx) {
            LOG(ERROR) << "FFmpeg: Failed to initialize software scaler context";
            return false;
        }
        VLOG(1) << "FFmpeg: Reallocated scaling context and buffer for resolution " << m_width << "x" << m_height;
    }
    return true;
}

} // namespace videodecoder
