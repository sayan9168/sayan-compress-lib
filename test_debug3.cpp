#include "compressor_engine.hpp"
#include <iostream>
#include <sstream>

using namespace compress;

int main() {
    // Test single byte - debug decompression
    std::vector<uint8_t> singleByte = {42};
    
    CompressorEngine engine;
    
    // Compress
    std::istringstream input(std::string(singleByte.begin(), singleByte.end()));
    std::ostringstream compressedOutput;
    
    auto stats = engine.compress(input, compressedOutput);
    
    const std::string& compressedStr = compressedOutput.str();
    
    // Now manually test decompression
    std::istringstream decompressInput(compressedStr);
    
    // Read header manually
    uint8_t magic1 = static_cast<uint8_t>(decompressInput.get());
    uint8_t magic2 = static_cast<uint8_t>(decompressInput.get());
    std::cout << "Magic: " << std::hex << (int)magic1 << " " << (int)magic2 << std::dec << std::endl;
    
    uint64_t originalSize = 0;
    for (int i = 0; i < 8; ++i) {
        uint64_t byte = static_cast<uint64_t>(static_cast<uint8_t>(decompressInput.get()));
        originalSize |= (byte << (i * 8));
    }
    std::cout << "Original size: " << originalSize << std::endl;
    
    int symbolRange = decompressInput.get();
    std::cout << "Symbol range: " << symbolRange << std::endl;
    
    // Read code lengths
    std::vector<uint8_t> codeLengths(512, 0);
    while (decompressInput.good()) {
        int symbol = decompressInput.get();
        if (symbol == 0xFF || symbol < 0) break;
        int length = decompressInput.get();
        if (length < 0) break;
        if (symbol >= 0 && symbol < 512) {
            codeLengths[symbol] = static_cast<uint8_t>(length);
            std::cout << "Code length for symbol " << symbol << ": " << (int)length << std::endl;
        }
    }
    
    // Build tree from code lengths
    HuffmanTree huffmanTree;
    huffmanTree.buildFromCodeLengths(codeLengths.data(), codeLengths.size());
    
    std::cout << "Tree canDecode: " << huffmanTree.canDecode() << std::endl;
    std::cout << "Tree isValid: " << huffmanTree.isValid() << std::endl;
    std::cout << "Tree symbolCount: " << huffmanTree.symbolCount() << std::endl;
    
    auto code = huffmanTree.getCode(42);
    std::cout << "Code for 42: code=" << code.code << ", length=" << (int)code.length << std::endl;
    
    return 0;
}
