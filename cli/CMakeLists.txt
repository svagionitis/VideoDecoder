# Resolve SDL2 dependency for the CLI application
set(SDL2_FOUND OFF)
if(WIN32)
    find_package(SDL2 CONFIG QUIET)
    if(SDL2_FOUND OR TARGET SDL2::SDL2 OR TARGET SDL2::SDL2-static)
        set(SDL2_FOUND ON)
        if(TARGET SDL2::SDL2)
            set(SDL2_TARGET SDL2::SDL2)
        else()
            set(SDL2_TARGET SDL2::SDL2-static)
        endif()
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
    if(SDL2_FOUND)
        set(SDL2_TARGET PkgConfig::SDL2)
    endif()
endif()

if(NOT SDL2_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-2.30.3
    )
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL2)
    set(SDL2_TARGET SDL2::SDL2)
endif()

add_executable(VideoDecoderCLI
    main.cpp
)

target_link_libraries(VideoDecoderCLI PRIVATE
    VideoDecoder
    ${SDL2_TARGET}
    ${GLOG_TARGET}
)

apply_compiler_flags(VideoDecoderCLI)
