cmake_minimum_required(VERSION 3.16)

# Skip Qt Creator's buggy automatic vcpkg setup and use our own logic below
set(QT_CREATOR_SKIP_VCPKG_SETUP ON)

# Self-healing: if CMAKE_TOOLCHAIN_FILE was incorrectly set to a directory (common Qt Creator bug), correct it to the actual toolchain file path
if(CMAKE_TOOLCHAIN_FILE AND IS_DIRECTORY "${CMAKE_TOOLCHAIN_FILE}")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_TOOLCHAIN_FILE}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain file" FORCE)
endif()

if(WIN32 AND NOT CMAKE_TOOLCHAIN_FILE)
    # Check known paths for vcpkg.cmake
    set(VCPKG_CANDIDATE_PATHS
        "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
        "$ENV{USERPROFILE}/workspace/vcpkg/scripts/buildsystems/vcpkg.cmake"
        "D:/vcpkg/scripts/buildsystems/vcpkg.cmake"
        "$ENV{USERPROFILE}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        "C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake"
    )
    foreach(candidate IN LISTS VCPKG_CANDIDATE_PATHS)
        if(EXISTS "${candidate}")
            set(CMAKE_TOOLCHAIN_FILE "${candidate}" CACHE FILEPATH "vcpkg toolchain file")
            message(STATUS "Auto-detected vcpkg toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
            break()
        endif()
    endforeach()
endif()

project(VideoDecoder LANGUAGES C CXX)

# Set standard build profiles/configurations
set(ALLOWED_BUILD_TYPES Debug Release RelWithDebInfo MinSizeRel)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(DEFAULT_BUILD_TYPE "Release")
    message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "${ALLOWED_BUILD_TYPES}")
endif()

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "${ALLOWED_BUILD_TYPES}" CACHE STRING "Supported build configuration types" FORCE)
endif()

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
    pkg_check_modules(GLOG IMPORTED_TARGET libglog)
else()
    # On Windows, try to find libraries via find_package first (vcpkg), then fallback.
    find_package(FFMPEG COMPONENTS avcodec avformat avutil swscale QUIET)
    find_package(glog CONFIG QUIET)

    if(FFmpeg_FOUND OR FFMPEG_FOUND)
        set(FFMPEG_VIA_FIND_PACKAGE ON)
    else()
        set(FFMPEG_VIA_FIND_PACKAGE OFF)
        # FFmpeg manual fallback
        find_path(FFMPEG_INCLUDE_DIR NAMES libavcodec/avcodec.h PATHS "${FFMPEG_ROOT}/include" "$ENV{FFMPEG_ROOT}/include")
        find_library(AVCODEC_LIBRARY NAMES avcodec PATHS "${FFMPEG_ROOT}/lib" "$ENV{FFMPEG_ROOT}/lib")
        find_library(AVFORMAT_LIBRARY NAMES avformat PATHS "${FFMPEG_ROOT}/lib" "$ENV{FFMPEG_ROOT}/lib")
        find_library(AVUTIL_LIBRARY NAMES avutil PATHS "${FFMPEG_ROOT}/lib" "$ENV{FFMPEG_ROOT}/lib")
        find_library(SWSCALE_LIBRARY NAMES swscale PATHS "${FFMPEG_ROOT}/lib" "$ENV{FFMPEG_ROOT}/lib")
    endif()

    # GStreamer and GLib fallback
    find_path(GST_INCLUDE_DIR NAMES gst/gst.h PATHS "${GSTREAMER_ROOT}/include/gstreamer-1.0" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/include/gstreamer-1.0" PATH_SUFFIXES "gstreamer-1.0")
    find_path(GLIB_INCLUDE_DIR NAMES glib.h PATHS "${GSTREAMER_ROOT}/include/glib-2.0" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/include/glib-2.0" PATH_SUFFIXES "glib-2.0")
    find_path(GLIBCONFIG_INCLUDE_DIR NAMES glibconfig.h PATHS "${GSTREAMER_ROOT}/lib/glib-2.0/include" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib/glib-2.0/include" PATH_SUFFIXES "glib-2.0/include")

    find_library(GST_LIBRARY NAMES gstreamer-1.0 PATHS "${GSTREAMER_ROOT}/lib" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib")
    find_library(GST_APP_LIBRARY NAMES gstapp-1.0 PATHS "${GSTREAMER_ROOT}/lib" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib")
    find_library(GST_VIDEO_LIBRARY NAMES gstvideo-1.0 PATHS "${GSTREAMER_ROOT}/lib" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib")
    find_library(GLIB_LIBRARY NAMES glib-2.0 PATHS "${GSTREAMER_ROOT}/lib" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib")
    find_library(GOBJECT_LIBRARY NAMES gobject-2.0 PATHS "${GSTREAMER_ROOT}/lib" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib")
endif()

# Check if glog is found, otherwise use FetchContent fallback
set(GLOG_FOUND OFF)
if(NOT IS_WINDOWS)
    if(GLOG_FOUND)
        set(GLOG_TARGET PkgConfig::GLOG)
    endif()
else()
    if(glog_FOUND)
        set(GLOG_TARGET glog::glog)
        set(GLOG_FOUND ON)
    endif()
endif()

if(NOT GLOG_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        glog
        GIT_REPOSITORY https://github.com/google/glog.git
        GIT_TAG        v0.6.0
    )
    set(WITH_GFLAGS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glog)
    set(GLOG_TARGET glog::glog)
endif()



# Add modular components via subdirectories
add_subdirectory(lib)
add_subdirectory(filters)

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
        "filters/*.cpp" "filters/*.h"
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
