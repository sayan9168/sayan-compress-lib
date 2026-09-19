/**
 * @file main.cpp
 * @brief Advanced demonstration driver for LZ77+Huffman compression engine
 * 
 * Demonstrates:
 * - Basic compression/decompression
 * - Multi-threaded compression with configurable levels
 * - CRC32 integrity verification
 * - Metadata embedding
 * - Performance benchmarking
 * - Round-trip verification
 */

#include "advanced_compression_engine.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <cstring>

using namespace compress;

// ============================================================================
// Utility Functions
// ============================================================================

void printUsage(const char* program) {
    std::cout << "Advanced LZ77+Huffman Compression Engine\n";
    std::cout << "========================================\n\n";
    std::cout << "Usage: " << program << " [command] [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  compress   <input> <output> [level]  - Compress file\n";
    std::cout << "  decompress <input> <output>          - Decompress file\n";
    std::cout << "  test       [size]                    - Run self-test\n";
    std::cout << "  benchmark  [size] [iterations]       - Performance benchmark\n";
    std::cout << "  compare                              - Compare compression levels\n\n";
    std::cout << "Compression Levels:\n";
    std::cout << "  0 - Fastest (minimal compression, max speed)\n";
    std::cout << "  1 - Fast    (light compression, high speed)\n";
    std::cout << "  2 - Balanced (default, good balance)\n";
    std::cout << "  3 - Good    (better compression, moderate speed)\n";
    std::cout << "  4 - Best    (maximum compression, slower)\n";
    std::cout << "  5 - Ultra   (extreme compression, slowest)\n";
}

std::vector<uint8_t> generateTestData(size_t size, bool includePatterns = true) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    
    std::vector<uint8_t> data(size);
    
    // Generate base random data
    for (size_t i = 0; i < size; ++i) {
        data[i] = dist(rng);
    }
    
    // Add patterns to improve compressibility
    if (includePatterns && size > 1000) {
        const char* pattern1 = "The quick brown fox jumps over the lazy dog. ";
        const char* pattern2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        const char* pattern3 = "0123456789ABCDEF";
        
        size_t pos = size / 4;
        while (pos < size - 100) {
            size_t patternLen = strlen(pattern1);
            for (size_t i = 0; i < patternLen && pos + i < size; ++i) {
                data[pos + i] = static_cast<uint8_t>(pattern1[i]);
            }
            pos += 50;
            
            patternLen = strlen(pattern2);
            for (size_t i = 0; i < patternLen && pos + i < size; ++i) {
                data[pos + i] = static_cast<uint8_t>(pattern2[i]);
            }
            pos += 30;
            
            patternLen = strlen(pattern3);
            for (size_t i = 0; i < patternLen && pos + i < size; ++i) {
                data[pos + i] = static_cast<uint8_t>(pattern3[i]);
            }
            pos += 20;
        }
    }
    
    return data;
}

std::string formatBytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < 3) {
        size /= 1024.0;
        ++unitIndex;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    return oss.str();
}

bool runSelfTest(size_t dataSize = 10000, CompressionLevel level = CompressionLevel::Balanced) {
    std::cout << "\n=== Self-Test Configuration ===\n";
    std::cout << "Data size:      " << formatBytes(dataSize) << "\n";
    std::cout << "Compression:    Level " << static_cast<int>(level) << "\n";
    std::cout << "Generating test data...\n";
    
    std::vector<uint8_t> testData = generateTestData(dataSize, true);
    
    CompressionConfig config;
    config.applyLevel(level);
    config.enableCRC = true;
    config.enableMetadata = true;
    
    AdvancedCompressorEngine engine(config);
    
    std::cout << "Running round-trip verification with CRC32...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    bool success = engine.verifyRoundTrip(testData);
    auto end = std::chrono::high_resolution_clock::now();
    
    double duration = std::chrono::duration<double, std::milli>(end - start).count();
    
    std::cout << "\n=== Self-Test Result ===\n";
    if (success) {
        std::cout << "✓ PASSED in " << std::fixed << std::setprecision(2) 
                  << duration << " ms\n";
        std::cout << "✓ CRC32 integrity verified\n";
        std::cout << "✓ Byte-perfect reconstruction confirmed\n";
        return true;
    } else {
        std::cout << "✗ FAILED!\n";
        return false;
    }
}

bool runBenchmark(size_t dataSize = 100000, int iterations = 5) {
    std::cout << "\n=== Performance Benchmark ===\n";
    std::cout << "Data size:   " << formatBytes(dataSize) << "\n";
    std::cout << "Iterations:  " << iterations << "\n\n";
    
    std::vector<uint8_t> testData = generateTestData(dataSize, true);
    
    struct BenchmarkResult {
        CompressionLevel level;
        double avgCompressTime;
        double avgDecompressTime;
        double avgRatio;
        size_t avgCompressedSize;
    };
    
    std::vector<BenchmarkResult> results;
    
    for (int lvl = 0; lvl <= 5; ++lvl) {
        CompressionLevel level = static_cast<CompressionLevel>(lvl);
        CompressionConfig config;
        config.applyLevel(level);
        config.enableCRC = false;
        config.enableMetadata = false;
        
        AdvancedCompressorEngine engine(config);
        
        double totalCompressTime = 0;
        double totalDecompressTime = 0;
        double totalRatio = 0;
        size_t totalCompressedSize = 0;
        
        for (int iter = 0; iter < iterations; ++iter) {
            std::vector<uint8_t> compressedData;
            std::vector<uint8_t> decompressedData;
            FileMetadata metadata;
            
            auto start = std::chrono::high_resolution_clock::now();
            CompressionStats stats = engine.compressData(testData, compressedData, metadata);
            auto end = std::chrono::high_resolution_clock::now();
            double compressTime = std::chrono::duration<double, std::milli>(end - start).count();
            
            start = std::chrono::high_resolution_clock::now();
            bool success = engine.decompressData(compressedData, decompressedData);
            end = std::chrono::high_resolution_clock::now();
            double decompressTime = std::chrono::duration<double, std::milli>(end - start).count();
            
            if (!success || decompressedData.size() != testData.size()) {
                std::cerr << "Error: Decompression failed at level " << lvl << "\n";
                continue;
            }
            
            totalCompressTime += compressTime;
            totalDecompressTime += decompressTime;
            totalRatio += stats.compressionRatio;
            totalCompressedSize += compressedData.size();
        }
        
        results.push_back({
            level,
            totalCompressTime / iterations,
            totalDecompressTime / iterations,
            totalRatio / iterations,
            static_cast<size_t>(totalCompressedSize / iterations)
        });
    }
    
    std::cout << "+---------+--------------+----------------+------------+-------------+\n";
    std::cout << "| Level   | Compress(ms) | Decompress(ms) | Ratio      | Size        |\n";
    std::cout << "+---------+--------------+----------------+------------+-------------+\n";
    
    for (const auto& result : results) {
        const char* levelNames[] = {"Fastest", "Fast", "Balanced", "Good", "Best", "Ultra"};
        
        std::cout << "| " << std::left << std::setw(7) << levelNames[static_cast<int>(result.level)]
                  << " | " << std::right << std::fixed << std::setprecision(2) 
                  << std::setw(12) << result.avgCompressTime
                  << " | " << std::setw(14) << result.avgDecompressTime
                  << " | " << std::setw(10) << std::fixed << std::setprecision(2) 
                  << result.avgRatio << ":1"
                  << " | " << std::setw(11) << formatBytes(result.avgCompressedSize)
                  << " |\n";
    }
    
    std::cout << "+---------+--------------+----------------+------------+-------------+\n";
    
    return true;
}

bool compareLevels(size_t dataSize = 50000) {
    std::cout << "\n=== Compression Level Comparison ===\n";
    std::cout << "Data size: " << formatBytes(dataSize) << "\n\n";
    
    std::vector<uint8_t> testData = generateTestData(dataSize, true);
    
    std::cout << "Testing all compression levels:\n\n";
    
    for (int lvl = 0; lvl <= 5; ++lvl) {
        CompressionLevel level = static_cast<CompressionLevel>(lvl);
        CompressionConfig config;
        config.applyLevel(level);
        config.enableCRC = true;
        config.enableMetadata = true;
        
        AdvancedCompressorEngine engine(config);
        
        std::vector<uint8_t> compressedData;
        std::vector<uint8_t> decompressedData;
        FileMetadata metadata;
        
        auto start = std::chrono::high_resolution_clock::now();
        CompressionStats stats = engine.compressData(testData, compressedData, metadata);
        auto end = std::chrono::high_resolution_clock::now();
        double compressTime = std::chrono::duration<double, std::milli>(end - start).count();
        
        start = std::chrono::high_resolution_clock::now();
        bool success = engine.decompressData(compressedData, decompressedData);
        end = std::chrono::high_resolution_clock::now();
        double decompressTime = std::chrono::duration<double, std::milli>(end - start).count();
        
        const char* levelNames[] = {"Fastest", "Fast", "Balanced", "Good", "Best", "Ultra"};
        
        std::cout << "Level " << lvl << " (" << levelNames[lvl] << "):\n";
        std::cout << "  Window: " << config.searchWindowSize << " bytes\n";
        std::cout << "  Lookahead: " << config.lookaheadBufferSize << " bytes\n";
        std::cout << "  Threads: " << config.numThreads << "\n";
        std::cout << "  Original:   " << formatBytes(stats.originalSize) << "\n";
        std::cout << "  Compressed: " << formatBytes(stats.compressedSize) << "\n";
        std::cout << "  Ratio:      " << std::fixed << std::setprecision(2) 
                  << stats.compressionRatio << ":1 ("
                  << std::fixed << std::setprecision(1) 
                  << stats.compressionPercentage << "% savings)\n";
        std::cout << "  Compress time:   " << std::fixed << std::setprecision(2) 
                  << compressTime << " ms\n";
        std::cout << "  Decompress time: " << std::fixed << std::setprecision(2) 
                  << decompressTime << " ms\n";
        std::cout << "  CRC32: 0x" << std::hex << metadata.crc32 << std::dec << "\n";
        std::cout << "  Verification: " << (success ? "PASSED" : "FAILED") << "\n\n";
    }
    
    return true;
}

bool compressFile(const std::string& inputPath, const std::string& outputPath, 
                  CompressionLevel level = CompressionLevel::Balanced) {
    std::cout << "\n=== File Compression ===\n";
    std::cout << "Input:  " << inputPath << "\n";
    std::cout << "Output: " << outputPath << "\n";
    std::cout << "Level:  " << static_cast<int>(level) << "\n\n";
    
    CompressionConfig config;
    config.applyLevel(level);
    config.enableCRC = true;
    config.enableMetadata = true;
    
    AdvancedCompressorEngine engine(config);
    
    try {
        auto stats = engine.compressFile(inputPath, outputPath);
        stats.print();
        
        std::cout << "\nCompression successful!\n";
        std::cout << "CRC32 checksum stored in header for integrity verification.\n";
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }
}

bool decompressFile(const std::string& inputPath, const std::string& outputPath) {
    std::cout << "\n=== File Decompression ===\n";
    std::cout << "Input:  " << inputPath << "\n";
    std::cout << "Output: " << outputPath << "\n\n";
    
    CompressionConfig config;
    config.enableCRC = true;
    config.enableMetadata = true;
    
    AdvancedCompressorEngine engine(config);
    
    try {
        auto start = std::chrono::high_resolution_clock::now();
        bool success = engine.decompressFile(inputPath, outputPath);
        auto end = std::chrono::high_resolution_clock::now();
        
        double duration = std::chrono::duration<double, std::milli>(end - start).count();
        
        if (success) {
            std::cout << "Decompression successful in " << std::fixed 
                      << std::setprecision(2) << duration << " ms!\n";
            std::cout << "CRC32 integrity verified.\n";
            return true;
        } else {
            std::cerr << "Decompression failed!\n";
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "test") {
        size_t dataSize = 10000;
        if (argc > 2) {
            dataSize = std::stoul(argv[2]);
        }
        return runSelfTest(dataSize) ? 0 : 1;
    }
    
    if (command == "benchmark") {
        size_t dataSize = 100000;
        int iterations = 5;
        
        if (argc > 2) dataSize = std::stoul(argv[2]);
        if (argc > 3) iterations = std::stoi(argv[3]);
        
        return runBenchmark(dataSize, iterations) ? 0 : 1;
    }
    
    if (command == "compare") {
        size_t dataSize = 50000;
        if (argc > 2) dataSize = std::stoul(argv[2]);
        
        return compareLevels(dataSize) ? 0 : 1;
    }
    
    if (command == "compress" || command == "decompress") {
        if (argc < 4) {
            std::cerr << "Error: Missing input or output file\n";
            printUsage(argv[0]);
            return 1;
        }
        
        std::string inputFile = argv[2];
        std::string outputFile = argv[3];
        
        if (command == "compress") {
            CompressionLevel level = CompressionLevel::Balanced;
            if (argc > 4) {
                int lvl = std::stoi(argv[4]);
                if (lvl < 0 || lvl > 5) {
                    std::cerr << "Error: Invalid compression level (must be 0-5)\n";
                    return 1;
                }
                level = static_cast<CompressionLevel>(lvl);
            }
            return compressFile(inputFile, outputFile, level) ? 0 : 1;
        } else {
            return decompressFile(inputFile, outputFile) ? 0 : 1;
        }
    }
    
    std::cerr << "Error: Unknown command '" << command << "'\n\n";
    printUsage(argv[0]);
    return 1;
}
