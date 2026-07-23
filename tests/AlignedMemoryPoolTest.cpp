/**
 * @file AlignedMemoryPoolTest.cpp
 * @brief Unit tests for SIMD-Aligned Memory Pool Allocator and AlignedBuffer.
 */

#include "AlignedMemoryPool.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

using namespace videodecoder;

TEST(AlignedMemoryPoolTest, AlignedBufferGuaranteesSIMDAlignment)
{
    AlignedBuffer buf(1024);
    ASSERT_NE(buf.data(), nullptr);
    EXPECT_EQ(buf.size(), 1024u);
    EXPECT_GE(buf.capacity(), 1024u);

    // Verify 64-byte or machine-optimal SIMD alignment requirement
    uintptr_t addr = reinterpret_cast<uintptr_t>(buf.data());
    EXPECT_EQ(addr % kOptimalSIMDAlignment, 0u) << "Buffer address 0x" << std::hex << addr << " is not aligned to "
                                                << std::dec << kOptimalSIMDAlignment << " bytes!";
}

TEST(AlignedMemoryPoolTest, AlignedBufferMoveSemantics)
{
    AlignedBuffer buf1(2048);
    uint8_t* originalData = buf1.data();
    ASSERT_NE(originalData, nullptr);

    // Move construct
    AlignedBuffer buf2(std::move(buf1));
    EXPECT_EQ(buf2.data(), originalData);
    EXPECT_EQ(buf2.size(), 2048u);
    EXPECT_EQ(buf1.data(), nullptr);
    EXPECT_EQ(buf1.size(), 0u);

    // Move assign
    AlignedBuffer buf3;
    buf3 = std::move(buf2);
    EXPECT_EQ(buf3.data(), originalData);
    EXPECT_EQ(buf3.size(), 2048u);
    EXPECT_EQ(buf2.data(), nullptr);
}

TEST(AlignedMemoryPoolTest, AlignedBufferResizeNoReallocWhenCapacitySufficient)
{
    AlignedBuffer buf(4096);
    uint8_t* originalData = buf.data();

    // Shrink size
    buf.resize(2048);
    EXPECT_EQ(buf.size(), 2048u);
    EXPECT_EQ(buf.capacity(), 4096u);
    EXPECT_EQ(buf.data(), originalData);

    // Expand size back up within capacity
    buf.resize(4096);
    EXPECT_EQ(buf.size(), 4096u);
    EXPECT_EQ(buf.data(), originalData);
}

TEST(AlignedMemoryPoolTest, AlignedMemoryPoolAcquireAndRecycle)
{
    AlignedMemoryPool pool(4, 2048);
    EXPECT_EQ(pool.poolSize(), 4u);

    // Acquire buffer
    AlignedBuffer buf1 = pool.acquire(2048);
    EXPECT_EQ(pool.poolSize(), 3u);
    EXPECT_EQ(buf1.size(), 2048u);
    uintptr_t addr1 = reinterpret_cast<uintptr_t>(buf1.data());
    EXPECT_EQ(addr1 % kOptimalSIMDAlignment, 0u);

    // Recycle buffer back into pool
    pool.recycle(std::move(buf1));
    EXPECT_EQ(pool.poolSize(), 4u);

    // Re-acquire from pool
    AlignedBuffer buf2 = pool.acquire(1024);
    EXPECT_EQ(buf2.size(), 1024u);
    EXPECT_EQ(buf2.capacity(), 2048u);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(buf2.data()), addr1);
}
