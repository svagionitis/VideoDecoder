# --- lib/CMakeLists.txt ---
cmake_minimum_required(VERSION 3.16)

add_library(VideoDecoder SHARED
    IVideoDecoder.h
    DecoderFactory.h
    DecoderTypes.h
    Visibility.h
    FFmpegDecoder.h
    FFmpegDecoder.cpp
    GStreamerDecoder.h
    GStreamerDecoder.cpp
    DecoderFactory.cpp
)

# Export macro for Windows/Linux visibility control
target_compile_definitions(VideoDecoder PRIVATE VIDEODECODER_EXPORTS)

# Public include directory (exposes headers in lib/ directly)
target_include_directories(VideoDecoder PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Link dependencies resolved in root CMakeLists
if(NOT IS_WINDOWS)
    target_link_libraries(VideoDecoder PRIVATE
        PkgConfig::AVCODEC
        PkgConfig::AVFORMAT
        PkgConfig::AVUTIL
        PkgConfig::SWSCALE
        PkgConfig::GST
        PkgConfig::GST_APP
        PkgConfig::GST_VIDEO
        ${GLOG_TARGET}
    )
else()
    if(FFMPEG_VIA_FIND_PACKAGE)
        target_link_libraries(VideoDecoder PRIVATE
            FFmpeg::avcodec
            FFmpeg::avformat
            FFmpeg::avutil
            FFmpeg::swscale
        )
    else()
        target_include_directories(VideoDecoder SYSTEM PRIVATE
            ${FFMPEG_INCLUDE_DIR}
        )
        target_link_libraries(VideoDecoder PRIVATE
            ${AVCODEC_LIBRARY}
            ${AVFORMAT_LIBRARY}
            ${AVUTIL_LIBRARY}
            ${SWSCALE_LIBRARY}
        )
    endif()

    target_include_directories(VideoDecoder SYSTEM PRIVATE
        ${GST_INCLUDE_DIR}
        ${GLIB_INCLUDE_DIR}
        ${GLIBCONFIG_INCLUDE_DIR}
    )
    target_link_libraries(VideoDecoder PRIVATE
        ${GST_LIBRARY}
        ${GST_APP_LIBRARY}
        ${GST_VIDEO_LIBRARY}
        ${GLIB_LIBRARY}
        ${GOBJECT_LIBRARY}
        ${GLOG_TARGET}
    )
endif()

