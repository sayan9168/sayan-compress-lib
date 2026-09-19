/**
 * @file config.hpp
 * @brief Production configuration and compile-time settings
 * 
 * Centralized configuration for the compression library.
 * Controls features, limits, and compilation options.
 * 
 * @author Sayan
 * @version 3.0.0 (Production Grade)
 * @license MIT
 */

#pragma once

#include <cstdint>

// ============================================================================
// VERSION INFORMATION
// ============================================================================

#define COMPRESSION_LIB_VERSION_MAJOR 3
#define COMPRESSION_LIB_VERSION_MINOR 0
#define COMPRESSION_LIB_VERSION_PATCH 0
#define COMPRESSION_LIB_VERSION_STRING "3.0.0"

// ============================================================================
// PLATFORM DETECTION
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define COMPRESSION_PLATFORM_WINDOWS 1
    #define COMPRESSION_PLATFORM_UNIX 0
    #define COMPRESSION_PLATFORM_MACOS 0
    #define COMPRESSION_PLATFORM_LINUX 0
#elif defined(__APPLE__) && defined(__MACH__)
    #define COMPRESSION_PLATFORM_WINDOWS 0
    #define COMPRESSION_PLATFORM_UNIX 1
    #define COMPRESSION_PLATFORM_MACOS 1
    #define COMPRESSION_PLATFORM_LINUX 0
#elif defined(__linux__)
    #define COMPRESSION_PLATFORM_WINDOWS 0
    #define COMPRESSION_PLATFORM_UNIX 1
    #define COMPRESSION_PLATFORM_MACOS 0
    #define COMPRESSION_PLATFORM_LINUX 1
#else
    #define COMPRESSION_PLATFORM_WINDOWS 0
    #define COMPRESSION_PLATFORM_UNIX 1
    #define COMPRESSION_PLATFORM_MACOS 0
    #define COMPRESSION_PLATFORM_LINUX 0
#endif

// ============================================================================
// COMPILER DETECTION
// ============================================================================

#if defined(__GNUC__) && !defined(__clang__)
    #define COMPRESSION_COMPILER_GCC 1
    #define COMPRESSION_COMPILER_CLANG 0
    #define COMPRESSION_COMPILER_MSVC 0
#elif defined(__clang__)
    #define COMPRESSION_COMPILER_GCC 0
    #define COMPRESSION_COMPILER_CLANG 1
    #define COMPRESSION_COMPILER_MSVC 0
#elif defined(_MSC_VER)
    #define COMPRESSION_COMPILER_GCC 0
    #define COMPRESSION_COMPILER_CLANG 0
    #define COMPRESSION_COMPILER_MSVC 1
#else
    #define COMPRESSION_COMPILER_GCC 0
    #define COMPRESSION_COMPILER_CLANG 0
    #define COMPRESSION_COMPILER_MSVC 0
#endif

// ============================================================================
// FEATURE FLAGS
// ============================================================================

#ifndef COMPRESSION_ENABLE_MULTITHREADING
    #define COMPRESSION_ENABLE_MULTITHREADING 1
#endif

#ifndef COMPRESSION_ENABLE_STREAMING
    #define COMPRESSION_ENABLE_STREAMING 1
#endif

#ifndef COMPRESSION_ENABLE_ADAPTIVE_HUFFMAN
    #define COMPRESSION_ENABLE_ADAPTIVE_HUFFMAN 1
#endif

#ifndef COMPRESSION_ENABLE_CRC_VERIFICATION
    #define COMPRESSION_ENABLE_CRC_VERIFICATION 1
#endif

#ifndef COMPRESSION_ENABLE_LOGGING
    #define COMPRESSION_ENABLE_LOGGING 1
#endif

#ifndef COMPRESSION_ENABLE_PROFILING
    #define COMPRESSION_ENABLE_PROFILING 0
#endif

#ifndef COMPRESSION_ENABLE_DEBUG_ASSERTIONS
    #ifdef NDEBUG
        #define COMPRESSION_ENABLE_DEBUG_ASSERTIONS 0
    #else
        #define COMPRESSION_ENABLE_DEBUG_ASSERTIONS 1
    #endif
#endif

// ============================================================================
// PERFORMANCE TUNING CONSTANTS
// ============================================================================

#ifndef COMPRESSION_DEFAULT_WINDOW_SIZE
    #define COMPRESSION_DEFAULT_WINDOW_SIZE 16384
#endif

#ifndef COMPRESSION_DEFAULT_LOOKAHEAD_SIZE
    #define COMPRESSION_DEFAULT_LOOKAHEAD_SIZE 256
#endif

#ifndef COMPRESSION_MIN_MATCH_LENGTH
    #define COMPRESSION_MIN_MATCH_LENGTH 3
#endif

#ifndef COMPRESSION_MAX_MATCH_LENGTH
    #define COMPRESSION_MAX_MATCH_LENGTH 258
#endif

#ifndef COMPRESSION_HASH_TABLE_SIZE
    #define COMPRESSION_HASH_TABLE_SIZE 65536
#endif

#ifndef COMPRESSION_MAX_HUFFMAN_BITS
    #define COMPRESSION_MAX_HUFFMAN_BITS 16
#endif

#ifndef COMPRESSION_MULTITHREAD_THRESHOLD
    #define COMPRESSION_MULTITHREAD_THRESHOLD 1048576
#endif

#ifndef COMPRESSION_MAX_THREADS
    #define COMPRESSION_MAX_THREADS 8
#endif

#ifndef COMPRESSION_STREAM_BUFFER_SIZE
    #define COMPRESSION_STREAM_BUFFER_SIZE 65536
#endif

#ifndef COMPRESSION_ADAPTIVE_UPDATE_INTERVAL
    #define COMPRESSION_ADAPTIVE_UPDATE_INTERVAL 4096
#endif

// ============================================================================
// SAFETY LIMITS
// ============================================================================

#ifndef COMPRESSION_MAX_FILE_SIZE
    #define COMPRESSION_MAX_FILE_SIZE (10ULL * 1024 * 1024 * 1024)
#endif

#ifndef COMPRESSION_MAX_ALLOCATION_SIZE
    #define COMPRESSION_MAX_ALLOCATION_SIZE (1024 * 1024 * 1024)
#endif

#ifndef COMPRESSION_MAX_RECURSION_DEPTH
    #define COMPRESSION_MAX_RECURSION_DEPTH 64
#endif

#ifndef COMPRESSION_MAX_HASH_CHAIN_DEPTH
    #define COMPRESSION_MAX_HASH_CHAIN_DEPTH 128
#endif

// ============================================================================
// FILE FORMAT CONSTANTS
// ============================================================================

namespace compress {
constexpr uint8_t MAGIC_BYTE_1 = 0x53;
constexpr uint8_t MAGIC_BYTE_2 = 0x43;
constexpr uint8_t FORMAT_VERSION = 0x03;

constexpr uint8_t FLAG_COMPRESSED = 0x01;
constexpr uint8_t FLAG_MULTITHREAD = 0x02;
constexpr uint8_t FLAG_STREAMING = 0x04;
constexpr uint8_t FLAG_HAS_CRC = 0x08;
}

// ============================================================================
// ATTRIBUTES
// ============================================================================

#if __cplusplus >= 201703L
    #define COMPRESSION_NODISCARD [[nodiscard]]
#else
    #define COMPRESSION_NODISCARD
#endif

#if __cplusplus >= 201907L
    #define COMPRESSION_LIKELY(x) (x) [[likely]]
    #define COMPRESSION_UNLIKELY(x) (x) [[unlikely]]
#else
    #define COMPRESSION_LIKELY(x) (x)
    #define COMPRESSION_UNLIKELY(x) (x)
#endif

#ifdef COMPRESSION_DEPRECATED
    #define COMPRESSION_DEPRECATED_MSG(msg) [[deprecated(msg)]]
#else
    #define COMPRESSION_DEPRECATED_MSG(msg)
#endif

// ============================================================================
// END OF CONFIG
// ============================================================================
