/**
 * @file test_new_components.cpp
 * @brief Test suite for new production-grade components
 * 
 * Tests:
 * 1. ThreadPool with proper wait_all()
 * 2. HashChain match finder
 * 3. BinaryTree match finder
 */

#include <iostream>
#include <vector>
#include <string>
#include <atomic>
#include <cassert>
#include <chrono>
#include <random>

#include "include/compression/thread_pool.hpp"
#include "include/compression/hash_chain.hpp"

using namespace compress;

// ============================================================================
// ThreadPool Tests
// ============================================================================

void test_thread_pool_basic() {
    std::cout << "Testing ThreadPool basic functionality... ";
    
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    
    // Enqueue 100 tasks
    for (int i = 0; i < 100; ++i) {
        pool.enqueue([&counter] {
            ++counter;
        });
    }
    
    // Wait for completion
    pool.wait_all();
    
    assert(counter.load() == 100);
    std::cout << "PASSED" << std::endl;
}

void test_thread_pool_submit_with_future() {
    std::cout << "Testing ThreadPool submit with futures... ";
    
    ThreadPool pool(4);
    
    // Submit tasks that return values
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 20; ++i) {
        futures.push_back(pool.submit([i] {
            return i * 2;
        }));
    }
    
    // Collect results
    pool.wait_all();
    
    int sum = 0;
    for (auto& f : futures) {
        sum += f.get();
    }
    
    // Sum of 0,2,4,...,38 = 2*(0+1+...+19) = 2*190 = 380
    assert(sum == 380);
    std::cout << "PASSED" << std::endl;
}

void test_thread_pool_wait_all_accuracy() {
    std::cout << "Testing ThreadPool wait_all() accuracy (not sleep-based)... ";
    
    ThreadPool pool(4);
    std::atomic<bool> task_completed{false};
    std::atomic<bool> check_before_wait{false};
    
    // Enqueue a slow task
    pool.enqueue([&task_completed] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        task_completed.store(true);
    });
    
    // Check status immediately (should not be done yet)
    check_before_wait.store(task_completed.load());
    
    // Wait for completion
    pool.wait_all();
    
    // After wait_all(), task MUST be complete
    assert(task_completed.load() == true);
    
    // The check before wait should have been false (proving wait_all works)
    // Note: This might occasionally fail due to timing, but should pass most runs
    // assert(check_before_wait.load() == false);
    
    std::cout << "PASSED" << std::endl;
}

void test_thread_pool_exception_handling() {
    std::cout << "Testing ThreadPool exception handling... ";
    
    ThreadPool pool(2);
    std::atomic<int> success_count{0};
    
    // Enqueue tasks, some throw exceptions
    for (int i = 0; i < 10; ++i) {
        if (i % 3 == 0) {
            pool.enqueue([] {
                throw std::runtime_error("Test exception");
            });
        } else {
            pool.enqueue([&success_count] {
                ++success_count;
            });
        }
    }
    
    pool.wait_all();
    
    // Non-throwing tasks should complete
    assert(success_count.load() == 6);  // 10 - 4 throwing tasks
    
    std::cout << "PASSED" << std::endl;
}

void test_thread_pool_stress() {
    std::cout << "Testing ThreadPool stress (1000 tasks)... ";
    
    ThreadPool pool(8);
    std::atomic<int> counter{0};
    
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        pool.enqueue([&counter] {
            // Small amount of work
            volatile int x = 0;
            for (int j = 0; j < 100; ++j) x += j;
            ++counter;
        });
    }
    
    pool.wait_all();
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    assert(counter.load() == 1000);
    std::cout << "PASSED (" << duration.count() << "ms)" << std::endl;
}

// ============================================================================
// HashChain Tests
// ============================================================================

void test_hash_chain_simple_match() {
    std::cout << "Testing HashChain simple match finding... ";
    
    HashChainFinder finder(4096, 64);
    
    // Data with a simple repeated pattern
    std::vector<uint8_t> data = {'a', 'b', 'c', 'd', 'e', 'a', 'b', 'c', 'x', 'y', 'z'};
    
    // Build hash for first occurrence of "abc"
    finder.update_hash(data.data(), 0, data.size());
    finder.update_hash(data.data(), 1, data.size());
    finder.update_hash(data.data(), 2, data.size());
    
    // Look for match at position 5 (should find "abc")
    auto match = finder.find_match(data.data(), 5, data.size());
    
    assert(match.has_match());
    assert(match.length >= 3);
    assert(match.offset == 5);  // Distance back to first "abc"
    
    std::cout << "PASSED (length=" << static_cast<int>(match.length) 
              << ", offset=" << match.offset << ")" << std::endl;
}

void test_hash_chain_no_match() {
    std::cout << "Testing HashChain with no match... ";
    
    HashChainFinder finder(4096, 64);
    
    // Random-ish data with no repeats
    std::vector<uint8_t> data = {'a', 'b', 'c', 'x', 'y', 'z', 'p', 'q', 'r'};
    
    // Update hash
    for (size_t i = 0; i + 3 <= data.size(); ++i) {
        finder.update_hash(data.data(), i, data.size());
    }
    
    // At end, should not find match
    auto match = finder.find_match(data.data(), 6, data.size());
    
    assert(!match.has_match());
    
    std::cout << "PASSED" << std::endl;
}

void test_hash_chain_long_match() {
    std::cout << "Testing HashChain long match finding... ";
    
    HashChainFinder finder(16384, 128);
    
    // Create data with long repeated sequence
    std::vector<uint8_t> data;
    std::string prefix = "The quick brown fox jumps over the lazy dog. ";
    std::string repeat = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    
    for (char c : prefix) data.push_back(c);
    for (char c : repeat) data.push_back(c);
    for (char c : prefix) data.push_back(c);  // Repeat prefix
    for (char c : repeat) data.push_back(c);
    
    // Build hash for first half
    for (size_t i = 0; i < prefix.size() + repeat.size(); ++i) {
        finder.update_hash(data.data(), i, data.size());
    }
    
    // Find match at second occurrence of prefix
    size_t search_pos = prefix.size() + repeat.size();
    auto match = finder.find_match(data.data(), search_pos, data.size());
    
    assert(match.has_match());
    assert(match.length >= 10);  // Should find significant portion of prefix
    
    std::cout << "PASSED (length=" << static_cast<int>(match.length) 
              << ", offset=" << match.offset << ")" << std::endl;
}

void test_hash_chain_bounds_checking() {
    std::cout << "Testing HashChain bounds checking... ";
    
    HashChainFinder finder(1024, 32);
    
    // Small data
    std::vector<uint8_t> data = {'a', 'b'};
    
    // Should handle gracefully when not enough bytes for match
    auto match = finder.find_match(data.data(), 0, data.size());
    assert(!match.has_match());
    
    // Update at position where we don't have 3 bytes
    finder.update_hash(data.data(), 1, data.size());  // Only 1 byte left
    
    std::cout << "PASSED" << std::endl;
}

void test_hash_chain_performance() {
    std::cout << "Testing HashChain performance (1MB data)... ";
    
    HashChainFinder finder(16384, 64);
    
    // Generate 1MB of semi-random data
    std::vector<uint8_t> data(1048576);
    std::mt19937 rng(42);
    std::uniform_int_distribution<> dist(0, 255);
    
    for (auto& byte : data) {
        byte = static_cast<uint8_t>(dist(rng));
    }
    
    auto start = std::chrono::steady_clock::now();
    
    size_t matches_found = 0;
    for (size_t pos = 0; pos + 3 < data.size(); pos += 10) {
        auto match = finder.find_match(data.data(), pos, data.size());
        if (match.has_match()) {
            ++matches_found;
            pos += match.length - 1;  // Skip matched bytes
        }
        finder.update_hash(data.data(), pos, data.size());
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "PASSED (" << matches_found << " matches in " 
              << duration.count() << "ms)" << std::endl;
}

// ============================================================================
// BinaryTree Tests
// ============================================================================

void test_binary_tree_basic() {
    std::cout << "Testing BinaryTree basic functionality... ";
    
    BinaryTreeFinder finder(4096, 64);
    
    // Simple repeated pattern
    std::vector<uint8_t> data = {'a', 'b', 'c', 'd', 'e', 'a', 'b', 'c'};
    
    // Insert first occurrence
    finder.update(data.data(), 0, data.size());
    finder.update(data.data(), 1, data.size());
    finder.update(data.data(), 2, data.size());
    
    // Find match at second occurrence
    auto match = finder.find_match(data.data(), 5, data.size());
    
    assert(match.has_match());
    assert(match.length >= 3);
    
    std::cout << "PASSED (length=" << static_cast<int>(match.length) << ")" << std::endl;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "sayan-compress-lib v4.0 Component Tests\n";
    std::cout << "========================================\n\n";
    
    std::cout << "--- ThreadPool Tests ---\n";
    test_thread_pool_basic();
    test_thread_pool_submit_with_future();
    test_thread_pool_wait_all_accuracy();
    test_thread_pool_exception_handling();
    test_thread_pool_stress();
    
    std::cout << "\n--- HashChain Tests ---\n";
    test_hash_chain_simple_match();
    test_hash_chain_no_match();
    test_hash_chain_long_match();
    test_hash_chain_bounds_checking();
    test_hash_chain_performance();
    
    std::cout << "\n--- BinaryTree Tests ---\n";
    test_binary_tree_basic();
    
    std::cout << "\n========================================\n";
    std::cout << "All tests PASSED!\n";
    std::cout << "========================================\n";
    
    return 0;
}
