/**
 * @file main.cpp
 * @brief Entry point for the VideoDecoder Command Line Interface (CLI) client.
 */

#include "DecoderFactory.h"
#include <chrono>
#include <glog/logging.h>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

/**
 * @brief Prints instructions on how to run the CLI program.
 * @param progName The filename of the program executable.
 */
void printUsage(const char* progName)
{
    std::cout << "Usage: " << progName << " --input <file_path> [--backend <ffmpeg|gstreamer>]\n"
              << "Options:\n"
              << "  -i, --input     Path to the input video file (required)\n"
              << "  -b, --backend   Decoder backend: 'ffmpeg' or 'gstreamer' (default: ffmpeg)\n"
              << "  -h, --help      Display this help menu\n";
}

int main(int argc, char* argv[])
{
    // Initialize Google Logging
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1; // Ensure logs stream directly to the terminal stderr

    std::string filePath;
    std::string backendStr = "ffmpeg";

    // Basic command line argument parser
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            google::ShutdownGoogleLogging();
            return 0;
        } else if ((arg == "--input" || arg == "-i") && i + 1 < argc) {
            filePath = argv[++i];
        } else if ((arg == "--backend" || arg == "-b") && i + 1 < argc) {
            backendStr = argv[++i];
        }
    }

    if (filePath.empty()) {
        LOG(ERROR) << "CLI Client: Missing required argument --input.";
        printUsage(argv[0]);
        google::ShutdownGoogleLogging();
        return 1;
    }

    videodecoder::BackendType backend = videodecoder::BackendType::FFMPEG;
    if (backendStr == "gstreamer") {
        backend = videodecoder::BackendType::GSTREAMER;
    } else if (backendStr != "ffmpeg") {
        LOG(WARNING) << "CLI Client: Unknown backend '" << backendStr << "' specified, defaulting to FFmpeg.";
    }

    LOG(INFO) << "CLI Client: Selected backend: "
              << (backend == videodecoder::BackendType::FFMPEG ? "FFmpeg" : "GStreamer");

    // Construct decoder using static factory
    auto decoder = videodecoder::DecoderFactory::create(backend);
    if (!decoder) {
        LOG(FATAL) << "CLI Client: Failed to instantiate requested decoder backend.";
    }

    // Initialize decoder
    if (!decoder->initialize(filePath)) {
        LOG(ERROR) << "CLI Client: Failed to initialize decoder on video file: " << filePath;
        google::ShutdownGoogleLogging();
        return 1;
    }

    int frameCount = 0;
    auto startTime = std::chrono::high_resolution_clock::now();

    // Hot loop traversal
    while (decoder->decodeNextFrame()) {
        frameCount++;
        auto frame = decoder->getRawFrameData();

        // Print statistics every 30 frames to avoid console clutter
        if (frameCount % 30 == 0 || frameCount == 1) {
            LOG(INFO) << "CLI Client: Decoded Frame #" << frameCount << " | Size: " << frame.width << "x"
                      << frame.height << " | Bytes: " << frame.size << " | PTS: " << frame.timestamp << " seconds";
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;

    LOG(INFO) << "CLI Client: Traversal completed.";
    LOG(INFO) << "CLI Client: Total frames decoded: " << frameCount;
    LOG(INFO) << "CLI Client: Time elapsed: " << elapsed.count() << " seconds";

    if (elapsed.count() > 0.0) {
        LOG(INFO) << "CLI Client: Average rendering performance: " << (frameCount / elapsed.count()) << " FPS";
    }

    decoder->close();
    google::ShutdownGoogleLogging();
    return 0;
}
