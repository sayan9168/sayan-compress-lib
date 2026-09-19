#include "compressor_engine.hpp"
#include <iostream>
#include <vector>

using namespace compress;

int main() {
    std::vector<uint8_t> testData = {'H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '!'};
    
    CompressorEngine engine;
    
    if (engine.verifyRoundTrip(testData)) {
        std::cout << "Basic test PASSED\n";
        return 0;
    } else {
        std::cout << "Basic test FAILED\n";
        return 1;
    }
}
