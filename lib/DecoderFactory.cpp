/**
 * @file DecoderFactory.cpp
 * @brief Implementation of the DecoderFactory.
 */

#include "DecoderFactory.h"
#include "FFmpegDecoder.h"
#include "GStreamerDecoder.h"
#include <glog/logging.h>

namespace videodecoder {

std::unique_ptr<IVideoDecoder> DecoderFactory::create(BackendType backend)
{
    switch (backend) {
    case BackendType::FFMPEG:
        VLOG(1) << "DecoderFactory: Creating FFmpegDecoder instance";
        return std::make_unique<FFmpegDecoder>();
    case BackendType::GSTREAMER:
        VLOG(1) << "DecoderFactory: Creating GStreamerDecoder instance";
        return std::make_unique<GStreamerDecoder>();
    default:
        LOG(ERROR) << "DecoderFactory: Invalid backend type requested";
        return nullptr;
    }
}

} // namespace videodecoder
