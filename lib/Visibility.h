/**
 * @file Visibility.h
 * @brief Platform-specific macros for controlling shared library symbol visibility.
 */

#pragma once

#if defined(_MSC_VER)
#ifdef VIDEODECODER_EXPORTS
#define VIDEODECODER_API __declspec(dllexport)
#else
#define VIDEODECODER_API __declspec(dllimport)
#endif
#else
#ifdef VIDEODECODER_EXPORTS
#define VIDEODECODER_API __attribute__((visibility("default")))
#else
#define VIDEODECODER_API
#endif
#endif
