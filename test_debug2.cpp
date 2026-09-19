#include "compressor_engine.hpp"
#include <iostream>
#include <vector>
#include <sstream>

using namespace compress;

int main() {
    std::vector<uint8_t> testData = {'H', 'e', 'l', 'l', 'o'};
    
    CompressorEngine engine;
    
    // Manual compress
    std::string inputStr(testData.begin(), testData.end());
    std::istringstream input(inputStr);
    std::ostringstream compressedOutput;
    
    auto stats = engine.compress(input, compressedOutput);
    std::cout << "Original: " << testData.size() << ", Compressed: " << compressedOutput.str().size() << "\n";
    
    // Manual decompress
    std::string compressedStr = compressedOutput.str();
    std::istringstream compressedInput(compressedStr);
    std::ostringstream decompressedOutput;
    
    bool success = engine.decompress(compressedInput, decompressedOutput);
    std::cout << "Decompress returned: " << (success ? "true" : "false") << "\n";
    
    std::string result = decompressedOutput.str();
    std::cout << "Decompressed size: " << result.size() << "\n";
    
    if (result.size() == testData.size()) {
        bool match = true;
        for (size_t i = 0; i < testData.size(); ++i) {
            if ((uint8_t)result[i] != testData[i]) {
                std::cout << "Mismatch at " << i << ": " << (uint8_t)result[i] << " vs " << (int)testData[i] << "\n";
                match = false;
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
