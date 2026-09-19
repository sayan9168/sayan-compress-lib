#include "advanced_compression_engine.hpp"
#include <iostream>
#include <vector>

using namespace compress;

int main() {
    std::vector<uint8_t> testData = {'H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '!'};
    
    CompressionConfig config;
    config.enableCRC = false;
    config.enableMetadata = false;
    
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
                std::cout << "Mismatch at " << i << ": " << testData[i] << " vs " << decompressedData[i] << "\n";
                match = false;
            }
        }
        if (match) {
            std::cout << "SUCCESS: Data matches!\n";
            return 0;
        }
    }
    
    std::cout << "FAILED\n";
    return 1;
}
