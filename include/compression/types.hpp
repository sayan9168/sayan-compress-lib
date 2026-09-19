/**
 * @file types.hpp
 * @brief Common type definitions and utilities
 * 
 * Production-grade type definitions, error codes, and utility structures.
 * 
 * @author Sayan
 * @version 3.0.0 (Production Grade)
 * @license MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <variant>
#include "config.hpp"

namespace compress {

// ============================================================================
// ERROR CODES
// ============================================================================

enum class ErrorCode : int {
    SUCCESS = 0,
    
    // File I/O errors
    FILE_NOT_FOUND = 1001,
    FILE_OPEN_FAILED = 1002,
    FILE_READ_ERROR = 1003,
    FILE_WRITE_ERROR = 1004,
    FILE_SEEK_ERROR = 1005,
    FILE_TOO_LARGE = 1006,
    
    // Compression errors
    COMPRESSION_FAILED = 2001,
    DECOMPRESSION_FAILED = 2002,
    INVALID_FORMAT = 2003,
    CORRUPTED_DATA = 2004,
    CRC_MISMATCH = 2005,
    UNSUPPORTED_VERSION = 2006,
    
    // Memory errors
    OUT_OF_MEMORY = 3001,
    ALLOCATION_FAILED = 3002,
    BUFFER_OVERFLOW = 3003,
    
    // Parameter errors
    INVALID_PARAMETER = 4001,
    NULL_POINTER = 4002,
    OUT_OF_RANGE = 4003,
    
    // Threading errors
    THREAD_CREATION_FAILED = 5001,
    THREAD_SYNC_ERROR = 5002,
    
    // Internal errors
    INTERNAL_ERROR = 9001,
    NOT_IMPLEMENTED = 9002,
    UNKNOWN_ERROR = 9999
};

// ============================================================================
// RESULT TYPE
// ============================================================================

/**
 * @brief Result type for operations that can fail
 * 
 * Usage:
 *   Result<size_t> result = compressFile(...);
 *   if (result) {
 *       size_t compressedSize = *result;
 *   } else {
 *       ErrorCode err = result.error();
 *   }
 */
template<typename T>
class Result {
public:
    using value_type = T;
    
    Result(T value) : data_(std::move(value)), is_success_(true) {}
    Result(ErrorCode error) : error_code_(error), is_success_(false) {}
    
    COMPRESSION_NODISCARD
    bool isSuccess() const noexcept { return is_success_; }
    
    COMPRESSION_NODISCARD
    bool hasError() const noexcept { return !is_success_; }
    
    explicit operator bool() const noexcept { return is_success_; }
    
    COMPRESSION_NODISCARD
    const T& value() const { 
        if (!is_success_) throw std::runtime_error("Accessing error state");
        return std::get<T>(data_); 
    }
    
    COMPRESSION_NODISCARD
    T& value() { 
        if (!is_success_) throw std::runtime_error("Accessing error state");
        return std::get<T>(data_); 
    }
    
    COMPRESSION_NODISCARD
    ErrorCode error() const { 
        if (is_success_) return ErrorCode::SUCCESS;
        return error_code_; 
    }
    
    COMPRESSION_NODISCARD
    std::string errorMessage() const {
        if (is_success_) return "Success";
        return errorCodeToString(error_code_);
    }
    
private:
    static std::string errorCodeToString(ErrorCode code) {
        switch (code) {
            case ErrorCode::SUCCESS: return "Success";
            case ErrorCode::FILE_NOT_FOUND: return "File not found";
            case ErrorCode::FILE_OPEN_FAILED: return "Failed to open file";
            case ErrorCode::FILE_READ_ERROR: return "File read error";
            case ErrorCode::FILE_WRITE_ERROR: return "File write error";
            case ErrorCode::COMPRESSION_FAILED: return "Compression failed";
            case ErrorCode::DECOMPRESSION_FAILED: return "Decompression failed";
            case ErrorCode::INVALID_FORMAT: return "Invalid file format";
            case ErrorCode::CORRUPTED_DATA: return "Corrupted data";
            case ErrorCode::CRC_MISMATCH: return "CRC verification failed";
            case ErrorCode::OUT_OF_MEMORY: return "Out of memory";
            case ErrorCode::INVALID_PARAMETER: return "Invalid parameter";
            default: return "Unknown error (code: " + std::to_string(static_cast<int>(code)) + ")";
        }
    }
    
    std::variant<T, std::monostate> data_;
    ErrorCode error_code_;
    bool is_success_;
};

// Specialization for void return type
template<>
class Result<void> {
public:
    Result() : is_success_(true) {}
    Result(ErrorCode error) : error_code_(error), is_success_(false) {}
    
    COMPRESSION_NODISCARD
    bool isSuccess() const noexcept { return is_success_; }
    
    COMPRESSION_NODISCARD
    bool hasError() const noexcept { return !is_success_; }
    
    explicit operator bool() const noexcept { return is_success_; }
    
    COMPRESSION_NODISCARD
    ErrorCode error() const { return error_code_; }
    
    COMPRESSION_NODISCARD
    std::string errorMessage() const {
        if (is_success_) return "Success";
        return "Error code: " + std::to_string(static_cast<int>(error_code_));
    }
    
private:
    ErrorCode error_code_;
    bool is_success_;
};

// ============================================================================
// STATISTICS STRUCTURES
// ============================================================================

/**
 * @brief Compression statistics
 */
struct CompressionStats {
    uint64_t original_size = 0;
    uint64_t compressed_size = 0;
    double ratio = 0.0;              // Compressed / Original * 100
    double compression_ratio = 0.0;   // Original / Compressed
    double space_savings = 0.0;       // Percentage saved
    uint64_t processing_time_ms = 0;
    double throughput_mbps = 0.0;
    bool success = false;
    std::string error_message;
    
    COMPRESSION_NODISCARD
    static CompressionStats fromSizes(uint64_t original, uint64_t compressed) {
        CompressionStats stats;
        stats.original_size = original;
        stats.compressed_size = compressed;
        if (original > 0) {
            stats.ratio = (static_cast<double>(compressed) / original) * 100.0;
            stats.compression_ratio = static_cast<double>(original) / compressed;
            stats.space_savings = 100.0 - stats.ratio;
        }
        stats.success = true;
        return stats;
    }
};

/**
 * @brief Decompression statistics
 */
struct DecompressionStats {
    uint64_t compressed_size = 0;
    uint64_t decompressed_size = 0;
    bool verified = false;
    uint64_t processing_time_ms = 0;
    double throughput_mbps = 0.0;
    bool success = false;
    std::string error_message;
};

/**
 * @brief Multi-thread compression statistics
 */
struct MultiThreadStats {
    uint64_t original_size = 0;
    uint64_t compressed_size = 0;
    double ratio = 0.0;
    int threads_used = 0;
    double speedup = 1.0;
    double parallel_efficiency = 0.0;
    bool success = false;
    std::string error_message;
};

/**
 * @brief Streaming compression statistics
 */
struct StreamingStats {
    uint64_t total_input_bytes = 0;
    uint64_t total_output_bytes = 0;
    uint64_t chunks_processed = 0;
    double average_ratio = 0.0;
    bool is_complete = false;
    bool is_streaming = false;
};

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @brief Compression levels/presets
 */
enum class CompressionLevel : int {
    Fastest = 1,      // Minimal compression, maximum speed
    Fast = 3,         // Good speed, decent compression
    Default = 5,      // Balanced speed/ratio
    Maximum = 7,      // Best compression, slower
    Insane = 9        // Extreme compression, very slow
};

/**
 * @brief Compression strategy
 */
enum class CompressionStrategy {
    Default,          // Automatic selection
    LZ77Only,         // Only LZ77, no Huffman
    HuffmanOnly,      // Only Huffman, no LZ77
    Combined,         // LZ77 + Huffman (default)
    Adaptive          // Dynamic strategy based on data
};

/**
 * @brief File format version
 */
enum class FormatVersion : uint8_t {
    V1 = 0x01,        // Basic format
    V2 = 0x02,        // Multi-thread support
    V3 = 0x03,        // Production grade with CRC
    Current = V3
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Convert bytes to human-readable string
 */
inline std::string formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        ++unit_index;
    }
    
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_index]);
    return std::string(buffer);
}

/**
 * @brief Get current timestamp in milliseconds
 */
inline uint64_t getCurrentTimeMs() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto duration = now.time_since_epoch();
    return duration_cast<milliseconds>(duration).count();
}

} // namespace compress
