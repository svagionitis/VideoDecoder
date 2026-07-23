/**
 * @file main.cpp
 * @brief Benchmark application comparing Synchronous vs. Asynchronous video decoding.
 */

#include "AsyncDecoderRunner.h"
#include "DecoderFactory.h"
#include "IVideoDecoder.h"

#include <glog/logging.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

using namespace videodecoder;

/**
 * @struct BenchmarkResults
 * @brief Holds benchmark metrics for a decoding pipeline run.
 */
struct BenchmarkResults {
    std::string pipelineName;
    double initTimeMs = 0.0;
    double totalTimeMs = 0.0;
    size_t totalFrames = 0;
    double avgFps = 0.0;
    double avgLatencyMs = 0.0;
    double minLatencyMs = 0.0;
    double maxLatencyMs = 0.0;
    size_t consumerStalls = 0;
};

/**
 * @brief Parses command line flags.
 */
struct BenchmarkOptions {
    std::string videoPath;
    BackendType backend = BackendType::FFMPEG;
    size_t maxFrames = 0; // 0 = decode all
    int simulatedWorkloadMs = 0; // Simulated consumer delay per frame in ms
};

static void printUsage(const char* progName)
{
    std::cout << "Usage: " << progName << " [options]\n"
              << "Options:\n"
              << "  --file <path>        Path to input video file (default: embedded test video)\n"
              << "  --backend <type>     Decoder backend: 'ffmpeg' or 'gstreamer' (default: ffmpeg)\n"
              << "  --frames <N>         Maximum frames to decode (default: 0 = decode all)\n"
              << "  --workload-ms <ms>   Simulated consumer workload delay per frame in ms (default: 0)\n"
              << "  --help               Display this help message\n";
}

static BenchmarkOptions parseArgs(int argc, char* argv[])
{
    BenchmarkOptions opts;
#ifdef DEFAULT_TEST_INPUT_PATH
    opts.videoPath = DEFAULT_TEST_INPUT_PATH;
#endif

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--file" && i + 1 < argc) {
            opts.videoPath = argv[++i];
        } else if (arg == "--backend" && i + 1 < argc) {
            std::string b = argv[++i];
            if (b == "gstreamer" || b == "gst") {
                opts.backend = BackendType::GSTREAMER;
            } else {
                opts.backend = BackendType::FFMPEG;
            }
        } else if (arg == "--frames" && i + 1 < argc) {
            opts.maxFrames = static_cast<size_t>(std::atol(argv[++i]));
        } else if (arg == "--workload-ms" && i + 1 < argc) {
            opts.simulatedWorkloadMs = std::atoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        }
    }
    return opts;
}

/**
 * @brief Benchmarks synchronous frame decoding on caller thread.
 */
static BenchmarkResults runSyncBenchmark(const BenchmarkOptions& opts)
{
    BenchmarkResults res;
    res.pipelineName = "Synchronous (Blocking)";

    auto decoder = DecoderFactory::create(opts.backend);
    if (!decoder) {
        LOG(ERROR) << "Failed to instantiate decoder for sync benchmark";
        return res;
    }

    auto initStart = std::chrono::high_resolution_clock::now();
    if (!decoder->initialize(opts.videoPath)) {
        LOG(ERROR) << "Failed to initialize decoder for file: " << opts.videoPath;
        return res;
    }
    auto initEnd = std::chrono::high_resolution_clock::now();
    res.initTimeMs = std::chrono::duration<double, std::milli>(initEnd - initStart).count();

    std::vector<double> frameLatencies;
    auto runStart = std::chrono::high_resolution_clock::now();

    while (true) {
        if (opts.maxFrames > 0 && res.totalFrames >= opts.maxFrames) {
            break;
        }

        auto frameStart = std::chrono::high_resolution_clock::now();
        if (!decoder->decodeNextFrame()) {
            break;
        }

        FrameInfo info = decoder->getRawFrameData();
        auto frameEnd = std::chrono::high_resolution_clock::now();
        double frameLatency = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();

        // Touch data to simulate pixel processing
        volatile uint8_t checksum = 0;
        if (info.data && info.size > 0) {
            checksum = info.data[0] ^ info.data[info.size - 1];
        }
        (void)checksum;

        if (opts.simulatedWorkloadMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(opts.simulatedWorkloadMs));
        }

        frameLatencies.push_back(frameLatency);
        res.totalFrames++;
    }

    auto runEnd = std::chrono::high_resolution_clock::now();
    res.totalTimeMs = std::chrono::duration<double, std::milli>(runEnd - runStart).count();

    if (res.totalTimeMs > 0.0) {
        res.avgFps = (static_cast<double>(res.totalFrames) / res.totalTimeMs) * 1000.0;
    }

    if (!frameLatencies.empty()) {
        double sum = std::accumulate(frameLatencies.begin(), frameLatencies.end(), 0.0);
        res.avgLatencyMs = sum / frameLatencies.size();
        res.minLatencyMs = *std::min_element(frameLatencies.begin(), frameLatencies.end());
        res.maxLatencyMs = *std::max_element(frameLatencies.begin(), frameLatencies.end());
    }

    decoder->close();
    return res;
}

/**
 * @brief Benchmarks asynchronous frame decoding via AsyncDecoderRunner + SPSCQueue.
 */
static BenchmarkResults runAsyncBenchmark(const BenchmarkOptions& opts)
{
    BenchmarkResults res;
    res.pipelineName = "Asynchronous (SPSC Queue)";

    auto decoder = DecoderFactory::create(opts.backend);
    if (!decoder) {
        LOG(ERROR) << "Failed to instantiate decoder for async benchmark";
        return res;
    }

    auto initStart = std::chrono::high_resolution_clock::now();
    if (!decoder->initialize(opts.videoPath)) {
        LOG(ERROR) << "Failed to initialize decoder for file: " << opts.videoPath;
        return res;
    }
    auto initEnd = std::chrono::high_resolution_clock::now();
    res.initTimeMs = std::chrono::duration<double, std::milli>(initEnd - initStart).count();

    AsyncDecoderRunner runner(std::move(decoder));
    if (!runner.start()) {
        LOG(ERROR) << "Failed to start AsyncDecoderRunner";
        return res;
    }

    std::vector<double> frameLatencies;
    auto runStart = std::chrono::high_resolution_clock::now();

    while (true) {
        if (opts.maxFrames > 0 && res.totalFrames >= opts.maxFrames) {
            break;
        }

        FramePayload payload;
        auto pollStart = std::chrono::high_resolution_clock::now();

        if (runner.popFrame(payload, std::chrono::milliseconds(100))) {
            auto pollEnd = std::chrono::high_resolution_clock::now();
            double latency = std::chrono::duration<double, std::milli>(pollEnd - pollStart).count();
            frameLatencies.push_back(latency);
            res.totalFrames++;

            // Touch pixel data
            volatile uint8_t checksum = 0;
            if (!payload.pixelData.empty()) {
                checksum = payload.pixelData.data()[0] ^ payload.pixelData.data()[payload.pixelData.size() - 1];
            }
            (void)checksum;

            if (opts.simulatedWorkloadMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(opts.simulatedWorkloadMs));
            }
        } else {
            if (runner.isEos()) {
                break;
            }
            res.consumerStalls++;
        }
    }

    auto runEnd = std::chrono::high_resolution_clock::now();
    res.totalTimeMs = std::chrono::duration<double, std::milli>(runEnd - runStart).count();

    runner.stop();

    if (res.totalTimeMs > 0.0) {
        res.avgFps = (static_cast<double>(res.totalFrames) / res.totalTimeMs) * 1000.0;
    }

    if (!frameLatencies.empty()) {
        double sum = std::accumulate(frameLatencies.begin(), frameLatencies.end(), 0.0);
        res.avgLatencyMs = sum / frameLatencies.size();
        res.minLatencyMs = *std::min_element(frameLatencies.begin(), frameLatencies.end());
        res.maxLatencyMs = *std::max_element(frameLatencies.begin(), frameLatencies.end());
    }

    return res;
}

/**
 * @brief Formats and prints a benchmark comparison table to std::cout.
 */
static void printReport(const BenchmarkResults& syncRes, const BenchmarkResults& asyncRes, const BenchmarkOptions& opts)
{
    std::cout << "\n";
    std::cout << "========================================================================\n";
    std::cout << "               VIDEO DECODER BENCHMARK COMPARISON REPORT                \n";
    std::cout << "========================================================================\n";
    std::cout << " Input File          : " << opts.videoPath << "\n";
    std::cout << " Backend             : " << (opts.backend == BackendType::FFMPEG ? "FFmpeg" : "GStreamer") << "\n";
    std::cout << " Consumer Workload   : " << opts.simulatedWorkloadMs << " ms per frame\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(28) << "Metric" << std::setw(22) << "Sync (Blocking)" << std::setw(22)
              << "Async (Lock-Free)"
              << "\n";
    std::cout << "------------------------------------------------------------------------\n";

    std::cout << std::left << std::setw(28) << "Initialization Latency" << std::setw(22)
              << (std::to_string(syncRes.initTimeMs) + " ms") << std::setw(22)
              << (std::to_string(asyncRes.initTimeMs) + " ms") << "\n";

    std::cout << std::left << std::setw(28) << "Total Frames Decoded" << std::setw(22) << syncRes.totalFrames
              << std::setw(22) << asyncRes.totalFrames << "\n";

    std::cout << std::left << std::setw(28) << "Total Elapsed Time" << std::setw(22)
              << (std::to_string(syncRes.totalTimeMs) + " ms") << std::setw(22)
              << (std::to_string(asyncRes.totalTimeMs) + " ms") << "\n";

    std::ostringstream ssSyncFps, ssAsyncFps;
    ssSyncFps << std::fixed << std::setprecision(2) << syncRes.avgFps << " FPS";
    ssAsyncFps << std::fixed << std::setprecision(2) << asyncRes.avgFps << " FPS";

    std::cout << std::left << std::setw(28) << "Throughput (FPS)" << std::setw(22) << ssSyncFps.str() << std::setw(22)
              << ssAsyncFps.str() << "\n";

    std::ostringstream ssSyncLat, ssAsyncLat;
    ssSyncLat << std::fixed << std::setprecision(3) << syncRes.avgLatencyMs << " ms";
    ssAsyncLat << std::fixed << std::setprecision(3) << asyncRes.avgLatencyMs << " ms";

    std::cout << std::left << std::setw(28) << "Avg Frame Fetch Latency" << std::setw(22) << ssSyncLat.str()
              << std::setw(22) << ssAsyncLat.str() << "\n";

    std::cout << std::left << std::setw(28) << "Consumer Wait Stalls" << std::setw(22) << "N/A" << std::setw(22)
              << asyncRes.consumerStalls << "\n";

    std::cout << "------------------------------------------------------------------------\n";

    if (syncRes.totalTimeMs > 0.0 && asyncRes.totalTimeMs > 0.0) {
        double speedup = syncRes.totalTimeMs / asyncRes.totalTimeMs;
        std::cout << " Performance Ratio    : Async is " << std::fixed << std::setprecision(2) << speedup << "x "
                  << (speedup >= 1.0 ? "FASTER" : "SLOWER") << " than Sync\n";
    }

    std::cout << "========================================================================\n\n";
}

int main(int argc, char* argv[])
{
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;

    BenchmarkOptions opts = parseArgs(argc, argv);

    std::cout << "[INFO] Starting Sync Benchmark...\n";
    BenchmarkResults syncRes = runSyncBenchmark(opts);

    std::cout << "[INFO] Starting Async Benchmark...\n";
    BenchmarkResults asyncRes = runAsyncBenchmark(opts);

    printReport(syncRes, asyncRes, opts);

    return 0;
}
