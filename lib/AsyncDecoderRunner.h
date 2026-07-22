/**
 * @file AsyncDecoderRunner.h
 * @brief Asynchronous thread worker managing non-blocking frame decoding via SPSCQueue.
 */

#pragma once

#include "IVideoDecoder.h"
#include "SPSCQueue.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace videodecoder {

/**
 * @struct FramePayload
 * @brief Thread-safe frame container passed through the SPSC ring buffer.
 */
struct FramePayload {
    std::vector<uint8_t> pixelData;
    int width = 0;
    int height = 0;
    size_t size = 0;
    double timestamp = 0.0;
    double decodeTimeMs = 0.0;
    PixelFormat format = PixelFormat::RGB24;
};

/**
 * @class AsyncDecoderRunner
 * @brief Asynchronous worker wrapper enabling non-blocking lock-free frame streaming.
 *
 * Runs `decodeNextFrame()` continuously in a dedicated producer thread and pushes decoded
 * `FramePayload` objects into a lock-free SPSC Ring Buffer (`SPSCQueue`). The UI or consumer thread
 * can pop frames without blocking or locking.
 */
class AsyncDecoderRunner {
public:
    /**
     * @brief Constructor.
     * @param decoder Shared pointer to initialized IVideoDecoder instance.
     */
    explicit AsyncDecoderRunner(std::shared_ptr<IVideoDecoder> decoder)
        : m_decoder(std::move(decoder))
    {
    }

    explicit AsyncDecoderRunner(std::unique_ptr<IVideoDecoder> decoder)
        : m_decoder(std::move(decoder))
    {
    }

    /**
     * @brief Destructor. Automatically stops worker thread.
     */
    ~AsyncDecoderRunner()
    {
        stop();
    }

    // Non-copyable
    AsyncDecoderRunner(const AsyncDecoderRunner&) = delete;
    AsyncDecoderRunner& operator=(const AsyncDecoderRunner&) = delete;

    /**
     * @brief Starts the background frame decoding worker thread.
     * @return true if successfully started, false otherwise.
     */
    bool start()
    {
        if (!m_decoder || m_running.load()) {
            return false;
        }

        m_running.store(true);
        m_eos.store(false);
        m_workerThread = std::thread(&AsyncDecoderRunner::workerLoop, this);
        return true;
    }

    /**
     * @brief Stops the background worker thread cleanly.
     */
    void stop()
    {
        if (m_running.exchange(false)) {
            if (m_workerThread.joinable()) {
                m_workerThread.join();
            }
        }
    }

    /**
     * @brief Non-blockingly tries to pop the next decoded frame (Consumer thread).
     * @param payload Reference to receive the popped FramePayload.
     * @return true if a frame was popped, false if queue is empty.
     */
    bool tryPopFrame(FramePayload& payload)
    {
        return m_queue.try_pop(payload);
    }

    /**
     * @brief Checks if end of stream (EOS) has been reached.
     * @return true if EOS reached and queue is empty, false otherwise.
     */
    bool isEos() const
    {
        return m_eos.load() && m_queue.empty();
    }

    /**
     * @brief Checks if the worker thread is actively running.
     * @return true if running, false otherwise.
     */
    bool isRunning() const
    {
        return m_running.load();
    }

    /**
     * @brief Accesses underlying IVideoDecoder instance.
     */
    std::shared_ptr<IVideoDecoder> getDecoder() const
    {
        return m_decoder;
    }

private:
    std::shared_ptr<IVideoDecoder> m_decoder;
    SPSCQueue<FramePayload, 16> m_queue;
    std::atomic<bool> m_running { false };
    std::atomic<bool> m_eos { false };
    std::thread m_workerThread;

    void workerLoop()
    {
        while (m_running.load()) {
            if (m_queue.full()) {
                // Yield briefly if consumer is falling behind
                std::this_thread::sleep_for(std::chrono::microseconds(500));
                continue;
            }

            if (!m_decoder->decodeNextFrame()) {
                m_eos.store(true);
                break;
            }

            FrameInfo info = m_decoder->getRawFrameData();
            if (info.data && info.size > 0) {
                FramePayload payload;
                payload.pixelData.assign(info.data, info.data + info.size);
                payload.width = info.width;
                payload.height = info.height;
                payload.size = info.size;
                payload.timestamp = info.timestamp;
                payload.decodeTimeMs = info.decodeTimeMs;
                payload.format = info.format;

                m_queue.try_push(std::move(payload));
            }
        }
    }
};

} // namespace videodecoder
