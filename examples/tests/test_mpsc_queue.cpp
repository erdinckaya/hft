#include "MPSCQueue.h"
#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <set>
#include <chrono>
#include <iostream>
#include <random>

// Test 1: Basic single-threaded operations
TEST(MPSCQueueTest, BasicOperations) {
    MPSCQueue<int> queue;

    EXPECT_TRUE(queue.empty());

    // Push and pop one item
    EXPECT_TRUE(queue.push(42));
    EXPECT_FALSE(queue.empty());

    int value = 0;
    EXPECT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(queue.empty());

    // Pop from empty queue
    EXPECT_FALSE(queue.pop(value));
}

// Test 2: FIFO ordering (single-threaded)
TEST(MPSCQueueTest, FIFOOrderSingleThreaded) {
    MPSCQueue<int> queue;
    constexpr int NUM_ITEMS = 100;

    // Push items in order
    for (int i = 0; i < NUM_ITEMS; ++i) {
        EXPECT_TRUE(queue.push(i));
    }

    // Pop items should come in FIFO order
    for (int i = 0; i < NUM_ITEMS; ++i) {
        int value = -1;
        EXPECT_TRUE(queue.pop(value));
        EXPECT_EQ(value, i);
    }

    EXPECT_TRUE(queue.empty());
}

// Test 3: Move semantics
TEST(MPSCQueueTest, MoveSemantics) {
    MPSCQueue<std::vector<int>> queue;

    // Push with move
    std::vector<int> vec1 = {1, 2, 3, 4, 5};
    EXPECT_TRUE(queue.push(std::move(vec1)));
    EXPECT_TRUE(vec1.empty());  // Should be moved from

    // Push with copy
    std::vector<int> vec2 = {6, 7, 8};
    EXPECT_TRUE(queue.push(vec2));
    EXPECT_FALSE(vec2.empty());  // Should still have data

    // Pop with move
    std::vector<int> result;
    EXPECT_TRUE(queue.pop(result));
    EXPECT_EQ(result, std::vector<int>({1, 2, 3, 4, 5}));  // FIFO

    EXPECT_TRUE(queue.pop(result));
    EXPECT_EQ(result, std::vector<int>({6, 7, 8}));
}

// Test 4: Single Producer Single Consumer (baseline)
TEST(MPSCQueueTest, SingleProducerSingleConsumer) {
    MPSCQueue<int> queue;
    constexpr int NUM_ITEMS = 10000;
    std::atomic<int> pop_count{0};
    std::atomic<bool> producer_done{false};

    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            queue.push(i);
        }
        producer_done.store(true, std::memory_order_release);
    });

    // Consumer thread
    std::thread consumer([&]() {
        int last_value = -1;
        int value;

        while (!producer_done.load(std::memory_order_acquire) ||
               !queue.empty()) {
            if (queue.pop(value)) {
                // FIFO: values should arrive in increasing order
                EXPECT_EQ(value, last_value + 1);
                last_value = value;
                pop_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(pop_count, NUM_ITEMS);
    EXPECT_TRUE(queue.empty());
}

// Test 5: Multiple Producer Single Consumer (main test)
TEST(MPSCQueueTest, MultipleProducerSingleConsumer) {
    MPSCQueue<int> queue;
    constexpr int NUM_PRODUCERS = 4;
    constexpr int ITEMS_PER_PRODUCER = 2500;
    constexpr int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    std::vector<int> popped_values;
    std::mutex popped_mutex;
    std::atomic<int> pop_count{0};
    std::atomic<int> push_count{0};

    // Multiple producer threads
    std::vector<std::thread> producers;
    for (int t = 0; t < NUM_PRODUCERS; ++t) {
        producers.emplace_back([&, t]() {
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                queue.push(t * 10000 + i);
                push_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Single consumer thread
    std::thread consumer([&]() {
        int value;
        while (pop_count < TOTAL_ITEMS) {
            if (queue.pop(value)) {
                {
                    std::lock_guard<std::mutex> lock(popped_mutex);
                    popped_values.push_back(value);
                }
                pop_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });

    // Wait for all producers
    for (auto& t : producers) t.join();

    // Wait for consumer to finish
    consumer.join();

    EXPECT_EQ(push_count, TOTAL_ITEMS);
    EXPECT_EQ(pop_count, TOTAL_ITEMS);
    EXPECT_TRUE(queue.empty());

    // Verify we received all values (check uniqueness)
    std::set<int> unique_values(popped_values.begin(), popped_values.end());
    EXPECT_EQ(unique_values.size(), TOTAL_ITEMS);
}

// Test 6: Concurrent push stress test
TEST(MPSCQueueTest, ConcurrentPushStress) {
    MPSCQueue<int> queue;
    constexpr int NUM_THREADS = 8;
    constexpr int OPS_PER_THREAD = 10000;
    constexpr int TOTAL_OPS = NUM_THREADS * OPS_PER_THREAD;

    std::atomic<int> push_count{0};
    std::vector<std::thread> threads;

    // All threads pushing concurrently
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                queue.push(t * 100000 + i);
                push_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads) t.join();

    // Consumer (main thread) pops all
    int value;
    int pop_count = 0;
    while (queue.pop(value)) {
        ++pop_count;
    }

    EXPECT_EQ(push_count, TOTAL_OPS);
    EXPECT_EQ(pop_count, TOTAL_OPS);
    EXPECT_TRUE(queue.empty());
}

// Test 7: Mixed operations with single consumer
TEST(MPSCQueueTest, MixedOperationsSingleConsumer) {
    MPSCQueue<int> queue;
    constexpr int DURATION_MS = 100;  // Short test
    std::atomic<bool> stop{false};
    std::atomic<int> total_pushes{0};
    std::atomic<int> total_pops{0};

    // Multiple producer threads
    std::vector<std::thread> producers;
    for (int t = 0; t < 4; ++t) {
        producers.emplace_back([&, t]() {
            std::mt19937 rng(t);
            std::uniform_int_distribution<int> dist(1, 10);

            while (!stop.load(std::memory_order_acquire)) {
                // Push random number of items
                int count = dist(rng);
                for (int i = 0; i < count; ++i) {
                    queue.push(t * 1000 + i);
                    total_pushes.fetch_add(1, std::memory_order_relaxed);
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    // Single consumer thread
    std::thread consumer([&]() {
        int value;
        while (!stop.load(std::memory_order_acquire)) {
            if (queue.pop(value)) {
                total_pops.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        }
    });

    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    stop.store(true, std::memory_order_release);

    for (auto& t : producers) t.join();
    consumer.join();

    // Pop any remaining items
    int value;
    while (queue.pop(value)) {
        total_pops.fetch_add(1, std::memory_order_relaxed);
    }

    EXPECT_GE(total_pushes, total_pops);  // Some might be in flight
    EXPECT_TRUE(queue.empty());

    std::cout << "Mixed ops: " << total_pushes << " pushes, "
              << total_pops << " pops" << std::endl;
}

// Test 8: Memory reclamation (no leaks)
TEST(MPSCQueueTest, NoMemoryLeak) {
    constexpr int NUM_OPERATIONS = 10000;

    {
        MPSCQueue<std::unique_ptr<int>> queue;

        // Push many unique_ptrs
        for (int i = 0; i < NUM_OPERATIONS; ++i) {
            queue.push(std::make_unique<int>(i));
        }

        // Pop half
        std::unique_ptr<int> ptr;
        for (int i = 0; i < NUM_OPERATIONS / 2; ++i) {
            EXPECT_TRUE(queue.pop(ptr));
        }

        // Other half will be destroyed when queue goes out of scope
    }

    // If there's a memory leak, valgrind or ASAN will catch it
    std::cout << "Memory test completed - check with valgrind/ASAN" << std::endl;
}

// Test 9: Performance benchmark
TEST(MPSCQueueTest, PerformanceBenchmark) {
    MPSCQueue<int> queue;
    constexpr int ITERATIONS = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    // Single-threaded push/pop benchmark
    for (int i = 0; i < ITERATIONS; ++i) {
        queue.push(i);
    }

    int value;
    for (int i = 0; i < ITERATIONS; ++i) {
        EXPECT_TRUE(queue.pop(value));
        EXPECT_EQ(value, i);  // FIFO order
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double ops_per_sec = (ITERATIONS * 2.0) / (duration.count() / 1e6);

    std::cout << "Single-thread performance: "
              << ops_per_sec / 1e6 << " million ops/sec" << std::endl;
    EXPECT_LT(duration.count(), 1000000);  // Should complete in <1 second
}

// Test 10: Empty queue operations
TEST(MPSCQueueTest, EmptyQueueOperations) {
    MPSCQueue<int> queue;

    // Multiple pops on empty queue should not crash
    int value = 42;
    EXPECT_FALSE(queue.pop(value));
    EXPECT_EQ(value, 42);  // Value should be unchanged

    EXPECT_FALSE(queue.pop(value));
    EXPECT_EQ(value, 42);

    EXPECT_TRUE(queue.empty());

    // Push then pop until empty
    queue.push(1);
    queue.push(2);

    EXPECT_TRUE(queue.pop(value));
    EXPECT_TRUE(queue.pop(value));
    EXPECT_FALSE(queue.pop(value));  // Now empty
}

// Test 11: Type safety with different types
TEST(MPSCQueueTest, DifferentTypes) {
    // Test with double
    MPSCQueue<double> double_queue;
    double_queue.push(3.14159);
    double dval = 0.0;
    EXPECT_TRUE(double_queue.pop(dval));
    EXPECT_DOUBLE_EQ(dval, 3.14159);

    // Test with string
    MPSCQueue<std::string> string_queue;
    string_queue.push("Hello");
    string_queue.push("World");
    std::string sval;
    EXPECT_TRUE(string_queue.pop(sval));
    EXPECT_EQ(sval, "Hello");  // FIFO
    EXPECT_TRUE(string_queue.pop(sval));
    EXPECT_EQ(sval, "World");
}

// Test 12: Large number of elements
TEST(MPSCQueueTest, LargeNumberOfElements) {
    MPSCQueue<int> queue;
    constexpr int LARGE_NUMBER = 100000;

    // Push large number
    for (int i = 0; i < LARGE_NUMBER; ++i) {
        queue.push(i);
    }

    EXPECT_FALSE(queue.empty());

    // Pop all (FIFO order)
    for (int i = 0; i < LARGE_NUMBER; ++i) {
        int value = -1;
        EXPECT_TRUE(queue.pop(value));
        EXPECT_EQ(value, i);
    }

    EXPECT_TRUE(queue.empty());
}

// Test 13: Producer-consumer with timing
TEST(MPSCQueueTest, ProducerConsumerLatency) {
    MPSCQueue<std::chrono::high_resolution_clock::time_point> queue;
    constexpr int NUM_MESSAGES = 1000;
    std::atomic<int> received{0};
    std::atomic<bool> stop{false};

    // Multiple producers sending timestamps
    std::vector<std::thread> producers;
    for (int t = 0; t < 2; ++t) {
        producers.emplace_back([&, t]() {
            for (int i = 0; i < NUM_MESSAGES / 2; ++i) {
                auto timestamp = std::chrono::high_resolution_clock::now();
                queue.push(timestamp);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    // Single consumer measuring latency
    std::thread consumer([&]() {
        std::chrono::high_resolution_clock::time_point timestamp;

        while (received < NUM_MESSAGES) {
            if (queue.pop(timestamp)) {
                auto now = std::chrono::high_resolution_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    now - timestamp).count();
                EXPECT_GE(latency, 0);  // Latency should be non-negative
                received.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
        stop.store(true, std::memory_order_release);
    });

    for (auto& t : producers) t.join();
    consumer.join();

    EXPECT_EQ(received, NUM_MESSAGES);
}

// Test 14: Interleaved producer activity
TEST(MPSCQueueTest, InterleavedProducers) {
    MPSCQueue<int> queue;
    constexpr int NUM_PRODUCERS = 3;
    constexpr int ITEMS_PER_PRODUCER = 100;

    std::vector<std::thread> producers;
    std::vector<std::vector<int>> producer_output(NUM_PRODUCERS);

    // Each producer pushes its sequence
    for (int t = 0; t < NUM_PRODUCERS; ++t) {
        producers.emplace_back([&, t]() {
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                int value = t * 1000 + i;
                queue.push(value);
                producer_output[t].push_back(value);
                std::this_thread::sleep_for(std::chrono::microseconds(t * 10));
            }
        });
    }

    // Single consumer
    std::vector<int> received;
    std::thread consumer([&]() {
        int value;
        int total_expected = NUM_PRODUCERS * ITEMS_PER_PRODUCER;
        while (received.size() < total_expected) {
            if (queue.pop(value)) {
                received.push_back(value);
            }
        }
    });

    for (auto& t : producers) t.join();
    consumer.join();

    // Verify all values received
    std::set<int> all_values;
    for (const auto& vec : producer_output) {
        all_values.insert(vec.begin(), vec.end());
    }

    std::set<int> received_set(received.begin(), received.end());
    EXPECT_EQ(all_values, received_set);
}

// Main function is provided by gtest_main