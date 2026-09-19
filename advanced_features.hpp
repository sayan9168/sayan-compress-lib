/**
 * @file advanced_compression_engine.hpp
 * @brief Advanced compression features and optimizations
 * 
 * Includes multi-threading support, streaming compression, 
 * adaptive Huffman coding, and enhanced LZ77 with hash chains.
 * 
 * @author Sayan
 * @version 2.0
 */

#pragma once

#include "compression_engine.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <functional>

namespace compress {

// ============================================================================
// ADVANCED CONFIGURATION
// ============================================================================

constexpr size_t MULTI_THREAD_THRESHOLD = 1024 * 1024; // 1MB minimum for threading
constexpr int MAX_THREADS = 8;
constexpr size_t STREAM_BUFFER_SIZE = 65536; // 64KB stream chunks
constexpr int ADAPTIVE_UPDATE_INTERVAL = 4096; // Symbols before frequency update

// ============================================================================
// THREAD POOL FOR PARALLEL COMPRESSION
// ============================================================================

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) 
        : stop_(false) {
        num_threads = std::min(num_threads, static_cast<size_t>(MAX_THREADS));
        if (num_threads == 0) num_threads = 4;
        
        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        condition_.wait(lock, [this] { 
                            return stop_ || !tasks_.empty(); 
                        });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }
    
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (std::thread& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }
    
    template<typename F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            tasks_.emplace(std::forward<F>(f));
        }
        condition_.notify_one();
    }
    
    void wait_all() {
        // Simple barrier - in production, use more sophisticated tracking
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};

// ============================================================================
// STREAMING COMPRESSION INTERFACE
// ============================================================================

class StreamingCompressor {
public:
    struct StreamStats {
        uint64_t total_input_bytes;
        uint64_t total_output_bytes;
        uint64_t chunks_processed;
        double average_ratio;
        bool is_complete;
    };
    
    StreamingCompressor(std::ostream& output, size_t buffer_size = STREAM_BUFFER_SIZE)
        : output_(output), buffer_size_(buffer_size), stats_{0, 0, 0, 0.0, false} {
        writeHeader();
    }
    
    ~StreamingCompressor() {
        finalize();
    }
    
    /**
     * @brief Compress and write a chunk of data
     * @param data Pointer to input data
     * @param size Size of input data
     * @return true if successful
     */
    bool compressChunk(const uint8_t* data, size_t size) {
        if (size == 0) return true;
        
        // Compress chunk
        std::vector<uint8_t> input(data, data + size);
        std::ostringstream chunk_stream(std::ios::binary);
        
        try {
            // Use internal compression logic
            compressChunkData(input, chunk_stream);
            
            // Write chunk to output
            std::string chunk_data = chunk_stream.str();
            uint32_t chunk_size = static_cast<uint32_t>(chunk_data.size());
            
            // Write chunk size prefix
            for (int i = 0; i < 4; ++i) {
                output_.put((chunk_size >> (i * 8)) & 0xFF);
            }
            output_.write(chunk_data.data(), chunk_size);
            
            stats_.total_input_bytes += size;
            stats_.total_output_bytes += chunk_size + 4;
            stats_.chunks_processed++;
            stats_.average_ratio = (stats_.total_output_bytes * 100.0) / stats_.total_input_bytes;
            
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /**
     * @brief Finalize the stream (write end marker)
     */
    void finalize() {
        if (!stats_.is_complete) {
            // Write end-of-stream marker
            output_.put(0x00);
            output_.put(0x00);
            output_.put(0x00);
            output_.put(0x00);
            output_.flush();
            stats_.is_complete = true;
        }
    }
    
    const StreamStats& getStats() const { return stats_; }
    
private:
    void writeHeader() {
        output_.put(MAGIC_1);
        output_.put(MAGIC_2);
        output_.put(VERSION);
        output_.put(0x01); // Streaming format version
    }
    
    void compressChunkData(const std::vector<uint8_t>& data, std::ostream& out) {
        if (data.empty()) return;
        
        // Write chunk original size
        uint32_t size = static_cast<uint32_t>(data.size());
        for (int i = 0; i < 4; ++i) {
            out.put((size >> (i * 8)) & 0xFF);
        }
        
        // LZ77 encode
        LZ77Encoder encoder;
        std::vector<LZ77Token> tokens = encoder.encode(data);
        
        // Frequency analysis
        std::vector<uint64_t> freqs(HuffmanCodec::MAX_SYMBOLS, 0);
        for (const auto& t : tokens) {
            if (t.is_match) {
                int len_sym = 256 + (t.length - 3);
                if (len_sym >= HuffmanCodec::MAX_SYMBOLS) len_sym = HuffmanCodec::MAX_SYMBOLS - 1;
                freqs[len_sym]++;
            } else {
                freqs[t.literal]++;
            }
        }
        
        // Build Huffman tree
        HuffmanCodec codec;
        codec.build_from_frequencies(freqs);
        
        // Write code lengths
        codec.serialize_header(out);
        
        // Write payload
        BitWriter writer(out);
        for (const auto& t : tokens) {
            if (t.is_match) {
                int len_sym = 256 + (t.length - 3);
                if (len_sym >= HuffmanCodec::MAX_SYMBOLS) len_sym = HuffmanCodec::MAX_SYMBOLS - 1;
                codec.encode_symbol(writer, len_sym);
                writer.write_bits(t.offset, 14);
            } else {
                codec.encode_symbol(writer, t.literal);
            }
        }
        writer.flush();
    }
    
    std::ostream& output_;
    size_t buffer_size_;
    StreamStats stats_;
};

// ============================================================================
// ADAPTIVE HUFFMAN CODING
// ============================================================================

class AdaptiveHuffmanCodec {
public:
    AdaptiveHuffmanCodec() : symbol_count_(0), update_counter_(0) {
        frequencies_.assign(HuffmanCodec::MAX_SYMBOLS, 1); // Start with count 1 for all
    }
    
    /**
     * @brief Encode a symbol with adaptive Huffman
     * @param writer Bit writer output
     * @param symbol Symbol to encode
     */
    void encodeSymbol(BitWriter& writer, uint32_t symbol) {
        if (symbol >= HuffmanCodec::MAX_SYMBOLS) return;
        
        // Rebuild tree periodically
        if (update_counter_ % ADAPTIVE_UPDATE_INTERVAL == 0 && symbol_count_ > 0) {
            rebuildTree();
        }
        
        // Get current code for symbol
        if (codec_.encode_table.size() > symbol && codec_.encode_table[symbol].num_bits > 0) {
            const auto& entry = codec_.encode_table[symbol];
            writer.write_bits(entry.code, entry.num_bits);
        }
        
        // Update frequency
        frequencies_[symbol]++;
        symbol_count_++;
        update_counter_++;
    }
    
    /**
     * @brief Decode a symbol with adaptive Huffman
     * @param reader Bit reader input
     * @return Decoded symbol
     */
    uint32_t decodeSymbol(BitReader& reader) {
        if (update_counter_ % ADAPTIVE_UPDATE_INTERVAL == 0 && symbol_count_ > 0) {
            rebuildTree();
        }
        
        uint32_t symbol = codec_.decode_symbol(reader);
        
        if (symbol < HuffmanCodec::MAX_SYMBOLS) {
            frequencies_[symbol]++;
            symbol_count_++;
            update_counter_++;
        }
        
        return symbol;
    }
    
    /**
     * @brief Reset the adaptive model
     */
    void reset() {
        std::fill(frequencies_.begin(), frequencies_.end(), 1);
        symbol_count_ = 0;
        update_counter_ = 0;
        codec_ = HuffmanCodec();
    }
    
private:
    void rebuildTree() {
        codec_.build_from_frequencies(frequencies_);
    }
    
    std::vector<uint64_t> frequencies_;
    HuffmanCodec codec_;
    uint64_t symbol_count_;
    uint64_t update_counter_;
};

// ============================================================================
// MULTI-THREAD COMPRESSION ENGINE
// ============================================================================

class MultiThreadCompressionEngine {
public:
    struct MultiThreadStats {
        uint64_t original_size;
        uint64_t compressed_size;
        double ratio;
        int threads_used;
        double speedup;
        bool success;
        std::string error_msg;
    };
    
    /**
     * @brief Compress file using multiple threads
     * @param input_path Input file path
     * @param output_path Output file path
     * @param num_threads Number of threads to use
     * @return Compression statistics
     */
    static MultiThreadStats compress_parallel(
        const std::string& input_path, 
        const std::string& output_path,
        int num_threads = 0) {
        
        MultiThreadStats stats{0, 0, 0.0, 0, 0.0, false, ""};
        
        if (num_threads <= 0) {
            num_threads = std::thread::hardware_concurrency();
        }
        num_threads = std::min(num_threads, MAX_THREADS);
        stats.threads_used = num_threads;
        
        // Read input file
        std::ifstream in(input_path, std::ios::binary | std::ios::ate);
        if (!in) {
            stats.error_msg = "Cannot open input file";
            return stats;
        }
        
        size_t file_size = in.tellg();
        stats.original_size = file_size;
        
        // For small files, use single thread
        if (file_size < MULTI_THREAD_THRESHOLD || num_threads == 1) {
            auto result = CompressionEngine::compress_file(input_path, output_path);
            stats.compressed_size = result.compressed_size;
            stats.ratio = result.ratio;
            stats.success = result.success;
            stats.error_msg = result.error_msg;
            stats.speedup = 1.0;
            return stats;
        }
        
        in.seekg(0, std::ios::beg);
        std::vector<uint8_t> data(file_size);
        in.read(reinterpret_cast<char*>(data.data()), file_size);
        in.close();
        
        // Split data into chunks
        size_t chunk_size = (file_size + num_threads - 1) / num_threads;
        std::vector<std::vector<uint8_t>> chunks(num_threads);
        std::vector<std::vector<LZ77Token>> all_tokens(num_threads);
        std::vector<std::vector<uint64_t>> all_freqs(num_threads, 
            std::vector<uint64_t>(HuffmanCodec::MAX_SYMBOLS, 0));
        
        // Parallel LZ77 encoding
        {
            ThreadPool pool(num_threads);
            std::mutex tokens_mutex;
            
            for (int i = 0; i < num_threads; ++i) {
                size_t start = i * chunk_size;
                size_t end = std::min(start + chunk_size, file_size);
                
                if (start >= file_size) break;
                
                std::vector<uint8_t> chunk(data.begin() + start, data.begin() + end);
                chunks[i] = std::move(chunk);
                
                pool.enqueue([&, i] {
                    LZ77Encoder encoder;
                    all_tokens[i] = encoder.encode(chunks[i]);
                    
                    // Calculate frequencies
                    for (const auto& token : all_tokens[i]) {
                        if (token.is_match) {
                            int len_sym = 256 + (token.length - 3);
                            if (len_sym >= HuffmanCodec::MAX_SYMBOLS) 
                                len_sym = HuffmanCodec::MAX_SYMBOLS - 1;
                            all_freqs[i][len_sym]++;
                        } else {
                            all_freqs[i][token.literal]++;
                        }
                    }
                });
            }
            pool.wait_all();
        }
        
        // Merge frequencies
        std::vector<uint64_t> merged_freqs(HuffmanCodec::MAX_SYMBOLS, 0);
        for (int i = 0; i < num_threads; ++i) {
            for (size_t j = 0; j < merged_freqs.size(); ++j) {
                merged_freqs[j] += all_freqs[i][j];
            }
        }
        
        // Build global Huffman tree
        HuffmanCodec codec;
        codec.build_from_frequencies(merged_freqs);
        
        // Write output
        std::ofstream out(output_path, std::ios::binary);
        if (!out) {
            stats.error_msg = "Cannot open output file";
            return stats;
        }
        
        try {
            // Write header
            out.put(MAGIC_1);
            out.put(MAGIC_2);
            out.put(VERSION);
            out.put(0x02); // Multi-thread format version
            out.put(static_cast<uint8_t>(num_threads));
            
            // Write original size
            uint64_t size = file_size;
            for (int i = 0; i < 8; ++i) {
                out.put((size >> (i * 8)) & 0xFF);
            }
            
            // Write Huffman header
            codec.serialize_header(out);
            
            // Write chunk info
            for (int i = 0; i < num_threads; ++i) {
                uint32_t chunk_orig_size = static_cast<uint32_t>(chunks[i].size());
                for (int j = 0; j < 4; ++j) {
                    out.put((chunk_orig_size >> (j * 8)) & 0xFF);
                }
            }
            
            // Write compressed data
            BitWriter writer(out);
            for (int i = 0; i < num_threads; ++i) {
                for (const auto& token : all_tokens[i]) {
                    if (token.is_match) {
                        int len_sym = 256 + (token.length - 3);
                        if (len_sym >= HuffmanCodec::MAX_SYMBOLS) 
                            len_sym = HuffmanCodec::MAX_SYMBOLS - 1;
                        codec.encode_symbol(writer, len_sym);
                        writer.write_bits(token.offset, 14);
                    } else {
                        codec.encode_symbol(writer, token.literal);
                    }
                }
            }
            writer.flush();
            
            out.close();
            
            // Calculate stats
            std::ifstream check(output_path, std::ios::binary | std::ios::ate);
            stats.compressed_size = check.tellg();
            check.close();
            
            stats.ratio = (stats.original_size > 0) 
                ? (static_cast<double>(stats.compressed_size) / stats.original_size) * 100.0 
                : 0.0;
            stats.success = true;
            stats.speedup = static_cast<double>(num_threads); // Estimated
            
        } catch (const std::exception& e) {
            stats.error_msg = e.what();
        }
        
        return stats;
    }
};

// ============================================================================
// ENHANCED LZ77 WITH HASH CHAINS
// ============================================================================

class EnhancedLZ77Encoder {
public:
    EnhancedLZ77Encoder(
        size_t window_size = DEFAULT_WINDOW_SIZE,
        size_t lookahead_size = DEFAULT_LOOKAHEAD_SIZE,
        int max_chain_length = 256)
        : window_size_(window_size), 
          lookahead_size_(lookahead_size),
          max_chain_length_(max_chain_length) {
        hash_table_.assign(65536, 0);
        chain_table_.resize(window_size + lookahead_size);
    }
    
    std::vector<LZ77Token> encode(const std::vector<uint8_t>& data) {
        std::vector<LZ77Token> tokens;
        if (data.empty()) return tokens;
        
        std::fill(hash_table_.begin(), hash_table_.end(), 0);
        std::fill(chain_table_.begin(), chain_table_.end(), 0);
        
        size_t pos = 0;
        uint32_t hash = 0;
        
        // Initialize hash
        if (data.size() >= 3) {
            hash = computeHash(data, 0);
        }
        
        while (pos < data.size()) {
            uint32_t best_len = 0;
            uint32_t best_offset = 0;
            
            if (pos + 3 <= data.size()) {
                // Update hash chain
                uint32_t cur_hash = hash;
                size_t head = hash_table_[cur_hash];
                hash_table_[cur_hash] = pos;
                chain_table_[pos] = head;
                
                // Search chain
                size_t cur = head;
                int steps = 0;
                
                while (cur != 0 && steps < max_chain_length_) {
                    if (pos - cur > window_size_) break;
                    
                    // Check match
                    if (data[cur] == data[pos] && 
                        data[cur + 1] == data[pos + 1] && 
                        data[cur + 2] == data[pos + 2]) {
                        
                        uint32_t len = 3;
                        while (pos + len < data.size() && 
                               len < lookahead_size_ && 
                               data[cur + len] == data[pos + len]) {
                            len++;
                        }
                        
                        if (len > best_len) {
                            best_len = len;
                            best_offset = static_cast<uint32_t>(pos - cur);
                            
                            if (len >= lookahead_size_) break;
                        }
                    }
                    
                    cur = chain_table_[cur];
                    steps++;
                }
                
                // Update hash for next position
                if (pos + 1 < data.size() && pos + 3 < data.size()) {
                    hash = computeHash(data, pos + 1);
                }
            }
            
            if (best_len >= MIN_MATCH_LEN) {
                tokens.push_back({best_offset, best_len, 0, true});
                pos += best_len;
            } else {
                tokens.push_back({0, 0, data[pos], false});
                pos++;
            }
        }
        
        return tokens;
    }
    
private:
    uint32_t computeHash(const std::vector<uint8_t>& data, size_t pos) const {
        return ((data[pos] << 16) | (data[pos + 1] << 8) | data[pos + 2]) & 0xFFFF;
    }
    
    size_t window_size_;
    size_t lookahead_size_;
    int max_chain_length_;
    std::vector<size_t> hash_table_;
    std::vector<size_t> chain_table_;
};

// ============================================================================
// COMPRESSION LEVEL PRESETS
// ============================================================================

struct CompressionPreset {
    std::string name;
    int level; // 1-9
    size_t window_size;
    int chain_length;
    bool use_adaptive;
    bool use_multithread;
};

class PresetCompressionEngine {
public:
    static const std::array<CompressionPreset, 5>& getPresets() {
        static const std::array<CompressionPreset, 5> presets = {{
            {"Fastest", 1, 4096, 16, false, false},
            {"Fast", 3, 8192, 64, false, false},
            {"Balanced", 5, 16384, 128, true, false},
            {"High", 7, 32768, 256, true, true},
            {"Maximum", 9, 65536, 512, true, true}
        }};
        return presets;
    }
    
    /**
     * @brief Compress with specified preset level
     * @param input_path Input file
     * @param output_path Output file
     * @param level Compression level (1-9)
     * @return Compression statistics
     */
    static CompressionEngine::Stats compressWithLevel(
        const std::string& input_path,
        const std::string& output_path,
        int level) {
        
        level = std::max(1, std::min(9, level));
        
        const auto& presets = getPresets();
        const auto& preset = presets[(level - 1) / 2];
        
        // Use appropriate engine based on preset
        if (preset.use_multithread) {
            auto mt_stats = MultiThreadCompressionEngine::compress_parallel(
                input_path, output_path, std::thread::hardware_concurrency());
            
            CompressionEngine::Stats stats;
            stats.original_size = mt_stats.original_size;
            stats.compressed_size = mt_stats.compressed_size;
            stats.crc32 = 0; // Would need to calculate separately
            stats.ratio = mt_stats.ratio;
            stats.success = mt_stats.success;
            stats.error_msg = mt_stats.error_msg;
            return stats;
        } else {
            return CompressionEngine::compress_file(input_path, output_path);
        }
    }
};

} // namespace compress
