/**
 * @file VideoFilters.cpp
 * @brief Implementations of the OpenCV-based video filters.
 */

#include "VideoFilters.h"
#include <opencv2/opencv.hpp>

namespace videodecoder {

// --- BrightnessContrastFilter ---
BrightnessContrastFilter::BrightnessContrastFilter(double alpha, int beta)
    : m_alpha(alpha)
    , m_beta(beta)
{
}

void BrightnessContrastFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    mat.convertTo(mat, -1, m_alpha, m_beta);
}

// --- GaussianBlurFilter ---
GaussianBlurFilter::GaussianBlurFilter(int kernelSize)
    : m_kernelSize(kernelSize)
{
}

void GaussianBlurFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    int ksize = m_kernelSize;
    if (ksize % 2 == 0) {
        ksize += 1;
    }
    if (ksize <= 0) {
        ksize = 1;
    }
    cv::GaussianBlur(mat, mat, cv::Size(ksize, ksize), 0);
}

// --- EdgeDetectionFilter ---
EdgeDetectionFilter::EdgeDetectionFilter(double threshold1, double threshold2)
    : m_threshold1(threshold1)
    , m_threshold2(threshold2)
{
}

void EdgeDetectionFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray, edges;

    if (format == PixelFormat::BGR24) {
        cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);
    } else {
        cv::cvtColor(mat, gray, cv::COLOR_RGB2GRAY);
    }

    cv::Canny(gray, edges, m_threshold1, m_threshold2);

    if (format == PixelFormat::BGR24) {
        cv::cvtColor(edges, mat, cv::COLOR_GRAY2BGR);
    } else {
        cv::cvtColor(edges, mat, cv::COLOR_GRAY2RGB);
    }
}

// --- TextOverlayFilter ---
TextOverlayFilter::TextOverlayFilter(const std::string& text, int x, int y, double scale)
    : m_text(text)
    , m_x(x)
    , m_y(y)
    , m_scale(scale)
{
}

void TextOverlayFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Scalar color = (format == PixelFormat::BGR24) ? cv::Scalar(0, 255, 255) : cv::Scalar(255, 255, 0); // Yellow

    cv::putText(mat, m_text, cv::Point(m_x, m_y), cv::FONT_HERSHEY_SIMPLEX, m_scale, color, 2);
}

// --- MirrorFilter ---
MirrorFilter::MirrorFilter(bool horizontal)
    : m_horizontal(horizontal)
{
}

void MirrorFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::flip(mat, mat, m_horizontal ? 1 : 0);
}

// --- InvertColorsFilter ---
void InvertColorsFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::bitwise_not(mat, mat);
}

// --- GrayscaleFilter ---
void GrayscaleFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray;
    if (format == PixelFormat::BGR24) {
        cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(gray, mat, cv::COLOR_GRAY2BGR);
    } else {
        cv::cvtColor(mat, gray, cv::COLOR_RGB2GRAY);
        cv::cvtColor(gray, mat, cv::COLOR_GRAY2RGB);
    }
}

// --- SepiaFilter ---
void SepiaFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);

    cv::Mat sepiaKernel;
    if (format == PixelFormat::BGR24) {
        // OpenCV BGR order: Blue (0), Green (1), Red (2)
        // Red = 0.393 * R + 0.769 * G + 0.189 * B
        // Green = 0.349 * R + 0.686 * G + 0.168 * B
        // Blue = 0.272 * R + 0.534 * G + 0.131 * B
        sepiaKernel = (cv::Mat_<float>(3, 3) << 0.131, 0.534, 0.272, // B
            0.168, 0.686, 0.349, // G
            0.189, 0.769, 0.393 // R
        );
    } else {
        // RGB order: Red (0), Green (1), Blue (2)
        sepiaKernel = (cv::Mat_<float>(3, 3) << 0.393, 0.769, 0.189, // R
            0.349, 0.686, 0.168, // G
            0.272, 0.534, 0.131 // B
        );
    }
    cv::transform(mat, mat, sepiaKernel);
}

// --- SharpenFilter ---
void SharpenFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat kernel = (cv::Mat_<float>(3, 3) << 0, -1, 0, -1, 5, -1, 0, -1, 0);
    cv::filter2D(mat, mat, mat.depth(), kernel);
}

// --- ColorTintFilter ---
ColorTintFilter::ColorTintFilter(double rScale, double gScale, double bScale)
    : m_rScale(rScale)
    , m_gScale(gScale)
    , m_bScale(bScale)
{
}

void ColorTintFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    std::vector<cv::Mat> channels;
    cv::split(mat, channels);

    if (format == PixelFormat::BGR24) {
        channels[0] *= m_bScale;
        channels[1] *= m_gScale;
        channels[2] *= m_rScale;
    } else {
        channels[0] *= m_rScale;
        channels[1] *= m_gScale;
        channels[2] *= m_bScale;
    }

    cv::merge(channels, mat);
}

// --- ClaheFilter ---
ClaheFilter::ClaheFilter(double clipLimit, int tileGridSize)
    : m_clipLimit(clipLimit)
    , m_tileGridSize(tileGridSize)
{
}

void ClaheFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat lab;
    cv::cvtColor(mat, lab, (format == PixelFormat::BGR24) ? cv::COLOR_BGR2Lab : cv::COLOR_RGB2Lab);
    std::vector<cv::Mat> channels;
    cv::split(lab, channels);

    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(m_clipLimit, cv::Size(m_tileGridSize, m_tileGridSize));
    clahe->apply(channels[0], channels[0]);

    cv::merge(channels, lab);
    cv::cvtColor(lab, mat, (format == PixelFormat::BGR24) ? cv::COLOR_Lab2BGR : cv::COLOR_Lab2RGB);
}

// --- BilateralFilter ---
BilateralFilter::BilateralFilter(int d, double sigmaColor, double sigmaSpace)
    : m_d(d)
    , m_sigmaColor(sigmaColor)
    , m_sigmaSpace(sigmaSpace)
{
}

void BilateralFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat temp;
    cv::bilateralFilter(mat, temp, m_d, m_sigmaColor, m_sigmaSpace);
    temp.copyTo(mat);
}

// --- GammaCorrectionFilter ---
GammaCorrectionFilter::GammaCorrectionFilter(double gamma)
    : m_gamma(gamma)
{
}

void GammaCorrectionFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0 || m_gamma <= 0.0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat lookUpTable(1, 256, CV_8U);
    uint8_t* p = lookUpTable.ptr();
    for (int i = 0; i < 256; ++i) {
        p[i] = cv::saturate_cast<uint8_t>(pow(i / 255.0, m_gamma) * 255.0);
    }
    cv::LUT(mat, lookUpTable, mat);
}

// --- VignetteFilter ---
void VignetteFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat mask = cv::Mat::zeros(height, width, CV_32F);
    cv::Point center(width / 2, height / 2);
    double max_dist = cv::norm(center);
    if (max_dist <= 0.0) {
        return;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double dist = cv::norm(cv::Point(x, y) - center);
            mask.at<float>(y, x) = static_cast<float>(1.0 - (dist / max_dist));
        }
    }

    std::vector<cv::Mat> channels;
    cv::split(mat, channels);
    for (auto& channel : channels) {
        cv::Mat fChan;
        channel.convertTo(fChan, CV_32F);
        cv::multiply(fChan, mask, fChan);
        fChan.convertTo(channel, CV_8U);
    }
    cv::merge(channels, mat);
}

// --- MosaicFilter ---
MosaicFilter::MosaicFilter(int blockSize)
    : m_blockSize(blockSize)
{
}

void MosaicFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0 || m_blockSize <= 1) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat temp;
    int w = std::max(1, width / m_blockSize);
    int h = std::max(1, height / m_blockSize);
    cv::resize(mat, temp, cv::Size(w, h), 0, 0, cv::INTER_NEAREST);
    cv::resize(temp, mat, mat.size(), 0, 0, cv::INTER_NEAREST);
}

// --- ThresholdFilter ---
ThresholdFilter::ThresholdFilter(double thresholdValue)
    : m_thresholdValue(thresholdValue)
{
}

void ThresholdFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray;
    if (format == PixelFormat::BGR24) {
        cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);
    } else {
        cv::cvtColor(mat, gray, cv::COLOR_RGB2GRAY);
    }
    cv::threshold(gray, gray, m_thresholdValue, 255, cv::THRESH_BINARY);
    if (format == PixelFormat::BGR24) {
        cv::cvtColor(gray, mat, cv::COLOR_GRAY2BGR);
    } else {
        cv::cvtColor(gray, mat, cv::COLOR_GRAY2RGB);
    }
}

} // namespace videodecoder
