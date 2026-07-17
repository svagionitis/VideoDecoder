/**
 * @file DecoderTests.cpp
 * @brief Google Test suite for the IVideoDecoder implementations.
 */

#include "DecoderFactory.h"
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <iostream>

#ifndef TEST_INPUT_PATH
#define TEST_INPUT_PATH "test_input.mp4"
#endif

using namespace videodecoder;

/**
 * @class DecoderTestFixture
 * @brief Test fixture for configuring glog in unit tests.
 */
class DecoderTestFixture : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        // Initialize logging once for the test suite
        google::InitGoogleLogging("VideoDecoderTests");
        FLAGS_logtostderr = 1;
    }

    static void TearDownTestSuite()
    {
        google::ShutdownGoogleLogging();
    }
};

// 1. Factory construction check
TEST_F(DecoderTestFixture, FactoryInstantiatesCorrectly)
{
    auto ffmpeg = DecoderFactory::create(BackendType::FFMPEG);
    EXPECT_NE(ffmpeg, nullptr);

    auto gstreamer = DecoderFactory::create(BackendType::GSTREAMER);
    EXPECT_NE(gstreamer, nullptr);
}

// 2. Error handling: invalid file paths
TEST_F(DecoderTestFixture, InvalidFilePathFailsToInitialize)
{
    auto ffmpeg = DecoderFactory::create(BackendType::FFMPEG);
    EXPECT_FALSE(ffmpeg->initialize("non_existent_file_name_12345.mp4"));

    auto gstreamer = DecoderFactory::create(BackendType::GSTREAMER);
    EXPECT_FALSE(gstreamer->initialize("non_existent_file_name_12345.mp4"));
}

// 3. Decoding traversal check (FFmpeg)
TEST_F(DecoderTestFixture, FFmpegDecodesFrames)
{
    auto decoder = DecoderFactory::create(BackendType::FFMPEG);
    ASSERT_NE(decoder, nullptr);

    bool initialized = decoder->initialize(TEST_INPUT_PATH);
    if (!initialized) {
        std::cerr << "[ WARNING ] Could not open test video " << TEST_INPUT_PATH
                  << ". Skipping frame traversal tests.\n";
        return;
    }

    int framesDecoded = 0;
    while (decoder->decodeNextFrame()) {
        framesDecoded++;
        auto frame = decoder->getRawFrameData();

        // Validate frame specifications
        EXPECT_NE(frame.data, nullptr);
        EXPECT_GT(frame.width, 0);
        EXPECT_GT(frame.height, 0);
        EXPECT_EQ(frame.size, static_cast<size_t>(frame.width * frame.height * 3));
        EXPECT_GE(frame.timestamp, 0.0);

        // Break early after 15 frames for quick test suite execution
        if (framesDecoded >= 15) {
            break;
        }
    }

    EXPECT_GT(framesDecoded, 0);
    decoder->close();
}

// 4. Decoding traversal check (GStreamer)
TEST_F(DecoderTestFixture, GStreamerDecodesFrames)
{
    auto decoder = DecoderFactory::create(BackendType::GSTREAMER);
    ASSERT_NE(decoder, nullptr);

    bool initialized = decoder->initialize(TEST_INPUT_PATH);
    if (!initialized) {
        std::cerr << "[ WARNING ] Could not open test video " << TEST_INPUT_PATH
                  << ". Skipping frame traversal tests.\n";
        return;
    }

    int framesDecoded = 0;
    while (decoder->decodeNextFrame()) {
        framesDecoded++;
        auto frame = decoder->getRawFrameData();

        // Validate frame specifications
        EXPECT_NE(frame.data, nullptr);
        EXPECT_GT(frame.width, 0);
        EXPECT_GT(frame.height, 0);
        EXPECT_EQ(frame.size, static_cast<size_t>(frame.width * frame.height * 3));
        EXPECT_GE(frame.timestamp, 0.0);

        // Break early after 15 frames
        if (framesDecoded >= 15) {
            break;
        }
    }

    EXPECT_GT(framesDecoded, 0);
    decoder->close();
}

// 5. Statistics/Metadata retrieval check
TEST_F(DecoderTestFixture, RetrievesVideoMetadata)
{
    // FFmpeg metadata test
    {
        auto decoder = DecoderFactory::create(BackendType::FFMPEG);
        ASSERT_NE(decoder, nullptr);

        bool initialized = decoder->initialize(TEST_INPUT_PATH);
        if (initialized) {
            auto meta = decoder->getVideoMetadata();
            EXPECT_GT(meta.width, 0);
            EXPECT_GT(meta.height, 0);
            EXPECT_GT(meta.frameRate, 0.0);
            EXPECT_GT(meta.duration, 0.0);
            EXPECT_FALSE(meta.codecName.empty());
            EXPECT_NE(meta.codecName, "unknown");
            decoder->close();
        }
    }

    // GStreamer metadata test
    {
        auto decoder = DecoderFactory::create(BackendType::GSTREAMER);
        ASSERT_NE(decoder, nullptr);

        bool initialized = decoder->initialize(TEST_INPUT_PATH);
        if (initialized) {
            auto meta = decoder->getVideoMetadata();
            EXPECT_GT(meta.width, 0);
            EXPECT_GT(meta.height, 0);
            EXPECT_GT(meta.frameRate, 0.0);
            EXPECT_GT(meta.duration, 0.0);
            EXPECT_FALSE(meta.codecName.empty());
            decoder->close();
        }
    }
}

// 6. Latency/Performance statistics check
TEST_F(DecoderTestFixture, MeasuresDecoderLatency)
{
    // FFmpeg performance test
    {
        auto decoder = DecoderFactory::create(BackendType::FFMPEG);
        ASSERT_NE(decoder, nullptr);

        bool initialized = decoder->initialize(TEST_INPUT_PATH);
        if (initialized) {
            auto statsBefore = decoder->getPerformanceStats();
            EXPECT_GT(statsBefore.initializationTimeMs, 0.0);
            EXPECT_EQ(statsBefore.totalDecodedFrames, 0);
            EXPECT_DOUBLE_EQ(statsBefore.averageDecodeTimeMs, 0.0);

            int framesDecoded = 0;
            while (decoder->decodeNextFrame()) {
                framesDecoded++;
                auto frame = decoder->getRawFrameData();
                EXPECT_GT(frame.decodeTimeMs, 0.0);
                if (framesDecoded >= 5) {
                    break;
                }
            }

            auto statsAfter = decoder->getPerformanceStats();
            EXPECT_EQ(statsAfter.totalDecodedFrames, static_cast<uint64_t>(framesDecoded));
            EXPECT_GT(statsAfter.averageDecodeTimeMs, 0.0);

            decoder->close();
        }
    }

    // GStreamer performance test
    {
        auto decoder = DecoderFactory::create(BackendType::GSTREAMER);
        ASSERT_NE(decoder, nullptr);

        bool initialized = decoder->initialize(TEST_INPUT_PATH);
        if (initialized) {
            auto statsBefore = decoder->getPerformanceStats();
            EXPECT_GT(statsBefore.initializationTimeMs, 0.0);
            EXPECT_EQ(statsBefore.totalDecodedFrames, 0);
            EXPECT_DOUBLE_EQ(statsBefore.averageDecodeTimeMs, 0.0);

            int framesDecoded = 0;
            while (decoder->decodeNextFrame()) {
                framesDecoded++;
                auto frame = decoder->getRawFrameData();
                EXPECT_GT(frame.decodeTimeMs, 0.0);
                if (framesDecoded >= 5) {
                    break;
                }
            }

            auto statsAfter = decoder->getPerformanceStats();
            EXPECT_EQ(statsAfter.totalDecodedFrames, static_cast<uint64_t>(framesDecoded));
            EXPECT_GT(statsAfter.averageDecodeTimeMs, 0.0);

            decoder->close();
        }
    }
}

// 7. Seek functionality check
TEST_F(DecoderTestFixture, SeeksSuccessfully)
{
    // FFmpeg seek test
    {
        auto decoder = DecoderFactory::create(BackendType::FFMPEG);
        ASSERT_NE(decoder, nullptr);

        bool initialized = decoder->initialize(TEST_INPUT_PATH);
        if (initialized) {
            EXPECT_TRUE(decoder->seek(1.0));
            EXPECT_TRUE(decoder->decodeNextFrame());
            auto frame = decoder->getRawFrameData();
            EXPECT_GE(frame.timestamp, 0.0);

            // Check that seeking out of bounds fails
            auto meta = decoder->getVideoMetadata();
            EXPECT_FALSE(decoder->seek(-1.0));
            EXPECT_FALSE(decoder->seek(meta.duration + 5.0));

            decoder->close();
        }
    }

    // GStreamer seek test
    {
        auto decoder = DecoderFactory::create(BackendType::GSTREAMER);
        ASSERT_NE(decoder, nullptr);

        bool initialized = decoder->initialize(TEST_INPUT_PATH);
        if (initialized) {
            EXPECT_TRUE(decoder->seek(1.0));
            EXPECT_TRUE(decoder->decodeNextFrame());
            auto frame = decoder->getRawFrameData();
            EXPECT_GE(frame.timestamp, 0.0);

            // Check that seeking out of bounds fails
            auto meta = decoder->getVideoMetadata();
            EXPECT_FALSE(decoder->seek(-1.0));
            EXPECT_FALSE(decoder->seek(meta.duration + 5.0));

            decoder->close();
        }
    }
}

// 8. Output Format Selection check
TEST_F(DecoderTestFixture, OutputFormatSelection)
{
    // FFmpeg format check
    {
        auto decoder = DecoderFactory::create(BackendType::FFMPEG);
        ASSERT_NE(decoder, nullptr);

        bool initialized = decoder->initialize(TEST_INPUT_PATH, PixelFormat::BGR24);
        if (initialized) {
            auto meta = decoder->getVideoMetadata();
            EXPECT_EQ(meta.format, PixelFormat::BGR24);

            EXPECT_TRUE(decoder->decodeNextFrame());
            auto frame = decoder->getRawFrameData();
            EXPECT_EQ(frame.format, PixelFormat::BGR24);
            decoder->close();
        }
    }

    // GStreamer format check
    {
        auto decoder = DecoderFactory::create(BackendType::GSTREAMER);
        ASSERT_NE(decoder, nullptr);

        bool initialized = decoder->initialize(TEST_INPUT_PATH, PixelFormat::BGR24);
        if (initialized) {
            auto meta = decoder->getVideoMetadata();
            EXPECT_EQ(meta.format, PixelFormat::BGR24);

            EXPECT_TRUE(decoder->decodeNextFrame());
            auto frame = decoder->getRawFrameData();
            EXPECT_EQ(frame.format, PixelFormat::BGR24);
            decoder->close();
        }
    }
}
