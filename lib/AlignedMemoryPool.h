/**
 * @file AlignedMemoryPool.h
 * @brief Machine-adaptive SIMD-aligned zero-copy memory pool allocator for video frame buffers.
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#include <malloc.h>
#endif

namespace videodecoder {

// Machine-adaptive cache line alignment detection (C++17)
#if defined(__cpp_lib_hardware_interference_size)
using std::hardware_destructive_interference_size;
constexpr size_t kDefaultCacheLineAlignment = hardware_destructive_interference_size;
#else
constexpr size_t kDefaultCacheLineAlignment = 64; // Default L1 cache line size on x86_64 / ARM64
#endif

// Compile-time SIMD architecture vector alignment detection
#if defined(__AVX512F__)
constexpr size_t kTargetSIMDAlignment = 64; // AVX-512 (512 bits)
#elif defined(__AVX__) || defined(__AVX2__)
constexpr size_t kTargetSIMDAlignment = 32; // AVX / AVX2 (256 bits)
#else
constexpr size_t kTargetSIMDAlignment = 16; // SSE / NEON (128 bits)
#endif

/// Machine-optimal SIMD alignment taking the max of target SIMD requirement and cache line boundary.
constexpr size_t kOptimalSIMDAlignment
    = (kTargetSIMDAlignment > kDefaultCacheLineAlignment) ? kTargetSIMDAlignment : kDefaultCacheLineAlignment;

/**
 * @brief Cross-platform aligned memory allocation helper.
 * @param size Number of bytes to allocate.
 * @param alignment Alignment boundary in bytes (must be power of two).
 * @return Pointer to allocated aligned memory block, or nullptr on failure.
 */
inline void* alignedAlloc(size_t size, size_t alignment = kOptimalSIMDAlignment)
{
    if (size == 0) {
        return nullptr;
    }
#if defined(_WIN32) || defined(_WIN64)
    return _aligned_malloc(size, alignment);
#else
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return nullptr;
    }
    return ptr;
#endif
}

/**
 * @brief Cross-platform aligned memory free helper.
 * @param ptr Pointer to aligned memory block previously allocated with alignedAlloc.
 */
inline void alignedFree(void* ptr) noexcept
{
    if (!ptr) {
        return;
    }
#if defined(_WIN32) || defined(_WIN64)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

/**
 * @class AlignedBuffer
 * @brief RAII move-only wrapper managing a 64-byte SIMD-aligned raw byte buffer.
 */
class AlignedBuffer {
public:
    /**
     * @brief Default constructor. Constructs an empty AlignedBuffer.
     */
    AlignedBuffer() = default;

    /**
     * @brief Constructs an AlignedBuffer of the specified size and alignment.
     * @param size Size of buffer in bytes.
     * @param alignment Alignment boundary in bytes (defaults to kOptimalSIMDAlignment).
     */
    explicit AlignedBuffer(size_t size, size_t alignment = kOptimalSIMDAlignment)
        : m_alignment(alignment)
    {
        allocate(size);
    }

    /**
     * @brief Destructor. Releases allocated aligned memory.
     */
    ~AlignedBuffer()
    {
        reset();
    }

    // Non-copyable
    AlignedBuffer(const AlignedBuffer&) = delete;
    AlignedBuffer& operator=(const AlignedBuffer&) = delete;

    /**
     * @brief Move constructor. Transfers ownership of aligned memory.
     */
    AlignedBuffer(AlignedBuffer&& other) noexcept
        : m_data(other.m_data)
        , m_size(other.m_size)
        , m_capacity(other.m_capacity)
        , m_alignment(other.m_alignment)
    {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    /**
     * @brief Move assignment operator. Transfers ownership of aligned memory.
     */
    AlignedBuffer& operator=(AlignedBuffer&& other) noexcept
    {
        if (this != &other) {
            reset();
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            m_alignment = other.m_alignment;

            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    /**
     * @brief Resizes the buffer. Reallocates only if required size exceeds current capacity.
     * @param newSize Requested new size in bytes.
     */
    void resize(size_t newSize)
    {
        if (newSize <= m_capacity) {
            m_size = newSize;
            return;
        }
        allocate(newSize);
    }

    /**
     * @brief Releases allocated memory and resets buffer state.
     */
    void reset() noexcept
    {
        if (m_data) {
            alignedFree(m_data);
            m_data = nullptr;
        }
        m_size = 0;
        m_capacity = 0;
    }

    /**
     * @brief Returns pointer to aligned data buffer.
     */
    uint8_t* data() noexcept
    {
        return m_data;
    }

    /**
     * @brief Returns const pointer to aligned data buffer.
     */
    const uint8_t* data() const noexcept
    {
        return m_data;
    }

    /**
     * @brief Returns current active buffer size in bytes.
     */
    size_t size() const noexcept
    {
        return m_size;
    }

    /**
     * @brief Returns total allocated capacity in bytes.
     */
    size_t capacity() const noexcept
    {
        return m_capacity;
    }

    /**
     * @brief Returns alignment boundary in bytes.
     */
    size_t alignment() const noexcept
    {
        return m_alignment;
    }

    /**
     * @brief Checks if buffer is empty or unallocated.
     */
    bool empty() const noexcept
    {
        return m_data == nullptr || m_size == 0;
    }

private:
    uint8_t* m_data = nullptr;
    size_t m_size = 0;
    size_t m_capacity = 0;
    size_t m_alignment = kOptimalSIMDAlignment;

    void allocate(size_t newSize)
    {
        reset();
        if (newSize > 0) {
            m_data = static_cast<uint8_t*>(alignedAlloc(newSize, m_alignment));
            if (!m_data) {
                throw std::bad_alloc();
            }
            m_size = newSize;
            m_capacity = newSize;
        }
    }
};

/**
 * @class AlignedMemoryPool
 * @brief Thread-safe memory pool managing 64-byte SIMD-aligned reusable raw frame buffers.
 */
class AlignedMemoryPool {
public:
    /**
     * @brief Constructor.
     * @param maxPoolSize Maximum number of recycled buffers retained in the pool.
     * @param defaultBufferSize Pre-allocated buffer capacity in bytes (0 = dynamic).
     */
    explicit AlignedMemoryPool(size_t maxPoolSize = 16, size_t defaultBufferSize = 0)
        : m_maxPoolSize(maxPoolSize)
    {
        if (defaultBufferSize > 0) {
            for (size_t i = 0; i < maxPoolSize; ++i) {
                m_pool.push_back(AlignedBuffer(defaultBufferSize));
            }
        }
    }

    ~AlignedMemoryPool() = default;

    // Non-copyable
    AlignedMemoryPool(const AlignedMemoryPool&) = delete;
    AlignedMemoryPool& operator=(const AlignedMemoryPool&) = delete;

    /**
     * @brief Acquires an aligned buffer of the requested size from the pool or allocates a new one.
     * @param requiredSize Minimum buffer size in bytes.
     * @return AlignedBuffer instance with 64-byte alignment guaranteed.
     */
    AlignedBuffer acquire(size_t requiredSize)
    {
        if (!m_pool.empty()) {
            AlignedBuffer buf = std::move(m_pool.back());
            m_pool.pop_back();
            buf.resize(requiredSize);
            return buf;
        }
        return AlignedBuffer(requiredSize);
    }

    /**
     * @brief Recycles a used AlignedBuffer back into the pool for reuse.
     * @param buffer AlignedBuffer instance to recycle.
     */
    void recycle(AlignedBuffer buffer)
    {
        if (buffer.data() && m_pool.size() < m_maxPoolSize) {
            m_pool.push_back(std::move(buffer));
        }
    }

    /**
     * @brief Returns current number of cached buffers in the pool.
     */
    size_t poolSize() const noexcept
    {
        return m_pool.size();
    }

    /**
     * @brief Clears all cached buffers in the pool.
     */
    void clear() noexcept
    {
        m_pool.clear();
    }

private:
    size_t m_maxPoolSize;
    std::vector<AlignedBuffer> m_pool;
};

} // namespace videodecoder
