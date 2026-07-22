cmake_minimum_required(VERSION 3.16)

project(VideoDecoder VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build Options
option(BUILD_GUI "Build the Qt-based Desktop GUI application client" OFF)
option(BUILD_CLI "Build the Command Line Interface (CLI) client application" ON)
option(BUILD_TESTS "Build the Google Test (gtest) unit test suites" ON)

# Compiler Flags & Modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerFlags)

# Find Common Dependencies
if(NOT WIN32)
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
else()
    # On Windows, try to find libraries via find_package first (vcpkg), then fallback.
    find_package(FFMPEG COMPONENTS avcodec avformat avutil swscale QUIET)

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

# Google Logging (glog) Dependency Resolution
find_package(glog CONFIG QUIET)
if(NOT glog_FOUND)
    find_package(glog QUIET)
endif()

if(TARGET glog::glog)
    set(GLOG_TARGET glog::glog)
elseif(NOT WIN32)
    find_package(PkgConfig QUIET)
    pkg_check_modules(GLOG IMPORTED_TARGET libglog)
    if(TARGET PkgConfig::GLOG)
        set(GLOG_TARGET PkgConfig::GLOG)
    endif()
endif()

# Fallback to FetchContent if glog is not installed on the host system
if(NOT GLOG_TARGET)
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

# Core Video Decoder Library
add_subdirectory(lib)

# Video Processing Filters Library
add_subdirectory(filters)

# CLI Client Executable (Built by default)
if(BUILD_CLI)
    add_subdirectory(cli)
endif()

# Qt Desktop GUI Client (Conditional)
if(BUILD_GUI)
    add_subdirectory(gui)
endif()

# Unit Testing (Conditional)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Clang-Format Code Formatting Target
find_program(CLANG_FORMAT_BIN NAMES clang-format clang-format-15 clang-format-14 clang-format-12)
if(CLANG_FORMAT_BIN)
    file(GLOB_RECURSE ALL_FORMAT_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/filters/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/filters/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/gui/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_BIN} -i -style=file ${ALL_FORMAT_FILES}
        COMMENT "Applying clang-format (WebKit style) to all C++ files..."
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_BIN} --dry-run --Werror -style=file ${ALL_FORMAT_FILES}
        COMMENT "Checking clang-format (WebKit style) compliance for all C++ files..."
    )
else()
    message(STATUS "clang-format executable not found. 'format' target disabled.")
endif()

# Automatically install Git pre-commit hook on CMake configuration
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/pre-commit")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/scripts/pre-commit"
        DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/.git/hooks"
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
    message(STATUS "Git pre-commit hook successfully installed to .git/hooks/pre-commit")
endif()
