# Atomics - Complete Guide

## What Atomics Are Used For
Atomics are used to update and read variables atomically. They provide three fundamental guarantees:
1. **Atomicity**: Operations complete entirely or not at all (no torn reads/writes)
2. **Visibility**: Changes become visible to other threads according to memory ordering rules
3. **Ordering**: Control over compiler and CPU instruction reordering

## Memory Model and Data Requirements
The data inside atomics must be **trivially copyable**. This is required because:
- The CPU must be able to copy the data with a single instruction
- Memory must be contiguous for cache operations
- No non-trivial constructors/destructors or virtual functions

```cpp
// Allowed: trivially copyable
struct Point { int x, y; };
std::atomic<Point> p;  // ✅

// Not allowed: non-trivially copyable  
struct Bad {
    std::string name;    // ❌ Non-trivial
    virtual void f();    // ❌ Virtual function
};
std::atomic<Bad> b;     // ❌ Compile error
```

## How Atomics Work - CPU Architecture
Modern CPUs have multiple cores, each with private L1/L2 caches and shared L3 cache. When accessing variables:

1. **Load**: Read from cache into register
2. **Store**: Write from register to store buffer, then to cache
3. **Atomic RMW**: Read-Modify-Write atomically (requires special handling)

## The Actual Mechanism (Modern CPUs)
**Common misconception**: "Atomics lock the CPU buses"
**Reality**: Modern CPUs use **cache coherency protocols** (MESI) rather than bus locking:

### x86 Implementation:
```assembly
; Atomic increment
lock add dword ptr [mem], 1  ; LOCK prefix, not bus lock
```

What actually happens:
1. **Cache Line Lock**: The `LOCK` prefix locks only the relevant cache line
2. **Cache Coherency**: MESI protocol ensures consistency
3. **Invalidation**: Other cores' cache lines are invalidated/updated
4. **Atomic Operation**: The operation completes atomically on the cache line

### MESI Protocol States:
- **Modified**: Exclusive, dirty
- **Exclusive**: Exclusive, clean  
- **Shared**: Multiple copies exist
- **Invalid**: Not in cache or stale

## Visibility Guarantee - Important Correction
**Wrong**: "Both threads will see the changes immediately"
**Correct**: "Threads see changes according to memory ordering rules"

```cpp
std::atomic<int> x{0};
std::atomic<int> y{0};

// Thread 1
x.store(1, std::memory_order_relaxed);  // Not immediately visible everywhere
y.store(1, std::memory_order_release);  // Release barrier

// Thread 2
if (y.load(std::memory_order_acquire) == 1) {  // Acquire barrier
    // If we get here, we GUARANTEE x == 1
    // Release-acquire pairing creates synchronization
    assert(x.load(std::memory_order_relaxed) == 1);
}
```

## Memory Ordering Options
1. **memory_order_relaxed**: Atomicity only, no ordering guarantees
2. **memory_order_acquire**: Subsequent reads see writes from releasing thread
3. **memory_order_release**: Previous writes visible to acquiring threads
4. **memory_order_acq_rel**: Both acquire and release semantics
5. **memory_order_seq_cst**: Total sequential consistency (default)

## Atomics vs. Mutex Performance
**Correct statements from your explanation:**
- ✅ "Atomics are faster than mutexes"
- ✅ "Mutexes can do kernel calls which are expensive when lock is contended"

**Performance comparison:**
```cpp
// Atomic: Deterministic, typically 10-50 ns
std::atomic<int> counter;
counter.fetch_add(1, std::memory_order_relaxed);  // ~10-20 ns

// Mutex: Non-deterministic, depends on contention
int counter_m = 0;
std::mutex mtx;
{
    std::lock_guard<std::mutex> lock(mtx);  // 50 ns - ∞
    counter_m++;
}
```

**Key differences:**
1. **Timing**: Atomic operations have deterministic timing; mutex lock times vary
2. **Kernel Involvement**: Mutexes may involve kernel calls on contention (~1000 ns overhead)
3. **Blocking**: Atomics are lock-free/wait-free; mutexes block threads
4. **Cache Effects**: Both can cause cache line issues (false sharing)

## Practical Example
```cpp
class Counter {
private:
    // Align to prevent false sharing
    alignas(64) std::atomic<uint64_t> value{0};
    
public:
    void increment() {
        // Relaxed ordering sufficient for simple counter
        value.fetch_add(1, std::memory_order_relaxed);
    }
    
    uint64_t get() const {
        // Acquire ensures we see all increments
        return value.load(std::memory_order_acquire);
    }
    
    bool try_reset() {
        uint64_t expected = value.load(std::memory_order_relaxed);
        // CAS requires release to publish new value
        return value.compare_exchange_strong(
            expected, 0,
            std::memory_order_release,
            std::memory_order_relaxed);
    }
};
```

## Common Use Cases
1. **Counters and statistics**: Packet counters, metrics
2. **Flags and status indicators**: Shutdown flags, ready flags
3. **Reference counting**: Shared pointers
4. **Lock-free data structures**: Queues, stacks, hash maps
5. **Memory synchronization**: Publish-subscribe patterns

## HFT (High Frequency Trading) Specific Considerations

### Why Atomics in HFT?
- **Latency**: Mutex lock/unlock 50-100ns, atomic 5-10ns
- **Predictability**: Atomic operations have deterministic timing
- **No lock contention**: Lock-free algorithms prevent thread blocking
- **Cache friendly**: Properly aligned atomics minimize cache line bouncing

### HFT Atomic Patterns
```cpp
// Market data update - low latency
class MarketData {
    alignas(64) std::atomic<double> bid_price;
    alignas(64) std::atomic<double> ask_price;
    alignas(64) std::atomic<uint64_t> last_update_ts;
    
    void update(double bid, double ask) {
        bid_price.store(bid, std::memory_order_relaxed);
        ask_price.store(ask, std::memory_order_relaxed);
        last_update_ts.store(get_nanoseconds(), 
                           std::memory_order_release);
    }
};

// Order quantity update - atomic increment
std::atomic<int64_t> filled_quantity{0};
void add_fill(int64_t qty) {
    filled_quantity.fetch_add(qty, std::memory_order_relaxed);
}
```

### False Sharing Prevention in HFT
```cpp
// BAD: False sharing
struct BadHFTData {
    std::atomic<int64_t> pnl;      // Thread 1 updates
    std::atomic<int64_t> trades;   // Thread 2 updates
    // Same cache line → cache invalidations
};

// GOOD: Cache line separation
struct GoodHFTData {
    alignas(64) std::atomic<int64_t> pnl;     // Cache line 0
    alignas(64) std::atomic<int64_t> trades;  // Cache line 1
    // No false sharing
};
```

### Memory Ordering in HFT
```cpp
// Price publication with proper ordering
class PricePublisher {
    double latest_price;
    std::atomic<bool> valid{false};
    
    void publish(double price) {
        latest_price = price;
        valid.store(true, std::memory_order_release);
    }
    
    double get_price() {
        if (valid.load(std::memory_order_acquire)) {
            return latest_price;  // Always sees correct price
        }
        return 0.0;
    }
};
```

## Advanced Topics

### Compare-And-Swap (CAS)
```cpp
std::atomic<int> value{0};

// CAS pattern
int expected = value.load(std::memory_order_relaxed);
do {
    int desired = expected + 1;
} while (!value.compare_exchange_weak(
    expected, desired,
    std::memory_order_acq_rel,
    std::memory_order_relaxed));
```

### Double-Word CAS (CMPXCHG16B)
```cpp
// For atomic updates of two values
struct alignas(16) Order {
    double price;
    int64_t quantity;
};

// Requires 16-byte aligned memory
alignas(64) std::atomic<Order> current_order;
```

## Best Practices
1. **Choose appropriate memory ordering**: Use weakest order that satisfies requirements
2. **Prevent false sharing**: Use `alignas(64)` for frequently accessed atomics
3. **Avoid unnecessary atomics**: Not all shared data needs atomic access
4. **Measure performance**: Profile to ensure atomics actually improve performance
5. **Understand hardware**: Different CPUs have different atomic costs

## Common Pitfalls
1. **Assuming immediate visibility**: Changes propagate according to memory order
2. **Using wrong memory order**: `relaxed` when `acquire-release` needed
3. **False sharing**: Unaligned atomics causing cache invalidations
4. **ABA problem**: CAS may succeed incorrectly if value changes back
5. **Overusing atomics**: Sometimes mutexes are simpler and good enough

## Key Takeaways
1. Atomics provide atomicity with memory ordering guarantees, not immediate visibility
2. Modern CPUs use cache coherency protocols, not bus locking
3. The `LOCK` prefix on x86 operates at cache line granularity
4. Always specify appropriate memory ordering for your use case
5. Atomics are generally faster than mutexes but require deeper understanding
6. Watch for false sharing - use `alignas(64)` for frequently accessed atomics
7. In HFT, every nanosecond counts - choose atomics carefully


![[atomic-order-ss.png]]![[atomic-op-table.png]]