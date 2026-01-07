// tests/test_spsc.cpp
#include "SPSCRingBuffer.h"
#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>
#include <type_traits>

// Test 1: Basic functionality
TEST(SPSCRingBufferTest, BasicFunctionality) {
    SPSCRingBuffer<int, 8> buffer;

    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_EQ(buffer.capacity(), 7);

    // Push one item
    EXPECT_TRUE(buffer.push(42));
    EXPECT_FALSE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    EXPECT_EQ(buffer.size(), 1);

    // Pop the item
    int value = 0;
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0);
}

// Test 2: Fill to capacity
TEST(SPSCRingBufferTest, FillToCapacity) {
    SPSCRingBuffer<int, 16> buffer;

    // Fill buffer to capacity
    for (int i = 0; i < 15; ++i) {
        EXPECT_TRUE(buffer.push(i));
        EXPECT_EQ(buffer.size(), i + 1);
    }

    // Should be full now
    EXPECT_TRUE(buffer.full());
    EXPECT_EQ(buffer.size(), 15);
    EXPECT_FALSE(buffer.empty());

    // Next push should fail
    EXPECT_FALSE(buffer.push(999));

    // Empty the buffer
    for (int i = 0; i < 15; ++i) {
        int val = -1;
        EXPECT_TRUE(buffer.pop(val));
        EXPECT_EQ(val, i);
    }

    // Should be empty now
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_FALSE(buffer.full());

    // Next pop should fail
    int val = -1;
    EXPECT_FALSE(buffer.pop(val));
    EXPECT_EQ(val, -1);
}

// Test 3: Wrap-around behavior
TEST(SPSCRingBufferTest, WrapAround) {
    SPSCRingBuffer<int, 8> buffer;

    // Fill and empty partially to cause wrap-around
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(buffer.push(i));
    }

    // Remove 3 items
    for (int i = 0; i < 3; ++i) {
        int val = -1;
        EXPECT_TRUE(buffer.pop(val));
        EXPECT_EQ(val, i);
    }

    // Buffer should have 2 items left
    EXPECT_EQ(buffer.size(), 2);

    // Fill remaining capacity (5 more items, total 7)
    for (int i = 5; i < 10; ++i) {
        EXPECT_TRUE(buffer.push(i));
    }

    // Should be full (7 items)
    EXPECT_TRUE(buffer.full());
    EXPECT_EQ(buffer.size(), 7);

    // Verify all items in correct order
    for (int i = 3; i < 10; ++i) {
        int val = -1;
        EXPECT_TRUE(buffer.pop(val));
        EXPECT_EQ(val, i);
    }

    EXPECT_TRUE(buffer.empty());
}

// Test 4: Move semantics
TEST(SPSCRingBufferTest, MoveSemantics) {
    SPSCRingBuffer<std::vector<int>, 8> buffer;

    // Push with move
    std::vector<int> vec1 = {1, 2, 3, 4, 5};
    ASSERT_EQ(vec1.size(), 5);
    EXPECT_TRUE(buffer.push(std::move(vec1)));
    EXPECT_TRUE(vec1.empty());

    // Push with copy
    std::vector<int> vec2 = {6, 7, 8};
    EXPECT_TRUE(buffer.push(vec2));
    EXPECT_EQ(vec2.size(), 3);

    // Pop with move
    std::vector<int> result;
    EXPECT_TRUE(buffer.pop(result));
    EXPECT_EQ(result, std::vector<int>({1, 2, 3, 4, 5}));
}

// Test 5: Single Producer Single Consumer thread safety
TEST(SPSCRingBufferTest, SPSCTthreadSafety) {
    SPSCRingBuffer<int, 1024> buffer;
    constexpr int NUM_ITEMS = 100000;
    std::atomic<int> producer_count{0};
    std::atomic<int> consumer_count{0};
    std::atomic<bool> producer_done{false};

    // Producer thread
    auto producer = [&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            while (!buffer.push(i)) {
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
                EXPECT_EQ(value, last_value + 1);
                last_value = value;
                consumer_count.fetch_add(1, std::memory_order_relaxed);
            } else {
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
    EXPECT_EQ(producer_count, NUM_ITEMS);
    EXPECT_EQ(consumer_count, NUM_ITEMS);
    EXPECT_TRUE(buffer.empty());
}

// Test 6: Performance measurement (not a real test, just benchmark)
TEST(SPSCRingBufferTest, PerformanceWithConsumer) {
    SPSCRingBuffer<int, 8192> buffer;
    constexpr int ITERATIONS = 100000;
    std::atomic<bool> consumer_done{false};
    std::atomic<int> items_processed{0};

    auto start = std::chrono::high_resolution_clock::now();

    // Consumer thread (pop items)
    std::thread consumer([&]() {
        int value;
        while (items_processed < ITERATIONS) {
            if (buffer.pop(value)) {
                items_processed.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
        consumer_done.store(true, std::memory_order_release);
    });

    // Producer (main thread)
    for (int i = 0; i < ITERATIONS; ++i) {
        while (!buffer.push(i)) {
            std::this_thread::yield();
        }
    }

    // Wait for consumer to finish
    while (!consumer_done.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_EQ(items_processed, ITERATIONS);
    std::cout << "Processed " << ITERATIONS << " items in "
              << duration.count() << " µs" << std::endl;
}
// Test 7: Stress test with small buffer
TEST(SPSCRingBufferTest, StressSmallBuffer) {
    SPSCRingBuffer<int, 4> buffer;
    constexpr int CYCLES = 10000;

    for (int cycle = 0; cycle < CYCLES; ++cycle) {
        // Fill buffer
        for (int i = 0; i < 3; ++i) {
            EXPECT_TRUE(buffer.push(i + cycle * 10));
        }

        // Should be full
        EXPECT_TRUE(buffer.full());

        int dummy;
        EXPECT_FALSE(buffer.push(999));

        // Empty buffer
        for (int i = 0; i < 3; ++i) {
            int val = -1;
            EXPECT_TRUE(buffer.pop(val));
            EXPECT_EQ(val, i + cycle * 10);
        }

        // Should be empty
        EXPECT_TRUE(buffer.empty());
        EXPECT_FALSE(buffer.pop(dummy));
    }
}

// Test 8: Type safety
TEST(SPSCRingBufferTest, TypeSafety) {
    SPSCRingBuffer<double, 8> double_buffer;
    EXPECT_TRUE(double_buffer.push(3.14159));
    double dval = 0.0;
    EXPECT_TRUE(double_buffer.pop(dval));
    EXPECT_DOUBLE_EQ(dval, 3.14159);
    

    SPSCRingBuffer<std::string, 8> string_buffer;
    EXPECT_TRUE(string_buffer.push("Hello"));
    EXPECT_TRUE(string_buffer.push("World"));
    std::string sval;
    EXPECT_TRUE(string_buffer.pop(sval));
    EXPECT_EQ(sval, "Hello");
    EXPECT_TRUE(string_buffer.pop(sval));
    EXPECT_EQ(sval, "World");
}

// Test 9: Multiple types and sizes
TEST(SPSCRingBufferTest, MultipleTypes) {
    // Test with different buffer sizes
    SPSCRingBuffer<int, 2> tiny_buffer;  // Capacity 1
    SPSCRingBuffer<int, 1024> large_buffer;  // Capacity 1023

    EXPECT_EQ(tiny_buffer.capacity(), 1);
    EXPECT_EQ(large_buffer.capacity(), 1023);

    // Test tiny buffer can hold exactly 1 item
    EXPECT_TRUE(tiny_buffer.push(42));
    EXPECT_TRUE(tiny_buffer.full());
    EXPECT_FALSE(tiny_buffer.push(43));

    int val = 0;
    EXPECT_TRUE(tiny_buffer.pop(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(tiny_buffer.empty());
}

// Test 10: Concurrent latency measurement
TEST(SPSCRingBufferTest, ConcurrentLatency) {
    SPSCRingBuffer<std::chrono::high_resolution_clock::time_point, 1024> buffer;
    constexpr int NUM_MESSAGES = 1000;
    std::atomic<int> received{0};
    std::atomic<bool> stop{false};

    // Producer: sends timestamps
    auto producer = [&]() {
        for (int i = 0; i < NUM_MESSAGES; ++i) {
            auto timestamp = std::chrono::high_resolution_clock::now();
            while (!buffer.push(timestamp)) {
                std::this_thread::yield();
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        stop.store(true, std::memory_order_release);
    };

    // Consumer: measures latency
    auto consumer = [&]() {
        std::chrono::high_resolution_clock::time_point timestamp;

        while (!stop.load(std::memory_order_acquire) || !buffer.empty()) {
            if (buffer.pop(timestamp)) {
                auto now = std::chrono::high_resolution_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        now - timestamp).count();
                EXPECT_GE(latency, 0);  // Latency should be non-negative
                received.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    };

    std::thread prod(producer);
    std::thread cons(consumer);

    prod.join();
    cons.join();

    EXPECT_EQ(received, NUM_MESSAGES);
}

// Test 11: Empty and full edge cases
TEST(SPSCRingBufferTest, EdgeCases) {
    SPSCRingBuffer<int, 4> buffer;  // Capacity 3

    // Test empty buffer operations
    int val = 42;
    EXPECT_FALSE(buffer.pop(val));
    EXPECT_EQ(val, 42);  // Should remain unchanged

    // Fill buffer
    EXPECT_TRUE(buffer.push(1));
    EXPECT_TRUE(buffer.push(2));
    EXPECT_TRUE(buffer.push(3));

    // Test full buffer operations
    EXPECT_FALSE(buffer.push(4));
    EXPECT_TRUE(buffer.full());

    // Partial empty and refill
    EXPECT_TRUE(buffer.pop(val));
    EXPECT_EQ(val, 1);
    EXPECT_FALSE(buffer.full());

    EXPECT_TRUE(buffer.push(4));
    EXPECT_TRUE(buffer.full());
}

// Test 12: Test with custom struct
struct TestStruct {
    int id;
    double value;
    std::string name;

    bool operator==(const TestStruct& other) const {
        return id == other.id && value == other.value && name == other.name;
    }
};

TEST(SPSCRingBufferTest, CustomStruct) {
    SPSCRingBuffer<TestStruct, 8> buffer;

    TestStruct ts1{1, 3.14, "pi"};
    TestStruct ts2{2, 2.71, "e"};

    EXPECT_TRUE(buffer.push(ts1));
    EXPECT_TRUE(buffer.push(std::move(ts2)));

    TestStruct result;
    EXPECT_TRUE(buffer.pop(result));
    EXPECT_EQ(result.id, 1);
    EXPECT_EQ(result.value, 3.14);
    EXPECT_EQ(result.name, "pi");

    EXPECT_TRUE(buffer.pop(result));
    EXPECT_EQ(result.id, 2);
    EXPECT_EQ(result.value, 2.71);
    EXPECT_EQ(result.name, "e");
}

// Test 13: Power of two requirement
TEST(SPSCRingBufferTest, PowerOfTwoCompiles) {
    // These should all compile fine
    SPSCRingBuffer<int, 2> buffer2;
    SPSCRingBuffer<int, 4> buffer4;
    SPSCRingBuffer<int, 8> buffer8;
    SPSCRingBuffer<int, 16> buffer16;
    SPSCRingBuffer<int, 32> buffer32;
    SPSCRingBuffer<int, 64> buffer64;
    SPSCRingBuffer<int, 128> buffer128;
    SPSCRingBuffer<int, 256> buffer256;
    SPSCRingBuffer<int, 512> buffer512;
    SPSCRingBuffer<int, 1024> buffer1024;

    // All should have correct capacity
    EXPECT_EQ(buffer2.capacity(), 1);
    EXPECT_EQ(buffer1024.capacity(), 1023);
}

// Test 14: No data races in single-threaded use
TEST(SPSCRingBufferTest, SingleThreadedNoDataRaces) {
    SPSCRingBuffer<int, 16> buffer;

    // Single thread should work without issues
    for (int i = 0; i < 1000; ++i) {
        EXPECT_TRUE(buffer.push(i));
        int val;
        EXPECT_TRUE(buffer.pop(val));
        EXPECT_EQ(val, i);
        EXPECT_TRUE(buffer.empty());
    }
}

// Test 15: Batch operations simulation
TEST(SPSCRingBufferTest, BatchOperations) {
    SPSCRingBuffer<int, 32> buffer;
    constexpr int BATCH_SIZE = 10;

    // Push batch
    for (int i = 0; i < BATCH_SIZE; ++i) {
        EXPECT_TRUE(buffer.push(i));
    }
    EXPECT_EQ(buffer.size(), BATCH_SIZE);

    // Pop batch
    for (int i = 0; i < BATCH_SIZE; ++i) {
        int val;
        EXPECT_TRUE(buffer.pop(val));
        EXPECT_EQ(val, i);
    }
    EXPECT_TRUE(buffer.empty());
}

// Main function is provided by gtest_main
// No need to write main() when linking with gtest_main