/**
 * @file GStreamerDecoder.h
 * @brief Header for the GStreamer-based video decoder backend.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "IVideoDecoder.h"
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

namespace videodecoder {

/**
 * @struct GstElementDeleter
 * @brief Custom deleter for GstElement pipelines. Sets state to NULL and unrefs.
 */
struct GstElementDeleter {
    void operator()(GstElement* ptr) const
    {
        if (ptr) {
            gst_element_set_state(ptr, GST_STATE_NULL);
            gst_object_unref(ptr);
        }
    }
};
using GstElementPtr = std::unique_ptr<GstElement, GstElementDeleter>;

/**
 * @struct GstSampleDeleter
 * @brief Custom deleter for GstSample.
 */
struct GstSampleDeleter {
    void operator()(GstSample* ptr) const
    {
        if (ptr) {
            gst_sample_unref(ptr);
        }
    }
};
using GstSamplePtr = std::unique_ptr<GstSample, GstSampleDeleter>;

/**
 * @struct GstCapsDeleter
 * @brief Custom deleter for GstCaps.
 */
struct GstCapsDeleter {
    void operator()(GstCaps* ptr) const
    {
        if (ptr) {
            gst_caps_unref(ptr);
        }
    }
};
using GstCapsPtr = std::unique_ptr<GstCaps, GstCapsDeleter>;

/**
 * @class GstMapInfoWrapper
 * @brief RAII wrapper for mapping GStreamer buffers.
 */
class GstMapInfoWrapper {
public:
    /**
     * @brief Constructor. Maps the buffer.
     * @param buffer Pointer to GstBuffer.
     * @param flags Map flags (e.g. GST_MAP_READ).
     */
    GstMapInfoWrapper(GstBuffer* buffer, GstMapFlags flags)
        : m_buffer(buffer)
        , m_mapped(false)
    {
        if (m_buffer) {
            m_mapped = gst_buffer_map(m_buffer, &m_info, flags);
        }
    }

    /**
     * @brief Destructor. Unmaps the buffer automatically.
     */
    ~GstMapInfoWrapper()
    {
        if (m_mapped && m_buffer) {
            gst_buffer_unmap(m_buffer, &m_info);
        }
    }

    // Disable copy
    GstMapInfoWrapper(const GstMapInfoWrapper&) = delete;
    GstMapInfoWrapper& operator=(const GstMapInfoWrapper&) = delete;

    /**
     * @brief Checks if the buffer was mapped successfully.
     */
    bool isMapped() const
    {
        return m_mapped;
    }

    /**
     * @brief Gets raw pointer to mapped memory.
     */
    const uint8_t* data() const
    {
        return m_info.data;
    }

    /**
     * @brief Gets size of mapped memory in bytes.
     */
    size_t size() const
    {
        return m_info.size;
    }

private:
    GstBuffer* m_buffer = nullptr;
    GstMapInfo m_info {};
    bool m_mapped = false;
};

/**
 * @class GStreamerDecoder
 * @brief Concrete implementation of IVideoDecoder using GStreamer APIs.
 */
class GStreamerDecoder : public IVideoDecoder {
public:
    /**
     * @brief Constructor.
     */
    GStreamerDecoder();

    /**
     * @brief Destructor.
     */
    ~GStreamerDecoder() override;

    bool initialize(std::string_view filePath, PixelFormat format = PixelFormat::RGB24, int threadCount = 0,
        DeviceType device = DeviceType::CPU) override;
    bool decodeNextFrame() override;
    FrameInfo getRawFrameData() const override;
    VideoMetadata getVideoMetadata() const override;
    DecoderPerformanceStats getPerformanceStats() const override;
    bool seek(double timeInSeconds) override;
    bool setDecodingThreadAffinity(const std::vector<int>& cpuIds) override;
    void addFrameProcessor(std::shared_ptr<IFrameProcessor> processor) override;
    void clearFrameProcessors() override;
    void close() override;

private:
    GstElementPtr m_pipeline; ///< GStreamer pipeline
    GstElement* m_sink = nullptr; ///< Weak reference to the appsink element inside the pipeline

    std::vector<uint8_t> m_rgbBuffer; ///< Reusable frame buffer in RGB24
    int m_width = 0; ///< Frame width
    int m_height = 0; ///< Frame height
    double m_timestamp = 0.0; ///< Presentation timestamp (PTS) in seconds
    double m_frameRate = 0.0; ///< Video frame rate (FPS)
    double m_duration = 0.0; ///< Video duration in seconds
    std::string m_codecName; ///< Video codec name
    PixelFormat m_outputFormat = PixelFormat::RGB24; ///< Requested output format
    double m_initTimeMs = 0.0; ///< Initialization time in milliseconds
    double m_lastDecodeTimeMs = 0.0; ///< Processing latency of the last decoded frame in milliseconds
    double m_totalDecodeTimeMs = 0.0; ///< Cumulative processing latency of all frames in milliseconds
    uint64_t m_decodedFramesCount = 0; ///< Cumulative count of successfully decoded frames
    std::string m_filePath; ///< Target video stream path cached for reconnection
    int m_reconnectAttempts = 0; ///< Retry count for live stream reconnection
    int m_threadCount = 0; ///< Number of threads for decoding
    DeviceType m_deviceType = DeviceType::CPU; ///< Requested hardware acceleration device
    DeviceType m_actualDeviceType = DeviceType::CPU; ///< Actual hardware acceleration device used
    std::vector<std::shared_ptr<IFrameProcessor>> m_processors; ///< List of post-processing frame filters
    bool m_isInitialized = false; ///< Status flag
    bool m_reachedEof = false; ///< End of stream flag

    /**
     * @brief Private helper to reinitialize the GStreamer pipeline for live streams.
     * @return true if reconnection succeeded, false otherwise.
     */
    bool reconnect();

    /**
     * @brief Private helper containing the core initialization logic.
     */
    bool initializeInternal(std::string_view filePath, PixelFormat format, int threadCount, DeviceType device);

    /**
     * @brief Static helper to initialize GStreamer runtime exactly once.
     */
    static void initGStreamer();
};

} // namespace videodecoder
