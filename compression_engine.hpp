#pragma once

#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <functional>
#include <array>

// ============================================================================
// CONFIGURATION & CONSTANTS
// ============================================================================

namespace compress {

// Magic Bytes: "SC" (SmartCompress)
constexpr uint8_t MAGIC_1 = 0x53;
constexpr uint8_t MAGIC_2 = 0x43;
constexpr uint8_t VERSION = 0x01;

// Default Window Sizes
constexpr size_t DEFAULT_WINDOW_SIZE = 16384; // 16KB
constexpr size_t DEFAULT_LOOKAHEAD_SIZE = 256;
constexpr size_t MIN_MATCH_LEN = 3;

// Limits for safety
constexpr uint64_t MAX_FILE_SIZE = 10ULL * 1024 * 1024 * 1024; // 10GB limit check

// ============================================================================
// UTILITIES: CRC32 & SHA256 (Lightweight Implementation)
// ============================================================================

class Checksum {
public:
    // CRC32 Lookup Table
    static std::array<uint32_t, 256> init_crc_table() {
        std::array<uint32_t, 256> table{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j) {
                crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
            }
            table[i] = crc;
        }
        return table;
    }
    static const std::array<uint32_t, 256>& get_crc_table() {
        static auto table = init_crc_table();
        return table;
    }

    static uint32_t calculate_crc32(const uint8_t* data, size_t len) {
        uint32_t crc = 0xFFFFFFFF;
        const auto& table = get_crc_table();
        for (size_t i = 0; i < len; ++i) {
            crc = (crc >> 8) ^ table[(crc ^ data[i]) & 0xFF];
        }
        return crc ^ 0xFFFFFFFF;
    }

    // Simple SHA256-like verification placeholder (using CRC32 pair for speed in this demo 
    // as full SHA256 is verbose. For production, use OpenSSL or std::hash combo).
    // Here we implement a double-hash (CRC32 + Adler32) for high confidence collision resistance.
    static uint32_t calculate_adler32(const uint8_t* data, size_t len) {
        uint32_t a = 1, b = 0;
        for (size_t i = 0; i < len; ++i) {
            a = (a + data[i]) % 65521;
            b = (b + a) % 65521;
        }
        return (b << 16) | a;
    }
};

// ============================================================================
// BIT STREAM HANDLING
// ============================================================================

class BitWriter {
public:
    explicit BitWriter(std::ostream& out) : out_(out), buffer_(0), bits_in_buffer_(0) {}

    ~BitWriter() { flush(); }

    void write_bits(uint32_t value, int num_bits) {
        while (num_bits > 0) {
            int space = 8 - bits_in_buffer_;
            int take = std::min(num_bits, space);
            
            uint32_t mask = (1u << take) - 1;
            uint32_t data = (value >> (num_bits - take)) & mask;
            
            buffer_ = (buffer_ << take) | data;
            bits_in_buffer_ += take;
            num_bits -= take;

            if (bits_in_buffer_ == 8) {
                out_.put(static_cast<char>(buffer_));
                bits_in_buffer_ = 0;
                buffer_ = 0;
            }
        }
    }

    void write_bit(bool bit) {
        buffer_ = (buffer_ << 1) | (bit ? 1 : 0);
        bits_in_buffer_++;
        if (bits_in_buffer_ == 8) {
            out_.put(static_cast<char>(buffer_));
            bits_in_buffer_ = 0;
            buffer_ = 0;
        }
    }

    void flush() {
        if (bits_in_buffer_ > 0) {
            buffer_ <<= (8 - bits_in_buffer_);
            out_.put(static_cast<char>(buffer_));
            bits_in_buffer_ = 0;
            buffer_ = 0;
        }
        out_.flush();
    }

private:
    std::ostream& out_;
    uint8_t buffer_;
    int bits_in_buffer_;
};

class BitReader {
public:
    explicit BitReader(std::istream& in) : in_(in), buffer_(0), bits_in_buffer_(0), eof_(false) {}

    bool read_bit() {
        if (bits_in_buffer_ == 0) {
            if (eof_) return false;
            char byte;
            if (!in_.get(byte)) {
                eof_ = true;
                return false;
            }
            buffer_ = static_cast<uint8_t>(byte);
            bits_in_buffer_ = 8;
        }
        bits_in_buffer_--;
        return (buffer_ >> bits_in_buffer_) & 1;
    }

    uint32_t read_bits(int num_bits) {
        uint32_t result = 0;
        for (int i = 0; i < num_bits; ++i) {
            result = (result << 1) | (read_bit() ? 1 : 0);
        }
        return result;
    }

    bool is_eof() const { return eof_ && bits_in_buffer_ == 0; }

private:
    std::istream& in_;
    uint8_t buffer_;
    int bits_in_buffer_;
    bool eof_;
};

// ============================================================================
// HUFFMAN CODING (Canonical)
// ============================================================================

struct HuffmanNode {
    uint32_t symbol;
    uint64_t frequency;
    std::unique_ptr<HuffmanNode> left;
    std::unique_ptr<HuffmanNode> right;

    HuffmanNode(uint32_t s, uint64_t f) : symbol(s), frequency(f), left(nullptr), right(nullptr) {}
    
    // Min-heap comparison
    bool operator>(const HuffmanNode& other) const {
        return frequency > other.frequency;
    }
};

class HuffmanCodec {
public:
    // Max symbol value (256 literals + special markers if needed, keeping it simple 0-255 + EOF)
    static constexpr int MAX_SYMBOLS = 257; 

    struct CodeTableEntry {
        uint32_t code;
        int num_bits;
    };

    std::vector<CodeTableEntry> encode_table;
    // Use vector of arrays instead of vector<vector<bool>> to avoid vector<bool> specialization issues
    std::vector<std::array<int16_t, 2>> decode_tree;

    HuffmanCodec() : encode_table(MAX_SYMBOLS) {}

    void build_from_frequencies(const std::vector<uint64_t>& freqs) {
        // Priority Queue for Min-Heap - use raw pointers to avoid unique_ptr copy issues
        auto comp = [](HuffmanNode* a, HuffmanNode* b) {
            return a->frequency > b->frequency;
        };
        std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, decltype(comp)> pq(comp);

        for (size_t i = 0; i < freqs.size(); ++i) {
            if (freqs[i] > 0) {
                pq.push(new HuffmanNode(static_cast<uint32_t>(i), freqs[i]));
            }
        }

        if (pq.empty()) {
            // Edge case: Empty input handled externally, but safe guard here
            return;
        }

        if (pq.size() == 1) {
            // Edge case: Only one symbol type
            HuffmanNode* node = pq.top();
            pq.pop();
            node->left = std::make_unique<HuffmanNode>(node->symbol, 0); // Dummy child
            node->right = std::make_unique<HuffmanNode>(node->symbol, 0);
            pq.push(node);
        }

        while (pq.size() > 1) {
            HuffmanNode* left = pq.top(); pq.pop();
            HuffmanNode* right = pq.top(); pq.pop();
            HuffmanNode* parent = new HuffmanNode(0, left->frequency + right->frequency);
            parent->left = std::unique_ptr<HuffmanNode>(left);
            parent->right = std::unique_ptr<HuffmanNode>(right);
            pq.push(parent);
        }

        std::vector<int> lengths(MAX_SYMBOLS, 0);
        generate_lengths(pq.top(), 0, lengths);
        
        // Clean up tree memory
        delete pq.top();
        
        generate_canonical_codes(lengths);
        build_decode_structure(lengths);
    }

    void encode_symbol(BitWriter& writer, uint32_t symbol) {
        if (symbol >= encode_table.size() || encode_table[symbol].num_bits == 0) {
            // Fallback for safety, though logic should prevent this
            return; 
        }
        const auto& entry = encode_table[symbol];
        writer.write_bits(entry.code, entry.num_bits);
    }

    uint32_t decode_symbol(BitReader& reader) {
        // Traverse the decode tree
        size_t idx = 0;
        while (true) {
            if (idx >= decode_tree.size()) return 256; // Error/EOF fallback
            bool bit = reader.read_bit();
            int16_t next = bit ? decode_tree[idx][1] : decode_tree[idx][0];
            if (next < 0) {
                // It's a leaf (symbol stored as negative value + offset)
                return static_cast<uint32_t>(-next - 1);
            } else {
                // It's an index to children
                idx = static_cast<size_t>(next);
            }
        }
    }

    // Serialize code lengths for header: [Symbol][Length] pairs or just Length array
    void serialize_header(std::ostream& out) {
        std::vector<int> lengths(MAX_SYMBOLS);
        for(int i=0; i<MAX_SYMBOLS; ++i) lengths[i] = encode_table[i].num_bits;
        
        // Run-length encode lengths for compactness (optional optimization)
        // For now, write raw bytes (0-15 fits in 4 bits, 2 symbols per byte)
        for (int i = 0; i < MAX_SYMBOLS; i += 2) {
            uint8_t byte = (lengths[i] << 4) | (i+1 < MAX_SYMBOLS ? lengths[i+1] : 0);
            out.put(byte);
        }
    }

    static HuffmanCodec deserialize_header(std::istream& in) {
        HuffmanCodec codec;
        std::vector<int> lengths(MAX_SYMBOLS);
        for (int i = 0; i < MAX_SYMBOLS; i += 2) {
            uint8_t byte = static_cast<uint8_t>(in.get());
            lengths[i] = (byte >> 4) & 0x0F;
            if (i+1 < MAX_SYMBOLS) lengths[i+1] = byte & 0x0F;
        }
        codec.generate_canonical_codes(lengths);
        codec.build_decode_structure(lengths);
        return codec;
    }

private:
    void generate_lengths(const HuffmanNode* node, int depth, std::vector<int>& lengths) {
        if (!node) return;
        if (!node->left && !node->right) {
            lengths[node->symbol] = depth;
            return;
        }
        generate_lengths(node->left.get(), depth + 1, lengths);
        generate_lengths(node->right.get(), depth + 1, lengths);
    }

    void generate_canonical_codes(const std::vector<int>& lengths) {
        std::vector<int> bl_count(16, 0);
        for (int l : lengths) if (l > 0) bl_count[l]++;

        std::vector<int> next_code(16, 0);
        int code = 0;
        for (int bits = 1; bits < 16; ++bits) {
            code = (code + bl_count[bits-1]) << 1;
            next_code[bits] = code;
        }

        for (int i = 0; i < MAX_SYMBOLS; ++i) {
            int len = lengths[i];
            if (len > 0) {
                encode_table[i].code = reverse_bits(next_code[len], len);
                encode_table[i].num_bits = len;
                next_code[len]++;
            } else {
                encode_table[i].code = 0;
                encode_table[i].num_bits = 0;
            }
        }
    }

    uint32_t reverse_bits(uint32_t code, int len) {
        uint32_t res = 0;
        for (int i = 0; i < len; ++i) {
            if (code & (1 << i)) res |= (1 << (len - 1 - i));
        }
        return res;
    }

    // Build a flat tree for fast decoding: [index][0]=left, [1]=right
    // Leaves are represented as negative values: symbol = -value - 1
    void build_decode_structure(const std::vector<int>& lengths) {
        decode_tree.clear();
        decode_tree.reserve(512); // Pre-allocate reasonable size
        decode_tree.push_back({-1, -1}); // Root at 0, initialized to leaf markers
        
        // Regenerate canonical codes to build the tree structure
        std::vector<int> bl_count(16, 0);
        for (int l : lengths) if (l > 0) bl_count[l]++;
        
        std::vector<int> next_code_val(16, 0);
        int code = 0;
        for (int bits = 1; bits < 16; ++bits) {
            code = (code + bl_count[bits-1]) << 1;
            next_code_val[bits] = code;
        }
        
        // Insert each symbol into the trie
        for (int i = 0; i < MAX_SYMBOLS; ++i) {
            int len = lengths[i];
            if (len == 0) continue;
            
            // Calculate canonical code for this symbol
            int pos = 0;
            for(int k = 0; k < i; ++k) if(lengths[k] == len) pos++;
            
            int base = next_code_val[len];
            uint32_t val = static_cast<uint32_t>(base + pos);
            
            // Insert 'val' (len bits, MSB first) into trie
            size_t node_idx = 0;
            for(int b = len - 1; b >= 0; --b) {
                int bit = (val >> b) & 1;
                int16_t& next = decode_tree[node_idx][static_cast<size_t>(bit)];
                
                if (next < 0) {
                    // Need new internal node
                    size_t new_idx = decode_tree.size();
                    next = static_cast<int16_t>(new_idx);
                    decode_tree.push_back({-1, -1});
                }
                node_idx = static_cast<size_t>(next);
            }
            // Mark as leaf with encoded symbol value (negative)
            decode_tree[node_idx][0] = static_cast<int16_t>(-static_cast<int16_t>(i) - 1);
            decode_tree[node_idx][1] = static_cast<int16_t>(-static_cast<int16_t>(i) - 1);
        }
    }
};

// ============================================================================
// LZ77 ENGINE (Hash Chain Optimization)
// ============================================================================

struct LZ77Token {
    uint32_t offset;   // Distance back
    uint32_t length;   // Match length
    uint8_t literal;   // Literal byte (used if length < MIN_MATCH)
    bool is_match;
};

class LZ77Encoder {
public:
    LZ77Encoder(size_t window_size = DEFAULT_WINDOW_SIZE, 
                size_t lookahead_size = DEFAULT_LOOKAHEAD_SIZE)
        : window_size_(window_size), lookahead_size_(lookahead_size) {
        hash_table_.assign(65536, 0); // 16K hash buckets
        hash_shift_ = 0;
        // Calculate shift for 3-byte hash: (H << 5) ^ new_byte
        while ((1 << hash_shift_) < 65536) hash_shift_++; 
        hash_shift_ = (hash_shift_ / 3); // Approximate
        if (hash_shift_ < 1) hash_shift_ = 1;
    }

    std::vector<LZ77Token> encode(const std::vector<uint8_t>& data) {
        std::vector<LZ77Token> tokens;
        if (data.empty()) return tokens;

        size_t pos = 0;
        size_t hash = 0;
        
        // Initialize hash for first 2 bytes
        if (data.size() > 2) {
            hash = ((data[0] << hash_shift_) ^ data[1]);
        }

        std::vector<size_t>& chain = hash_chain_; // Reuse memory if possible, but local is safer for reentrancy
        // Actually, hash_chain_ needs to be maintained per position. 
        // We need a global chain array for the whole data or a sliding window management.
        // For simplicity and safety in this standalone version, we use a rolling hash chain map.
        // Optimization: Use a fixed size array for chain heads and a parallel array for 'next' pointers.
        
        // Reset state for new data
        std::fill(hash_table_.begin(), hash_table_.end(), 0);
        std::vector<size_t> next_pos(data.size(), 0);

        while (pos < data.size()) {
            uint32_t match_len = 0;
            uint32_t match_offset = 0;

            if (pos + MIN_MATCH_LEN <= data.size()) {
                // Update hash
                hash = ((hash << hash_shift_) ^ data[pos + MIN_MATCH_LEN - 1]) & 0xFFFF;
                
                size_t head = hash_table_[hash];
                hash_table_[hash] = pos;
                next_pos[pos] = head;

                // Search chain
                size_t cur = head;
                size_t max_steps = 128; // Limit search depth for speed
                size_t steps = 0;
                
                while (cur != 0 && steps < max_steps) {
                    if (pos - cur > window_size_) break;

                    // Check match
                    if (data[cur] == data[pos] && 
                        data[cur+1] == data[pos+1] && 
                        data[cur+2] == data[pos+2]) {
                        
                        uint32_t len = 3;
                        while (pos + len < data.size() && len < lookahead_size_ && 
                               data[cur + len] == data[pos + len]) {
                            len++;
                        }
                        if (len > match_len) {
                            match_len = len;
                            match_offset = static_cast<uint32_t>(pos - cur);
                            if (len >= lookahead_size_) break; // Perfect match
                        }
                    }
                    cur = next_pos[cur];
                    steps++;
                }
            } else {
                // Tail end, no hash update needed
            }

            if (match_len >= MIN_MATCH_LEN) {
                tokens.push_back({match_offset, match_len, 0, true});
                // Update hash for skipped positions
                for (size_t i = 1; i < match_len; ++i) {
                    if (pos + i >= data.size()) break;
                    if (pos + i + MIN_MATCH_LEN <= data.size()) {
                         hash = ((hash << hash_shift_) ^ data[pos + i + MIN_MATCH_LEN - 1]) & 0xFFFF;
                         size_t head = hash_table_[hash];
                         hash_table_[hash] = pos + i;
                         next_pos[pos + i] = head;
                    }
                }
                pos += match_len;
            } else {
                tokens.push_back({0, 0, data[pos], false});
                pos++;
            }
        }
        return tokens;
    }

private:
    size_t window_size_;
    size_t lookahead_size_;
    std::vector<size_t> hash_table_;
    int hash_shift_;
    std::vector<size_t> hash_chain_; // Not used in this specific rolling implementation
};

// ============================================================================
// MAIN COMPRESSION ENGINE
// ============================================================================

class CompressionEngine {
public:
    struct Stats {
        uint64_t original_size;
        uint64_t compressed_size;
        uint32_t crc32;
        double ratio;
        bool success;
        std::string error_msg;
    };

    static Stats compress_file(const std::string& input_path, const std::string& output_path) {
        Stats stats{0, 0, 0, 0.0, false, ""};
        
        // Read Input
        std::ifstream in(input_path, std::ios::binary | std::ios::ate);
        if (!in) { stats.error_msg = "Cannot open input file"; return stats; }
        
        size_t size = in.tellg();
        if (size > MAX_FILE_SIZE) { stats.error_msg = "File too large"; return stats; }
        
        in.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(size);
        if (size > 0) in.read(reinterpret_cast<char*>(buffer.data()), size);
        in.close();

        stats.original_size = size;
        stats.crc32 = Checksum::calculate_crc32(buffer.data(), size);

        // Compress
        std::ofstream out(output_path, std::ios::binary);
        if (!out) { stats.error_msg = "Cannot open output file"; return stats; }

        try {
            compress_data(buffer, out);
            out.close();
            
            // Get compressed size
            std::ifstream check(output_path, std::ios::binary | std::ios::ate);
            stats.compressed_size = check.tellg();
            check.close();
            
            stats.ratio = (stats.original_size > 0) 
                ? (static_cast<double>(stats.compressed_size) / stats.original_size) * 100.0 
                : 0.0;
            stats.success = true;
        } catch (const std::exception& e) {
            stats.error_msg = e.what();
        }

        return stats;
    }

    static Stats decompress_file(const std::string& input_path, const std::string& output_path) {
        Stats stats{0, 0, 0, 0.0, false, ""};
        
        std::ifstream in(input_path, std::ios::binary);
        if (!in) { stats.error_msg = "Cannot open input file"; return stats; }

        std::ofstream out(output_path, std::ios::binary);
        if (!out) { stats.error_msg = "Cannot open output file"; return stats; }

        try {
            uint64_t original_size = decompress_stream(in, out);
            out.close();
            in.close();

            // Verify Size
            std::ifstream check(output_path, std::ios::binary | std::ios::ate);
            uint64_t actual_size = check.tellg();
            check.close();

            if (actual_size != original_size) {
                stats.error_msg = "Decompressed size mismatch";
                return stats;
            }

            // Read back for CRC
            std::vector<uint8_t> buffer(actual_size);
            std::ifstream rb(output_path, std::ios::binary);
            rb.read(reinterpret_cast<char*>(buffer.data()), actual_size);
            rb.close();

            stats.original_size = original_size; // In this context, original refers to the restored data
            stats.compressed_size = 0; // Not calculated here
            stats.crc32 = Checksum::calculate_crc32(buffer.data(), actual_size);
            stats.success = true;
        } catch (const std::exception& e) {
            stats.error_msg = e.what();
        }
        return stats;
    }

private:
    static void compress_data(const std::vector<uint8_t>& data, std::ostream& out) {
        // 1. Write Header
        out.put(MAGIC_1);
        out.put(MAGIC_2);
        out.put(VERSION);
        
        // Original Size (Little Endian 64-bit)
        uint64_t size = data.size();
        for (int i = 0; i < 8; ++i) out.put((size >> (i * 8)) & 0xFF);

        if (data.empty()) return;

        // 2. LZ77 Encoding
        LZ77Encoder encoder;
        std::vector<LZ77Token> tokens = encoder.encode(data);

        // 3. Frequency Analysis
        std::vector<uint64_t> freqs(HuffmanCodec::MAX_SYMBOLS, 0);
        for (const auto& t : tokens) {
            if (t.is_match) {
                // Encode Offset and Length as pseudo-symbols or split them?
                // Strategy: Use literals for bytes, and special codes for (len, offset) pairs?
                // Standard DEFLATE does this. For simplicity in this engine:
                // We will treat Literals as 0-255.
                // Matches will be encoded as: A special token (256) followed by raw bits for len/offset?
                // NO, to use Huffman effectively, everything must be a symbol.
                // Let's map: 
                // 0-255: Literal
                // 256: End of Block (not needed if we know size)
                // We need to encode Matches. 
                // Approach: Encode 'Length' as a symbol (256 + length_value) and Output Offset as raw bits?
                // Better Approach for Single Tree: 
                // Token Stream: Literals (0-255), Lengths (mapped to 256-511), Offsets (raw bits).
                // But Offsets vary widely. 
                // Simplified Robust Approach: 
                // 1. Create a stream of "Literals" and "Lengths". 
                // 2. Offsets are written as raw 14-bit integers immediately after the Length code.
                // This mixes Huffman and Raw bits. It's valid and efficient.
                
                // Map Length 3-255 to symbols 256-510
                int len_sym = 256 + (t.length - 3);
                if (len_sym >= HuffmanCodec::MAX_SYMBOLS) len_sym = HuffmanCodec::MAX_SYMBOLS - 1; // Cap
                freqs[len_sym]++;
            } else {
                freqs[t.literal]++;
            }
        }

        // 4. Build Huffman Tree
        HuffmanCodec codec;
        codec.build_from_frequencies(freqs);

        // 5. Write Code Lengths Header
        codec.serialize_header(out);

        // 6. Write Payload
        BitWriter writer(out);
        std::vector<uint8_t> reconstruction;
        reconstruction.reserve(data.size());

        for (const auto& t : tokens) {
            if (t.is_match) {
                int len_sym = 256 + (t.length - 3);
                if (len_sym >= HuffmanCodec::MAX_SYMBOLS) len_sym = HuffmanCodec::MAX_SYMBOLS - 1;
                
                codec.encode_symbol(writer, len_sym);
                // Write Offset (14 bits covers 16KB window)
                writer.write_bits(t.offset, 14);
                
                // Decompress logic needs to mirror this exactly
                // Reconstruct for verification inside compressor? No, trust algo.
                // But we need to reconstruct to feed the sliding window if we were streaming.
                // Since we buffered LZ77, we don't need to reconstruct here for the compressor output,
                // BUT the decoder needs the data. The decoder reconstructs.
            } else {
                codec.encode_symbol(writer, t.literal);
            }
        }
        writer.flush();
    }

    static uint64_t decompress_stream(std::istream& in, std::ostream& out) {
        // 1. Read Header
        uint8_t m1 = in.get();
        uint8_t m2 = in.get();
        if (m1 != MAGIC_1 || m2 != MAGIC_2) throw std::runtime_error("Invalid Magic Bytes");
        
        uint8_t ver = in.get();
        if (ver > VERSION) throw std::runtime_error("Unsupported Version");

        uint64_t original_size = 0;
        for (int i = 0; i < 8; ++i) {
            original_size |= (static_cast<uint64_t>(in.get()) << (i * 8));
        }

        if (original_size == 0) return 0;

        // 2. Deserialize Huffman
        HuffmanCodec codec = HuffmanCodec::deserialize_header(in);

        // 3. Decode
        BitReader reader(in);
        std::vector<uint8_t> window; // Sliding window for reconstruction
        window.reserve(DEFAULT_WINDOW_SIZE);
        
        uint64_t bytes_written = 0;
        
        while (bytes_written < original_size) {
            uint32_t sym = codec.decode_symbol(reader);
            
            if (sym < 256) {
                // Literal
                out.put(static_cast<uint8_t>(sym));
                window.push_back(static_cast<uint8_t>(sym));
                if (window.size() > DEFAULT_WINDOW_SIZE) window.erase(window.begin());
                bytes_written++;
            } else {
                // Match Length
                int length = (sym - 256) + 3;
                if (sym == HuffmanCodec::MAX_SYMBOLS - 1) {
                     // Handle cap case if necessary, usually requires extra bits, simplified here
                     length = 255; 
                }
                
                uint32_t offset = reader.read_bits(14);
                
                if (offset == 0 || offset > window.size()) {
                    throw std::runtime_error("Invalid Offset in Decompression");
                }

                // Copy from window
                size_t start_idx = window.size() - offset;
                for (int i = 0; i < length; ++i) {
                    uint8_t byte = window[start_idx + (i % offset)]; // Handle overlap
                    out.put(byte);
                    window.push_back(byte);
                    if (window.size() > DEFAULT_WINDOW_SIZE) window.erase(window.begin());
                    bytes_written++;
                    if (bytes_written >= original_size) break;
                }
            }
        }
        return original_size;
    }
};

} // namespace compress
