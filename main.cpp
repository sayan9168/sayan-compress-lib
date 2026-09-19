#include "compression_engine.hpp"
#include <iomanip>
#include <random>
#include <cmath>

using namespace compress;

// ============================================================================
// TEST DATA GENERATORS
// ============================================================================

std::vector<uint8_t> generate_test_data(size_t size, int pattern_type) {
    std::vector<uint8_t> data(size);
    
    switch (pattern_type) {
        case 0: // Random
            {
                std::mt19937 gen(42);
                std::uniform_int_distribution<> dis(0, 255);
                for (size_t i = 0; i < size; ++i) data[i] = static_cast<uint8_t>(dis(gen));
            }
            break;
            
        case 1: // Repetitive single byte
            std::fill(data.begin(), data.end(), 0x41); // 'A'
            break;
            
        case 2: // Text-like (English prose simulation)
            {
                const char* text = "The quick brown fox jumps over the lazy dog. ";
                size_t len = strlen(text);
                for (size_t i = 0; i < size; ++i) data[i] = text[i % len];
            }
            break;
            
        case 3: // Repeating pattern
            {
                const char* pattern = "ABCDEFGHIJ";
                size_t len = strlen(pattern);
                for (size_t i = 0; i < size; ++i) data[i] = pattern[i % len];
            }
            break;
            
        case 4: // Zero-filled
            std::fill(data.begin(), data.end(), 0x00);
            break;
    }
    
    return data;
}

// ============================================================================
// VERIFICATION UTILITIES
// ============================================================================

bool verify_byte_parity(const std::string& file1, const std::string& file2) {
    std::ifstream f1(file1, std::ios::binary | std::ios::ate);
    std::ifstream f2(file2, std::ios::binary | std::ios::ate);
    
    if (!f1 || !f2) return false;
    
    auto size1 = f1.tellg();
    auto size2 = f2.tellg();
    
    if (size1 != size2) return false;
    
    f1.seekg(0, std::ios::beg);
    f2.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buf1(size1), buf2(size2);
    f1.read(reinterpret_cast<char*>(buf1.data()), size1);
    f2.read(reinterpret_cast<char*>(buf2.data()), size2);
    
    return std::equal(buf1.begin(), buf1.end(), buf2.begin());
}

std::string format_size(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit_idx = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_idx < 3) {
        size /= 1024.0;
        unit_idx++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit_idx];
    return oss.str();
}

// ============================================================================
// BENCHMARK FUNCTIONS
// ============================================================================

struct BenchmarkResult {
    std::string test_name;
    size_t original_size;
    size_t compressed_size;
    double compression_ratio;
    double compress_speed_mbps;
    double decompress_speed_mbps;
    bool parity_verified;
    uint32_t original_crc;
    uint32_t restored_crc;
};

BenchmarkResult run_benchmark(const std::string& test_name, 
                              const std::vector<uint8_t>& data,
                              const std::string& temp_comp = "/tmp/test.sc",
                              const std::string& temp_decomp = "/tmp/test_out.bin") {
    BenchmarkResult result;
    result.test_name = test_name;
    result.original_size = data.size();
    
    // Write original to temp file
    std::string temp_orig = "/tmp/test_orig.bin";
    {
        std::ofstream out(temp_orig, std::ios::binary);
        if (!data.empty()) out.write(reinterpret_cast<const char*>(data.data()), data.size());
    }
    
    // Compression
    auto start_comp = std::chrono::high_resolution_clock::now();
    auto comp_stats = CompressionEngine::compress_file(temp_orig, temp_comp);
    auto end_comp = std::chrono::high_resolution_clock::now();
    
    // Decompression
    auto start_decomp = std::chrono::high_resolution_clock::now();
    auto decomp_stats = CompressionEngine::decompress_file(temp_comp, temp_decomp);
    auto end_decomp = std::chrono::high_resolution_clock::now();
    
    // Calculate timings
    double comp_time = std::chrono::duration<double>(end_comp - start_comp).count();
    double decomp_time = std::chrono::duration<double>(end_decomp - start_decomp).count();
    
    result.compressed_size = comp_stats.compressed_size;
    result.compression_ratio = comp_stats.ratio;
    
    // Speed in MB/s
    if (comp_time > 0) {
        result.compress_speed_mbps = (result.original_size / (1024.0 * 1024.0)) / comp_time;
    }
    if (decomp_time > 0) {
        result.decompress_speed_mbps = (result.original_size / (1024.0 * 1024.0)) / decomp_time;
    }
    
    // Verification
    result.parity_verified = verify_byte_parity(temp_orig, temp_decomp);
    result.original_crc = comp_stats.crc32;
    result.restored_crc = decomp_stats.crc32;
    
    // Cleanup
    std::remove(temp_orig.c_str());
    std::remove(temp_comp.c_str());
    std::remove(temp_decomp.c_str());
    
    return result;
}

void print_benchmark_result(const BenchmarkResult& res) {
    std::cout << "\n┌─────────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ Test: " << std::left << std::setw(58) << res.test_name << " │\n";
    std::cout << "├─────────────────────────────────────────────────────────────────┤\n";
    std::cout << "│ Original Size:     " << std::right << std::setw(15) << format_size(res.original_size) << std::setw(28) << "│\n";
    std::cout << "│ Compressed Size:   " << std::right << std::setw(15) << format_size(res.compressed_size) << std::setw(28) << "│\n";
    std::cout << "│ Compression Ratio: " << std::right << std::setw(14) << std::fixed << std::setprecision(2) << res.compression_ratio << "%" << std::setw(27) << "│\n";
    std::cout << "├─────────────────────────────────────────────────────────────────┤\n";
    std::cout << "│ Compress Speed:    " << std::right << std::setw(14) << std::fixed << std::setprecision(2) << res.compress_speed_mbps << " MB/s" << std::setw(26) << "│\n";
    std::cout << "│ Decompress Speed:  " << std::right << std::setw(14) << std::fixed << std::setprecision(2) << res.decompress_speed_mbps << " MB/s" << std::setw(26) << "│\n";
    std::cout << "├─────────────────────────────────────────────────────────────────┤\n";
    std::cout << "│ Original CRC32:    0x" << std::hex << std::setw(8) << std::setfill('0') << res.original_crc << std::dec << std::setw(22) << "│\n";
    std::cout << "│ Restored CRC32:    0x" << std::hex << std::setw(8) << std::setfill('0') << res.restored_crc << std::dec << std::setw(22) << "│\n";
    std::cout << "├─────────────────────────────────────────────────────────────────┤\n";
    
    std::string parity_status = res.parity_verified ? "✓ PASSED" : "✗ FAILED";
    std::cout << "│ Byte Parity:       " << std::right << std::setw(15) << parity_status << std::setw(28) << "│\n";
    std::cout << "└─────────────────────────────────────────────────────────────────┘\n";
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

void run_edge_case_tests() {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    EDGE CASE TEST SUITE                        ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    std::vector<std::pair<std::string, std::vector<uint8_t>>> test_cases;
    
    // Empty file
    test_cases.emplace_back("Empty File (0 bytes)", std::vector<uint8_t>{});
    
    // Single byte
    test_cases.emplace_back("Single Byte", std::vector<uint8_t>{0x42});
    
    // Two bytes
    test_cases.emplace_back("Two Bytes", std::vector<uint8_t>{0x41, 0x42});
    
    // Minimum match length - 1
    std::vector<uint8_t> small_pattern = {0x41, 0x42};
    test_cases.emplace_back("Below Min Match", small_pattern);
    
    // Minimum match length
    std::vector<uint8_t> min_match = {0x41, 0x42, 0x43};
    test_cases.emplace_back("Min Match Length (3)", min_match);
    
    // All same byte
    std::vector<uint8_t> same_byte(1000, 0x55);
    test_cases.emplace_back("Repetitive Single Byte (1KB)", same_byte);
    
    // Alternating pattern
    std::vector<uint8_t> alternating;
    for (int i = 0; i < 500; ++i) {
        alternating.push_back(0xAA);
        alternating.push_back(0x55);
    }
    test_cases.emplace_back("Alternating Pattern (1KB)", alternating);
    
    // Large file simulation (10MB random)
    std::cout << "\nGenerating large test data (10MB)...\n";
    auto large_random = generate_test_data(10 * 1024 * 1024, 0);
    test_cases.emplace_back("Large Random Data (10MB)", large_random);
    
    // Large repetitive
    auto large_repetitive = generate_test_data(10 * 1024 * 1024, 2);
    test_cases.emplace_back("Large Text-like Data (10MB)", large_repetitive);
    
    int passed = 0;
    int total = test_cases.size();
    
    for (const auto& [name, data] : test_cases) {
        std::cout << "\nTesting: " << name << "... ";
        
        // Write temp
        std::string temp_in = "/tmp/edge_in.bin";
        std::string temp_out = "/tmp/edge_out.bin";
        std::string temp_comp = "/tmp/edge_comp.sc";
        
        {
            std::ofstream out(temp_in, std::ios::binary);
            if (!data.empty()) out.write(reinterpret_cast<const char*>(data.data()), data.size());
        }
        
        // Compress
        auto comp_stats = CompressionEngine::compress_file(temp_in, temp_comp);
        if (!comp_stats.success) {
            std::cout << "✗ Compression Failed: " << comp_stats.error_msg << "\n";
            continue;
        }
        
        // Decompress
        auto decomp_stats = CompressionEngine::decompress_file(temp_comp, temp_out);
        if (!decomp_stats.success) {
            std::cout << "✗ Decompression Failed: " << decomp_stats.error_msg << "\n";
            continue;
        }
        
        // Verify
        if (verify_byte_parity(temp_in, temp_out) && comp_stats.crc32 == decomp_stats.crc32) {
            std::cout << "✓ PASSED";
            passed++;
        } else {
            std::cout << "✗ FAILED (Parity/CRC mismatch)";
        }
        
        // Cleanup
        std::remove(temp_in.c_str());
        std::remove(temp_out.c_str());
        std::remove(temp_comp.c_str());
    }
    
    std::cout << "\n\n┌─────────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ Edge Case Results: " << std::right << std::setw(3) << passed << "/" << total 
              << " tests passed" << std::setw(35) << "│\n";
    std::cout << "└─────────────────────────────────────────────────────────────────┘\n";
}

// ============================================================================
// COMPREHENSIVE BENCHMARK SUITE
// ============================================================================

void run_full_benchmark_suite() {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                  COMPREHENSIVE BENCHMARK SUITE                 ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    std::vector<std::pair<std::string, int>> benchmarks = {
        {"Random Data (No Compressibility)", 0},
        {"Single Byte Repetition (Best Case)", 1},
        {"English Text Simulation", 2},
        {"Simple Repeating Pattern", 3},
        {"Zero-Filled Data", 4}
    };
    
    std::vector<size_t> sizes = {
        1 * 1024,         // 1 KB
        100 * 1024,       // 100 KB
        1 * 1024 * 1024,  // 1 MB
        10 * 1024 * 1024  // 10 MB
    };
    
    std::vector<BenchmarkResult> all_results;
    
    for (const auto& [pattern_name, pattern_type] : benchmarks) {
        for (size_t size : sizes) {
            std::string test_name = pattern_name + " (" + format_size(size) + ")";
            std::cout << "\nRunning: " << test_name << " ...";
            
            auto data = generate_test_data(size, pattern_type);
            auto result = run_benchmark(test_name, data);
            all_results.push_back(result);
            
            std::cout << " Done!";
        }
    }
    
    // Print summary table
    std::cout << "\n\n╔════════════════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                       BENCHMARK SUMMARY                                ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║ Test Name                          │ Size    │ Ratio  │ Compress │Decompress│ Parity    ║\n";
    std::cout << "╠────────────────────────────────────┼─────────┼────────┼──────────┼──────────┼───────────╣\n";
    
    for (const auto& res : all_results) {
        std::cout << "║ " << std::left << std::setw(34) << res.test_name.substr(0, 34);
        std::cout << "│ " << std::right << std::setw(7) << format_size(res.original_size);
        std::cout << " │ " << std::setw(6) << std::fixed << std::setprecision(1) << res.compression_ratio << "%";
        std::cout << " │ " << std::setw(8) << std::fixed << std::setprecision(2) << res.compress_speed_mbps;
        std::cout << " │ " << std::setw(8) << std::fixed << std::setprecision(2) << res.decompress_speed_mbps;
        std::string parity = res.parity_verified ? "✓" : "✗";
        std::cout << " │ " << std::setw(9) << parity << " ║\n";
    }
    
    std::cout << "╚════════════════════════════════════════════════════════════════════════════════════════════╝\n";
}

// ============================================================================
// FILE OPERATIONS
// ============================================================================

int compress_user_file(const std::string& input, const std::string& output) {
    std::cout << "Compressing: " << input << " → " << output << "\n";
    auto stats = CompressionEngine::compress_file(input, output);
    
    if (!stats.success) {
        std::cerr << "Error: " << stats.error_msg << "\n";
        return 1;
    }
    
    std::cout << "Original:   " << format_size(stats.original_size) << "\n";
    std::cout << "Compressed: " << format_size(stats.compressed_size) << "\n";
    std::cout << "Ratio:      " << std::fixed << std::setprecision(2) << stats.ratio << "%\n";
    std::cout << "CRC32:      0x" << std::hex << stats.crc32 << std::dec << "\n";
    std::cout << "Status:     ✓ Success\n";
    
    return 0;
}

int decompress_user_file(const std::string& input, const std::string& output) {
    std::cout << "Decompressing: " << input << " → " << output << "\n";
    auto stats = CompressionEngine::decompress_file(input, output);
    
    if (!stats.success) {
        std::cerr << "Error: " << stats.error_msg << "\n";
        return 1;
    }
    
    std::cout << "Decompressed: " << format_size(stats.original_size) << "\n";
    std::cout << "CRC32:        0x" << std::hex << stats.crc32 << std::dec << "\n";
    std::cout << "Status:       ✓ Success\n";
    
    return 0;
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

void print_usage(const char* prog) {
    std::cout << "SmartCompress Engine v1.0 - LZ77 + Huffman Compression\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << prog << " compress <input> <output>\n";
    std::cout << "  " << prog << " decompress <input> <output>\n";
    std::cout << "  " << prog << " benchmark\n";
    std::cout << "  " << prog << " edge-tests\n";
    std::cout << "  " << prog << " demo\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << prog << " compress myfile.txt myfile.sc\n";
    std::cout << "  " << prog << " decompress myfile.sc restored.txt\n";
    std::cout << "  " << prog << " benchmark  # Run full benchmark suite\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "compress" && argc >= 4) {
        return compress_user_file(argv[2], argv[3]);
    }
    else if (command == "decompress" && argc >= 4) {
        return decompress_user_file(argv[2], argv[3]);
    }
    else if (command == "benchmark") {
        run_full_benchmark_suite();
        return 0;
    }
    else if (command == "edge-tests") {
        run_edge_case_tests();
        return 0;
    }
    else if (command == "demo") {
        std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║              SMARTCOMPRESS DEMONSTRATION                       ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
        
        // Demo with various data types
        std::vector<std::pair<std::string, int>> demos = {
            {"Short Text", 2},
            {"Repetitive Data", 1},
            {"Code/Source File Pattern", 3}
        };
        
        for (const auto& [name, type] : demos) {
            std::cout << "\n--- " << name << " ---\n";
            auto data = generate_test_data(100 * 1024, type); // 100KB
            auto result = run_benchmark(name, data);
            print_benchmark_result(result);
        }
        
        return 0;
    }
    else {
        std::cerr << "Unknown command or missing arguments.\n";
        print_usage(argv[0]);
        return 1;
    }
}
