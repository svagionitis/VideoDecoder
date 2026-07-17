cmake_minimum_required(VERSION 3.16)
project(VideoDecoder LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Helper flag for Windows vs Linux/macOS
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(IS_WINDOWS ON)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    set(IS_WINDOWS OFF)
endif()

# Include custom compiler warning options, hardening, and sanitizer configurations
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerFlags)

# Build options (defaulting to building the CLI and Tests, but NOT the GUI)
option(BUILD_CLI "Build the Command Line Interface (CLI) client application" ON)
option(BUILD_GUI "Build the Qt-based Desktop GUI application client" OFF)
option(BUILD_TESTS "Build the Google Test (gtest) unit test suites" ON)

# We need package config on non-Windows platforms
if(NOT IS_WINDOWS)
    find_package(PkgConfig REQUIRED)

    # FFmpeg components
    pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
    pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
    pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
    pkg_check_modules(SWSCALE REQUIRED IMPORTED_TARGET libswscale)

    # GStreamer components
    pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0)
    pkg_check_modules(GST_APP REQUIRED IMPORTED_TARGET gstreamer-app-1.0)
    pkg_check_modules(GST_VIDEO REQUIRED IMPORTED_TARGET gstreamer-video-1.0)

    # Google Logging (glog)
    pkg_check_modules(GLOG REQUIRED IMPORTED_TARGET libglog)
else()
    # On Windows, we try to find libraries via custom path variables or CMake finders.

    # FFmpeg fallback
    find_path(FFMPEG_INCLUDE_DIR NAMES libavcodec/avcodec.h PATHS "$ENV{FFMPEG_ROOT}/include")
    find_library(AVCODEC_LIBRARY NAMES avcodec PATHS "$ENV{FFMPEG_ROOT}/lib")
    find_library(AVFORMAT_LIBRARY NAMES avformat PATHS "$ENV{FFMPEG_ROOT}/lib")
    find_library(AVUTIL_LIBRARY NAMES avutil PATHS "$ENV{FFMPEG_ROOT}/lib")
    find_library(SWSCALE_LIBRARY NAMES swscale PATHS "$ENV{FFMPEG_ROOT}/lib")

    # GStreamer fallback
    find_path(GST_INCLUDE_DIR NAMES gst/gst.h PATHS "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/include/gstreamer-1.0")

    # glog fallback
    find_package(glog CONFIG REQUIRED)
endif()


# Add modular components via subdirectories
add_subdirectory(lib)

if(BUILD_CLI)
    add_subdirectory(cli)
endif()

if(BUILD_GUI)
    add_subdirectory(gui)
endif()

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Code formatting target using clang-format
find_program(CLANG_FORMAT_BIN NAMES clang-format)
if(CLANG_FORMAT_BIN)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        "lib/*.cpp" "lib/*.h"
        "cli/*.cpp"
        "gui/*.cpp" "gui/*.h"
        "tests/*.cpp"
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_BIN} -i ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Formatting codebase with WebKit style conventions..."
    )
endif()

# Automatically install Git pre-commit hook on CMake configuration
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    message(STATUS "Git repository detected. Installing pre-commit hook...")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/.githooks/pre-commit"
        "${CMAKE_CURRENT_SOURCE_DIR}/.git/hooks/pre-commit"
        COPYONLY
    )
    if(NOT IS_WINDOWS)
        execute_process(COMMAND chmod +x "${CMAKE_CURRENT_SOURCE_DIR}/.git/hooks/pre-commit")
    endif()
endif()
