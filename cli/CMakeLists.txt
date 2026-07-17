# --- cli/CMakeLists.txt ---
cmake_minimum_required(VERSION 3.16)

add_executable(VideoDecoderCLI
    main.cpp
)

target_link_libraries(VideoDecoderCLI PRIVATE
    VideoDecoder
)

if(NOT IS_WINDOWS)
    target_link_libraries(VideoDecoderCLI PRIVATE PkgConfig::GLOG)
else()
    target_link_libraries(VideoDecoderCLI PRIVATE glog::glog)
endif()
