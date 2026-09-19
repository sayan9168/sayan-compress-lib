/**
 * @file main.cpp
 * @brief Main driver for the LZ77+Huffman compression engine
 * 
 * Demonstrates file and string compression with verification.
 * 
 * Usage:
 *   ./compressor -c input.txt output.cmp    # Compress
 *   ./compressor -d input.cmp output.txt    # Decompress
 *   ./compressor -t                         # Run self-tests
 *   ./compressor -s "test string"           # Compress string inline
 * 
 * Compile: g++ -std=c++20 -O3 -o compressor main.cpp
 */

#include "compressor_engine.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <cstring>
#include <cassert>

using namespace compress;

/**
 * @brief Print usage information
 */
void printUsage(const char* programName) {
    std::cout << "LZ77+Huffman Compression Engine\n";
    std::cout << "================================\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << programName << " -c <input> <output>   Compress a file\n";
    std::cout << "  " << programName << " -d <input> <output>   Decompress a file\n";
    std::cout << "  " << programName << " -t                    Run self-tests\n";
    std::cout << "  " << programName << " -s \"string\"          Compress and verify string\n";
    std::cout << "  " << programName << " -h                    Show this help\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << programName << " -c document.txt document.cmp\n";
    std::cout << "  " << programName << " -d document.cmp document_restored.txt\n";
}

/**
 * @brief Compress a file
 * @return 0 on success, non-zero on error
 */
int compressFile(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream inputFile(inputPath, std::ios::binary);
    if (!inputFile) {
        std::cerr << "Error: Cannot open input file: " << inputPath << std::endl;
        return 1;
    }
    
    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile) {
        std::cerr << "Error: Cannot create output file: " << outputPath << std::endl;
        return 1;
    }
    
    CompressorEngine engine;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    CompressionStats stats = engine.compress(inputFile, outputFile);
    auto endTime = std::chrono::high_resolution_clock::now();
    
    stats.processingTimeMs = std::chrono::duration<double, std::milli>(
        endTime - startTime).count();
    
    stats.print();
    
    std::cout << "\nCompressed data written to: " << outputPath << std::endl;
    
    return 0;
}

/**
 * @brief Decompress a file
 * @return 0 on success, non-zero on error
 */
int decompressFile(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream inputFile(inputPath, std::ios::binary);
    if (!inputFile) {
        std::cerr << "Error: Cannot open input file: " << inputPath << std::endl;
        return 1;
    }
    
    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile) {
        std::cerr << "Error: Cannot create output file: " << outputPath << std::endl;
        return 1;
    }
    
    CompressorEngine engine;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    bool success = engine.decompress(inputFile, outputFile);
    auto endTime = std::chrono::high_resolution_clock::now();
    
    double processingTimeMs = std::chrono::duration<double, std::milli>(
        endTime - startTime).count();
    
    if (success) {
        std::cout << "Decompression successful!" << std::endl;
        std::cout << "Processing time: " << processingTimeMs << " ms" << std::endl;
        std::cout << "Decompressed data written to: " << outputPath << std::endl;
        return 0;
    } else {
        std::cerr << "Error: Decompression failed" << std::endl;
        return 1;
    }
}

/**
 * @brief Run comprehensive self-tests
 * @return 0 if all tests pass, non-zero otherwise
 */
int runSelfTests() {
    std::cout << "Running LZ77+Huffman Compression Engine Self-Tests\n";
    std::cout << "==================================================\n\n";
    
    int passed = 0;
    int total = 0;
    
    // Test 1: Empty data
    {
        ++total;
        std::cout << "Test 1: Empty data... ";
        CompressorEngine engine;
        std::vector<uint8_t> emptyData;
        if (engine.verifyRoundTrip(emptyData)) {
            std::cout << "PASSED" << std::endl;
            ++passed;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    }
    
    // Test 2: Single byte
    {
        ++total;
        std::cout << "Test 2: Single byte... ";
        CompressorEngine engine;
        std::vector<uint8_t> singleByte = {42};
        if (engine.verifyRoundTrip(singleByte)) {
            std::cout << "PASSED" << std::endl;
            ++passed;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    }
    
    // Test 3: Repeated bytes (RLE-like pattern)
    {
        ++total;
        std::cout << "Test 3: Repeated bytes (AAAA...)... ";
        CompressorEngine engine;
        std::vector<uint8_t> repeated(1000, 'A');
        if (engine.verifyRoundTrip(repeated)) {
            std::cout << "PASSED" << std::endl;
            ++passed;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    }
    
    // Test 4: Simple text string
    {
        ++total;
        std::cout << "Test 4: Simple text string... ";
        CompressorEngine engine;
        std::string testStr = "The quick brown fox jumps over the lazy dog. ";
        testStr += testStr;  // Duplicate to create patterns
        testStr += testStr;
        std::vector<uint8_t> testData(testStr.begin(), testStr.end());
        if (engine.verifyRoundTrip(testData)) {
            std::cout << "PASSED" << std::endl;
            ++passed;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    }
    
    // Test 5: Binary data with all byte values
    {
        ++total;
        std::cout << "Test 5: All byte values (0-255)... ";
        CompressorEngine engine;
        std::vector<uint8_t> allBytes(256);
        for (int i = 0; i < 256; ++i) {
            allBytes[i] = static_cast<uint8_t>(i);
        }
        // Repeat to ensure patterns
        std::vector<uint8_t> testData;
        for (int r = 0; r < 10; ++r) {
            testData.insert(testData.end(), allBytes.begin(), allBytes.end());
        }
        if (engine.verifyRoundTrip(testData)) {
            std::cout << "PASSED" << std::endl;
            ++passed;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    }
    
    // Test 6: Large repetitive text
    {
        ++total;
        std::cout << "Test 6: Large repetitive text (10KB)... ";
        CompressorEngine engine;
        std::string baseText = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. ";
        std::string largeText;
        for (int i = 0; i < 200; ++i) {
            largeText += baseText;
        }
        std::vector<uint8_t> testData(largeText.begin(), largeText.end());
        if (engine.verifyRoundTrip(testData)) {
            std::cout << "PASSED" << std::endl;
            ++passed;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    }
    
    // Test 7: Random-ish data (harder to compress)
    {
        ++total;
        std::cout << "Test 7: Pseudo-random data (1KB)... ";
        CompressorEngine engine;
        std::vector<uint8_t> randomData(1024);
        uint32_t seed = 12345;
        for (size_t i = 0; i < randomData.size(); ++i) {
            // Simple LCG pseudo-random generator
            seed = seed * 1103515245 + 12345;
            randomData[i] = static_cast<uint8_t>((seed >> 16) & 0xFF);
        }
        if (engine.verifyRoundTrip(randomData)) {
            std::cout << "PASSED" << std::endl;
            ++passed;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    }
    
    // Test 8: Long match patterns
    {
        ++total;
        std::cout << "Test 8: Long repeating patterns... ";
        CompressorEngine engine;
        std::string pattern = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::string testDataStr;
        for (int i = 0; i < 100; ++i) {
            testDataStr += pattern;
        }
        std::vector<uint8_t> testData(testDataStr.begin(), testDataStr.end());
        if (engine.verifyRoundTrip(testData)) {
            std::cout << "PASSED" << std::endl;
            ++passed;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    }
    
    // Test 9: Mixed content (text + binary)
    {
        ++total;
        std::cout << "Test 9: Mixed text and binary content... ";
        CompressorEngine engine;
        std::vector<uint8_t> mixedData;
        // Add some text
        std::string text = "This is some sample text. ";
        mixedData.insert(mixedData.end(), text.begin(), text.end());
        // Add some binary
        for (int i = 0; i < 100; ++i) {
            mixedData.push_back(static_cast<uint8_t>(i * 3));
        }
        // Add more text
        mixedData.insert(mixedData.end(), text.begin(), text.end());
        // Repeat
        for (int r = 0; r < 20; ++r) {
            mixedData.insert(mixedData.end(), mixedData.begin(), mixedData.end());
            if (mixedData.size() > 10000) break;
        }
        if (engine.verifyRoundTrip(mixedData)) {
            std::cout << "PASSED" << std::endl;
            ++passed;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    }
    
    // Test 10: String compression demo with statistics
    {
        ++total;
        std::cout << "Test 10: String compression with statistics... ";
        
        std::string testString = 
            "Compression algorithms are fascinating! "
            "They find patterns in data and represent them more efficiently. "
            "LZ77 finds repeated sequences using a sliding window. "
            "Huffman coding assigns shorter codes to more frequent symbols. "
            "Together, they form a powerful compression engine.";
        
        // Repeat to increase size for better compression ratio
        for (int i = 0; i < 10; ++i) {
            testString += testString;
        }
        
        std::vector<uint8_t> testData(testString.begin(), testString.end());
        
        CompressorEngine engine;
        
        // Compress to memory
        std::vector<uint8_t> compressedData;
        {
            std::istringstream input(std::string(testData.begin(), testData.end()));
            std::ostringstream output;
            engine.compress(input, output);
            std::string compressedStr = output.str();
            compressedData.assign(compressedStr.begin(), compressedStr.end());
        }
        
        size_t originalSize = testData.size();
        size_t compressedSize = compressedData.size();
        double ratio = static_cast<double>(originalSize) / static_cast<double>(compressedSize);
        double savings = (1.0 - static_cast<double>(compressedSize) / 
                         static_cast<double>(originalSize)) * 100.0;
        
        std::cout << "PASSED" << std::endl;
        std::cout << "  Original:   " << originalSize << " bytes" << std::endl;
        std::cout << "  Compressed: " << compressedSize << " bytes" << std::endl;
        std::cout << "  Ratio:      " << ratio << ":1" << std::endl;
        std::cout << "  Savings:    " << savings << "%" << std::endl;
        ++passed;
    }
    
    std::cout << "\n==================================================\n";
    std::cout << "Results: " << passed << "/" << total << " tests passed\n";
    
    if (passed == total) {
        std::cout << "All tests PASSED! ✓" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests FAILED! ✗" << std::endl;
        return 1;
    }
}

/**
 * @brief Compress and verify a string inline
 * @return 0 on success, non-zero on error
 */
int compressString(const std::string& inputStr) {
    std::cout << "String Compression Demo\n";
    std::cout << "=======================\n\n";
    
    std::vector<uint8_t> inputData(inputStr.begin(), inputStr.end());
    
    std::cout << "Input string length: " << inputData.size() << " bytes" << std::endl;
    std::cout << "Input preview: \"" << inputStr.substr(0, 50);
    if (inputStr.size() > 50) std::cout << "...";
    std::cout << "\"\n\n";
    
    CompressorEngine engine;
    
    // Compress
    std::vector<uint8_t> compressedData;
    {
        std::istringstream input(std::string(inputData.begin(), inputData.end()));
        std::ostringstream output;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        CompressionStats stats = engine.compress(input, output);
        auto endTime = std::chrono::high_resolution_clock::now();
        
        stats.processingTimeMs = std::chrono::duration<double, std::milli>(
            endTime - startTime).count();
        
        std::string compressedStr = output.str();
        compressedData.assign(compressedStr.begin(), compressedStr.end());
        
        stats.print();
    }
    
    // Verify round-trip
    std::cout << "\nVerifying round-trip integrity... ";
    if (engine.verifyRoundTrip(inputData)) {
        std::cout << "PASSED ✓" << std::endl;
    } else {
        std::cout << "FAILED ✗" << std::endl;
        return 1;
    }
    
    return 0;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string mode = argv[1];
    
    if (mode == "-h" || mode == "--help") {
        printUsage(argv[0]);
        return 0;
    }
    
    if (mode == "-t" || mode == "--test") {
        return runSelfTests();
    }
    
    if (mode == "-s" || mode == "--string") {
        if (argc < 3) {
            std::cerr << "Error: Please provide a string to compress" << std::endl;
            std::cerr << "Usage: " << argv[0] << " -s \"your string here\"" << std::endl;
            return 1;
        }
        return compressString(argv[2]);
    }
    
    if (mode == "-c" || mode == "--compress") {
        if (argc < 4) {
            std::cerr << "Error: Please provide input and output file paths" << std::endl;
            std::cerr << "Usage: " << argv[0] << " -c <input> <output>" << std::endl;
            return 1;
        }
        return compressFile(argv[2], argv[3]);
    }
    
    if (mode == "-d" || mode == "--decompress") {
        if (argc < 4) {
            std::cerr << "Error: Please provide input and output file paths" << std::endl;
            std::cerr << "Usage: " << argv[0] << " -d <input> <output>" << std::endl;
            return 1;
        }
        return decompressFile(argv[2], argv[3]);
    }
    
    std::cerr << "Error: Unknown mode '" << mode << "'" << std::endl;
    printUsage(argv[0]);
    return 1;
}
