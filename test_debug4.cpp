#include "advanced_compression_engine.hpp"
#include <iostream>
#include <vector>

using namespace compress;

int main() {
    // Small data to use serial path
    std::vector<uint8_t> testData(1000);
    for (size_t i = 0; i < testData.size(); ++i) {
        testData[i] = static_cast<uint8_t>(i % 256);
    }
    
    CompressionConfig config;
    config.enableCRC = false;
    config.enableMetadata = false;
    config.blockSize = 64 * 1024;
    
    AdvancedCompressorEngine engine(config);
    
    std::vector<uint8_t> compressedData;
    std::vector<uint8_t> decompressedData;
    FileMetadata metadata;
    
    auto stats = engine.compressData(testData, compressedData, metadata);
    std::cout << "Original size: " << testData.size() << "\n";
    std::cout << "Compressed size: " << compressedData.size() << "\n";
    
    bool success = engine.decompressData(compressedData, decompressedData);
    std::cout << "Decompress returned: " << (success ? "true" : "false") << "\n";
    std::cout << "Decompressed size: " << decompressedData.size() << "\n";
    
    if (decompressedData.size() == testData.size()) {
        bool match = true;
        for (size_t i = 0; i < testData.size(); ++i) {
            if (testData[i] != decompressedData[i]) {
                std::cout << "Mismatch at " << i << ": " << (int)testData[i] << " vs " << (int)decompressedData[i] << "\n";
                match = false;
                break;
            }
        }
        if (match) {
            std::cout << "SUCCESS!\n";
            return 0;
        }
    }
    
    std::cout << "FAILED\n";
    return 1;
}
