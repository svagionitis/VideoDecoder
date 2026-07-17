Act as an expert Senior C++ Video Infrastructure Engineer. Write a complete, production-ready, cross-platform (Linux and Windows) video decoding solution built as a C++17 shared library. The project must feature production-grade logging, robust testing, a CLI client, and a Qt-based GUI application, optimized for low latency and high memory efficiency.

The architecture, build system, documentation, and directory structure must meet the following strict technical requirements:

1. Project Directory Structure & C4 Architecture Documentation:
- Create a dedicated `docs/` directory at the root of the project to hold architectural design documentation.
- Inside `docs/`, provide C4 Model documentation to describe the system architecture at multiple levels (System Context, Container, and Component levels).
- Embed clean, well-formatted ASCII art diagrams directly inside these C4 model documents to visually illustrate each architectural level (Context, Container, Component) and the frame lifecycle.
- Additionally, render the C4 diagrams using text-based diagramming formats (such as Mermaid or PlantUML) alongside the ASCII versions so they can be version-controlled and rendered visually if needed.

2. Code Documentation & Doxygen:
- Document all classes, interfaces, structures, methods, parameters, and return values using standard Doxygen syntax.
- Use tags such as `/** ... */`, `@brief`, `@param`, `@return`, `@throws`, and `@note` for clear code maintainability.
- Ensure the abstract interface and factory are meticulously documented so any external developer can easily understand the API contract.

3. Resource Management & RAII:
- Strictly adhere to RAII (Resource Acquisition Is Initialization) design patterns.
- Encapsulate all native C-style handles from FFmpeg (e.g., AVFormatContext, AVCodecContext, AVFrame, AVPacket) and GStreamer (e.g., GstElement, GstBuffer, GstMessage) into custom RAII wrappers or smart pointers with custom deleters (e.g., unique_ptr with custom deleter).
- Ensure no resource leaks occur during standard execution, error handling, or sudden thread termination.

4. Core Library Architecture (FFmpeg & GStreamer backends):
- Define an abstract interface `IVideoDecoder` with clean virtual methods: `initialize(std::string_view filePath)`, `decodeNextFrame()`, `getRawFrameData()`, and `close()`.
- Implement concrete classes `FFmpegDecoder` and `GStreamerDecoder` inheriting from `IVideoDecoder`.
- Use a Factory pattern (`DecoderFactory`) to safely instantiate the correct backend based on an enum class `BackendType { FFMPEG, GSTREAMER }`.
- FFmpeg: Implement the modern `avcodec_send_packet` / `avcodec_receive_frame` non-blocking decoding loop.
- GStreamer: Construct a stable pipeline (e.g., `filesrc ! decodebin ! appsink`) and map the raw pixels out of the appsink using GstMapInfo.

5. Low Latency & Memory Optimization:
- Optimize the hot path to avoid memory reallocations during runtime. Reuse frame storage buffers.
- Leverage zero-copy operations or explicit shared memory mapping to bridge raw frame data from the library to the clients.
- Fine-tune GStreamer/FFmpeg buffer sizes to guarantee minimum latency for real-time video streaming.

6. Infrastructure, Logging, & Unit Tests:
- Ensure seamless cross-platform compilation on both Linux (GCC/Clang) and Windows (MSVC).
- Integrate Google Logging (glog) for comprehensive event tracking, warnings, and error categorization (INFO, WARNING, ERROR, VLOG).
- Write rigorous Unit Tests using Google Test (gtest) covering edge cases like file-not-found, codec-not-supported, and standard frame-by-frame traversal.

7. Client Applications:
- CLI Client: A minimal, high-speed terminal app that instantiates the decoder, parses input flags for the file path/backend, and outputs frame statistics via glog.
- GUI Client (Qt): A cross-platform UI built on Qt6 (or Qt5) that dynamically renders decoded frames (via QOpenGLWidget or QVideoSink). The decoder must run entirely on a separate background thread (e.g., via standard worker thread or QThread) to avoid freezing the UI event loop.

8. Build System (CMake & Ninja):
- Provide a clean, robust CMakeLists.txt script that fully supports the Ninja build generator on both Windows and Linux.
- Use modern CMake targets (`target_include_directories`, `target_link_libraries`).
- Ensure proper configuration for locating and linking all external dependencies: FFmpeg, GStreamer, glog, gtest, and Qt.

Deliver the complete software architecture directory layout, the C4 documents containing both the ASCII diagrams and Mermaid/PlantUML syntax, the primary source files with Doxygen comments, and the modern cross-platform CMake configuration.

