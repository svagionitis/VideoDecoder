/**
 * @file GStreamerDecoder.cpp
 * @brief Implementation of the GStreamer-based video decoder.
 */

#include "GStreamerDecoder.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <glog/logging.h>
#include <gst/video/video.h>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

namespace videodecoder {

int GStreamerDecoder::onAutoplugSelect(
    GstElement* bin, GstPad* pad, GstCaps* caps, GstElementFactory* factory, gpointer user_data)
{
    (void)bin;
    (void)pad;
    (void)caps;
    auto* decoder = static_cast<GStreamerDecoder*>(user_data);
    if (!decoder) {
        return GST_AUTOPLUG_SELECT_TRY;
    }

    if (decoder->m_deviceType == DeviceType::CPU) {
        const gchar* klass = gst_element_factory_get_klass(factory);
        const gchar* name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));

        std::string klassStr(klass ? klass : "");
        std::string nameStr(name ? name : "");
        std::transform(nameStr.begin(), nameStr.end(), nameStr.begin(), ::tolower);

        // Filter out hardware-accelerated decoders if CPU decoding is requested
        if (klassStr.find("Hardware") != std::string::npos || nameStr.find("nvdec") != std::string::npos
            || nameStr.find("cuvid") != std::string::npos || nameStr.find("nvh264") != std::string::npos
            || nameStr.find("vaapi") != std::string::npos || nameStr.find("d3d11") != std::string::npos
            || nameStr.find("omx") != std::string::npos || nameStr.find("msdk") != std::string::npos) {
            LOG(INFO) << "GStreamer: Skipping hardware decoder element: " << nameStr << " (CPU decoding requested)";
            return GST_AUTOPLUG_SELECT_SKIP;
        }
    }
    return GST_AUTOPLUG_SELECT_TRY;
}

void GStreamerDecoder::onElementAdded(GstBin* bin, GstElement* element, gpointer user_data)
{
    (void)bin;
    auto* decoder = static_cast<GStreamerDecoder*>(user_data);
    if (!decoder)
        return;

    int threads = decoder->m_threadCount;
    if (threads > 0 && g_object_class_find_property(G_OBJECT_GET_CLASS(element), "max-threads")) {
        g_object_set(element, "max-threads", threads, nullptr);
    }
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(element), "connection-timeout")) {
        g_object_set(element, "connection-timeout", 1000, nullptr);
    }

    gchar* name = gst_element_get_name(element);
    if (name) {
        std::string nameStr(name);
        g_free(name);
        if (nameStr.find("decodebin") != std::string::npos || nameStr.find("uridecodebin") != std::string::npos) {
            g_signal_connect(element, "autoplug-select", G_CALLBACK(onAutoplugSelect), decoder);
        }
    }
}

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

bool GStreamerDecoder::initializeInternal(
    std::string_view filePath, PixelFormat format, int threadCount, DeviceType device)
{
    close();
    auto start = std::chrono::high_resolution_clock::now();
    m_outputFormat = format;
    m_filePath = std::string(filePath);
    m_threadCount = threadCount;
    m_deviceType = device;
    m_actualDeviceType = DeviceType::CPU;

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

    std::string formatStr = (m_outputFormat == PixelFormat::BGR24) ? "BGR" : "RGB";

    std::string pipelineDesc;
    if (enableNative) {
        pipelineDesc = sourceBin
            + " ! videoconvert ! tee name=t ! queue ! autovideosink t. ! queue ! appsink name=sink caps=\"video/x-raw, "
              "format="
            + formatStr + "\"";
    } else {
        pipelineDesc = sourceBin + " ! videoconvert ! appsink name=sink caps=\"video/x-raw, format=" + formatStr + "\"";
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

    g_signal_connect(m_pipeline.get(), "element-added", G_CALLBACK(onElementAdded), this);

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

    // Determine actual device type by scanning elements
    GstIterator* hwIt = gst_bin_iterate_elements(GST_BIN(m_pipeline.get()));
    if (hwIt) {
        GValue val = G_VALUE_INIT;
        gboolean done = FALSE;
        while (!done) {
            switch (gst_iterator_next(hwIt, &val)) {
            case GST_ITERATOR_OK: {
                GstElement* element = GST_ELEMENT(g_value_get_object(&val));
                if (element) {
                    gchar* name = gst_element_get_name(element);
                    if (name) {
                        std::string nameStr(name);
                        std::transform(nameStr.begin(), nameStr.end(), nameStr.begin(), ::tolower);
                        if (nameStr.find("nvdec") != std::string::npos || nameStr.find("cuvid") != std::string::npos
                            || nameStr.find("nvh264") != std::string::npos) {
                            m_actualDeviceType = DeviceType::CUDA;
                        } else if (nameStr.find("vaapi") != std::string::npos
                            || (nameStr.find("va") != std::string::npos && nameStr.find("dec") != std::string::npos)) {
                            m_actualDeviceType = DeviceType::VAAPI;
                        } else if (nameStr.find("d3d11") != std::string::npos) {
                            m_actualDeviceType = DeviceType::D3D11VA;
                        }
                        g_free(name);
                    }
                }
                g_value_reset(&val);
                break;
            }
            case GST_ITERATOR_RESYNC:
                gst_iterator_resync(hwIt);
                break;
            case GST_ITERATOR_DONE:
                done = TRUE;
                break;
            case GST_ITERATOR_ERROR:
                done = TRUE;
                break;
            }
        }
        g_value_unset(&val);
        gst_iterator_free(hwIt);
    }

    // Query width, height, and frame rate from appsink caps
    GstPad* pad = gst_element_get_static_pad(m_sink, "sink");
    if (pad) {
        GstCaps* caps = gst_pad_get_current_caps(pad);
        if (caps) {
            GstStructure* structure = gst_caps_get_structure(caps, 0);
            if (structure) {
                int w = 0, h = 0;
                if (gst_structure_get_int(structure, "width", &w)) {
                    m_width = w;
                }
                if (gst_structure_get_int(structure, "height", &h)) {
                    m_height = h;
                }
                int fps_num = 0, fps_den = 1;
                if (gst_structure_get_fraction(structure, "framerate", &fps_num, &fps_den) && fps_den > 0) {
                    m_frameRate = static_cast<double>(fps_num) / fps_den;
                }
            }
            gst_caps_unref(caps);
        }
        gst_object_unref(pad);
    }

    // Query duration
    gint64 duration_ns = 0;
    if (gst_element_query_duration(m_pipeline.get(), GST_FORMAT_TIME, &duration_ns)) {
        m_duration = static_cast<double>(duration_ns) / GST_SECOND;
    } else {
        m_duration = 0.0;
    }

    // Attempt to discover the codec name by searching for the decoder element in the bin
    m_codecName = "unknown";
    GstIterator* it = gst_bin_iterate_elements(GST_BIN(m_pipeline.get()));
    if (it) {
        GValue val = G_VALUE_INIT;
        gboolean done = FALSE;
        while (!done) {
            switch (gst_iterator_next(it, &val)) {
            case GST_ITERATOR_OK: {
                GstElement* element = GST_ELEMENT(g_value_get_object(&val));
                gchar* name = gst_element_get_name(element);
                if (name) {
                    std::string nameStr(name);
                    g_free(name);

                    // Decoder elements usually end with "dec" or contain "dec" in their factory/name
                    // e.g. avdec_h264, vp9dec, jpegdec, but not decodebin / uridecodebin / appsink
                    if (nameStr.find("dec") != std::string::npos && nameStr.find("decodebin") == std::string::npos
                        && nameStr.find("uridecodebin") == std::string::npos) {
                        m_codecName = nameStr;
                        done = TRUE; // break early
                    }
                }
                g_value_reset(&val);
                break;
            }
            case GST_ITERATOR_RESYNC:
                gst_iterator_resync(it);
                break;
            case GST_ITERATOR_ERROR:
            case GST_ITERATOR_DONE:
                done = TRUE;
                break;
            }
        }
        g_value_unset(&val);
        gst_iterator_free(it);
    }

    m_reachedEof = false;
    m_isInitialized = true;

    auto end = std::chrono::high_resolution_clock::now();
    m_initTimeMs = std::chrono::duration<double, std::milli>(end - start).count();

    LOG(INFO) << "GStreamer: Pipeline successfully initialized and pre-rolled for file: " << filePath
              << " | Resolution: " << m_width << "x" << m_height << " | Frame Rate: " << m_frameRate
              << " FPS | Duration: " << m_duration << "s | Codec: " << m_codecName << " | Init Time: " << m_initTimeMs
              << " ms";
    return true;
}

bool GStreamerDecoder::initialize(std::string_view filePath, PixelFormat format, int threadCount, DeviceType device)
{
    bool success = initializeInternal(filePath, format, threadCount, device);
    if (!success && device != DeviceType::CPU) {
        std::string deviceStr = "CPU";
        if (device == DeviceType::CUDA)
            deviceStr = "CUDA";
        else if (device == DeviceType::VAAPI)
            deviceStr = "VAAPI";
        else if (device == DeviceType::D3D11VA)
            deviceStr = "D3D11VA";

        LOG(WARNING) << "GStreamer: Failed to initialize with requested GPU device (" << deviceStr
                     << "). Falling back to CPU.";
        success = initializeInternal(filePath, format, threadCount, DeviceType::CPU);
    }
    return success;
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
    GstSample* sampleRaw = nullptr;
    while (true) {
        sampleRaw = gst_app_sink_pull_sample(GST_APP_SINK(m_sink));
        if (sampleRaw) {
            break;
        }

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

        bool isLive = (m_duration <= 0.0);
        if (isLive && m_reconnectAttempts < 3) {
            LOG(WARNING) << "GStreamer: Live stream disconnected. "
                         << "Attempting auto-reconnection (attempt " << m_reconnectAttempts + 1 << "/3)...";
            m_reconnectAttempts++;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (reconnect()) {
                m_reconnectAttempts = 0;
                // Retry pulling sample
                continue;
            }
        }

        m_reachedEof = true;
        return false;
    }

    auto decodeStart = std::chrono::high_resolution_clock::now();

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
        std::memcpy(m_rgbBuffer.data(), map.data(), (std::min)(map.size(), expectedSize));
    }

    // Apply frame processors (filters) on the output buffer in-place
    for (auto& processor : m_processors) {
        if (processor) {
            processor->process(m_rgbBuffer.data(), m_width, m_height, m_outputFormat);
        }
    }

    // Extract Presentation Timestamp (PTS)
    double pts = 0.0;
    GstClockTime bufferPts = GST_BUFFER_PTS(buffer);
    if (GST_CLOCK_TIME_IS_VALID(bufferPts)) {
        pts = static_cast<double>(bufferPts) / GST_SECOND;
    }
    m_timestamp = pts;

    auto decodeEnd = std::chrono::high_resolution_clock::now();
    m_lastDecodeTimeMs = std::chrono::duration<double, std::milli>(decodeEnd - decodeStart).count();
    m_totalDecodeTimeMs += m_lastDecodeTimeMs;
    m_decodedFramesCount++;

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
    info.decodeTimeMs = m_lastDecodeTimeMs;
    info.format = m_outputFormat;
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
    m_frameRate = 0.0;
    m_duration = 0.0;
    m_codecName.clear();
    m_outputFormat = PixelFormat::RGB24;
    m_initTimeMs = 0.0;
    m_lastDecodeTimeMs = 0.0;
    m_totalDecodeTimeMs = 0.0;
    m_decodedFramesCount = 0;
    m_reconnectAttempts = 0;
    m_threadCount = 0;
    m_deviceType = DeviceType::CPU;
    m_actualDeviceType = DeviceType::CPU;
    clearFrameProcessors();
    m_isInitialized = false;
    m_reachedEof = false;
}

VideoMetadata GStreamerDecoder::getVideoMetadata() const
{
    VideoMetadata meta;
    meta.width = m_width;
    meta.height = m_height;
    meta.frameRate = m_frameRate;
    meta.duration = m_duration;
    meta.codecName = m_codecName;
    meta.format = m_outputFormat;
    meta.deviceType = m_actualDeviceType;
    return meta;
}

DecoderPerformanceStats GStreamerDecoder::getPerformanceStats() const
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

bool GStreamerDecoder::seek(double timeInSeconds)
{
    if (!m_isInitialized || !m_pipeline) {
        LOG(ERROR) << "GStreamer: Cannot seek. Decoder not initialized.";
        return false;
    }

    // Check if the stream is a live stream (duration <= 0)
    bool isLive = (m_duration <= 0.0);
    if (isLive) {
        LOG(WARNING) << "GStreamer: Seeking is unsupported for live network streams.";
        return false;
    }

    if (timeInSeconds < 0.0 || timeInSeconds > m_duration) {
        LOG(ERROR) << "GStreamer: Seek target " << timeInSeconds << "s is out of bounds [0, " << m_duration << "]";
        return false;
    }

    gint64 targetNs = static_cast<gint64>(timeInSeconds * GST_SECOND);

    // Simple seek: flush pipeline (removing old queued frames) and seek to nearest keyframe
    gboolean success = gst_element_seek_simple(m_pipeline.get(), GST_FORMAT_TIME,
        static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), targetNs);

    if (!success) {
        LOG(ERROR) << "GStreamer: Seek failed to target time " << timeInSeconds << "s";
        return false;
    }

    m_reachedEof = false;
    m_timestamp = timeInSeconds;

    VLOG(1) << "GStreamer: Successfully seeked to timestamp " << timeInSeconds << "s";
    return true;
}

bool GStreamerDecoder::setDecodingThreadAffinity(const std::vector<int>& cpuIds)
{
    if (cpuIds.empty()) {
        LOG(WARNING) << "GStreamer: Empty CPU list. Thread affinity not set.";
        return false;
    }
#ifdef _WIN32
    DWORD_PTR mask = 0;
    for (int cpu : cpuIds) {
        if (cpu >= 0 && cpu < static_cast<int>(sizeof(DWORD_PTR) * 8)) {
            mask |= (static_cast<DWORD_PTR>(1) << cpu);
        }
    }
    if (SetThreadAffinityMask(GetCurrentThread(), mask) == 0) {
        LOG(ERROR) << "GStreamer: Failed to set thread affinity. Error: " << GetLastError();
        return false;
    }
    LOG(INFO) << "GStreamer: Successfully set thread affinity for current thread.";
    return true;
#else
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int cpu : cpuIds) {
        if (cpu >= 0 && cpu < CPU_SETSIZE) {
            CPU_SET(cpu, &cpuset);
        }
    }
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        LOG(ERROR) << "GStreamer: Failed to set thread affinity. Error: " << rc;
        return false;
    }
    LOG(INFO) << "GStreamer: Successfully set thread affinity for current thread.";
    return true;
#endif
}

bool GStreamerDecoder::reconnect()
{
    LOG(INFO) << "GStreamer: Reconnecting to source: " << m_filePath;

    // Cache the connection parameters
    std::string cachedPath = m_filePath;
    PixelFormat cachedFormat = m_outputFormat;
    int cachedThreads = m_threadCount;
    DeviceType cachedDevice = m_deviceType;

    // Close resource structures
    close();

    // Restore m_filePath so close() calls don't discard it, and call initialize
    m_filePath = cachedPath;
    bool success = initialize(cachedPath, cachedFormat, cachedThreads, cachedDevice);
    if (success) {
        LOG(INFO) << "GStreamer: Successfully reconnected to live stream.";
        return true;
    }
    LOG(ERROR) << "GStreamer: Reconnection failed.";
    return false;
}

void GStreamerDecoder::addFrameProcessor(std::shared_ptr<IFrameProcessor> processor)
{
    if (processor) {
        m_processors.push_back(processor);
    }
}

void GStreamerDecoder::clearFrameProcessors()
{
    m_processors.clear();
}

} // namespace videodecoder
