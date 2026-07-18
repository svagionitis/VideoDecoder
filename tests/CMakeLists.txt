# --- tests/CMakeLists.txt ---
cmake_minimum_required(VERSION 3.16)

# Find GTest dependency or download via FetchContent
find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
    find_package(GTest QUIET)
endif()

if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    set(GTEST_TARGET gtest)
    set(GTEST_MAIN_TARGET gtest_main)
else()
    set(GTEST_TARGET GTest::gtest)
    set(GTEST_MAIN_TARGET GTest::gtest_main)
endif()

add_executable(VideoDecoderTests
    DecoderTests.cpp
)

# Set compile-defined absolute path to test video file in local tests/ folder
target_compile_definitions(VideoDecoderTests PRIVATE TEST_INPUT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/test_input.mp4")

target_link_libraries(VideoDecoderTests PRIVATE
    VideoDecoder
    VideoFilters
    ${GTEST_TARGET}
    ${GTEST_MAIN_TARGET}
    ${GLOG_TARGET}
)

# Discover tests automatically
include(GoogleTest)
gtest_discover_tests(VideoDecoderTests)
