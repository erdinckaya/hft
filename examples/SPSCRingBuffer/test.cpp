#include "../include/SPSCRingBuffer.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

// Test 1: Basic functionality
void test_basic_functionality() {
    std::cout << "Test 1: Basic functionality... ";

    SPSCRingBuffer<int, 8> buffer;  // Actual capacity = 7

    // Test empty buffer
    assert(buffer.empty());
    assert(!buffer.full());
    assert(buffer.size() == 0);
    assert(buffer.capacity() == 7);

    // Push one item
    assert(buffer.push(42));
    assert(!buffer.empty());
    assert(!buffer.full());
    assert(buffer.size() == 1);

    // Pop the item
    int value = 0;
    assert(buffer.pop(value));
    assert(value == 42);
    assert(buffer.empty());
    assert(buffer.size() == 0);

    std::cout << "PASSED\n";
}

// Test 2: Fill to capacity
void test_fill_to_capacity() {
    std::cout << "Test 2: Fill to capacity... ";

    SPSCRingBuffer<int, 16> buffer;  // Capacity = 15

    // Fill buffer to capacity
    for (int i = 0; i < 15; ++i) {
        assert(buffer.push(i));
        assert(buffer.size() == i + 1);
    }

    // Should be full now
    assert(buffer.full());
    assert(buffer.size() == 15);
    assert(!buffer.empty());

    // Next push should fail
    assert(!buffer.push(999));

    // Empty the buffer
    for (int i = 0; i < 15; ++i) {
        int val = -1;
        assert(buffer.pop(val));
        assert(val == i);
    }

    // Should be empty now
    assert(buffer.empty());
    assert(buffer.size() == 0);
    assert(!buffer.full());

    // Next pop should fail
    int val = -1;
    assert(!buffer.pop(val));
    assert(val == -1);  // Unchanged

    std::cout << "PASSED\n";
}

// Test 3: Wrap-around behavior
void test_wrap_around() {
    std::cout << "Test 3: Wrap-around behavior... ";

    SPSCRingBuffer<int, 8> buffer;  // Capacity = 7

    // Fill and empty partially to cause wrap-around
    for (int i = 0; i < 5; ++i) {
        assert(buffer.push(i));
    }

    // Remove 3 items
    for (int i = 0; i < 3; ++i) {
        int val = -1;
        assert(buffer.pop(val));
        assert(val == i);
    }

    // Buffer should have 2 items left
    assert(buffer.size() == 2);

    // Fill remaining capacity (5 more items, total 7)
    for (int i = 5; i < 10; ++i) {
        assert(buffer.push(i));
    }

    // Should be full (7 items)
    assert(buffer.full());
    assert(buffer.size() == 7);

    // Verify all items in correct order
    for (int i = 3; i < 10; ++i) {
        int val = -1;
        assert(buffer.pop(val));
        assert(val == i);
    }

    assert(buffer.empty());

    std::cout << "PASSED\n";
}

// Test 4: Move semantics
void test_move_semantics() {
    std::cout << "Test 4: Move semantics... ";

    SPSCRingBuffer<std::vector<int>, 8> buffer;

    // Push with move
    std::vector<int> vec1 = {1, 2, 3, 4, 5};
    assert(buffer.push(std::move(vec1)));
    assert(vec1.empty());  // Should be moved from

    // Push with copy
    std::vector<int> vec2 = {6, 7, 8};
    assert(buffer.push(vec2));
    assert(!vec2.empty());  // Should still have data

    // Pop with move
    std::vector<int> result;
    assert(buffer.pop(result));
    assert(result == std::vector<int>({1, 2, 3, 4, 5}));

    std::cout << "PASSED\n";
}

// Test 5: Single Producer Single Consumer thread safety
void test_spsc_thread_safety() {
    std::cout << "Test 5: SPSC thread safety... ";

    SPSCRingBuffer<int, 1024> buffer;
    constexpr int NUM_ITEMS = 1000000;
    std::atomic<int> producer_count{0};
    std::atomic<int> consumer_count{0};
    std::atomic<bool> producer_done{false};

    // Producer thread
    auto producer = [&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            while (!buffer.push(i)) {
                // Buffer full, yield
                std::this_thread::yield();
            }
            producer_count.fetch_add(1, std::memory_order_relaxed);
        }
        producer_done.store(true, std::memory_order_release);
    };

    // Consumer thread
    auto consumer = [&]() {
        int last_value = -1;
        int value;

        while (!producer_done.load(std::memory_order_acquire) ||
               !buffer.empty()) {
            if (buffer.pop(value)) {
                // Verify sequence
                assert(value == last_value + 1);
                last_value = value;
                consumer_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                // Buffer empty, yield
                std::this_thread::yield();
            }
        }
    };

    // Start threads
    std::thread prod_thread(producer);
    std::thread cons_thread(consumer);

    // Wait for completion
    prod_thread.join();
    cons_thread.join();

    // Verify counts
    assert(producer_count == NUM_ITEMS);
    assert(consumer_count == NUM_ITEMS);
    assert(buffer.empty());

    std::cout << "PASSED\n";
}

// Test 6: Performance measurement
void test_performance() {
    std::cout << "Test 6: Performance measurement... ";

    SPSCRingBuffer<int, 8192> buffer;
    constexpr int ITERATIONS = 1000000;

    auto start = std::chrono::high_resolution_clock::now();

    // Producer
    for (int i = 0; i < ITERATIONS; ++i) {
        while (!buffer.push(i)) {
            // In real HFT, you'd handle this differently
            std::this_thread::yield();
        }
    }

    // Consumer
    int value;
    for (int i = 0; i < ITERATIONS; ++i) {
        while (!buffer.pop(value)) {
            std::this_thread::yield();
        }
        assert(value == i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double ops_per_sec = (ITERATIONS * 2.0) / (duration.count() / 1e6);

    std::cout << "PASSED (";
    std::cout << duration.count() << " µs, ";
    std::cout << ops_per_sec / 1e6 << " million ops/sec)\n";
}

// Test 7: Stress test with small buffer
void test_stress_small_buffer() {
    std::cout << "Test 7: Stress test with small buffer... ";

    SPSCRingBuffer<int, 4> buffer;  // Capacity = 3
    constexpr int CYCLES = 100000;

    for (int cycle = 0; cycle < CYCLES; ++cycle) {
        // Fill buffer
        for (int i = 0; i < 3; ++i) {
            assert(buffer.push(i + cycle * 10));
        }

        // Should be full
        assert(buffer.full());
        assert(!buffer.push(999));

        // Empty buffer
        for (int i = 0; i < 3; ++i) {
            int val = -1;
            assert(buffer.pop(val));
            assert(val == i + cycle * 10);
        }

        // Should be empty
        assert(buffer.empty());
        int val;
        assert(!buffer.pop(val));
    }

    std::cout << "PASSED\n";
}

// Test 8: Type safety and template parameters
void test_type_safety() {
    std::cout << "Test 8: Type safety... ";

    // Test with different types
    SPSCRingBuffer<double, 8> double_buffer;
    assert(double_buffer.push(3.14159));
    double dval;
    assert(double_buffer.pop(dval));
    assert(dval == 3.14159);

    SPSCRingBuffer<std::string, 8> string_buffer;
    assert(string_buffer.push("Hello"));
    assert(string_buffer.push("World"));
    std::string sval;
    assert(string_buffer.pop(sval));
    assert(sval == "Hello");
    assert(string_buffer.pop(sval));
    assert(sval == "World");

    // Test power of two assertion (compile-time)
    // SPSCRingBuffer<int, 7> bad_buffer;  // Should fail to compile

    std::cout << "PASSED\n";
}

// Test 9: Concurrent producer-consumer with timing
void test_concurrent_timing() {
    std::cout << "Test 9: Concurrent producer-consumer... ";

    SPSCRingBuffer<std::chrono::high_resolution_clock::time_point, 1024> buffer;
    constexpr int NUM_MESSAGES = 10000;
    std::atomic<int> received{0};
    std::atomic<bool> stop{false};

    // Producer: sends timestamps
    auto producer = [&]() {
        for (int i = 0; i < NUM_MESSAGES; ++i) {
            auto timestamp = std::chrono::high_resolution_clock::now();
            while (!buffer.push(timestamp)) {
                std::this_thread::yield();
            }
            std::this_thread::sleep_for(std::chrono::microseconds(10));  // Simulate work
        }
        stop.store(true, std::memory_order_release);
    };

    // Consumer: measures latency
    auto consumer = [&]() {
        std::chrono::high_resolution_clock::time_point timestamp;
        long long total_latency_ns = 0;

        while (!stop.load(std::memory_order_acquire) || !buffer.empty()) {
            if (buffer.pop(timestamp)) {
                auto now = std::chrono::high_resolution_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        now - timestamp).count();
                total_latency_ns += latency;
                received.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }

        if (received > 0) {
            double avg_latency_ns = static_cast<double>(total_latency_ns) / received;
            std::cout << "avg latency: " << avg_latency_ns << " ns ";
        }
    };

    std::thread prod(producer);
    std::thread cons(consumer);

    prod.join();
    cons.join();

    assert(received == NUM_MESSAGES);
    std::cout << "PASSED\n";
}

// Main test runner
int main() {
    std::cout << "=== SPSCRingBuffer Test Suite ===\n\n";

    try {
        test_basic_functionality();
        test_fill_to_capacity();
        test_wrap_around();
        test_move_semantics();
        test_spsc_thread_safety();
        test_performance();
        test_stress_small_buffer();
        test_type_safety();
        test_concurrent_timing();

        std::cout << "\n=== All tests PASSED! ===\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n=== TEST FAILED: " << e.what() << " ===\n";
        return 1;
    } catch (...) {
        std::cerr << "\n=== TEST FAILED with unknown exception ===\n";
        return 1;
    }
}