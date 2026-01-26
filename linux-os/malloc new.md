
# Memory Management in HFT C++ Systems - Senior Engineer Interview Guide

## Overview
Understanding memory management nuances is critical for HFT systems where performance predictability is paramount. This guide covers why standard memory allocation is problematic and what alternatives are used.

---

## Why Standard Allocation is Problematic in HFT

### 🚫 Performance Overhead
| Issue | Impact | Typical Latency |
|-------|--------|-----------------|
| **System Call Overhead** | `malloc`/`new` → kernel calls (`brk`/`mmap`) | 1000+ ns |
| **Lock Contention** | Global allocator locks in multi-threaded environments | Variable, up to µs |
| **Cache Inefficiency** | Poor locality, scattered heap allocations | 10-100 ns penalty |

### ⚠️ Non-Deterministic Latency
- **Allocation time varies** with heap state (fragmentation, free lists)
- **Page faults** can cause µs-level delays (unacceptable for HFT)
- **Memory exhaustion** handling unpredictable
- **TLB thrashing** from dispersed allocations

### 🔍 Memory Placement Issues
- **NUMA-unaware allocations**: Memory may come from wrong NUMA node
- **False sharing**: Unrelated objects share cache lines
- **Poor alignment**: Standard allocators don't guarantee cache-line alignment

### 📉 Fragmentation Problems
- **External fragmentation**: Memory gaps reduce usable space
- **Internal fragmentation**: Padding wastes memory
- **Memory blow-up**: Forces more OS memory requests

---

## HFT Memory Management Strategies

### ✅ Strategy 1: Pre-allocation & Object Pools

```cpp
// Pre-allocate at startup
class OrderProcessor {
    static constexpr size_t MAX_ORDERS = 1000000;
    static constexpr size_t CACHE_LINE = 64;
    
    struct alignas(CACHE_LINE) Order {
        uint64_t order_id;
        double price;
        uint32_t quantity;
        // ... other fields
    };
    
    Order orders_buffer_[MAX_ORDERS];
    std::atomic<size_t> next_free_{0};
    
public:
    Order* allocate_order() {
        size_t idx = next_free_.fetch_add(1, std::memory_order_acq_rel);
        if (idx >= MAX_ORDERS) throw std::runtime_error("Out of orders");
        return &orders_buffer_[idx];
    }
    
    void deallocate_order(Order*) {
        // Simple counter-based or reuse logic
    }
};
```

**Interview Question**: *How would you handle the "out of orders" case in production?*

### ✅ Strategy 2: Custom Allocators

```cpp
// Arena allocator for temporary data
class ArenaAllocator {
    char* buffer_;
    size_t size_;
    std::atomic<size_t> offset_{0};
    
public:
    ArenaAllocator(size_t size) : size_(size) {
        buffer_ = static_cast<char*>(aligned_alloc(64, size));
    }
    
    void* allocate(size_t size, size_t alignment = 8) {
        // Align the offset
        size_t current = offset_.load(std::memory_order_acquire);
        size_t aligned = (current + alignment - 1) & ~(alignment - 1);
        
        if (aligned + size > size_) return nullptr;
        
        offset_.store(aligned + size, std::memory_order_release);
        return buffer_ + aligned;
    }
    
    void reset() { offset_.store(0, std::memory_order_release); }
    
    ~ArenaAllocator() { free(buffer_); }
};

// Usage
void process_market_data(ArenaAllocator& arena) {
    auto* quotes = static_cast<Quote*>(arena.allocate(sizeof(Quote) * 1000, 64));
    // Process...
    arena.reset(); // Fast cleanup
}
```

**Interview Question**: *How would you make this allocator thread-safe without locks?*

### ✅ Strategy 3: Stack Allocation & SBO

```cpp
// Small Buffer Optimization for message types
class MarketMessage {
    static constexpr size_t STACK_SIZE = 256;
    
    union {
        char stack_buffer_[STACK_SIZE];
        void* heap_buffer_;
    };
    size_t size_;
    bool on_heap_{false};
    
public:
    MarketMessage(size_t size) : size_(size) {
        if (size <= STACK_SIZE) {
            on_heap_ = false;
        } else {
            on_heap_ = true;
            heap_buffer_ = aligned_alloc(64, size);
        }
    }
    
    ~MarketMessage() {
        if (on_heap_) free(heap_buffer_);
    }
    
    void* data() { 
        return on_heap_ ? heap_buffer_ : stack_buffer_;
    }
};
```

**Interview Question**: *When would SBO actually hurt performance?*

### ✅ Strategy 4: Memory Mapping & Huge Pages

```cpp
class MappedMarketData {
    void* data_{nullptr};
    size_t size_{0};
    
public:
    MappedMarketData(size_t size) : size_(size) {
        // Request huge pages for better TLB performance
        data_ = mmap(nullptr, size, 
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                     -1, 0);
        
        if (data_ == MAP_FAILED) {
            // Fallback to standard pages
            data_ = mmap(nullptr, size,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS,
                         -1, 0);
        }
        
        // Lock pages in memory to prevent swapping
        mlock(data_, size);
    }
    
    ~MappedMarketData() {
        if (data_) {
            munlock(data_, size_);
            munmap(data_, size_);
        }
    }
};
```

**Interview Question**: *What are the trade-offs of using huge pages?*

---

## Interview Questions & Answers

### **Q1: How do you avoid heap allocations in hot paths?**
**A:** 
- Pre-allocate all necessary memory during initialization
- Use ring buffers for message passing
- Implement object pools for frequently allocated types
- Use stack allocation via SBO or alloca() carefully
- Employ arena allocators that reset between batches

### **Q2: What metrics would you monitor for memory performance?**
**A:**
- **Cache misses** (L1, L2, L3)
- **TLB misses** and page walks
- **Memory bandwidth** utilization
- **Allocation latency distribution** (p50, p99, p999)
- **Heap fragmentation** over time
- **NUMA node cross-accesses**

### **Q3: How would you design a lock-free allocator?**
**A:**
```cpp
class LockFreePool {
    struct Node { std::atomic<Node*> next; };
    std::atomic<Node*> head_{nullptr};
    char* block_{nullptr};
    
public:
    LockFreePool(size_t count, size_t obj_size) {
        block_ = aligned_alloc(64, count * obj_size);
        // Build free list
        for (size_t i = 0; i < count; ++i) {
            Node* node = reinterpret_cast<Node*>(block_ + i * obj_size);
            node->next.store(head_.load(std::memory_order_relaxed));
            while(!head_.compare_exchange_weak(node->next, node));
        }
    }
    
    void* allocate() {
        Node* old_head = head_.load(std::memory_order_acquire);
        while (old_head && 
               !head_.compare_exchange_weak(old_head, old_head->next));
        return old_head;
    }
    
    void deallocate(void* ptr) {
        Node* node = static_cast<Node*>(ptr);
        node->next.store(head_.load(std::memory_order_relaxed));
        while (!head_.compare_exchange_weak(node->next, node));
    }
};
```

### **Q4: How do you handle memory exhaustion in HFT?**
**A:**
- **Pre-allocated emergency pools** for critical paths
- **Degraded mode operation** with reduced functionality
- **Circuit breakers** to stop accepting new work
- **Static analysis** to guarantee upper bounds
- **Redundancy** with failover to backup systems

### **Q5: What's the impact of false sharing and how to avoid it?**
**A:**
**Impact:** Cache line ping-pong between cores, 100+ ns penalties
**Solutions:**
```cpp
// Pad critical data to cache line size
struct alignas(64) Counter {
    std::atomic<uint64_t> value;
    char padding[64 - sizeof(std::atomic<uint64_t>)];
};

// Separate frequently-written data
struct Writer {
    alignas(64) std::atomic<uint64_t> write_count;
    // ... other fields
};
```

---

## Best Practices Summary

### ✅ **Do:**
- Pre-allocate all memory during initialization
- Use custom allocators tailored to access patterns
- Align data to cache line boundaries
- Use memory mapping for large datasets
- Implement object pools for frequent allocations
- Profile with cache-aware tools (perf, VTune)

### ❌ **Don't:**
- Use `new`/`delete` in hot paths
- Rely on standard allocator for performance
- Ignore NUMA topology
- Allow swapping of critical memory
- Use variable-length allocations in real-time paths

### 🔧 **Tools to Master:**
- `perf` for cache profiling
- `numactl` for NUMA control
- `mlock()` to prevent swapping
- `madvise()` for access pattern hints
- Custom allocator instrumentation

---

## Performance Numbers to Remember

| Operation | Typical Latency | HFT Target |
|-----------|----------------|------------|
| L1 cache hit | 1 ns | ✓ |
| L2 cache hit | 4 ns | ✓ |
| L3 cache hit | 15 ns | ✓ |
| RAM access | 100 ns | Marginal |
| `malloc`/`new` | 100-1000 ns | ❌ |
| Page fault | 10,000 ns | ❌ |
| Context switch | 1000-5000 ns | ❌ |

---

## Sample Interview Scenario

**Interviewer**: "We're seeing latency spikes in our order matching engine. How would you investigate and fix memory-related issues?"

**Your Answer Structure:**
1. **Measurement**: First, I'd use `perf` to check cache miss rates and TLB misses
2. **Allocation Analysis**: Instrument allocators to track allocation patterns
3. **Hot Path Audit**: Review critical code paths for any `new`/`delete`
4. **Solutions**:
   - Convert heap allocations to object pools
   - Ensure cache line alignment
   - Use huge pages for large data structures
   - Implement arena allocators for batch processing
5. **Verification**: A/B test with realistic loads, monitor tail latency

---

## Further Reading
1. *Efficient Memory Management for High-Frequency Trading* - ACM Queue
2. *What Every Programmer Should Know About Memory* - Ulrich Drepper
3. Intel Optimization Manuals for cache-aware programming
4. CppCon talks on custom allocators and memory mapping

---

*Remember: In HFT interviews, focus on predictability first, then raw speed. A slower but predictable system often outperforms a faster but jittery one.*

















### First: what problem is NUMA trying to solve?

Imagine a computer with **multiple CPUs** (or CPU cores).  
All of them need to read and write **memory (RAM)**.

If _every_ CPU had to go through the same memory door, that door would get crowded and slow. 🚪🚶🚶🚶

NUMA is a way to organize memory so things stay fast.

---

### What does NUMA stand for?

**N**on-**U**niform **M**emory **A**ccess

Scary name, simple idea.

---

### The basic idea (the key takeaway)

> **Some memory is closer to a CPU, and some memory is farther away.**

- **Close memory = fast**
- **Far memory = slower**

And that difference **matters**.

---

### A simple analogy 🏠

Think of a big house with roommates:

- Each roommate (CPU) has **their own fridge** (local memory)
- There’s also a **shared fridge in the basement** (remote memory)

If you grab food from:

- **Your own fridge** → super fast
- **Basement fridge** → slower, stairs, annoying

NUMA just admits this reality and designs the system around it.

---

### How a NUMA system is structured

- The computer is split into **NUMA nodes**
    
- Each node has:
    
    - One or more CPUs
    - Some memory that is **local** to those CPUs

So:

- CPU accesses **its own node’s memory** → fast
- CPU accesses **another node’s memory** → slower

That’s the “non-uniform” part.

---

### How this differs from the old way (UMA)

**UMA (Uniform Memory Access):**

- Every CPU accesses all memory at the same speed
- Simple, but doesn’t scale well for big machines

**NUMA:**

- Memory speed depends on _where_ the CPU and memory are
- More complex, but **much faster at scale**

---

### Why NUMA exists (why you should care)

NUMA is used because:

- Modern servers have **many CPUs**
- Sharing one memory pool becomes a bottleneck
- NUMA lets systems scale without becoming painfully slow

If software is NUMA-aware:

- Faster performance
- Better cache usage
- Less waiting

If software ignores NUMA:

- Random slowdowns 😬
- “Why is this server fast sometimes and slow other times?”

---

### One sentence summary 🧠

> **NUMA means memory access time depends on how close the memory is to the CPU that’s using it.**