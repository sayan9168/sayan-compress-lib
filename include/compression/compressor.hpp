/**
 * @file compressor.hpp
 * @brief Production-grade compression library public API
 * 
 * Clean, modern C++20 interface for compression operations.
 * Zero external dependencies. Thread-safe. Comprehensive error handling.
 * 
 * @author Sayan
 * @version 4.0.0 (Production Grade - Redesigned)
 * @license MIT
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <span>
#include <optional>
#include <variant>
#include <system_error>
#include <fstream>
#include <chrono>

// Internal includes
#include "types.hpp"
#include "config.hpp"
#include "thread_pool.hpp"
#include "hash_chain.hpp"

namespace compress {

// ============================================================================
// Error Handling - Modern C++ std::expected style
// ============================================================================

/**
 * @brief Error codes specific to compression operations
 */
enum class CompressErrc : int {
    success = 0,
    
    // File I/O errors
    file_not_found = 1001,
    file_open_failed,
    file_read_error,
    file_write_error,
    file_too_large,
    invalid_file_format,
    unsupported_version,
    
    // Compression errors
    compression_failed,
    decompression_failed,
    corrupted_data,
    crc_mismatch,
    invalid_parameter,
    buffer_overflow,
    
    // Resource errors
    out_of_memory,
    allocation_failed,
    
    // Internal errors
    internal_error,
    not_implemented
};

/**
 * @brief Make error code for compression errors
 */
inline std::error_code make_error_code(CompressErrc e) {
    return {static_cast<int>(e), std::generic_category()};
}

/**
 * @brief Result type for compression operations
 * 
 * Usage:
 *   auto result = compress(data);
 *   if (result) {
 *       use(*result);
 *   } else {
 *       handle_error(result.error());
 *   }
 */
template<typename T>
using Result = std::variant<T, std::error_code>;

// Helper functions for Result
template<typename T>
[[nodiscard]] inline bool is_ok(const Result<T>& r) {
    return std::holds_alternative<T>(r);
}

template<typename T>
[[nodiscard]] inline bool is_err(const Result<T>& r) {
    return !std::holds_alternative<T>(r);
}

template<typename T>
[[nodiscard]] inline const T* ok_ptr(const Result<T>& r) {
    return std::get_if<T>(&r);
}

template<typename T>
[[nodiscard]] inline std::error_code error(const Result<T>& r) {
    if (auto err = std::get_if<std::error_code>(&r)) {
        return *err;
    }
    return {};
}

// ============================================================================
// Configuration Structures
// ============================================================================

/**
 * @brief Compression level preset (1-9)
 */
enum class Level : int {
    fastest = 1,    // Minimal compression, maximum speed
    fast = 3,       // Good speed, decent compression
    default_ = 5,   // Balanced (default)
    good = 6,       // Better compression
    max = 7,        // Maximum compression
    ultra = 9       // Extreme compression, very slow
};

/**
 * @brief Compression strategy selection
 */
enum class Strategy {
    automatic,      // Auto-select based on data
    lz77_only,      // Skip entropy coding
    huffman_only,   // Skip LZ77
    combined,       // LZ77 + Huffman (default)
    adaptive        // Dynamic strategy switching
};

/**
 * @brief Complete compression configuration
 */
struct Options {
    Level level = Level::default_;           // Compression level 1-9
    Strategy strategy = Strategy::combined;  // Algorithm strategy
    size_t window_size = DEFAULT_WINDOW_SIZE;// Sliding window size
    size_t block_size = 64 * 1024;          // Block size for streaming
    unsigned threads = 0;                    // 0 = auto-detect
    bool enable_crc = true;                  // Enable CRC verification
    bool enable_mt = true;                   // Enable multi-threading
    
    /**
     * @brief Validate and normalize options
     * @return true if valid, false if corrected
     */
    [[nodiscard]] bool validate() {
        bool corrected = false;
        
        // Clamp level
        if (static_cast<int>(level) < 1) {
            level = Level::fastest;
            corrected = true;
        } else if (static_cast<int>(level) > 9) {
            level = Level::ultra;
            corrected = true;
        }
        
        // Auto-detect threads
        if (threads == 0) {
            threads = std::thread::hardware_concurrency();
            if (threads == 0) threads = 4;
            if (threads > 8) threads = 8;  // Reasonable limit
        }
        
        // Window size limits
        if (window_size < 1024) window_size = 1024;
        if (window_size > 65536) window_size = 65536;
        
        return !corrected;
    }
};

/**
 * @brief Compression statistics
 */
struct Stats {
    uint64_t input_bytes = 0;
    uint64_t output_bytes = 0;
    double ratio = 0.0;              // output/input * 100
    double compression_ratio = 0.0;   // input/output
    double space_savings = 0.0;       // percentage saved
    uint64_t elapsed_ms = 0;
    double throughput_mbps = 0.0;
    unsigned threads_used = 1;
    bool success = false;
    std::error_code error;
    
    static Stats from_sizes(uint64_t input, uint64_t output, uint64_t time_ms) {
        Stats s;
        s.input_bytes = input;
        s.output_bytes = output;
        s.elapsed_ms = time_ms;
        
        if (input > 0) {
            s.ratio = (static_cast<double>(output) / input) * 100.0;
            if (output > 0) {
                s.compression_ratio = static_cast<double>(input) / output;
            }
            s.space_savings = 100.0 - s.ratio;
        }
        
        if (time_ms > 0) {
            s.throughput_mbps = (static_cast<double>(input) / 1048576.0) 
                              / (static_cast<double>(time_ms) / 1000.0);
        }
        
        s.success = true;
        return s;
    }
};

// ============================================================================
// Main Compression API
// ============================================================================

/**
 * @brief Compress a file
 * @param input_path Path to input file
 * @param output_path Path to output compressed file
 * @param opts Compression options
 * @return Result containing output path on success, error code on failure
 * 
 * Example:
 *   auto result = compress_file("data.txt", "data.szc");
 *   if (is_ok(result)) {
 *       std::cout << "Compressed to: " << *ok_ptr(result) << "\n";
 *   } else {
 *       std::cerr << "Error: " << error(result).message() << "\n";
 *   }
 */
[[nodiscard]]
Result<std::string> compress_file(
    const std::string& input_path,
    const std::string& output_path,
    Options opts = {});

/**
 * @brief Decompress a file
 * @param input_path Path to compressed file
 * @param output_path Path to output decompressed file
 * @return Result containing output path on success, error code on failure
 */
[[nodiscard]]
Result<std::string> decompress_file(
    const std::string& input_path,
    const std::string& output_path);

/**
 * @brief Compress data in memory
 * @param input Input data span
 * @param opts Compression options
 * @return Result containing compressed data or error code
 */
[[nodiscard]]
Result<std::vector<uint8_t>> compress_memory(
    std::span<const uint8_t> input,
    Options opts = {});

/**
 * @brief Decompress data from memory
 * @param input Compressed data span
 * @return Result containing decompressed data or error code
 */
[[nodiscard]]
Result<std::vector<uint8_t>> decompress_memory(
    std::span<const uint8_t> input);

/**
 * @brief Get recommended options for a given compression level
 * @param level Desired compression level
 * @return Optimized options structure
 */
[[nodiscard]]
Options get_options_for_level(Level level);

/**
 * @brief Get library version string
 */
[[nodiscard]]
constexpr const char* version() noexcept {
    return "4.0.0";
}

/**
 * @brief Check if multi-threading is available
 */
[[nodiscard]]
inline bool multithreading_available() noexcept {
#if defined(__cpp_lib_jthread) || COMPRESSION_ENABLE_MULTITHREADING
    return std::thread::hardware_concurrency() > 1;
#else
    return false;
#endif
}

/**
 * @brief Get number of hardware threads
 */
[[nodiscard]]
inline unsigned hardware_threads() noexcept {
    unsigned n = std::thread::hardware_concurrency();
    return n > 0 ? n : 4;
}

// ============================================================================
// Streaming Compression API
// ============================================================================

/**
 * @class stream_compressor
 * @brief Streaming compression for large files or real-time data
 * 
 * Usage:
 *   std::ofstream out("output.szc", std::ios::binary);
 *   stream_compressor compressor(out);
 *   
 *   for (const auto& chunk : chunks) {
 *       compressor.write(chunk.data(), chunk.size());
 *   }
 *   
 *   compressor.finalize();
 */
class stream_compressor {
public:
    /**
     * @brief Construct streaming compressor
     * @param output Output stream to write compressed data
     * @param opts Compression options
     */
    explicit stream_compressor(
        std::ostream& output,
        Options opts = {});
    
    ~stream_compressor();
    
    // Non-copyable
    stream_compressor(const stream_compressor&) = delete;
    stream_compressor& operator=(const stream_compressor&) = delete;
    
    /**
     * @brief Write and compress data chunk
     * @param data Pointer to input data
     * @param size Size of input data
     * @return true on success, false on error
     */
    [[nodiscard]]
    bool write(const void* data, size_t size);
    
    /**
     * @brief Write and compress data from span
     * @param data Input data span
     * @return true on success, false on error
     */
    [[nodiscard]]
    bool write(std::span<const uint8_t> data);
    
    /**
     * @brief Finalize compression and write footer
     * @return true on success, false on error
     */
    [[nodiscard]]
    bool finalize();
    
    /**
     * @brief Get compression statistics
     */
    [[nodiscard]]
    const Stats& stats() const noexcept { return stats_; }
    
    /**
     * @brief Check if an error occurred
     */
    [[nodiscard]]
    bool good() const noexcept { return !error_; }
    
    /**
     * @brief Get last error code
     */
    [[nodiscard]]
    std::error_code error() const noexcept { return error_; }

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
    std::ostream& output_;
    Options opts_;
    Stats stats_;
    std::error_code error_;
    bool finalized_ = false;
};

/**
 * @class stream_decompressor
 * @brief Streaming decompression
 * 
 * Usage:
 *   std::ifstream in("input.szc", std::ios::binary);
 *   stream_decompressor decompressor(in);
 *   
 *   std::vector<uint8_t> buffer(65536);
 *   while (!decompressor.eof()) {
 *       size_t n = decompressor.read(buffer.data(), buffer.size());
 *       if (n > 0) process(buffer.data(), n);
 *   }
 */
class stream_decompressor {
public:
    /**
     * @brief Construct streaming decompressor
     * @param input Input stream with compressed data
     */
    explicit stream_decompressor(std::istream& input);
    
    ~stream_decompressor();
    
    // Non-copyable
    stream_decompressor(const stream_decompressor&) = delete;
    stream_decompressor& operator=(const stream_decompressor&) = delete;
    
    /**
     * @brief Read and decompress data
     * @param output Buffer for decompressed data
     * @param size Maximum bytes to read
     * @return Number of bytes actually read (0 = EOF)
     */
    size_t read(void* output, size_t size);
    
    /**
     * @brief Check end of stream
     */
    [[nodiscard]]
    bool eof() const noexcept { return eof_; }
    
    /**
     * @brief Check if an error occurred
     */
    [[nodiscard]]
    bool good() const noexcept { return !error_; }
    
    /**
     * @brief Get last error code
     */
    [[nodiscard]]
    std::error_code error() const noexcept { return error_; }

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
    std::istream& input_;
    bool eof_ = false;
    std::error_code error_;
};

// ============================================================================
// Advanced: Direct Engine Access
// ============================================================================

/**
 * @brief Low-level LZ77 encoder for custom applications
 */
class lz77_encoder {
public:
    explicit lz77_encoder(size_t window_size = DEFAULT_WINDOW_SIZE);
    
    /**
     * @brief Encode data to LZ77 tokens
     * @param data Input data
     * @return Vector of (offset, length, literal) tuples
     */
    [[nodiscard]]
    std::vector<std::tuple<uint16_t, uint8_t, uint8_t>> encode(
        std::span<const uint8_t> data);

private:
    HashChainFinder finder_;
};

/**
 * @brief Low-level Huffman codec
 */
class huffman_codec {
public:
    /**
     * @brief Build codec from symbol frequencies
     * @param freqs Frequency array (index = symbol)
     */
    void build_from_frequencies(std::span<const uint64_t> freqs);
    
    /**
     * @brief Encode a symbol to bits
     * @param symbol Symbol to encode
     * @return (code, bit_length) pair
     */
    [[nodiscard]]
    std::pair<uint32_t, uint8_t> encode_symbol(int symbol) const;
    
    /**
     * @brief Decode symbol from bit reader
     * @tparam BitReader Functor returning next bit
     * @param reader Bit reader function
     * @return Decoded symbol
     */
    template<typename BitReader>
    [[nodiscard]]
    int decode_symbol(BitReader reader) const;
    
    /**
     * @brief Serialize code lengths to header format
     * @param out Output stream
     */
    void serialize_header(std::ostream& out) const;
    
    /**
     * @brief Deserialize code lengths from header
     * @param in Input stream
     * @return Codec with loaded code lengths
     */
    [[nodiscard]]
    static huffman_codec deserialize_header(std::istream& in);

private:
    struct code_entry {
        uint32_t code;
        uint8_t length;
    };
    std::vector<code_entry> codes_;
    std::vector<std::pair<int16_t, int16_t>> decode_tree_;
};

} // namespace compress

// ============================================================================
// User-defined literals (convenience)
// ============================================================================

namespace compress::literals {

/**
 * @brief String literal for compression level
 * Usage: using namespace compress::literals;
 *        auto opt = Options{.level = "fast"_lvl};
 */
[[nodiscard]]
inline constexpr Level operator""_lvl(const char* str, size_t) {
    if (str[0] == '1' || str[0] == 'f') return Level::fastest;
    if (str[0] == '3' || str[0] == 'F') return Level::fast;
    if (str[0] == '5' || str[0] == 'd') return Level::default_;
    if (str[0] == '7' || str[0] == 'm') return Level::max;
    if (str[0] == '9' || str[0] == 'u') return Level::ultra;
    return Level::default_;
}

} // namespace compress::literals
