#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <utility>

/**
 * @brief A Single Producer Single Consumer (SPSC) lock-free ring buffer.
 *
 * This ring buffer provides high-performance, low-latency data transfer
 * between exactly one producer thread and one consumer thread without locks.
 *
 * @tparam T The type of elements stored in the buffer.
 * @tparam CAPACITY Maximum number of elements the buffer can hold.
 *                 Must be a power of two for performance optimization.
 *
 * Features:
 * - Lock-free: No mutexes, spinlocks, or kernel calls
 * - Bounded: Fixed capacity prevents unbounded memory growth
 * - Cache-friendly: Variables aligned to prevent false sharing
 * - Wait-free operations: Push and pop complete in bounded time
 * - Move-aware: Supports efficient move semantics
 * - Exception-safe: All operations are noexcept
 *
 * Design:
 * - One slot is always kept empty to distinguish between full and empty states
 * - Power of two capacity enables efficient modulo via bitwise AND
 * - Separate cache lines for head/tail prevent false sharing
 *
 * Usage Constraints:
 * - Exactly ONE thread may call push() methods (Producer)
 * - Exactly ONE thread may call pop() methods (Consumer)
 * - Violating single-threaded constraints causes undefined behavior
 *
 * Typical Use Cases:
 * - Audio processing pipelines
 * - Network packet buffering
 * - Real-time data acquisition systems
 * - Inter-thread communication in HFT systems
 */
template<typename T, std::size_t CAPACITY>
class SPSCRingBuffer {
    // Compile-time validation of capacity requirements
    static_assert((CAPACITY & (CAPACITY - 1)) == 0,
                  "CAPACITY must be a power of two for bitwise modulo optimization");
    static_assert(CAPACITY >= 2, "CAPACITY must be at least 2 to hold at least one element");

public:
    /**
     * @brief Push an item onto the buffer (copy version).
     *
     * @param item The item to push.
     * @return true if the item was successfully pushed.
     * @return false if the buffer is full.
     *
     * Thread Safety: May ONLY be called by the single producer thread.
     *
     * Memory Ordering:
     * - relaxed tail load: Only this thread modifies tail
     * - acquire head load: Must see consumer's head updates
     * - release tail store: Make pushed item visible to consumer
     *
     * Time Complexity: O(1), wait-free
     */
    bool push(const T& item) noexcept {
        // Load current tail position (relaxed: only we write to tail)
        size_t tail = mTail.load(std::memory_order_relaxed);

        // Calculate next tail position with bitwise modulo
        // Equivalent to (tail + 1) % CAPACITY but faster
        size_t next = (tail + 1) & (CAPACITY - 1);

        // Check if buffer is full by comparing with consumer's head
        // acquire ensures we see latest consumer progress
        if (next == mHead.load(std::memory_order_acquire)) {
            return false;  // Buffer full, cannot push
        }

        // Store item at current tail position
        mBuffer[tail] = item;

        // Update tail to make item available to consumer
        // release ensures item is visible before tail update
        mTail.store(next, std::memory_order_release);
        return true;
    }

    /**
     * @brief Push an item onto the buffer (move version).
     *
     * @param item The item to move onto the buffer.
     * @return true if the item was successfully pushed.
     * @return false if the buffer is full.
     *
     * More efficient than copy for types with expensive copy operations.
     * Same thread safety and performance characteristics as copy version.
     */
    bool push(T&& item) noexcept {
        size_t tail = mTail.load(std::memory_order_relaxed);
        size_t next = (tail + 1) & (CAPACITY - 1);

        if (next == mHead.load(std::memory_order_acquire)) {
            return false;  // Buffer full
        }

        // Move item into buffer (more efficient for large objects)
        mBuffer[tail] = std::move(item);
        mTail.store(next, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pop an item from the buffer.
     *
     * @param item Reference to store the popped item.
     * @return true if an item was successfully popped.
     * @return false if the buffer is empty.
     *
     * Thread Safety: May ONLY be called by the single consumer thread.
     *
     * Memory Ordering:
     * - relaxed head load: Only this thread modifies head
     * - acquire tail load: Must see producer's tail updates
     * - release head store: Signal to producer that slot is free
     *
     * Time Complexity: O(1), wait-free
     */
    bool pop(T& item) noexcept {
        // Load current head position (relaxed: only we write to head)
        size_t head = mHead.load(std::memory_order_relaxed);

        // Check if buffer is empty by comparing with producer's tail
        // acquire ensures we see latest producer progress
        if (head == mTail.load(std::memory_order_acquire)) {
            return false;  // Buffer empty, nothing to pop
        }

        // Move item out of buffer for efficiency
        item = std::move(mBuffer[head]);

        // Update head to free the slot for producer
        // release ensures item consumption is visible before head update
        mHead.store((head + 1) & (CAPACITY - 1), std::memory_order_release);
        return true;
    }

    // ==================== UTILITY FUNCTIONS ====================

    /**
     * @brief Check if the buffer is empty.
     *
     * @return true if the buffer contains no elements.
     * @return false if the buffer contains at least one element.
     *
     * Note: Result is instantaneous and may change immediately after call.
     * Intended for monitoring/debugging, not for control flow.
     */
    bool empty() const noexcept {
        // Both loads use acquire to get consistent snapshot
        return mHead.load(std::memory_order_acquire) ==
               mTail.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if the buffer is full.
     *
     * @return true if no more elements can be pushed.
     * @return false if there is space for at least one more element.
     *
     * Note: Result is instantaneous and may change immediately after call.
     */
    bool full() const noexcept {
        size_t tail = mTail.load(std::memory_order_acquire);
        size_t next = (tail + 1) & (CAPACITY - 1);
        return next == mHead.load(std::memory_order_acquire);
    }

    /**
     * @brief Get the current number of elements in the buffer.
     *
     * @return size_t Approximate number of elements (0 to capacity()).
     *
     * Note: Result is instantaneous and approximate due to concurrent
     * producer/consumer activity. Useful for monitoring, not synchronization.
     *
     * Implementation uses bitwise arithmetic to handle wrap-around:
     * - When tail >= head: size = tail - head
     * - When tail < head:  size = CAPACITY - head + tail
     * Combined formula: (tail - head + CAPACITY) & (CAPACITY - 1)
     */
    size_t size() const noexcept {
        size_t tail = mTail.load(std::memory_order_acquire);
        size_t head = mHead.load(std::memory_order_acquire);
        // Bitwise formula for efficient wrap-around calculation
        return (tail - head + CAPACITY) & (CAPACITY - 1);
    }

    /**
     * @brief Get the maximum number of elements the buffer can hold.
     *
     * @return size_t Maximum number of storable elements (CAPACITY - 1).
     *
     * Note: One slot is always kept empty to distinguish between
     * full and empty states without additional synchronization.
     */
    size_t capacity() const noexcept {
        return CAPACITY - 1;  // One slot always empty for full/empty distinction
    }

private:
    // Modern CPU cache line size (typically 64 bytes)
    static constexpr size_t CACHE_LINE = std::hardware_destructive_interference_size;

    /**
     * @brief Consumer's read position (head).
     *
     * Aligned to cache line boundary to prevent false sharing with
     * producer's tail variable. False sharing occurs when variables
     * on the same cache line are accessed by different CPU cores.
     *
     * Only modified by consumer thread, read by producer thread.
     */
    alignas(CACHE_LINE) std::atomic<size_t> mHead{0};

    /**
     * @brief Producer's write position (tail).
     *
     * Aligned to cache line boundary to prevent false sharing.
     *
     * Only modified by producer thread, read by consumer thread.
     */
    alignas(CACHE_LINE) std::atomic<size_t> mTail{0};

    /**
     * @brief Fixed-size circular buffer storage.
     *
     * Aligned to cache line to minimize interference with head/tail.
     * Uses std::array for compile-time size and stack allocation.
     */
    alignas(CACHE_LINE) std::array<T, CAPACITY> mBuffer;

    // Note: Default constructor is sufficient (zero-initialization)
    // Note: Copy/move operations are deleted due to atomic members
    // Note: Destructor is trivial (no dynamic allocation to clean up)
};