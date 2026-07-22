/**
 * @file SPSCQueue.h
 * @brief Lock-free Single-Producer Single-Consumer (SPSC) Ring Buffer template.
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <utility>

namespace videodecoder {

#if defined(__cpp_lib_hardware_interference_size)
using std::hardware_destructive_interference_size;
#else
constexpr size_t hardware_destructive_interference_size = 64; // Default L1 cache line size on x86/ARM
#endif

/**
 * @class SPSCQueue
 * @brief Lock-free Single-Producer Single-Consumer (SPSC) Ring Buffer.
 *
 * This queue provides sub-microsecond, zero-allocation lock-free thread synchronization for transferring
 * frames or data packets between a single producer thread (e.g. video decoder) and a single consumer thread
 * (e.g. UI/renderer). Head and tail indices are padded to distinct 64-byte cache lines to eliminate false sharing.
 *
 * @tparam T The element type stored in the queue.
 * @tparam Capacity Maximum capacity of the ring buffer.
 */
template <typename T, size_t Capacity> class SPSCQueue {
public:
    static_assert(Capacity > 0, "SPSCQueue capacity must be greater than zero");

    /**
     * @brief Default constructor.
     */
    SPSCQueue()
        : m_head(0)
        , m_tail(0)
    {
    }

    ~SPSCQueue() = default;

    // Non-copyable and non-movable
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    /**
     * @brief Pushes an item onto the queue (Producer thread only).
     * @param value Item to copy-push into the queue.
     * @return true if successfully pushed, false if the queue is full.
     */
    bool try_push(const T& value)
    {
        const size_t current_tail = m_tail.load(std::memory_order_relaxed);
        if (current_tail - m_head.load(std::memory_order_acquire) >= Capacity) {
            return false; // Queue is full
        }
        m_buffer[current_tail % Capacity] = value;
        m_tail.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Emplaces an item onto the queue in-place (Producer thread only).
     * @tparam Args Argument types for T constructor.
     * @param args Arguments passed to construct T.
     * @return true if successfully pushed, false if the queue is full.
     */
    template <typename... Args> bool try_emplace(Args&&... args)
    {
        const size_t current_tail = m_tail.load(std::memory_order_relaxed);
        if (current_tail - m_head.load(std::memory_order_acquire) >= Capacity) {
            return false; // Queue is full
        }
        m_buffer[current_tail % Capacity] = T(std::forward<Args>(args)...);
        m_tail.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pops an item from the queue (Consumer thread only).
     * @param value Reference to receive the popped item.
     * @return true if successfully popped, false if the queue is empty.
     */
    bool try_pop(T& value)
    {
        const size_t current_head = m_head.load(std::memory_order_relaxed);
        if (current_head == m_tail.load(std::memory_order_acquire)) {
            return false; // Queue is empty
        }
        value = std::move(m_buffer[current_head % Capacity]);
        m_head.store(current_head + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Checks if the queue is currently empty.
     * @return true if empty, false otherwise.
     */
    bool empty() const
    {
        return m_head.load(std::memory_order_relaxed) == m_tail.load(std::memory_order_relaxed);
    }

    /**
     * @brief Checks if the queue is currently full.
     * @return true if full, false otherwise.
     */
    bool full() const
    {
        return (m_tail.load(std::memory_order_relaxed) - m_head.load(std::memory_order_relaxed)) >= Capacity;
    }

    /**
     * @brief Gets the current number of elements in the queue.
     * @return Approximate count of enqueued elements.
     */
    size_t size() const
    {
        const size_t tail = m_tail.load(std::memory_order_relaxed);
        const size_t head = m_head.load(std::memory_order_relaxed);
        return (tail >= head) ? (tail - head) : 0;
    }

    /**
     * @brief Gets the maximum capacity of the queue.
     * @return Maximum capacity.
     */
    constexpr size_t capacity() const
    {
        return Capacity;
    }

private:
    std::array<T, Capacity> m_buffer {};

    // Padded to separate 64-byte L1 cache lines to prevent false sharing
    alignas(hardware_destructive_interference_size) std::atomic<size_t> m_head;
    alignas(hardware_destructive_interference_size) std::atomic<size_t> m_tail;
};

} // namespace videodecoder
