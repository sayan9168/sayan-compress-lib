#include "huffman.hpp"
#include <iostream>
#include <cstring>

using namespace compress;

int main() {
    // Simulate what happens with single byte 42
    FrequencyCounter counter;
    std::vector<uint8_t> data = {42};
    counter.count(data.data(), data.size());
    
    const auto& freqs = counter.frequencies();
    std::cout << "Frequencies: ";
    int nonZero = 0;
    for (size_t i = 0; i < freqs.size(); ++i) {
        if (freqs[i] > 0) {
            std::cout << "[" << i << "=" << freqs[i] << "] ";
            ++nonZero;
        }
    }
    std::cout << "\nNon-zero symbols: " << nonZero << std::endl;
    
    HuffmanTree tree;
    tree.buildFromFrequencies(freqs.data(), freqs.size());
    
    std::cout << "Tree valid: " << tree.isValid() << std::endl;
    std::cout << "Tree canDecode: " << tree.canDecode() << std::endl;
    std::cout << "Symbol count: " << tree.symbolCount() << std::endl;
    
    auto codeLengths = tree.getCodeLengths(512);
    std::cout << "Code lengths (non-zero): ";
    for (size_t i = 0; i < codeLengths.size(); ++i) {
        if (codeLengths[i] > 0) {
            std::cout << "[" << i << "=" << (int)codeLengths[i] << "] ";
        }
    }
    std::cout << std::endl;
    
    // Now try to rebuild from code lengths
    HuffmanTree tree2;
    tree2.buildFromCodeLengths(codeLengths.data(), codeLengths.size());
    
    std::cout << "Rebuilt tree canDecode: " << tree2.canDecode() << std::endl;
    std::cout << "Rebuilt tree symbol count: " << tree2.symbolCount() << std::endl;
    
    // Try to decode
    auto code = tree2.getCode(42);
    std::cout << "Code for symbol 42: code=" << code.code << ", length=" << (int)code.length << std::endl;
    
    return 0;
}
