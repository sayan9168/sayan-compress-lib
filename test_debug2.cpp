#include "compressor_engine.hpp"
#include <iostream>
#include <sstream>

using namespace compress;

int main() {
    // Test single byte - debug step by step
    std::vector<uint8_t> singleByte = {42};
    
    CompressorEngine engine;
    
    // Compress
    std::istringstream input(std::string(singleByte.begin(), singleByte.end()));
    std::ostringstream compressedOutput;
    
    auto stats = engine.compress(input, compressedOutput);
    
    const std::string& compressedStr = compressedOutput.str();
    std::cout << "Compressed size: " << compressedStr.size() << " bytes" << std::endl;
    
    // Parse header manually
    std::cout << "\nHeader analysis:" << std::endl;
    std::cout << "Magic: " << std::hex << (int)(uint8_t)compressedStr[0] << " " << (int)(uint8_t)compressedStr[1] << std::dec << std::endl;
    
    uint64_t origSize = 0;
    for (int i = 0; i < 8; ++i) {
        origSize |= ((uint64_t)(uint8_t)compressedStr[2+i]) << (i*8);
    }
    std::cout << "Original size: " << origSize << std::endl;
    
    std::cout << "Symbol range byte: " << (int)(uint8_t)compressedStr[10] << std::endl;
    
    // Find code lengths
    size_t pos = 11;
    std::cout << "Code length entries:" << std::endl;
    while (pos < compressedStr.size() && (uint8_t)compressedStr[pos] != 0xFF) {
        uint8_t symbol = (uint8_t)compressedStr[pos];
        uint8_t length = (uint8_t)compressedStr[pos+1];
        std::cout << "  Symbol " << (int)symbol << ": length=" << (int)length << std::endl;
        pos += 2;
    }
    std::cout << "Sentinel at pos " << pos << std::endl;
    std::cout << "Remaining bits: " << (compressedStr.size() - pos - 1) << " bytes" << std::endl;
    
    return 0;
}
