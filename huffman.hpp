/**
 * @file huffman.hpp
 * @brief Canonical Huffman coding implementation
 * 
 * Implements frequency-based variable-length prefix coding.
 * Uses canonical Huffman codes for efficient serialization.
 * 
 * Time Complexity: O(K log K) for tree construction where K is alphabet size
 *                  O(N) for encoding/decoding where N is data size
 * Space Complexity: O(K) for code tables where K is alphabet size
 */

#pragma once

#include <cstdint>
#include <vector>
#include <queue>
#include <array>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

// Forward declaration - LZ77Token is defined in lz77.hpp
namespace compress {
    struct LZ77Token;
}

namespace compress {

// Maximum number of symbols (literals + length codes + offset codes)
constexpr int MAX_SYMBOLS = 512;
constexpr int MAX_CODE_BITS = 16;

/**
 * @struct HuffmanNode
 * @brief Node in the Huffman tree
 */
struct HuffmanNode {
    int symbol;                     // Symbol value (-1 for internal nodes)
    uint32_t frequency;             // Frequency count
    std::shared_ptr<HuffmanNode> left;   // Left child (bit 0)
    std::shared_ptr<HuffmanNode> right;  // Right child (bit 1)
    
    HuffmanNode(int sym, uint32_t freq)
        : symbol(sym), frequency(freq), left(nullptr), right(nullptr) {}
    
    bool isLeaf() const { return left == nullptr && right == nullptr; }
};

/**
 * @struct HuffmanCode
 * @brief Represents a Huffman code for a symbol
 */
struct HuffmanCode {
    uint32_t code;      // The actual bit pattern
    uint8_t length;     // Number of bits in the code
    
    HuffmanCode() : code(0), length(0) {}
    HuffmanCode(uint32_t c, uint8_t l) : code(c), length(l) {}
};

/**
 * @class HuffmanTree
 * @brief Builds and manages Huffman coding trees
 * 
 * Supports both standard and canonical Huffman codes.
 * Canonical codes are used for compact header serialization.
 */
class HuffmanTree {
public:
    /**
     * @brief Construct empty Huffman tree
     */
    HuffmanTree() : root_(nullptr) {}
    
    /**
     * @brief Build Huffman tree from frequency table
     * @param frequencies Array of symbol frequencies
     * @param numSymbols Number of symbols in the frequency table
     * 
     * Time: O(K log K) where K is number of symbols
     * Space: O(K) for tree nodes
     */
    void buildFromFrequencies(const uint32_t* frequencies, int numSymbols) {
        // Clear previous tree
        root_ = nullptr;
        codes_.clear();
        decodeTable_.clear();
        
        // Create leaf nodes for all symbols with non-zero frequency
        using NodePtr = std::shared_ptr<HuffmanNode>;
        using NodeQueue = std::priority_queue<
            NodePtr,
            std::vector<NodePtr>,
            decltype([](const NodePtr& a, const NodePtr& b) {
                return a->frequency > b->frequency;  // Min-heap
            })
        >;
        
        NodeQueue pq;
        
        for (int i = 0; i < numSymbols; ++i) {
            if (frequencies[i] > 0) {
                pq.push(std::make_shared<HuffmanNode>(i, frequencies[i]));
            }
        }
        
        // Handle edge case: single symbol
        if (pq.size() == 1) {
            NodePtr single = pq.top();
            pq.pop();
            root_ = std::make_shared<HuffmanNode>(-1, single->frequency);
            root_->left = single;
            // Assign code of length 1 to the single symbol
            codes_[single->symbol] = HuffmanCode(0, 1);
            return;
        }
        
        // Build tree by repeatedly combining two lowest-frequency nodes
        while (pq.size() > 1) {
            NodePtr left = pq.top();
            pq.pop();
            NodePtr right = pq.top();
            pq.pop();
            
            NodePtr parent = std::make_shared<HuffmanNode>(-1, left->frequency + right->frequency);
            parent->left = left;
            parent->right = right;
            
            pq.push(parent);
        }
        
        if (!pq.empty()) {
            root_ = pq.top();
        }
        
        // Generate codes from tree
        if (root_) {
            generateCodes(root_, 0, 0);
            buildDecodeTable();
        }
    }
    
    /**
     * @brief Build tree from canonical code lengths
     * @param codeLengths Array of code lengths for each symbol
     * @param numSymbols Number of symbols
     * 
     * Time: O(K) where K is number of symbols
     * Space: O(K) for tree and tables
     */
    void buildFromCodeLengths(const uint8_t* codeLengths, int numSymbols) {
        root_ = nullptr;
        codes_.clear();
        decodeTable_.clear();
        
        // Count symbols at each code length
        std::array<int, MAX_CODE_BITS + 1> lengthCount{};
        int totalSymbols = 0;
        for (int i = 0; i < numSymbols; ++i) {
            if (codeLengths[i] > 0 && codeLengths[i] <= MAX_CODE_BITS) {
                ++lengthCount[codeLengths[i]];
                ++totalSymbols;
            }
        }
        
        // Handle edge case: no symbols or single symbol
        if (totalSymbols == 0) {
            return;  // Empty tree
        }
        
        if (totalSymbols == 1) {
            // Find the single symbol
            for (int i = 0; i < numSymbols; ++i) {
                if (codeLengths[i] > 0) {
                    codes_[i] = HuffmanCode(0, 1);
                    break;
                }
            }
            buildDecodeTable();
            return;
        }
        
        // Calculate starting code for each length (canonical property)
        std::array<uint32_t, MAX_CODE_BITS + 1> nextCode{};
        uint32_t code = 0;
        for (int len = 1; len <= MAX_CODE_BITS; ++len) {
            code <<= 1;
            nextCode[len] = code;
            code += lengthCount[len];
        }
        
        // Assign codes to symbols
        for (int sym = 0; sym < numSymbols; ++sym) {
            uint8_t len = codeLengths[sym];
            if (len > 0 && len <= MAX_CODE_BITS) {
                codes_[sym] = HuffmanCode(nextCode[len]++, len);
            }
        }
        
        // Build decode table for fast decoding
        buildDecodeTable();
    }
    
    /**
     * @brief Get the Huffman code for a symbol
     * @param symbol The symbol to encode
     * @return The Huffman code, or invalid code if symbol not in tree
     * 
     * Time: O(1)
     */
    HuffmanCode getCode(int symbol) const {
        auto it = codes_.find(symbol);
        if (it != codes_.end()) {
            return it->second;
        }
        return HuffmanCode(0, 0);  // Invalid code
    }
    
    /**
     * @brief Decode bits to find the corresponding symbol
     * @param getBit Function that returns the next bit
     * @return The decoded symbol, or -1 if no valid code found
     * 
     * Time: O(L) where L is code length (typically <= 16)
     */
    template<typename BitGetter>
    int decodeSymbol(BitGetter getBit) const {
        if (decodeTable_.empty()) {
            return -1;
        }
        
        // Use decode table for fast lookup
        // Read up to MAX_CODE_BITS and look up in table
        uint32_t bits = 0;
        int bitsRead = 0;
        
        for (int len = 1; len <= MAX_CODE_BITS; ++len) {
            bits = (bits << 1) | (getBit() ? 1 : 0);
            ++bitsRead;
            
            // Check if we have a complete code
            auto it = decodeTable_.find((len << 24) | bits);
            if (it != decodeTable_.end()) {
                return it->second;
            }
        }
        
        return -1;  // No valid code found
    }
    
    /**
     * @brief Get code lengths for all symbols (for canonical serialization)
     * @param numSymbols Maximum number of symbols to check
     * @return Vector of code lengths
     * 
     * Time: O(K) where K is number of symbols
     */
    std::vector<uint8_t> getCodeLengths(int numSymbols) const {
        std::vector<uint8_t> lengths(numSymbols, 0);
        for (int i = 0; i < numSymbols; ++i) {
            auto it = codes_.find(i);
            if (it != codes_.end()) {
                lengths[i] = it->second.length;
            }
        }
        return lengths;
    }
    
    /**
     * @brief Check if tree is valid (has codes)
     */
    bool isValid() const { return !codes_.empty(); }
    
    /**
     * @brief Get number of symbols in the tree
     */
    size_t symbolCount() const { return codes_.size(); }
    
    /**
     * @brief Check if tree can decode at least one symbol
     */
    bool canDecode() const { return !decodeTable_.empty(); }
    
private:
    /**
     * @brief Recursively generate codes from tree
     * Time: O(K) where K is number of leaf nodes
     */
    void generateCodes(std::shared_ptr<HuffmanNode> node, uint32_t code, int depth) {
        if (!node) return;
        
        if (node->isLeaf()) {
            if (depth > 0) {  // Don't assign code to root
                codes_[node->symbol] = HuffmanCode(code, depth);
            }
            return;
        }
        
        // Left branch = 0, Right branch = 1
        generateCodes(node->left, code << 1, depth + 1);
        generateCodes(node->right, (code << 1) | 1, depth + 1);
    }
    
    /**
     * @brief Build reverse lookup table for decoding
     * Maps (length, code) -> symbol
     * Time: O(K) where K is number of symbols
     */
    void buildDecodeTable() {
        decodeTable_.clear();
        for (const auto& [symbol, huffCode] : codes_) {
            // Store as (length << 24) | code for efficient lookup
            uint32_t key = (static_cast<uint32_t>(huffCode.length) << 24) | huffCode.code;
            decodeTable_[key] = symbol;
        }
    }
    
    std::shared_ptr<HuffmanNode> root_;
    std::unordered_map<int, HuffmanCode> codes_;
    std::unordered_map<uint32_t, int> decodeTable_;
};

/**
 * @class FrequencyCounter
 * @brief Counts symbol frequencies for Huffman analysis
 * 
 * Time Complexity: O(N) for counting where N is data size
 * Space Complexity: O(K) where K is alphabet size
 */
class FrequencyCounter {
public:
    /**
     * @brief Construct counter with specified alphabet size
     */
    explicit FrequencyCounter(int alphabetSize = MAX_SYMBOLS)
        : frequencies_(alphabetSize, 0) {}
    
    /**
     * @brief Count frequencies in input data
     * @param data Pointer to input bytes
     * @param size Number of bytes
     * 
     * Time: O(N) where N is data size
     */
    void count(const uint8_t* data, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            if (data[i] < frequencies_.size()) {
                ++frequencies_[data[i]];
            }
        }
    }
    
    /**
     * @brief Count frequencies from LZ77 tokens
     * @param tokens Vector of LZ77 tokens
     * 
     * Encodes literals, lengths, and offsets as separate symbol ranges:
     * - 0-255: Literal bytes
     * - 256-511: Length codes (offset by 256)
     * - 512+: Offset codes (not used in basic implementation)
     * 
     * Time: O(T) where T is number of tokens
     */
    void countTokens(const std::vector<LZ77Token>& tokens) {
        for (const auto& token : tokens) {
            if (token.isLiteral()) {
                // Literal byte: symbol 0-255
                ensureCapacity(token.literal);
                ++frequencies_[token.literal];
            } else if (token.isMatch()) {
                // Length code: symbol 256-511 (length values 3-258 mapped to 0-255)
                int lengthSymbol = 256 + (token.length - 3);
                if (lengthSymbol >= 0 && lengthSymbol < 512) {
                    ensureCapacity(lengthSymbol);
                    ++frequencies_[lengthSymbol];
                }
                
                // For simplicity, we encode offset separately or use fixed bits
                // In a full implementation, offsets would also be Huffman-coded
            }
        }
    }
    
    /**
     * @brief Get frequency array
     */
    const std::vector<uint32_t>& frequencies() const { return frequencies_; }
    
    /**
     * @brief Reset all frequencies to zero
     */
    void reset() {
        std::fill(frequencies_.begin(), frequencies_.end(), 0);
    }
    
private:
    void ensureCapacity(int index) {
        if (index >= static_cast<int>(frequencies_.size())) {
            frequencies_.resize(index + 1, 0);
        }
    }
    
    std::vector<uint32_t> frequencies_;
};

} // namespace compress
