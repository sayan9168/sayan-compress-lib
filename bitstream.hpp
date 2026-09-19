/**
 * @file bitstream.hpp
 * @brief Bit-level read/write operations for compression engine
 * 
 * Provides efficient bit manipulation for serializing/deserializing
 * variable-length codes used in Huffman encoding.
 * 
 * Time Complexity: O(1) per bit operation
 * Space Complexity: O(1) auxiliary (uses internal buffer)
 */

#pragma once

#include <cstdint>
#include <vector>
#include <fstream>
#include <stdexcept>

namespace compress {

/**
 * @class BitWriter
 * @brief Writes bits sequentially to an output stream
 * 
 * Buffers bits internally and flushes complete bytes to the stream.
 * Uses big-endian bit ordering (MSB first).
 */
class BitWriter {
public:
    explicit BitWriter(std::ostream& out) 
        : out_(out), buffer_(0), bits_in_buffer_(0), bytes_written_(0) {}
    
    ~BitWriter() {
        flush();
    }
    
    /**
     * @brief Write a single bit
     * @param bit The bit value (0 or 1)
     * Time: O(1) amortized
     */
    void writeBit(bool bit) {
        buffer_ = (buffer_ << 1) | (bit ? 1 : 0);
        ++bits_in_buffer_;
        if (bits_in_buffer_ == 8) {
            flushByte();
        }
    }
    
    /**
     * @brief Write multiple bits from a value
     * @param value The value containing bits
     * @param num_bits Number of bits to write (1-32)
     * Time: O(num_bits)
     */
    void writeBits(uint32_t value, int num_bits) {
        if (num_bits <= 0 || num_bits > 32) {
            throw std::invalid_argument("num_bits must be between 1 and 32");
        }
        // Write from MSB to LSB
        for (int i = num_bits - 1; i >= 0; --i) {
            writeBit((value >> i) & 1);
        }
    }
    
    /**
     * @brief Flush remaining bits in buffer (padded with zeros)
     * Time: O(1)
     */
    void flush() {
        if (bits_in_buffer_ > 0) {
            // Pad with zeros to complete the byte
            buffer_ <<= (8 - bits_in_buffer_);
            flushByte();
            bits_in_buffer_ = 0;
            buffer_ = 0;
        }
        out_.flush();
    }
    
    /**
     * @brief Get total bytes written to stream
     */
    size_t bytesWritten() const { return bytes_written_; }
    
private:
    void flushByte() {
        out_.put(static_cast<char>(buffer_));
        ++bytes_written_;
        bits_in_buffer_ = 0;
        buffer_ = 0;
    }
    
    std::ostream& out_;
    uint8_t buffer_;
    int bits_in_buffer_;
    size_t bytes_written_;
};

/**
 * @class BitReader
 * @brief Reads bits sequentially from an input stream
 * 
 * Buffers bytes internally and provides bit-level access.
 * Uses big-endian bit ordering (MSB first).
 */
class BitReader {
public:
    explicit BitReader(std::istream& in) 
        : in_(in), buffer_(0), bits_in_buffer_(0), end_of_stream_(false) {}
    
    /**
     * @brief Read a single bit
     * @return The bit value (0 or 1)
     * Time: O(1) amortized
     */
    bool readBit() {
        if (bits_in_buffer_ == 0 && !end_of_stream_) {
            refillBuffer();
        }
        if (bits_in_buffer_ == 0) {
            end_of_stream_ = true;
            return false;
        }
        --bits_in_buffer_;
        return (buffer_ >> bits_in_buffer_) & 1;
    }
    
    /**
     * @brief Read multiple bits into a value
     * @param num_bits Number of bits to read (1-32)
     * @return The value containing the read bits
     * Time: O(num_bits)
     */
    uint32_t readBits(int num_bits) {
        if (num_bits <= 0 || num_bits > 32) {
            throw std::invalid_argument("num_bits must be between 1 and 32");
        }
        uint32_t result = 0;
        for (int i = 0; i < num_bits; ++i) {
            result = (result << 1) | (readBit() ? 1 : 0);
        }
        return result;
    }
    
    /**
     * @brief Check if end of stream has been reached
     */
    bool eof() const { return end_of_stream_ && bits_in_buffer_ == 0; }
    
    /**
     * @brief Reset reader state for reuse
     */
    void reset() {
        buffer_ = 0;
        bits_in_buffer_ = 0;
        end_of_stream_ = false;
    }
    
private:
    void refillBuffer() {
        int ch = in_.get();
        if (ch == EOF) {
            end_of_stream_ = true;
        } else {
            buffer_ = static_cast<uint8_t>(ch);
            bits_in_buffer_ = 8;
        }
    }
    
    std::istream& in_;
    uint8_t buffer_;
    int bits_in_buffer_;
    bool end_of_stream_;
};

} // namespace compress
