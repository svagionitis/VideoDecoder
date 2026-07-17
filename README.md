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
├── filters/
│   ├── CMakeLists.txt          # Filters CMake targets
│   ├── VideoFilters.h          # Post-processing filters header
│   └── VideoFilters.cpp        # OpenCV-based filter implementations
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
Use FFmpeg to output a dummy 11-second test video for unit tests and traversal checks:
```bash
ffmpeg -y -f lavfi -i testsrc=duration=11:size=320x240:rate=30 -c:v libx264 -pix_fmt yuv420p ./tests/test_input.mp4
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

## Post-Processing Video Filters

The solution features a dedicated post-processing filters library (`VideoFilters`) built on top of OpenCV. It allows for modular, in-place frame processing on the decoded RGB24/BGR24 buffer.

### Available Filters
- **Brightness & Contrast**: Adjust levels via `BrightnessContrastFilter`.
- **Gaussian Blur**: Blur frames using `GaussianBlurFilter`.
- **Edge Detection**: Overlay Canny edges using `EdgeDetectionFilter`.
- **Text Overlay**: Draw dynamic text labels via `TextOverlayFilter`.
- **Mirror/Flip**: Horizontal/Vertical mirror via `MirrorFilter`.
- **Color Operations**: Negative/Invert (`InvertColorsFilter`), `GrayscaleFilter`, `SepiaFilter`, and channels scaling (`ColorTintFilter`).
- **Advanced Processing**: Local contrast enhancement (`ClaheFilter`), edge-preserving smoothing (`BilateralFilter`), `GammaCorrectionFilter`, `VignetteFilter`, pixelation (`MosaicFilter`), and binarization (`ThresholdFilter`).

### Integration Example
Filters implement the `IFrameProcessor` interface and are attached directly to any `IVideoDecoder` instance. They run sequentially on the hot-path:

```cpp
#include "DecoderFactory.h"
#include "VideoFilters.h"

// 1. Instantiate decoder and filter
auto decoder = DecoderFactory::create(BackendType::FFMPEG);
auto blurFilter = std::make_shared<videodecoder::GaussianBlurFilter>(9);

// 2. Register filter
decoder->addFrameProcessor(blurFilter);

// 3. Initialize and decode - filters run in-place automatically
if (decoder->initialize("video.mp4")) {
    while (decoder->decodeNextFrame()) {
        FrameInfo frame = decoder->getRawFrameData();
        // frame.data now contains the blurred image
    }
}
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

This project is dual-licensed:
- **Open Source**: GNU General Public License version 3 (GPLv3).
- **Commercial**: For use in commercial, proprietary, or closed-source applications.

See the [LICENSE](LICENSE) file for details.
