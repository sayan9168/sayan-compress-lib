/**
 * @file compressor_engine.hpp
 * @brief Main compression engine combining LZ77 and Huffman coding
 * 
 * Implements the complete compression/decompression pipeline:
 * 1. LZ77 dictionary encoding
 * 2. Huffman frequency analysis and code generation
 * 3. Bit-level serialization with custom header format
 * 
 * File Format:
 * +------------------+
 * | Magic (0x53, 0x4A)|  2 bytes
 * +------------------+
 * | Original Size    |  8 bytes (uint64_t, little-endian)
 * +------------------+
 * | Num Symbols      |  2 bytes (uint16_t)
 * +------------------+
 * | Code Lengths     |  N bytes (one per symbol)
 * +------------------+
 * | Compressed Bits  |  Variable length
 * +------------------+
 * 
 * Time Complexity: O(N log K) for compression where N is input size, K is alphabet
 *                  O(N) for decompression
 * Space Complexity: O(W + K) where W is window size, K is alphabet size
 */

#pragma once

#include "bitstream.hpp"
#include "lz77.hpp"
#include "huffman.hpp"

#include <cstdint>
#include <vector>
#include <fstream>
#include <cstring>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <algorithm>

namespace compress {

// File format constants
constexpr uint8_t MAGIC_BYTE_1 = 0x53;
constexpr uint8_t MAGIC_BYTE_2 = 0x4A;
constexpr int HEADER_SYMBOL_RANGE = 512;  // Support up to 512 symbols

/**
 * @struct CompressionStats
 * @brief Statistics about compression operation
 */
struct CompressionStats {
    size_t originalSize;
    size_t compressedSize;
    double compressionRatio;
    double compressionPercentage;
    double processingTimeMs;
    
    void print() const {
        std::cout << "=== Compression Statistics ===" << std::endl;
        std::cout << "Original size:     " << originalSize << " bytes" << std::endl;
        std::cout << "Compressed size:   " << compressedSize << " bytes" << std::endl;
        std::cout << "Compression ratio: " << compressionRatio << ":1" << std::endl;
        std::cout << "Space savings:     " << compressionPercentage << "%" << std::endl;
        if (processingTimeMs > 0) {
            std::cout << "Processing time:   " << processingTimeMs << " ms" << std::endl;
        }
        std::cout << "==============================" << std::endl;
    }
};

/**
 * @class CompressorEngine
 * @brief Main compression/decompression engine
 * 
 * Combines LZ77 and Huffman coding for efficient lossless compression.
 */
class CompressorEngine {
public:
    /**
     * @brief Construct engine with default or custom parameters
     * @param searchWindowSize LZ77 search window size
     * @param lookaheadBufferSize LZ77 lookahead buffer size
     */
    explicit CompressorEngine(
        size_t searchWindowSize = DEFAULT_SEARCH_WINDOW_SIZE,
        size_t lookaheadBufferSize = DEFAULT_LOOKAHEAD_BUFFER_SIZE)
        : lz77Encoder_(searchWindowSize, lookaheadBufferSize),
          lz77Decoder_(searchWindowSize) {}
    
    /**
     * @brief Compress data from input stream to output stream
     * @param input Input stream
     * @param output Output stream
     * @return Compression statistics
     * 
     * Time: O(N log K) where N is input size
     * Space: O(N + W + K) for tokens, window, and Huffman tables
     */
    CompressionStats compress(std::istream& input, std::ostream& output) {
        CompressionStats stats{};
        
        // Read all input data
        std::vector<uint8_t> inputData(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>()
        );
        stats.originalSize = inputData.size();
        
        if (inputData.empty()) {
            // Handle empty file: write header only
            writeHeader(output, 0, {});
            stats.compressedSize = output.tellp();
            stats.compressionRatio = 1.0;
            stats.compressionPercentage = 0.0;
            return stats;
        }
        
        // Phase 1: LZ77 encoding
        std::vector<LZ77Token> tokens = lz77Encoder_.encode(
            inputData.data(), inputData.size());
        
        // Phase 2: Build frequency table and Huffman tree
        FrequencyCounter counter;
        counter.countTokens(tokens);
        
        const auto& frequencies = counter.frequencies();
        HuffmanTree huffmanTree;
        huffmanTree.buildFromFrequencies(frequencies.data(), frequencies.size());
        
        // Get code lengths for header
        std::vector<uint8_t> codeLengths = huffmanTree.getCodeLengths(HEADER_SYMBOL_RANGE);
        
        // Phase 3: Write header
        writeHeader(output, stats.originalSize, codeLengths);
        
        // Phase 4: Encode tokens using Huffman coding
        BitWriter bitWriter(output);
        encodeTokens(tokens, huffmanTree, bitWriter);
        bitWriter.flush();
        
        // Calculate statistics
        stats.compressedSize = static_cast<size_t>(output.tellp());
        if (stats.originalSize > 0) {
            stats.compressionRatio = static_cast<double>(stats.originalSize) / 
                                     static_cast<double>(stats.compressedSize);
            stats.compressionPercentage = (1.0 - static_cast<double>(stats.compressedSize) / 
                                          static_cast<double>(stats.originalSize)) * 100.0;
        } else {
            stats.compressionRatio = 1.0;
            stats.compressionPercentage = 0.0;
        }
        
        return stats;
    }
    
    /**
     * @brief Decompress data from input stream to output stream
     * @param input Input stream (compressed data)
     * @param output Output stream (decompressed data)
     * @return True if successful, false otherwise
     * 
     * Time: O(N) where N is output size
     * Space: O(W + K) for window and Huffman tables
     */
    bool decompress(std::istream& input, std::ostream& output) {
        // Phase 1: Read and parse header
        uint64_t originalSize;
        std::vector<uint8_t> codeLengths;
        
        if (!readHeader(input, originalSize, codeLengths)) {
            std::cerr << "Error: Invalid header format" << std::endl;
            return false;
        }
        
        // Handle empty file
        if (originalSize == 0) {
            return true;
        }
        
        // Phase 2: Reconstruct Huffman tree from code lengths
        HuffmanTree huffmanTree;
        huffmanTree.buildFromCodeLengths(codeLengths.data(), codeLengths.size());
        
        if (!huffmanTree.canDecode()) {
            std::cerr << "Error: Invalid Huffman tree" << std::endl;
            return false;
        }
        
        // Phase 3: Decode Huffman-encoded tokens
        BitReader bitReader(input);
        std::vector<LZ77Token> tokens;
        
        // Create bit getter lambda for decoder
        auto getBit = [&bitReader]() -> bool {
            return bitReader.readBit();
        };
        
        // Decode tokens until we have enough data
        size_t decodedBytes = 0;
        while (decodedBytes < originalSize && !bitReader.eof()) {
            int symbol = huffmanTree.decodeSymbol(getBit);
            
            if (symbol < 0) {
                std::cerr << "Error: Invalid Huffman code encountered" << std::endl;
                return false;
            }
            
            if (symbol < 256) {
                // Literal byte
                tokens.push_back(LZ77Token{0, 0, static_cast<uint8_t>(symbol)});
                ++decodedBytes;
            } else {
                // Length code (symbol 256-511 represents length 3-258)
                int length = symbol - 256 + 3;
                
                // Read offset (fixed 12 bits for now, supports up to 4096 window)
                uint32_t offset = bitReader.readBits(12);
                
                if (offset == 0 || offset > DEFAULT_SEARCH_WINDOW_SIZE) {
                    std::cerr << "Error: Invalid offset value" << std::endl;
                    return false;
                }
                
                tokens.push_back(LZ77Token{
                    static_cast<uint16_t>(offset),
                    static_cast<uint8_t>(length),
                    0
                });
                decodedBytes += length;
            }
        }
        
        // Phase 4: LZ77 decoding to reconstruct original data
        LZ77Decoder decoder(DEFAULT_SEARCH_WINDOW_SIZE);
        std::vector<uint8_t> decompressed = decoder.decode(tokens, originalSize);
        
        // Write output (truncate to exact original size)
        if (decompressed.size() > originalSize) {
            decompressed.resize(originalSize);
        }
        
        output.write(reinterpret_cast<const char*>(decompressed.data()), 
                     decompressed.size());
        
        return true;
    }
    
    /**
     * @brief Verify round-trip compression integrity
     * @param originalData Original data bytes
     * @return True if decompressed data matches original exactly
     * 
     * Time: O(N) for compress + decompress + compare
     */
    bool verifyRoundTrip(const std::vector<uint8_t>& originalData) {
        // Compress to memory
        std::vector<uint8_t> compressedData;
        {
            std::istringstream input(
                std::string(originalData.begin(), originalData.end()));
            std::ostringstream output;
            compress(input, output);
            
            std::string compressedStr = output.str();
            compressedData.assign(compressedStr.begin(), compressedStr.end());
        }
        
        // Decompress from memory
        std::vector<uint8_t> decompressedData;
        {
            std::istringstream input(
                std::string(compressedData.begin(), compressedData.end()));
            std::ostringstream output;
            if (!decompress(input, output)) {
                return false;
            }
            
            std::string decompressedStr = output.str();
            decompressedData.assign(decompressedStr.begin(), decompressedStr.end());
        }
        
        // Compare byte-by-byte
        if (decompressedData.size() != originalData.size()) {
            std::cerr << "Size mismatch: original=" << originalData.size() 
                      << ", decompressed=" << decompressedData.size() << std::endl;
            return false;
        }
        
        for (size_t i = 0; i < originalData.size(); ++i) {
            if (originalData[i] != decompressedData[i]) {
                std::cerr << "Byte mismatch at position " << i 
                          << ": original=" << static_cast<int>(originalData[i])
                          << ", decompressed=" << static_cast<int>(decompressedData[i])
                          << std::endl;
                return false;
            }
        }
        
        return true;
    }
    
private:
    /**
     * @brief Write file header to output stream
     * Time: O(K) where K is number of symbols
     */
    void writeHeader(std::ostream& output, uint64_t originalSize, 
                     const std::vector<uint8_t>& codeLengths) {
        // Magic bytes
        output.put(MAGIC_BYTE_1);
        output.put(MAGIC_BYTE_2);
        
        // Original size (little-endian)
        for (int i = 0; i < 8; ++i) {
            output.put(static_cast<char>((originalSize >> (i * 8)) & 0xFF));
        }
        
        // Number of symbols with non-zero code lengths
        uint16_t numSymbols = 0;
        for (uint8_t len : codeLengths) {
            if (len > 0) ++numSymbols;
        }
        
        // Write number of symbols (for quick parsing)
        output.put(static_cast<char>(codeLengths.size()));  // Symbol range
        
        // Write code lengths (sparse encoding could be added for optimization)
        // For now, write all lengths (including zeros)
        for (size_t i = 0; i < codeLengths.size(); ++i) {
            if (codeLengths[i] > 0) {
                output.put(static_cast<char>(i));       // Symbol
                output.put(static_cast<char>(codeLengths[i]));  // Length
            }
        }
        
        // Sentinel value to mark end of code length table
        output.put(static_cast<char>(0xFF));
    }
    
    /**
     * @brief Read and parse file header from input stream
     * Time: O(K) where K is number of symbols
     */
    bool readHeader(std::istream& input, uint64_t& originalSize, 
                    std::vector<uint8_t>& codeLengths) {
        // Initialize code lengths to zero
        codeLengths.assign(HEADER_SYMBOL_RANGE, 0);
        
        // Read magic bytes
        uint8_t magic1 = static_cast<uint8_t>(input.get());
        uint8_t magic2 = static_cast<uint8_t>(input.get());
        
        if (magic1 != MAGIC_BYTE_1 || magic2 != MAGIC_BYTE_2) {
            return false;
        }
        
        // Read original size (little-endian)
        originalSize = 0;
        for (int i = 0; i < 8; ++i) {
            uint64_t byte = static_cast<uint64_t>(static_cast<uint8_t>(input.get()));
            originalSize |= (byte << (i * 8));
        }
        
        // Read symbol range
        int symbolRange = input.get();
        if (symbolRange <= 0 || symbolRange > HEADER_SYMBOL_RANGE) {
            symbolRange = HEADER_SYMBOL_RANGE;
        }
        
        // Read code lengths until sentinel
        while (input.good()) {
            int symbol = input.get();
            if (symbol == 0xFF || symbol < 0) break;  // Sentinel or EOF
            
            int length = input.get();
            if (length < 0) break;
            
            if (symbol >= 0 && symbol < symbolRange) {
                codeLengths[symbol] = static_cast<uint8_t>(length);
            }
        }
        
        return true;
    }
    
    /**
     * @brief Encode LZ77 tokens using Huffman coding
     * Time: O(T * L) where T is token count, L is average code length
     */
    void encodeTokens(const std::vector<LZ77Token>& tokens, 
                      const HuffmanTree& huffmanTree,
                      BitWriter& bitWriter) {
        for (const auto& token : tokens) {
            if (token.isLiteral()) {
                // Encode literal byte
                HuffmanCode code = huffmanTree.getCode(token.literal);
                if (code.length > 0) {
                    bitWriter.writeBits(code.code, code.length);
                }
            } else if (token.isMatch()) {
                // Encode length code
                int lengthSymbol = 256 + (token.length - 3);
                HuffmanCode code = huffmanTree.getCode(lengthSymbol);
                if (code.length > 0) {
                    bitWriter.writeBits(code.code, code.length);
                }
                
                // Encode offset as fixed-length bits (12 bits for 4096 window)
                bitWriter.writeBits(token.offset, 12);
            }
        }
    }
    
    LZ77Encoder lz77Encoder_;
    LZ77Decoder lz77Decoder_;
};

} // namespace compress
