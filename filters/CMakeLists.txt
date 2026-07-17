# --- filters/CMakeLists.txt ---
cmake_minimum_required(VERSION 3.16)

# Locate OpenCV dependency
find_package(OpenCV REQUIRED COMPONENTS core imgproc)

add_library(VideoFilters SHARED
    VideoFilters.cpp
    VideoFilters.h
)

# Export macro definitions for Windows/Linux compilation visibility
target_compile_definitions(VideoFilters PRIVATE -DVIDEOFILTERS_EXPORTS)

# Link directories and include definitions
target_include_directories(VideoFilters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Link OpenCV and the core video decoder library
target_link_libraries(VideoFilters PUBLIC
    VideoDecoder
    ${OpenCV_LIBS}
)

# Enforce compiler warning and hardening options if defined in root project
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/CompilerFlags.cmake")
    target_compile_options(VideoFilters PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
