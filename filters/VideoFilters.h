/**
 * @file VideoFilters.h
 * @brief Header declaring OpenCV-based post-processing video filters.
 */

#pragma once

#include "DecoderTypes.h"
#include <string>

// Visibility macros for shared library export/import
#if defined(_MSC_VER)
#ifdef VIDEOFILTERS_EXPORTS
#define VIDEOFILTERS_API __declspec(dllexport)
#else
#define VIDEOFILTERS_API __declspec(dllimport)
#endif
#else
#ifdef VIDEOFILTERS_EXPORTS
#define VIDEOFILTERS_API __attribute__((visibility("default")))
#else
#define VIDEOFILTERS_API
#endif
#endif

namespace videodecoder {

/**
 * @class BrightnessContrastFilter
 * @brief Adjusts brightness and contrast of a video frame in-place.
 */
class VIDEOFILTERS_API BrightnessContrastFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param alpha Contrast multiplier (1.0 is normal).
     * @param beta Brightness offset (0 is normal).
     */
    BrightnessContrastFilter(double alpha = 1.0, int beta = 0);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

private:
    double m_alpha;
    int m_beta;
};

/**
 * @class GaussianBlurFilter
 * @brief Applies Gaussian blur to a video frame in-place.
 */
class VIDEOFILTERS_API GaussianBlurFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param kernelSize Size of Gaussian blur kernel (must be positive and odd).
     */
    GaussianBlurFilter(int kernelSize = 5);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

private:
    int m_kernelSize;
};

/**
 * @class EdgeDetectionFilter
 * @brief Performs Canny edge detection overlay on a video frame.
 */
class VIDEOFILTERS_API EdgeDetectionFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param threshold1 First threshold for the hysteresis procedure.
     * @param threshold2 Second threshold for the hysteresis procedure.
     */
    EdgeDetectionFilter(double threshold1 = 50.0, double threshold2 = 150.0);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

private:
    double m_threshold1;
    double m_threshold2;
};

/**
 * @class TextOverlayFilter
 * @brief Overlays a customizable string overlay onto the corner of video frames.
 */
class VIDEOFILTERS_API TextOverlayFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param text Text message to display.
     * @param x X coordinate offset from top-left.
     * @param y Y coordinate offset from top-left.
     * @param scale Text font scale factor.
     */
    TextOverlayFilter(const std::string& text, int x = 10, int y = 30, double scale = 1.0);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

private:
    std::string m_text;
    int m_x;
    int m_y;
    double m_scale;
};

/**
 * @class MirrorFilter
 * @brief Flips the video frame horizontally or vertically in-place.
 */
class VIDEOFILTERS_API MirrorFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param horizontal If true, flips horizontally. If false, flips vertically.
     */
    MirrorFilter(bool horizontal = true);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

private:
    bool m_horizontal;
};

/**
 * @class InvertColorsFilter
 * @brief Inverts the color channels of a video frame in-place (negative effect).
 */
class VIDEOFILTERS_API InvertColorsFilter : public IFrameProcessor {
public:
    InvertColorsFilter() = default;

    void process(uint8_t* data, int width, int height, PixelFormat format) override;
};

/**
 * @class GrayscaleFilter
 * @brief Converts a color video frame to grayscale in-place, preserving its 3-channel structure.
 */
class VIDEOFILTERS_API GrayscaleFilter : public IFrameProcessor {
public:
    GrayscaleFilter() = default;

    void process(uint8_t* data, int width, int height, PixelFormat format) override;
};

/**
 * @class SepiaFilter
 * @brief Applies a vintage sepia color transformation to a video frame in-place.
 */
class VIDEOFILTERS_API SepiaFilter : public IFrameProcessor {
public:
    SepiaFilter() = default;

    void process(uint8_t* data, int width, int height, PixelFormat format) override;
};

/**
 * @class SharpenFilter
 * @brief Applies a sharpening 2D kernel convolution to a video frame in-place.
 */
class VIDEOFILTERS_API SharpenFilter : public IFrameProcessor {
public:
    SharpenFilter() = default;

    void process(uint8_t* data, int width, int height, PixelFormat format) override;
};

/**
 * @class ColorTintFilter
 * @brief Multiplies color channels by specified scale factors in-place to achieve color tinting.
 */
class VIDEOFILTERS_API ColorTintFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param rScale Red channel scaling factor.
     * @param gScale Green channel scaling factor.
     * @param bScale Blue channel scaling factor.
     */
    ColorTintFilter(double rScale = 1.0, double gScale = 1.0, double bScale = 1.0);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

private:
    double m_rScale;
    double m_gScale;
    double m_bScale;
};

} // namespace videodecoder
