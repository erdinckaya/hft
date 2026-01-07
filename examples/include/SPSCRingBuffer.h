#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <utility>

template<typename T, std::size_t CAPACITY>
class SPSCRingBuffer {
    static_assert((CAPACITY & (CAPACITY - 1)) == 0,
    "CAPACITY must be a power of two for performance");
    static_assert(CAPACITY >= 2, "CAPACITY must be at least 2");

public:
    // Producer interface (single thread only)
    bool push(const T& item) noexcept {
        size_t tail = mTail.load(std::memory_order_relaxed);
        size_t next = (tail + 1) & (CAPACITY - 1);

        if (next == mHead.load(std::memory_order_acquire)) {
            return false;  // Buffer full
        }

        mBuffer[tail] = item;
        mTail.store(next, std::memory_order_release);
        return true;
    }

    bool push(T&& item) noexcept {
        size_t tail = mTail.load(std::memory_order_relaxed);
        size_t next = (tail + 1) & (CAPACITY - 1);

        if (next == mHead.load(std::memory_order_acquire)) {
            return false;  // Buffer full
        }

        mBuffer[tail] = std::move(item);
        mTail.store(next, std::memory_order_release);
        return true;
    }

    // Consumer interface (single thread only)
    bool pop(T& item) noexcept {
        size_t head = mHead.load(std::memory_order_relaxed);

        if (head == mTail.load(std::memory_order_acquire)) {
            return false;  // Buffer empty
        }

        item = std::move(mBuffer[head]);
        mHead.store((head + 1) & (CAPACITY - 1), std::memory_order_release);
        return true;
    }

    // Utility functions
    bool empty() const noexcept {
        return mHead.load(std::memory_order_acquire) ==
               mTail.load(std::memory_order_acquire);
    }

    bool full() const noexcept {
        size_t tail = mTail.load(std::memory_order_acquire);
        size_t next = (tail + 1) & (CAPACITY - 1);
        return next == mHead.load(std::memory_order_acquire);
    }

    size_t size() const noexcept {
        size_t tail = mTail.load(std::memory_order_acquire);
        size_t head = mHead.load(std::memory_order_acquire);
        return (tail - head + CAPACITY) & (CAPACITY - 1);
    }

    size_t capacity() const noexcept {
        return CAPACITY - 1;  // One slot always empty
    }

private:
    static constexpr size_t CACHE_LINE = std::hardware_destructive_interference_size;

    // Optimized cache layout
    alignas(CACHE_LINE) std::atomic<size_t> mHead{0};
    alignas(CACHE_LINE) std::atomic<size_t> mTail{0};
    alignas(CACHE_LINE) std::array<T, CAPACITY> mBuffer;
};