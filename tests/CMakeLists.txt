# --- tests/CMakeLists.txt ---
cmake_minimum_required(VERSION 3.16)

# Find GTest dependency locally within this subdirectory configuration
find_package(GTest REQUIRED)

add_executable(VideoDecoderTests
    DecoderTests.cpp
)

# Set compile-defined absolute path to test video file in local tests/ folder
target_compile_definitions(VideoDecoderTests PRIVATE TEST_INPUT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/test_input.mp4")

target_link_libraries(VideoDecoderTests PRIVATE
    VideoDecoder
    GTest::gtest
    GTest::gtest_main
)

if(NOT IS_WINDOWS)
    target_link_libraries(VideoDecoderTests PRIVATE PkgConfig::GLOG)
else()
    target_link_libraries(VideoDecoderTests PRIVATE glog::glog)
endif()

# Discover tests automatically
include(GoogleTest)
gtest_discover_tests(VideoDecoderTests)
