# --- cli/CMakeLists.txt ---
cmake_minimum_required(VERSION 3.16)

# Resolve SDL2 dependency using pkg-config locally for the CLI application
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_executable(VideoDecoderCLI
    main.cpp
)

target_link_libraries(VideoDecoderCLI PRIVATE
    VideoDecoder
    PkgConfig::SDL2
)

if(NOT IS_WINDOWS)
    target_link_libraries(VideoDecoderCLI PRIVATE PkgConfig::GLOG)
else()
    target_link_libraries(VideoDecoderCLI PRIVATE glog::glog)
endif()
