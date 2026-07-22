/**
 * @file SPSCQueueTest.cpp
 * @brief Google Test suite for the lock-free SPSCQueue template and AsyncDecoderRunner.
 */

#include "SPSCQueue.h"
#include "AsyncDecoderRunner.h"
#include "DecoderFactory.h"
#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#ifndef TEST_INPUT_PATH
#define TEST_INPUT_PATH "test_input.mp4"
#endif

using namespace videodecoder;

// 1. Single-threaded push/pop invariants
TEST(SPSCQueueTest, SingleThreadBasicPushPop)
{
    SPSCQueue<int, 4> queue;
    EXPECT_TRUE(queue.empty());
    EXPECT_FALSE(queue.full());
    EXPECT_EQ(queue.capacity(), static_cast<size_t>(4));
    EXPECT_EQ(queue.size(), static_cast<size_t>(0));

    EXPECT_TRUE(queue.try_push(10));
    EXPECT_TRUE(queue.try_push(20));
    EXPECT_TRUE(queue.try_push(30));
    EXPECT_TRUE(queue.try_push(40));

    EXPECT_TRUE(queue.full());
    EXPECT_FALSE(queue.try_push(50)); // Should fail when full
    EXPECT_EQ(queue.size(), static_cast<size_t>(4));

    int val = 0;
    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 10);

    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 20);

    EXPECT_EQ(queue.size(), static_cast<size_t>(2));

    EXPECT_TRUE(queue.try_push(50));
    EXPECT_EQ(queue.size(), static_cast<size_t>(3));

    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 30);
    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 40);
    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 50);

    EXPECT_TRUE(queue.empty());
    EXPECT_FALSE(queue.try_pop(val)); // Should fail when empty
}

// 2. High-throughput multi-threaded Single-Producer Single-Consumer correctness test
TEST(SPSCQueueTest, ConcurrentProducerConsumerStressTest)
{
    constexpr size_t totalItems = 100000;
    SPSCQueue<size_t, 1024> queue;

    std::atomic<bool> producerDone { false };
    size_t consumedCount = 0;
    bool dataCorrupted = false;

    // Producer thread
    std::thread producer([&]() {
        for (size_t i = 0; i < totalItems; ++i) {
            while (!queue.try_push(i)) {
                std::this_thread::yield();
            }
        }
        producerDone.store(true);
    });

    // Consumer thread
    std::thread consumer([&]() {
        size_t expected = 0;
        while (!producerDone.load() || !queue.empty()) {
            size_t item = 0;
            if (queue.try_pop(item)) {
                if (item != expected) {
                    dataCorrupted = true;
                }
                expected++;
                consumedCount++;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_FALSE(dataCorrupted);
    EXPECT_EQ(consumedCount, totalItems);
    EXPECT_TRUE(queue.empty());
}

// 3. AsyncDecoderRunner integration test
TEST(SPSCQueueTest, AsyncDecoderRunnerDecodesFramesNonBlocking)
{
    auto decoder = DecoderFactory::create(BackendType::FFMPEG);
    ASSERT_NE(decoder, nullptr);

    if (!decoder->initialize(TEST_INPUT_PATH)) {
        std::cerr << "[ WARNING ] Could not open test video " << TEST_INPUT_PATH
                  << ". Skipping AsyncDecoderRunner tests.\n";
        return;
    }

    AsyncDecoderRunner runner(std::move(decoder));
    EXPECT_TRUE(runner.start());
    EXPECT_TRUE(runner.isRunning());

    int framesReceived = 0;
    auto startTime = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - startTime < std::chrono::seconds(3)) {
        FramePayload payload;
        if (runner.tryPopFrame(payload)) {
            framesReceived++;
            EXPECT_FALSE(payload.pixelData.empty());
            EXPECT_GT(payload.width, 0);
            EXPECT_GT(payload.height, 0);
            EXPECT_GE(payload.timestamp, 0.0);

            if (framesReceived >= 10) {
                break;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    runner.stop();
    EXPECT_FALSE(runner.isRunning());
    EXPECT_GT(framesReceived, 0);
}
