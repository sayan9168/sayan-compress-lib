#include "compressor_engine.hpp"
#include <iostream>
#include <sstream>

using namespace compress;

int main() {
    std::string testStr = "Hello, World! Hello, World!";
    std::vector<uint8_t> testData(testStr.begin(), testStr.end());
    
    CompressorEngine engine;
    
    // Compress
    std::istringstream input(std::string(testData.begin(), testData.end()));
    std::ostringstream compressedOutput;
    
    auto stats = engine.compress(input, compressedOutput);
    std::cout << "Original: " << testData.size() << " bytes" << std::endl;
    std::cout << "Compressed: " << compressedOutput.str().size() << " bytes" << std::endl;
    
    // Decompress
    std::istringstream decompressInput(compressedOutput.str());
    std::ostringstream decompressedOutput;
    
    if (engine.decompress(decompressInput, decompressedOutput)) {
        std::string result = decompressedOutput.str();
        std::cout << "Decompressed: " << result.size() << " bytes" << std::endl;
        
        if (result == testStr) {
            std::cout << "SUCCESS!" << std::endl;
            return 0;
        } else {
            std::cout << "FAILED - content mismatch!" << std::endl;
            return 1;
        }
    } else {
        std::cout << "FAILED - decompression error!" << std::endl;
        return 1;
    }
}
