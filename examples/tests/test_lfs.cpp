#include "LockFreeStack.h"
#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <set>
#include <chrono>
#include <iostream>
#include <random>

// Test 1: Basic push and pop
TEST(LockFreeStackTest, BasicPushPop) {
    LockFreeStack<int> stack;

    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.size(), 0);

    // Push one item
    stack.push(42);
    EXPECT_FALSE(stack.empty());

    // Pop the item
    int value = 0;
    EXPECT_TRUE(stack.pop(value));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(stack.empty());

    // Pop from empty stack
    EXPECT_FALSE(stack.pop(value));
}

// Test 2: Multiple pushes and pops (LIFO order)
TEST(LockFreeStackTest, LIFOOrder) {
    LockFreeStack<int> stack;

    // Push multiple items
    for (int i = 0; i < 10; ++i) {
        stack.push(i);
    }

    EXPECT_FALSE(stack.empty());

    // Pop should return in LIFO order
    for (int i = 9; i >= 0; --i) {
        int value = -1;
        EXPECT_TRUE(stack.pop(value));
        EXPECT_EQ(value, i);
    }

    EXPECT_TRUE(stack.empty());

    // Verify no more items
    int value = -1;
    EXPECT_FALSE(stack.pop(value));
    EXPECT_EQ(value, -1);  // Should not be modified
}

// Test 3: Move semantics
TEST(LockFreeStackTest, MoveSemantics) {
    LockFreeStack<std::vector<int>> stack;

    // Push with move
    std::vector<int> vec1 = {1, 2, 3, 4, 5};
    stack.push(std::move(vec1));
    EXPECT_TRUE(vec1.empty());  // Should be moved from

    // Push with copy
    std::vector<int> vec2 = {6, 7, 8};
    stack.push(vec2);
    EXPECT_FALSE(vec2.empty());  // Should still have data

    // Pop with move
    std::vector<int> result;
    EXPECT_TRUE(stack.pop(result));
    EXPECT_EQ(result, std::vector<int>({6, 7, 8}));  // LIFO, so vec2 first

    EXPECT_TRUE(stack.pop(result));
    EXPECT_EQ(result, std::vector<int>({1, 2, 3, 4, 5}));
}

// Test 4: Single Producer Single Consumer
TEST(LockFreeStackTest, SingleProducerSingleConsumer) {
    LockFreeStack<int> stack;
    constexpr int NUM_ITEMS = 10000;
    std::atomic<int> pop_count{0};
    std::atomic<bool> producer_done{false};

    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            stack.push(i);
        }
        producer_done.store(true, std::memory_order_release);
    });

    // Consumer thread
    std::thread consumer([&]() {
        int value;

        while (!producer_done.load(std::memory_order_acquire) ||
               !stack.empty()) {
            if (stack.pop(value)) {
                pop_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(pop_count, NUM_ITEMS);
    EXPECT_TRUE(stack.empty());
}

// Test 5: Multiple Producer Single Consumer
TEST(LockFreeStackTest, MultipleProducerSingleConsumer) {
    LockFreeStack<int> stack;
    constexpr int NUM_PRODUCERS = 4;
    constexpr int ITEMS_PER_PRODUCER = 2500;
    constexpr int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    std::atomic<int> pop_count{0};
    std::vector<int> popped_values;
    std::mutex popped_mutex;
    std::atomic<bool> producers_done{false};

    // Multiple producer threads
    std::vector<std::thread> producers;
    for (int t = 0; t < NUM_PRODUCERS; ++t) {
        producers.emplace_back([&, t]() {
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                stack.push(t * 10000 + i);
            }
        });
    }

    // Single consumer thread
    std::thread consumer([&]() {
        int value;
        while (pop_count < TOTAL_ITEMS) {
            if (stack.pop(value)) {
                {
                    std::lock_guard<std::mutex> lock(popped_mutex);
                    popped_values.push_back(value);
                }
                pop_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
        producers_done.store(true, std::memory_order_release);
    });

    // Wait for all producers
    for (auto& t : producers) t.join();

    // Wait for consumer to finish
    consumer.join();

    EXPECT_EQ(pop_count, TOTAL_ITEMS);
    EXPECT_TRUE(stack.empty());

    // Verify all values were popped (no lost items)
    std::set<int> unique_values(popped_values.begin(), popped_values.end());
    EXPECT_EQ(unique_values.size(), TOTAL_ITEMS);
}

// Test 6: Multiple Producer Multiple Consumer
TEST(LockFreeStackTest, MultipleProducerMultipleConsumer) {
    LockFreeStack<int> stack;
    constexpr int NUM_THREADS = 4;
    constexpr int ITEMS_PER_THREAD = 5000;
    constexpr int TOTAL_ITEMS = NUM_THREADS * ITEMS_PER_THREAD;

    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};
    std::atomic<bool> stop{false};

    // All threads do both push and pop
    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            // Producer phase
            for (int i = 0; i < ITEMS_PER_THREAD; ++i) {
                stack.push(t * 100000 + i);
                push_count.fetch_add(1, std::memory_order_relaxed);
            }

            // Consumer phase
            int value;
            int local_pop_count = 0;
            while (local_pop_count < ITEMS_PER_THREAD) {
                if (stack.pop(value)) {
                    local_pop_count++;
                    pop_count.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    // Wait a bit for any stragglers
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(push_count, TOTAL_ITEMS);
    EXPECT_EQ(pop_count, TOTAL_ITEMS);
    EXPECT_TRUE(stack.empty());
}

// Test 7: Stress test with small stack
TEST(LockFreeStackTest, StressTest) {
    LockFreeStack<int> stack;
    constexpr int NUM_ITERATIONS = 10000;

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        // Push and pop in same thread
        stack.push(iter);
        int value = -1;
        EXPECT_TRUE(stack.pop(value));
        EXPECT_EQ(value, iter);
        EXPECT_TRUE(stack.empty());
    }
}

// Test 8: Concurrent push pop mix
TEST(LockFreeStackTest, ConcurrentMix) {
    LockFreeStack<int> stack;
    constexpr int DURATION_MS = 1000;  // Run for 1 second
    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};
    std::atomic<bool> stop{false};

    auto start = std::chrono::steady_clock::now();

    // Multiple threads doing random push/pop
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(t);
            std::uniform_int_distribution<int> dist(0, 1);

            while (!stop.load(std::memory_order_acquire)) {
                if (dist(rng) == 0) {
                    // Push
                    stack.push(push_count.fetch_add(1, std::memory_order_relaxed));
                } else {
                    // Pop
                    int value;
                    if (stack.pop(value)) {
                        pop_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        });
    }

    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    stop.store(true, std::memory_order_release);

    for (auto& t : threads) t.join();

    // Pop any remaining items
    int value;
    while (stack.pop(value)) {
        pop_count.fetch_add(1, std::memory_order_relaxed);
    }

    // push_count should be >= pop_count (some pushes might not have been popped yet)
    EXPECT_GE(push_count, pop_count);

    std::cout << "Concurrent mix: " << push_count << " pushes, "
              << pop_count << " pops in " << DURATION_MS << "ms" << std::endl;
}

// Test 9: Memory leak check
TEST(LockFreeStackTest, NoMemoryLeak) {
    constexpr int NUM_OPERATIONS = 10000;

    {
        LockFreeStack<std::unique_ptr<int>> stack;

        // Push many unique_ptrs
        for (int i = 0; i < NUM_OPERATIONS; ++i) {
            stack.push(std::make_unique<int>(i));
        }

        // Pop half
        std::unique_ptr<int> ptr;
        for (int i = 0; i < NUM_OPERATIONS / 2; ++i) {
            EXPECT_TRUE(stack.pop(ptr));
        }

        // Other half will be destroyed when stack goes out of scope
    }

    // If there's a memory leak, valgrind or ASAN will catch it
    std::cout << "Memory test completed - check with valgrind/ASAN" << std::endl;
}

// Test 10: Performance benchmark
TEST(LockFreeStackTest, PerformanceBenchmark) {
    LockFreeStack<int> stack;
    constexpr int ITERATIONS = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    // Single-threaded push/pop benchmark
    for (int i = 0; i < ITERATIONS; ++i) {
        stack.push(i);
    }

    int value;
    for (int i = 0; i < ITERATIONS; ++i) {
        EXPECT_TRUE(stack.pop(value));
        EXPECT_EQ(value, ITERATIONS - i - 1);  // LIFO order
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double ops_per_sec = (ITERATIONS * 2.0) / (duration.count() / 1e6);

    std::cout << "Single-thread performance: "
              << ops_per_sec / 1e6 << " million ops/sec" << std::endl;
    EXPECT_LT(duration.count(), 1000000);  // Should complete in <1 second
}

// Test 11: Empty stack operations
TEST(LockFreeStackTest, EmptyStackOperations) {
    LockFreeStack<int> stack;

    // Multiple pops on empty stack should not crash
    int value = 42;
    EXPECT_FALSE(stack.pop(value));
    EXPECT_EQ(value, 42);  // Value should be unchanged

    EXPECT_FALSE(stack.pop(value));
    EXPECT_EQ(value, 42);

    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.size(), 0);

    // Push then pop until empty
    stack.push(1);
    stack.push(2);

    EXPECT_TRUE(stack.pop(value));
    EXPECT_TRUE(stack.pop(value));
    EXPECT_FALSE(stack.pop(value));  // Now empty
}

// Test 12: Type safety with different types
TEST(LockFreeStackTest, DifferentTypes) {
    // Test with double
    LockFreeStack<double> double_stack;
    double_stack.push(3.14159);
    double dval = 0.0;
    EXPECT_TRUE(double_stack.pop(dval));
    EXPECT_DOUBLE_EQ(dval, 3.14159);

    // Test with string
    LockFreeStack<std::string> string_stack;
    string_stack.push("Hello");
    string_stack.push("World");
    std::string sval;
    EXPECT_TRUE(string_stack.pop(sval));
    EXPECT_EQ(sval, "World");  // LIFO
    EXPECT_TRUE(string_stack.pop(sval));
    EXPECT_EQ(sval, "Hello");
}

// Test 13: Large number of elements
TEST(LockFreeStackTest, LargeNumberOfElements) {
    LockFreeStack<int> stack;
    constexpr int LARGE_NUMBER = 100000;

    // Push large number
    for (int i = 0; i < LARGE_NUMBER; ++i) {
        stack.push(i);
    }

    EXPECT_FALSE(stack.empty());

    // Pop all
    for (int i = LARGE_NUMBER - 1; i >= 0; --i) {
        int value = -1;
        EXPECT_TRUE(stack.pop(value));
        EXPECT_EQ(value, i);
    }

    EXPECT_TRUE(stack.empty());
}

// Test 14: Interleaved push/pop from multiple threads
TEST(LockFreeStackTest, InterleavedOperations) {
    LockFreeStack<int> stack;
    constexpr int OPS_PER_THREAD = 5000;
    constexpr int NUM_THREADS = 8;

    std::atomic<int> total_pops{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(t);
            std::uniform_real_distribution<double> dist(0.0, 1.0);

            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                if (dist(rng) < 0.7) {  // 70% push, 30% pop
                    stack.push(t * 10000 + i);
                } else {
                    int value;
                    if (stack.pop(value)) {
                        total_pops.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    // Pop any remaining items
    int value;
    while (stack.pop(value)) {
        total_pops.fetch_add(1, std::memory_order_relaxed);
    }

    std::cout << "Interleaved test: " << total_pops << " successful pops" << std::endl;
}

// Main function is provided by gtest_main