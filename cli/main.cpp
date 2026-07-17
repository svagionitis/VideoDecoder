/**
 * @file main.cpp
 * @brief Entry point for the advanced VideoDecoder CLI client featuring SDL2, Console, and Braille playback modes.
 */

#include "DecoderFactory.h"
#include <SDL2/SDL.h>
#include <chrono>
#include <cstring>
#include <fstream>
#include <glog/logging.h>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#else
#include <conio.h>
#endif

/**
 * @brief Structure containing the state and lifecycle handlers of the SDL2 window.
 */
struct SDLContext {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;

    bool init(int w, int h)
    {
        width = w;
        height = h;
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            LOG(ERROR) << "SDL2: Failed to initialize Video: " << SDL_GetError();
            return false;
        }
        window = SDL_CreateWindow("VideoDecoder CLI Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width,
            height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!window) {
            LOG(ERROR) << "SDL2: Failed to create window: " << SDL_GetError();
            return false;
        }
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
            renderer = SDL_CreateRenderer(window, -1, 0);
        }
        if (!renderer) {
            LOG(ERROR) << "SDL2: Failed to create renderer: " << SDL_GetError();
            return false;
        }
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, width, height);
        if (!texture) {
            LOG(ERROR) << "SDL2: Failed to create texture: " << SDL_GetError();
            return false;
        }
        return true;
    }

    void render(const uint8_t* data)
    {
        if (texture) {
            SDL_UpdateTexture(texture, nullptr, data, width * 3);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        }
    }

    void close()
    {
        if (texture)
            SDL_DestroyTexture(texture);
        if (renderer)
            SDL_DestroyRenderer(renderer);
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

#ifndef _WIN32
/**
 * @class TerminalRawMode
 * @brief RAII utility to switch the terminal to non-canonical (raw) input mode.
 */
class TerminalRawMode {
public:
    TerminalRawMode()
    {
        m_valid = (tcgetattr(STDIN_FILENO, &m_origTermios) == 0);
        if (m_valid) {
            termios raw = m_origTermios;
            raw.c_lflag &= ~(ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        }
    }
    ~TerminalRawMode()
    {
        if (m_valid) {
            tcsetattr(STDIN_FILENO, TCSANOW, &m_origTermios);
        }
    }

private:
    termios m_origTermios;
    bool m_valid = false;
};
#endif

/**
 * @brief Checks if the Escape key was pressed in the console/terminal.
 */
bool checkEscapePressed()
{
#ifdef _WIN32
    if (_kbhit()) {
        int c = _getch();
        if (c == 27) { // Escape key
            return true;
        }
    }
#else
    struct timeval tv = { 0, 0 };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
    if (FD_ISSET(STDIN_FILENO, &fds)) {
        char c;
        if (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == 27) { // Escape key
                return true;
            }
        }
    }
#endif
    return false;
}

/**
 * @brief Fetches active terminal dimensions.
 */
void getTerminalSize(int& cols, int& rows)
{
    cols = 80;
    rows = 24;
#ifndef _WIN32
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        if (w.ws_col > 0)
            cols = w.ws_col;
        if (w.ws_row > 0)
            rows = w.ws_row;
    }
#endif
}

/**
 * @brief Renders downscaled truecolor ANSI half-blocks directly to the terminal.
 */
void renderConsole(const uint8_t* frameData, int width, int height, int cols, int rows)
{
    double videoAspect = static_cast<double>(width) / height;

    int targetWidth = cols;
    int targetHeight = static_cast<int>(targetWidth / videoAspect);
    if (targetHeight > rows * 2) {
        targetHeight = rows * 2;
        targetWidth = static_cast<int>(targetHeight * videoAspect);
    }
    if (targetWidth % 2 != 0)
        targetWidth--;
    if (targetHeight % 2 != 0)
        targetHeight--;
    if (targetWidth <= 0 || targetHeight <= 0)
        return;

    std::string out;
    out.reserve(targetWidth * (targetHeight / 2) * 40);
    out += "\x1b[H"; // Move cursor to top-left corner (prevents scrolling flicker)

    for (int y = 0; y < targetHeight; y += 2) {
        for (int x = 0; x < targetWidth; ++x) {
            int srcX = x * width / targetWidth;
            int srcY_top = y * height / targetHeight;
            int srcY_bot = (y + 1) * height / targetHeight;

            int topIdx = (srcY_top * width + srcX) * 3;
            int botIdx = (srcY_bot * width + srcX) * 3;

            uint8_t r_top = frameData[topIdx];
            uint8_t g_top = frameData[topIdx + 1];
            uint8_t b_top = frameData[topIdx + 2];

            uint8_t r_bot = 0, g_bot = 0, b_bot = 0;
            if (y + 1 < targetHeight) {
                r_bot = frameData[botIdx];
                g_bot = frameData[botIdx + 1];
                b_bot = frameData[botIdx + 2];
            }

            char cell[64];
            std::sprintf(cell, "\x1b[48;2;%d;%d;%dm\x1b[38;2;%d;%d;%dm▄", r_top, g_top, b_top, r_bot, g_bot, b_bot);
            out += cell;
        }
        out += "\x1b[0m\n";
    }
    std::printf("%s", out.c_str());
    std::fflush(stdout);
}

/**
 * @brief Renders high-resolution truecolor Braille character grids directly in the console.
 */
void renderBraille(const uint8_t* frameData, int width, int height, int cols, int rows)
{
    double videoAspect = static_cast<double>(width) / height;

    int targetWidth = cols * 2;
    int targetHeight = static_cast<int>(targetWidth / videoAspect);
    if (targetHeight > rows * 4) {
        targetHeight = rows * 4;
        targetWidth = static_cast<int>(targetHeight * videoAspect);
    }
    targetWidth = (targetWidth / 2) * 2;
    targetHeight = (targetHeight / 4) * 4;
    if (targetWidth <= 0 || targetHeight <= 0)
        return;

    std::string out;
    out.reserve((targetWidth / 2) * (targetHeight / 4) * 45);
    out += "\x1b[H"; // Reposition cursor

    // Unicode Braille dot mapping layout
    const int dotMap[4][2] = { { 0x01, 0x08 }, { 0x02, 0x10 }, { 0x04, 0x20 }, { 0x40, 0x80 } };

    for (int cy = 0; cy < targetHeight; cy += 4) {
        for (int cx = 0; cx < targetWidth; cx += 2) {
            int r_sum = 0, g_sum = 0, b_sum = 0;
            int subpixelLuma[4][2];
            int totalLuma = 0;

            for (int dy = 0; dy < 4; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    int srcX = (cx + dx) * width / targetWidth;
                    int srcY = (cy + dy) * height / targetHeight;
                    int idx = (srcY * width + srcX) * 3;

                    uint8_t r = frameData[idx];
                    uint8_t g = frameData[idx + 1];
                    uint8_t b = frameData[idx + 2];

                    r_sum += r;
                    g_sum += g;
                    b_sum += b;

                    int luma = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b);
                    subpixelLuma[dy][dx] = luma;
                    totalLuma += luma;
                }
            }

            int avgLuma = totalLuma / 8;
            int offset = 0;
            int activeCount = 0;
            int actR = 0, actG = 0, actB = 0;

            for (int dy = 0; dy < 4; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    if (subpixelLuma[dy][dx] >= avgLuma) {
                        offset |= dotMap[dy][dx];

                        int srcX = (cx + dx) * width / targetWidth;
                        int srcY = (cy + dy) * height / targetHeight;
                        int idx = (srcY * width + srcX) * 3;
                        actR += frameData[idx];
                        actG += frameData[idx + 1];
                        actB += frameData[idx + 2];
                        activeCount++;
                    }
                }
            }

            uint8_t r_draw = 0, g_draw = 0, b_draw = 0;
            if (activeCount > 0) {
                r_draw = actR / activeCount;
                g_draw = actG / activeCount;
                b_draw = actB / activeCount;
            } else {
                r_draw = r_sum / 8;
                g_draw = g_sum / 8;
                b_draw = b_sum / 8;
            }

            // UTF-8 encode Unicode Braille range U+2800 + offset (3-byte sequence)
            char utf8_bytes[4];
            utf8_bytes[0] = static_cast<char>(0xE2);
            utf8_bytes[1] = static_cast<char>(0xA0 | (offset >> 6));
            utf8_bytes[2] = static_cast<char>(0x80 | (offset & 0x3F));
            utf8_bytes[3] = '\0';

            char cell[64];
            std::sprintf(cell, "\x1b[38;2;%d;%d;%dm%s", r_draw, g_draw, b_draw, utf8_bytes);
            out += cell;
        }
        out += "\x1b[0m\n";
    }
    std::printf("%s", out.c_str());
    std::fflush(stdout);
}

/**
 * @brief Saves raw RGB24 frame pixels to standard PPM format.
 */
void saveSnapshot(const std::string& filename, const uint8_t* data, int width, int height)
{
    if (!data || width <= 0 || height <= 0)
        return;
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        LOG(ERROR) << "CLI Client: Failed to open snapshot file for writing: " << filename;
        return;
    }
    out << "P6\n" << width << " " << height << "\n255\n";
    out.write(reinterpret_cast<const char*>(data), width * height * 3);
    LOG(INFO) << "CLI Client: Saved final snapshot to " << filename;
}

/**
 * @brief Prints usage details.
 */
void printUsage(const char* progName)
{
    std::cout << "Usage: " << progName << " <rtsp_url> <--ffmpeg | --gstreamer> [options]\n\n"
              << "Arguments:\n"
              << "  <rtsp_url>       The RTSP stream URL to test (e.g. rtsp://192.168.1.100:554/stream1)\n\n"
              << "Backend Selection (Required):\n"
              << "  --ffmpeg         Use FFmpeg decoder backend.\n"
              << "  --gstreamer      Use GStreamer decoder backend.\n\n"
              << "Options:\n"
              << "  --sdl2           Enable SDL2 window playback mode (default for FFmpeg, optional for GStreamer).\n"
              << "  --console        Enable terminal/console-only playback mode.\n"
              << "                   Renders downscaled truecolor video frames directly in the terminal\n"
              << "                   using ANSI escape codes. Recommended for headless servers / SSH sessions.\n\n"
              << "  --braille        Enable terminal/console-only playback mode using Unicode Braille\n"
              << "                   characters. Renders dithered high-resolution subpixels with\n"
              << "                   truecolor mapping, capturing 8 subpixels per character cell.\n\n"
              << "  -h, --help       Display this help message and exit.\n\n"
              << "Note:\n"
              << "  For GStreamer, if neither --sdl2 nor --console is specified, GStreamer native windowing\n"
              << "  (autovideosink) is used for playback by default.\n\n"
              << "Key Controls during Playback:\n"
              << "  Escape           Exit the playback loop and take a final snapshot.\n";
}

int main(int argc, char* argv[])
{
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;

    if (argc < 2) {
        printUsage(argv[0]);
        google::ShutdownGoogleLogging();
        return 1;
    }

    std::string rtspUrl = argv[1];
    if (rtspUrl == "--help" || rtspUrl == "-h") {
        printUsage(argv[0]);
        google::ShutdownGoogleLogging();
        return 0;
    }

    bool hasBackend = false;
    videodecoder::BackendType backend = videodecoder::BackendType::FFMPEG;

    bool useSdl2 = false;
    bool useConsole = false;
    bool useBraille = false;

    // Parse options
    for (int i = 2; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--ffmpeg") {
            backend = videodecoder::BackendType::FFMPEG;
            hasBackend = true;
        } else if (arg == "--gstreamer") {
            backend = videodecoder::BackendType::GSTREAMER;
            hasBackend = true;
        } else if (arg == "--sdl2") {
            useSdl2 = true;
        } else if (arg == "--console") {
            useConsole = true;
        } else if (arg == "--braille") {
            useBraille = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            google::ShutdownGoogleLogging();
            return 0;
        }
    }

    if (!hasBackend) {
        std::cerr << "Error: Backend selection (--ffmpeg or --gstreamer) is required.\n\n";
        printUsage(argv[0]);
        google::ShutdownGoogleLogging();
        return 1;
    }

    // Default playback modes if unspecified
    if (backend == videodecoder::BackendType::FFMPEG) {
        if (!useConsole && !useBraille) {
            useSdl2 = true;
        }
    }

    // If GStreamer is selected and no manual rendering mode is set, use GStreamer's native autovideosink window
    bool nativeGstreamer = (backend == videodecoder::BackendType::GSTREAMER && !useSdl2 && !useConsole && !useBraille);
    if (nativeGstreamer) {
        // This env var tells the GStreamer backend decoder to construct a pipeline splitting output to autovideosink
        setenv("GST_USE_NATIVE_SINK", "1", 1);
        LOG(INFO) << "CLI Client: Native GStreamer autovideosink enabled.";
    }

    auto decoder = videodecoder::DecoderFactory::create(backend);
    if (!decoder) {
        LOG(FATAL) << "CLI Client: Failed to create decoder backend.";
    }

    if (!decoder->initialize(rtspUrl)) {
        LOG(ERROR) << "CLI Client: Failed to initialize decoder on source: " << rtspUrl;
        google::ShutdownGoogleLogging();
        return 1;
    }

    // Retrieve and print video statistics
    auto metadata = decoder->getVideoMetadata();
    LOG(INFO) << "========================================";
    LOG(INFO) << "CLI Client: Video Stream Statistics:";
    LOG(INFO) << "  - Resolution: " << metadata.width << "x" << metadata.height;
    LOG(INFO) << "  - Frame Rate: " << metadata.frameRate << " FPS";
    LOG(INFO) << "  - Duration:   " << metadata.duration << " seconds";
    LOG(INFO) << "  - Codec:      " << metadata.codecName;
    LOG(INFO) << "========================================";

    // Initialize raw terminal mode for console/braille or native escape key interceptions
    std::unique_ptr<TerminalRawMode> rawMode;
    if (useConsole || useBraille || nativeGstreamer) {
        if (useConsole || useBraille) {
            // Clear screen and hide terminal cursor
            std::printf("\x1b[2J\x1b[?25l");
            std::fflush(stdout);
        }
        rawMode = std::make_unique<TerminalRawMode>();
    }

    SDLContext sdl;
    bool sdlInit = false;
    bool quit = false;

    std::vector<uint8_t> lastFrameBytes;
    int lastWidth = 0;
    int lastHeight = 0;

    double lastPts = -1.0;
    auto lastFrameTime = std::chrono::high_resolution_clock::now();

    while (!quit && decoder->decodeNextFrame()) {
        auto frame = decoder->getRawFrameData();
        if (!frame.data || frame.width <= 0 || frame.height <= 0) {
            continue;
        }

        // Cache frame details for the final snapshot on exit
        lastFrameBytes.assign(frame.data, frame.data + frame.size);
        lastWidth = frame.width;
        lastHeight = frame.height;

        if (useSdl2) {
            if (!sdlInit) {
                if (!sdl.init(frame.width, frame.height)) {
                    LOG(FATAL) << "CLI Client: Failed to initialize SDL2 window.";
                }
                sdlInit = true;
            }
            sdl.render(frame.data);

            // Handle SDL windows events (close window or key esc exit)
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    quit = true;
                } else if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        quit = true;
                    }
                }
            }
        } else if (useConsole) {
            int cols, rows;
            getTerminalSize(cols, rows);
            renderConsole(frame.data, frame.width, frame.height, cols, rows);
        } else if (useBraille) {
            int cols, rows;
            getTerminalSize(cols, rows);
            renderBraille(frame.data, frame.width, frame.height, cols, rows);
        }

        // Non-blocking Escape key check for console, braille, and native autovideosink modes
        if (!useSdl2) {
            if (checkEscapePressed()) {
                quit = true;
            }
        }

        // Frame timing pacing (maintains smooth playback matching PTS)
        if (lastPts >= 0.0) {
            double ptsDiff = frame.timestamp - lastPts;
            if (ptsDiff > 0.001 && ptsDiff < 5.0) {
                auto now = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> actualElapsed = now - lastFrameTime;
                double sleepTime = ptsDiff - actualElapsed.count();
                if (sleepTime > 0.001) {
                    std::this_thread::sleep_for(std::chrono::microseconds(static_cast<long long>(sleepTime * 1000000)));
                }
            }
        }
        lastPts = frame.timestamp;
        lastFrameTime = std::chrono::high_resolution_clock::now();
    }

    // Clean up
    if (useConsole || useBraille) {
        // Restore cursor visibility and clear traits
        std::printf("\x1b[?25h\x1b[0m\n");
        std::fflush(stdout);
    }
    rawMode.reset();

    if (sdlInit) {
        sdl.close();
    }

    // Take final snapshot on exit
    if (!lastFrameBytes.empty()) {
        saveSnapshot("snapshot.ppm", lastFrameBytes.data(), lastWidth, lastHeight);
    }

    decoder->close();
    google::ShutdownGoogleLogging();
    return 0;
}
