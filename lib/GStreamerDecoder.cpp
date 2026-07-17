/**
 * @file GStreamerDecoder.cpp
 * @brief Implementation of the GStreamer-based video decoder.
 */

#include "GStreamerDecoder.h"
#include <algorithm>
#include <cstring>
#include <glog/logging.h>
#include <gst/video/video.h>

namespace videodecoder {

GStreamerDecoder::GStreamerDecoder()
{
    initGStreamer();
}

GStreamerDecoder::~GStreamerDecoder()
{
    close();
}

void GStreamerDecoder::initGStreamer()
{
    static bool initialized = false;
    if (!initialized) {
        GError* error = nullptr;
        if (!gst_init_check(nullptr, nullptr, &error)) {
            LOG(ERROR) << "GStreamer: Failed to initialize (error: " << (error ? error->message : "unknown") << ")";
            if (error)
                g_error_free(error);
            throw std::runtime_error("Failed to initialize GStreamer runtime");
        }
        initialized = true;
        LOG(INFO) << "GStreamer: Runtime initialized successfully.";
    }
}

bool GStreamerDecoder::initialize(std::string_view filePath)
{
    close();

    // Escape file path for pipeline construction
    std::string escapedPath;
    for (char c : filePath) {
        if (c == '\\' || c == '"') {
            escapedPath += '\\';
        }
        escapedPath += c;
    }

    // Determine if input is a URI (e.g., rtsp://, http://) or a local file path
    std::string sourceBin;
    if (escapedPath.find("://") != std::string::npos) {
        sourceBin = "uridecodebin uri=\"" + escapedPath + "\"";
    } else {
        sourceBin = "filesrc location=\"" + escapedPath + "\" ! decodebin";
    }

    // Check environment flag to see if we should split the stream to GStreamer's native autovideosink
    const char* useNativeSink = std::getenv("GST_USE_NATIVE_SINK");
    bool enableNative = (useNativeSink && std::string(useNativeSink) == "1");

    std::string pipelineDesc;
    if (enableNative) {
        pipelineDesc = sourceBin
            + " ! videoconvert ! tee name=t ! queue ! autovideosink t. ! queue ! appsink name=sink caps=\"video/x-raw, "
              "format=RGB\"";
    } else {
        pipelineDesc = sourceBin + " ! videoconvert ! appsink name=sink caps=\"video/x-raw, format=RGB\"";
    }

    GError* error = nullptr;
    GstElement* pipelineRaw = gst_parse_launch(pipelineDesc.c_str(), &error);
    if (!pipelineRaw) {
        LOG(ERROR) << "GStreamer: Failed to parse pipeline description: " << (error ? error->message : "unknown error");
        if (error)
            g_error_free(error);
        return false;
    }
    m_pipeline.reset(pipelineRaw);

    // Retrieve appsink element
    m_sink = gst_bin_get_by_name(GST_BIN(m_pipeline.get()), "sink");
    if (!m_sink) {
        LOG(ERROR) << "GStreamer: Failed to retrieve appsink element 'sink' from pipeline";
        close();
        return false;
    }

    // Set pipeline to PLAYING state
    GstStateChangeReturn stateRet = gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
    if (stateRet == GST_STATE_CHANGE_FAILURE) {
        LOG(ERROR) << "GStreamer: Failed to set pipeline to PLAYING state";
        close();
        return false;
    }

    // For file sources, the transition is usually asynchronous. We wait for state change (or pre-roll)
    // up to a reasonable timeout to verify it initialized successfully.
    stateRet = gst_element_get_state(m_pipeline.get(), nullptr, nullptr, 5 * GST_SECOND);
    if (stateRet == GST_STATE_CHANGE_FAILURE) {
        LOG(ERROR) << "GStreamer: Pipeline state change failed during pre-roll";
        close();
        return false;
    }

    m_reachedEof = false;
    m_isInitialized = true;

    LOG(INFO) << "GStreamer: Pipeline successfully initialized and pre-rolled for file: " << filePath;
    return true;
}

bool GStreamerDecoder::decodeNextFrame()
{
    if (!m_isInitialized) {
        LOG(ERROR) << "GStreamer: Cannot decode. Decoder not initialized.";
        return false;
    }

    if (m_reachedEof) {
        return false;
    }

    // Pull next sample from appsink (blocking call)
    GstSample* sampleRaw = gst_app_sink_pull_sample(GST_APP_SINK(m_sink));
    if (!sampleRaw) {
        // Stream ended or pipeline error. Retrieve message from bus.
        GstBus* bus = gst_element_get_bus(m_pipeline.get());
        if (bus) {
            GstMessage* msg
                = gst_bus_pop_filtered(bus, static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
            if (msg) {
                if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                    GError* err = nullptr;
                    gchar* debug = nullptr;
                    gst_message_parse_error(msg, &err, &debug);
                    LOG(ERROR) << "GStreamer: Pipeline error: " << (err ? err->message : "unknown") << " | "
                               << (debug ? debug : "");
                    if (err)
                        g_error_free(err);
                    g_free(debug);
                } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
                    VLOG(1) << "GStreamer: Pipeline reached End-Of-Stream.";
                }
                gst_message_unref(msg);
            }
            gst_object_unref(bus);
        }
        m_reachedEof = true;
        return false;
    }

    // Wrap sample in RAII unique_ptr
    GstSamplePtr sample(sampleRaw);

    GstBuffer* buffer = gst_sample_get_buffer(sample.get());
    if (!buffer) {
        LOG(ERROR) << "GStreamer: Sample has no buffer";
        return false;
    }

    GstCaps* caps = gst_sample_get_caps(sample.get());
    if (!caps) {
        LOG(ERROR) << "GStreamer: Sample has no caps";
        return false;
    }

    GstStructure* structure = gst_caps_get_structure(caps, 0);
    int width = 0, height = 0;
    if (!gst_structure_get_int(structure, "width", &width) || !gst_structure_get_int(structure, "height", &height)) {
        LOG(ERROR) << "GStreamer: Failed to extract width/height from caps";
        return false;
    }

    // Map buffer memory
    GstMapInfoWrapper map(buffer, GST_MAP_READ);
    if (!map.isMapped()) {
        LOG(ERROR) << "GStreamer: Failed to map sample buffer memory";
        return false;
    }

    m_width = width;
    m_height = height;
    m_rgbBuffer.resize(m_width * m_height * 3);

    // Extract structure/stride and copy to our packed buffer
    GstVideoInfo info;
    if (gst_video_info_from_caps(&info, caps)) {
        const uint8_t* src = map.data();
        uint8_t* dst = m_rgbBuffer.data();
        int srcStride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
        int dstStride = m_width * 3;

        if (srcStride == dstStride) {
            std::memcpy(dst, src, dstStride * m_height);
        } else {
            // Handle aligned/padded strides by copying row-by-row
            for (int r = 0; r < m_height; ++r) {
                std::memcpy(dst + r * dstStride, src + r * srcStride, dstStride);
            }
        }
    } else {
        // Fallback: direct copy if video_info_from_caps fails
        size_t expectedSize = m_width * m_height * 3;
        std::memcpy(m_rgbBuffer.data(), map.data(), std::min(map.size(), expectedSize));
    }

    // Extract Presentation Timestamp (PTS)
    double pts = 0.0;
    GstClockTime bufferPts = GST_BUFFER_PTS(buffer);
    if (GST_CLOCK_TIME_IS_VALID(bufferPts)) {
        pts = static_cast<double>(bufferPts) / GST_SECOND;
    }
    m_timestamp = pts;

    return true;
}

FrameInfo GStreamerDecoder::getRawFrameData() const
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
    return info;
}

void GStreamerDecoder::close()
{
    if (m_sink) {
        gst_object_unref(m_sink);
        m_sink = nullptr;
    }
    m_pipeline.reset(); // This sets state to NULL and unrefs the pipeline

    m_rgbBuffer.clear();
    m_width = 0;
    m_height = 0;
    m_timestamp = 0.0;
    m_isInitialized = false;
    m_reachedEof = false;
}

} // namespace videodecoder
