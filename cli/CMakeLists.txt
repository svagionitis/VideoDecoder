# --- cli/CMakeLists.txt ---
cmake_minimum_required(VERSION 3.16)

# Resolve SDL2 dependency for the CLI application
if(IS_WINDOWS)
    find_package(SDL2 CONFIG REQUIRED)
    if(TARGET SDL2::SDL2)
        set(SDL2_TARGET SDL2::SDL2)
    else()
        set(SDL2_TARGET SDL2::SDL2-static)
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    set(SDL2_TARGET PkgConfig::SDL2)
endif()

add_executable(VideoDecoderCLI
    main.cpp
)

target_link_libraries(VideoDecoderCLI PRIVATE
    VideoDecoder
    ${SDL2_TARGET}
)

if(NOT IS_WINDOWS)
    target_link_libraries(VideoDecoderCLI PRIVATE PkgConfig::GLOG)
else()
    target_link_libraries(VideoDecoderCLI PRIVATE glog::glog)
endif()

