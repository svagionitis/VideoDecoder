# Cross-Platform C++17 Video Decoder Solution

A complete, production-ready, cross-platform (Linux and Windows) video decoding solution built as a C++17 shared library. The project features concrete decoder implementations for **FFmpeg** and **GStreamer**, a multi-threaded **Qt6 Desktop GUI** application, a high-speed **CLI client**, and Google Test suites.

---

## Features

- **Abstract Interface & Factory**: Generic `IVideoDecoder` interface with a concrete factory `DecoderFactory` supporting backend selection.
- **FFmpeg Backend**: Uses the modern, non-blocking `avcodec_send_packet` / `avcodec_receive_frame` decode loop.
- **GStreamer Backend**: Uses a stable appsink pipeline (`filesrc ! decodebin ! videoconvert ! appsink`) configured for raw RGB output.
- **Low Latency & Memory Reuse**: The color space conversion (YUV -> RGB24) writes directly into a pre-allocated reusable internal buffer. Sizing and context allocation occur only if the resolution changes, ensuring **zero reallocations on the hot path**.
- **RAII Resource Management**: All native C-style handles from FFmpeg (e.g. `AVFormatContext`, `AVCodecContext`, `AVFrame`, `AVPacket`) and GStreamer (`GstElement`, `GstSample`) are encapsulated into unique smart pointer wrappers with custom deleters. Buffer mapping uses the RAII wrapper `GstMapInfoWrapper`.
- **Desktop Client**: Cross-platform Qt6 desktop application running the decoder inside a background `QThread` (to keep the UI event loop smooth) and painting frames centered while maintaining the correct aspect ratio.
- **Console Client**: High-speed command-line tool reporting frame sizes, presentation timestamps (PTS), elapsed time, and decoding framerates (FPS).
- **Unit Tests**: Full verification suite using Google Test (gtest).
- **Logging**: Production-grade log tracking using Google Logging (glog).
- **Formatting & Style Enforcement**: Standardized code formatting using WebKit style conventions, managed via `.clang-format` and integrated with CMake and Git pre-commit hooks.

---

## Directory Structure

```text
├── CMakeLists.txt              # Root CMake configuration
├── LICENSE                     # MIT License
├── README.md                   # Project documentation
├── .gitignore                  # Git ignore rules
├── .editorconfig               # Editor coding standards
├── .clang-format               # WebKit code formatting rules
├── .githooks/
│   └── pre-commit              # Git pre-commit formatter hook
├── docs/
│   └── architecture.md         # Architectural docs (C4 model & frame lifecycle)
├── lib/
│   ├── CMakeLists.txt          # Library CMake targets
│   ├── IVideoDecoder.h         # Abstract decoder interface
│   ├── DecoderFactory.h        # Factory header
│   ├── DecoderFactory.cpp      # Factory source
│   ├── DecoderTypes.h          # Public structures & enums
│   ├── Visibility.h            # Symbol export/import macro
│   ├── FFmpegDecoder.h/cpp     # FFmpeg backend decoder
│   └── GStreamerDecoder.h/cpp  # GStreamer backend decoder
├── cli/
│   ├── CMakeLists.txt          # CLI CMake target
│   └── main.cpp                # CLI app main entry point
├── gui/
│   ├── CMakeLists.txt          # GUI CMake target
│   ├── main.cpp                # GUI entry point
│   ├── MainWindow.h/cpp        # Playback control panel layout
│   ├── VideoWidget.h/cpp       # Custom double-buffered paint canvas
│   └── VideoWorker.h/cpp       # Background decoder QThread
└── tests/
    ├── CMakeLists.txt          # Test CMake target
    └── DecoderTests.cpp        # GTest unit tests
```

---

## Dependencies

Ensure the following runtimes and SDKs are installed on your system:
- **C++17 Compiler** (GCC 9+, Clang 10+, or MSVC 2019+)
- **CMake** (v3.16+) and **Ninja** build generator
- **FFmpeg Libraries**: `libavformat`, `libavcodec`, `libavutil`, `libswscale`
- **GStreamer Libraries**: `gstreamer-1.0`, `gstreamer-app-1.0`, `gstreamer-video-1.0`
- **Google Logging (glog)**
- **Google Test (gtest)**
- **Qt6 SDK** (Core, Gui, Widgets modules - *Only required if building the GUI application*)

---

## Build Configurations

We support modular building using the following CMake options:
- `BUILD_CLI` (Default: `ON`): Compile the CLI executable.
- `BUILD_GUI` (Default: `OFF`): Compile the Qt GUI desktop app. (Bypasses Qt search if disabled).
- `BUILD_TESTS` (Default: `ON`): Compile unit tests. (Bypasses GTest search if disabled).

### Compiling Default Targets (CLI and Tests)
```bash
cmake -B build -G Ninja
ninja -C build
```

### Compiling All Targets (including Qt6 GUI)
```bash
cmake -B build -DBUILD_GUI=ON -G Ninja
ninja -C build
```

---

## Execution

### Generating a Test Video
Use FFmpeg to output a dummy test video for traversal checks:
```bash
ffmpeg -y -f lavfi -i testsrc=duration=5:size=320x240:rate=30 -c:v libx264 ./tests/test_input.mp4
```

### Running the Unit Tests
```bash
./build/tests/VideoDecoderTests
```

### Running the CLI Client
```bash
# Decode using FFmpeg
./build/cli/VideoDecoderCLI --input ./tests/test_input.mp4 --backend ffmpeg

# Decode using GStreamer
./build/cli/VideoDecoderCLI --input ./tests/test_input.mp4 --backend gstreamer
```

### Running the GUI Client
```bash
./build/gui/VideoDecoderGUI
```

---

## Code Quality & Style Enforcement

The project follows **WebKit** C++ style guidelines.

- **On-demand Formatting**: Format the entire project using CMake and `clang-format`:
  ```bash
  ninja -C build format
  ```
- **Git Pre-commit Hook**: CMake automatically installs the pre-commit hook from `.githooks/pre-commit` into `.git/hooks/` upon configuration. Staged C++ modifications will automatically format upon running `git commit`.

---

## License

This project is licensed under the [MIT License](LICENSE).
