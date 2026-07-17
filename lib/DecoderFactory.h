/**
 * @file DecoderFactory.h
 * @brief Factory class to safely construct video decoder instances.
 */

#pragma once

#include "DecoderTypes.h"
#include "IVideoDecoder.h"
#include "Visibility.h"
#include <memory>

namespace videodecoder {

/**
 * @class DecoderFactory
 * @brief Factory for instantiating concrete implementations of IVideoDecoder.
 */
class VIDEODECODER_API DecoderFactory {
public:
    /**
     * @brief Creates a video decoder instance using the requested backend.
     *
     * @param backend The backend type (FFMPEG or GSTREAMER).
     * @return A std::unique_ptr pointing to the newly created IVideoDecoder instance.
     * @note The caller takes ownership of the returned pointer.
     */
    static std::unique_ptr<IVideoDecoder> create(BackendType backend);
};

} // namespace videodecoder
