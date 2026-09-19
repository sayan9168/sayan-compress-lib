#include "compressor_engine.hpp"
#include <iostream>
#include <sstream>

using namespace compress;

int main() {
    std::string testStr = "Hello, World! Hello, World!";
    std::vector<uint8_t> testData(testStr.begin(), testStr.end());
    
    CompressorEngine engine;
    
    // First just do LZ77 and see what tokens we get
    LZ77Encoder lzEncoder;
    auto tokens = lzEncoder.encode(testData.data(), testData.size());
    
    std::cout << "LZ77 Tokens:" << std::endl;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& t = tokens[i];
        if (t.isLiteral()) {
            std::cout << "  [" << i << "] Literal: '" << (char)t.literal << "' (" << (int)t.literal << ")" << std::endl;
        } else {
            std::cout << "  [" << i << "] Match: offset=" << t.offset << ", length=" << (int)t.length << ", literal=" << (int)t.literal << std::endl;
        }
    }
    
    // Count frequencies
    FrequencyCounter counter;
    counter.countTokens(tokens);
    const auto& freqs = counter.frequencies();
    
    std::cout << "\nFrequencies (non-zero):" << std::endl;
    for (size_t i = 0; i < freqs.size(); ++i) {
        if (freqs[i] > 0) {
            if (i < 256) {
                std::cout << "  [" << i << "] '" << (char)i << "': " << freqs[i] << std::endl;
            } else {
                std::cout << "  [" << i << "] LengthCode(" << (i-256+3) << "): " << freqs[i] << std::endl;
            }
        }
    }
    
    return 0;
}
