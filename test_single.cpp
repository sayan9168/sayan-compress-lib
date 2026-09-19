#include "compressor_engine.hpp"
#include <iostream>
#include <sstream>

using namespace compress;

int main() {
    // Test single byte
    std::vector<uint8_t> singleByte = {42};
    
    // Compress
    std::istringstream input(std::string(singleByte.begin(), singleByte.end()));
    std::ostringstream compressedOutput;
    
    CompressorEngine engine;
    auto stats = engine.compress(input, compressedOutput);
    
    std::cout << "Compressed size: " << compressedOutput.str().size() << " bytes" << std::endl;
    
    // Show header info
    const std::string& compressedStr = compressedOutput.str();
    std::cout << "First few bytes (hex): ";
    for (size_t i = 0; i < std::min(compressedStr.size(), size_t(20)); ++i) {
        printf("%02X ", static_cast<uint8_t>(compressedStr[i]));
    }
    std::cout << std::endl;
    
    // Decompress
    std::istringstream decompressInput(compressedStr);
    std::ostringstream decompressedOutput;
    
    if (!engine.decompress(decompressInput, decompressedOutput)) {
        std::cerr << "Decompression failed!" << std::endl;
        return 1;
    }
    
    std::string result = decompressedOutput.str();
    std::cout << "Decompressed size: " << result.size() << " bytes" << std::endl;
    
    if (result.size() == 1 && static_cast<uint8_t>(result[0]) == 42) {
        std::cout << "SUCCESS!" << std::endl;
        return 0;
    } else {
        std::cout << "FAILED! Got: " << (result.empty() ? "empty" : std::to_string(static_cast<uint8_t>(result[0]))) << std::endl;
        return 1;
    }
}
