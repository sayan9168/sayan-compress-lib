/**
 * @file lz77.hpp
 * @brief LZ77 sliding window dictionary compression algorithm
 * 
 * Implements Lempel-Ziv 1977 compression with configurable window sizes.
 * Uses a hash table for efficient pattern matching in the search buffer.
 * 
 * Time Complexity: O(N) average case with hash table optimization
 *                  O(N * W) worst case where W is window size
 * Space Complexity: O(W + H) where W is window size, H is hash table size
 */

#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <cstring>
#include <algorithm>

namespace compress {

// Default configuration constants
constexpr size_t DEFAULT_SEARCH_WINDOW_SIZE = 4096;  // 4KB sliding window
constexpr size_t DEFAULT_LOOKAHEAD_BUFFER_SIZE = 256; // 256 bytes lookahead
constexpr size_t MAX_MATCH_LENGTH = 258;              // Maximum match length (DEFLATE-style)
constexpr size_t HASH_TABLE_SIZE = 1 << 16;           // 64K hash buckets
constexpr uint32_t HASH_MASK = HASH_TABLE_SIZE - 1;

/**
 * @struct LZ77Token
 * @brief Represents a single LZ77 compression token
 * 
 * Token types:
 * - Literal: offset=0, length=0, literal=byte value
 * - Match: offset>0, length>0, literal=next byte after match (or 0)
 */
struct LZ77Token {
    uint16_t offset;    // Distance back in the window (0 for literals)
    uint8_t length;     // Match length (0 for literals)
    uint8_t literal;    // Literal byte or next byte after match
    
    bool isLiteral() const { return offset == 0 && length == 0; }
    bool isMatch() const { return offset > 0 && length > 0; }
};

/**
 * @class LZ77Encoder
 * @brief LZ77 compression encoder with hash-based pattern matching
 * 
 * Uses a rolling hash (simple XOR-based) to quickly find potential
 * match positions in the search window, then verifies with byte comparison.
 */
class LZ77Encoder {
public:
    /**
     * @brief Construct encoder with configurable window sizes
     * @param searchWindowSize Size of the sliding search window
     * @param lookaheadBufferSize Size of the lookahead buffer
     */
    explicit LZ77Encoder(
        size_t searchWindowSize = DEFAULT_SEARCH_WINDOW_SIZE,
        size_t lookaheadBufferSize = DEFAULT_LOOKAHEAD_BUFFER_SIZE)
        : searchWindowSize_(std::min(searchWindowSize, static_cast<size_t>(UINT16_MAX))),
          lookaheadBufferSize_(std::min(lookaheadBufferSize, static_cast<size_t>(MAX_MATCH_LENGTH))) {
        hashTable_.fill(0xFFFFFFFF);  // Initialize with invalid positions
    }
    
    /**
     * @brief Encode input data into LZ77 tokens
     * @param input Pointer to input data
     * @param inputSize Size of input data in bytes
     * @return Vector of LZ77 tokens
     * 
     * Time: O(N) average, where N is input size
     * Space: O(N) for output tokens
     */
    std::vector<LZ77Token> encode(const uint8_t* input, size_t inputSize) {
        std::vector<LZ77Token> tokens;
        tokens.reserve(inputSize / 2);  // Heuristic: expect ~2x compression
        
        if (inputSize == 0) {
            return tokens;
        }
        
        // Reset hash table for new encoding session
        hashTable_.fill(0xFFFFFFFF);
        
        size_t pos = 0;
        while (pos < inputSize) {
            LZ77Token token = findBestMatch(input, inputSize, pos);
            tokens.push_back(token);
            
            // Advance position based on token type
            if (token.isLiteral()) {
                // Update hash for literal byte and advance by 1
                if (pos + 3 <= inputSize) {
                    updateHash(input, pos, inputSize);
                }
                ++pos;
            } else {
                // Match: advance by match length
                size_t matchEnd = pos + token.length;
                // Update hash for all positions in the match
                for (size_t i = pos; i < matchEnd && i + 3 <= inputSize; ++i) {
                    updateHash(input, i, inputSize);
                }
                pos += token.length;
            }
        }
        
        return tokens;
    }
    
    /**
     * @brief Get the search window size
     */
    size_t searchWindowSize() const { return searchWindowSize_; }
    
    /**
     * @brief Get the lookahead buffer size
     */
    size_t lookaheadBufferSize() const { return lookaheadBufferSize_; }
    
private:
    /**
     * @brief Compute hash for 3-byte sequence starting at pos
     * Time: O(1)
     */
    uint32_t computeHash(const uint8_t* data, size_t pos) const {
        // Simple but effective hash: combine 3 bytes
        return ((data[pos] << 16) | (data[pos + 1] << 8) | data[pos + 2]) & HASH_MASK;
    }
    
    /**
     * @brief Update hash table with position's 3-byte sequence
     * Time: O(1)
     */
    void updateHash(const uint8_t* data, size_t pos, size_t dataSize) {
        if (pos + 3 > dataSize) return;
        
        uint32_t hash = computeHash(data, pos);
        hashTable_[hash] = static_cast<uint32_t>(pos);
    }
    
    /**
     * @brief Find the best match in the search window
     * @param input Input data pointer
     * @param inputSize Total input size
     * @param pos Current position in input
     * @return Best matching token (literal if no good match found)
     * 
     * Time: O(W) worst case where W is window size
     */
    LZ77Token findBestMatch(const uint8_t* input, size_t inputSize, size_t pos) {
        LZ77Token bestToken{0, 0, input[pos]};  // Default: literal token
        
        // Not enough data for hashing, emit literal
        if (pos + 3 > inputSize) {
            return bestToken;
        }
        
        // Get potential match position from hash table
        uint32_t hash = computeHash(input, pos);
        uint32_t matchPos = hashTable_[hash];
        
        // Validate match position is within search window
        if (matchPos >= pos || pos - matchPos > searchWindowSize_) {
            return bestToken;
        }
        
        // Calculate search range boundaries
        size_t searchStart = (pos > searchWindowSize_) ? (pos - searchWindowSize_) : 0;
        size_t maxMatchLen = std::min(lookaheadBufferSize_, inputSize - pos);
        
        // Try to find a better match by checking multiple positions
        // (could be extended with hash chains for better compression)
        int bestLength = 0;
        size_t bestOffset = 0;
        
        // Check the hashed position first
        size_t currentMatchPos = matchPos;
        while (currentMatchPos >= searchStart && currentMatchPos < pos) {
            // Calculate match length
            size_t matchLen = 0;
            while (matchLen < maxMatchLen &&
                   matchLen < MAX_MATCH_LENGTH &&
                   input[currentMatchPos + matchLen] == input[pos + matchLen]) {
                ++matchLen;
            }
            
            // Update best match if this is longer
            if (matchLen > static_cast<size_t>(bestLength)) {
                bestLength = static_cast<int>(matchLen);
                bestOffset = pos - currentMatchPos;
                
                // Early exit if we found maximum possible match
                if (matchLen >= maxMatchLen || 
                    matchLen >= MAX_MATCH_LENGTH) {
                    break;
                }
            }
            
            // For simplicity, just check the primary hash match
            // (hash chains would check more positions here)
            break;
        }
        
        // Only use match if it provides actual compression benefit
        // Minimum match length of 3 to justify the token overhead
        if (bestLength >= 3 && bestOffset <= searchWindowSize_ && bestOffset > 0) {
            bestToken.offset = static_cast<uint16_t>(bestOffset);
            bestToken.length = static_cast<uint8_t>(bestLength);
            // Include next byte after match as literal (if available)
            size_t nextPos = pos + bestLength;
            if (nextPos < inputSize) {
                bestToken.literal = input[nextPos];
            } else {
                bestToken.literal = 0;
            }
        }
        
        return bestToken;
    }
    
    size_t searchWindowSize_;
    size_t lookaheadBufferSize_;
    std::array<uint32_t, HASH_TABLE_SIZE> hashTable_;
};

/**
 * @class LZ77Decoder
 * @brief LZ77 decompression decoder
 * 
 * Reconstructs original data from LZ77 tokens using a sliding window buffer.
 * 
 * Time Complexity: O(N) where N is output size
 * Space Complexity: O(W) where W is window size
 */
class LZ77Decoder {
public:
    /**
     * @brief Construct decoder with configurable window size
     * @param windowSize Size of the reconstruction window
     */
    explicit LZ77Decoder(size_t windowSize = DEFAULT_SEARCH_WINDOW_SIZE)
        : windowSize_(windowSize) {
        window_.resize(windowSize);
        writePos_ = 0;
    }
    
    /**
     * @brief Decode LZ77 tokens back to original data
     * @param tokens Vector of LZ77 tokens
     * @param outputSize Expected output size (for pre-allocation)
     * @return Decoded data as byte vector
     * 
     * Time: O(N) where N is total output bytes
     * Space: O(N) for output
     */
    std::vector<uint8_t> decode(const std::vector<LZ77Token>& tokens, size_t outputSize = 0) {
        std::vector<uint8_t> output;
        if (outputSize > 0) {
            output.reserve(outputSize);
        }
        
        for (const auto& token : tokens) {
            if (token.isLiteral()) {
                // Literal byte: copy directly to output
                output.push_back(token.literal);
                updateWindow(token.literal);
            } else if (token.isMatch()) {
                // Match: copy bytes from window at specified offset
                size_t readPos = (writePos_ + windowSize_ - token.offset) % windowSize_;
                
                // Copy 'length' bytes, handling wrap-around
                for (uint8_t i = 0; i < token.length; ++i) {
                    uint8_t byte = window_[readPos];
                    output.push_back(byte);
                    updateWindow(byte);
                    
                    // Handle self-referential matches (RLE-like patterns)
                    readPos = (readPos + 1) % windowSize_;
                }
                
                // If there's a trailing literal in the token, output it
                if (token.length > 0 && token.literal != 0) {
                    // Note: In our encoding scheme, the literal field contains
                    // the next byte only when we need it for lookahead
                }
            }
        }
        
        return output;
    }
    
private:
    /**
     * @brief Update circular window buffer with new byte
     * Time: O(1)
     */
    void updateWindow(uint8_t byte) {
        window_[writePos_] = byte;
        writePos_ = (writePos_ + 1) % windowSize_;
    }
    
    size_t windowSize_;
    std::vector<uint8_t> window_;
    size_t writePos_;
};

} // namespace compress
