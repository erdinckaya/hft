# CPU Caches: The Complete Guide

## What Are CPU Caches?

CPU caches are **small, ultra-fast memory units** located on or near the CPU die that store frequently accessed data and instructions. They act as a **buffer between the CPU and slower main memory (RAM)**, dramatically reducing latency and improving overall system performance.

## Why Caches Exist: The Memory Hierarchy

### The Memory Hierarchy Pyramid
```
Registers (1 cycle)        → Fastest, smallest, most expensive
│
▼
L1 Cache (2-4 cycles)      → 32-64KB per core
│
▼  
L2 Cache (10-25 cycles)    → 256KB-1MB per core
│
▼
L3 Cache (30-50 cycles)    → 2-32MB shared
│
▼
Main Memory (100-300 cycles) → GBs
│
▼
Storage (millions of cycles) → TBs
```

**Key Insight**: Each level is **~10x slower** but **~10x larger** than the level above it.

## Cache Architecture & Levels

### 1. **L1 Cache** (Level 1)
- **Split into**: Instruction Cache (L1i) and Data Cache (L1d)
- **Size**: Typically 32-64KB per core
- **Latency**: 2-4 clock cycles
- **Fully dedicated to each core**

### 2. **L2 Cache** (Level 2)
- **Size**: 256KB to 1MB per core
- **Latency**: 10-25 cycles
- **Usually unified** (stores both instructions and data)
- **Can be shared or private** depending on architecture

### 3. **L3 Cache** (Level 3)
- **Size**: 2-32MB (modern CPUs)
- **Latency**: 30-50 cycles
- **Shared** among all cores
- **Serves as victim cache** for L2 evictions

## Fundamental Cache Concepts

### 1. **Cache Lines**
- Basic unit of data transfer between cache levels
- **Typical size**: 64 bytes (most modern CPUs)
- Why 64 bytes? Matches memory bus width and DRAM burst length

### 2. **Cache Organization**
Three main designs:

#### **Direct-Mapped Cache**
- Each memory block maps to **exactly one cache line**
- Simple but prone to **conflict misses**
- Formula: `Cache line = (Memory address / line size) % number of lines`

#### **Fully Associative Cache**
- Any memory block can go into **any cache line**
- Optimal placement but **expensive to search**
- Requires content-addressable memory (CAM)

	#### **Set-Associative Cache**
- **Compromise**: Cache divided into "ways" (typically 4, 8, or 16)
- Memory block can go into any line within its set
- **n-way set associative**: n possible locations for each block
- Most modern caches use this (L1: 8-way, L2: 16-way, L3: 16-20 way)

### 3. **Cache Policies**

#### **Write Policies**
- **Write-through**: Write to cache AND main memory simultaneously
- **Write-back**: Write only to cache; memory updated later when line evicted
- **Write-allocate**: On write miss, load cache line then write
- **No-write-allocate**: On write miss, write directly to memory

#### **Replacement Policies** (for associative caches)
- **LRU** (Least Recently Used): Track usage, evict oldest
- **pseudo-LRU**: Approximate LRU with less hardware
- **Random**: Simple but less effective
- **FIFO** (First-In, First-Out)

## Types of Cache Misses

### 1. **Compulsory Misses** (Cold Misses)
- First access to a memory location
- **Solution**: Prefetching

### 2. **Capacity Misses**
- Working set larger than cache size
- **Solution**: Larger caches, better algorithms

### 3. **Conflict Misses**
- Multiple memory blocks map to same cache line (in direct/set-associative)
- **Solution**: Higher associativity

### 4. **Coherence Misses** (in multi-core systems)
- Cache line invalidated by another core's write
- **Solution**: Better cache coherence protocols

## Cache Coherence in Multi-Core Systems

### The Problem
When multiple cores have their own caches, how do we keep data consistent?

### **MESI Protocol** (Most Common)
Four states per cache line:
- **M**odified: Dirty, exclusive to this cache
- **E**xclusive: Clean, exclusive to this cache
- **S**hared: Clean, possibly in other caches
- **I**nvalid: Not valid (or stale)

### Other Protocols
- **MOESI**: Adds Owned state
- **MESIF**: Adds Forward state (Intel)
- **Directory-based**: For many-core systems

## Optimization Techniques

### 1. **Prefetching**
- Hardware Prefetching: CPU detects access patterns
- Software Prefetching: Programmer hints (`prefetch` intrinsics)
- **Types**: Sequential, stride, software-directed

### 2. **Cache-Friendly Programming**
```c
// BAD: Column-major access in row-major array
for (int col = 0; col < N; col++)
    for (int row = 0; row < N; row++)
        sum += matrix[row][col];

// GOOD: Row-major access
for (int row = 0; row < N; row++)
    for (int col = 0; col < N; col++)
        sum += matrix[row][col];
```

### 3. **Data Structure Optimizations**
- **Structure of Arrays** (SoA) vs **Array of Structures** (AoS)
- **Padding** to cache line boundaries
- **Alignment** to cache line size

## Measuring Cache Performance

### Key Metrics
- **Hit Rate**: `hits / (hits + misses)`
- **Miss Rate**: `1 - hit rate`
- **AMAT** (Average Memory Access Time):
  ```
  AMAT = Hit time + (Miss rate × Miss penalty)
  ```

### Tools for Analysis
- **Linux**: `perf stat`, `perf record`, `valgrind --tool=cachegrind`
- **Intel**: VTune Profiler
- **Hardware counters**: `perf list | grep cache`

## Advanced Topics

### 1. **Non-Temporal Access**
- Bypass cache for streaming data (e.g., `movnt` instructions)
- Avoid polluting cache with data used only once

### 2. **Cache Partitioning**
- **Intel CAT**: Cache Allocation Technology
- Isolate critical applications from noisy neighbors

### 3. **Victim Caches**
- Small fully-associative cache for recently evicted lines
- Catches conflict misses

### 4. **Inclusive vs Exclusive Caches**
- **Inclusive**: L3 contains all lines in L2/L1 (Intel)
- **Exclusive**: No duplication between levels (AMD)
- **Non-inclusive**: Some duplication allowed

## Practical Implications

### For Programmers
- **Locality of reference**: Temporal and spatial
- **Loop tiling/blocking**: Process data in cache-sized chunks
- **False sharing**: Different cores writing to same cache line
```cpp
// False sharing example
struct Bad {
    int x;  // Core 1 writes here
    int y;  // Core 2 writes here - same cache line!
};

struct Good {
    alignas(64) int x;  // Separate cache line
    alignas(64) int y;  // Separate cache line
};
```

### For System Designers
- **NUMA** (Non-Uniform Memory Access) awareness
- **Cache-aware scheduling**
- **Memory bandwidth considerations**

## Modern CPU Examples

### Intel (Ice Lake / Alder Lake)
- L1: 48KB data + 32KB instruction per core
- L2: 1.25MB per core
- L3: Up to 30MB shared
- **Inclusive L3**, **non-inclusive mid-level cache**

### AMD (Zen 3 / Zen 4)
- L1: 32KB data + 32KB instruction
- L2: 512KB per core
- L3: 32-64MB per CCD
- **Exclusive cache hierarchy**

### Apple M-series
- **Unified memory architecture**
- Large shared L2/L3 caches
- Focus on bandwidth over latency

## Future Trends

1. **Larger caches**: L4 cache (eDRAM) in some CPUs
2. **3D stacking**: Cache on different die layers
3. **ML-based prefetchers**: Neural network predictors
4. **Specialized caches**: For AI workloads, encryption, etc.



# Why MESI in One Sentence:
**Because without it, multiple CPU cores would see different values for the same memory location, breaking the fundamental guarantee that computers must give consistent results.**

## The Core Problem:
When Core 1 updates a value in its cache, Core 2 might still have the old value in its cache → **inconsistent data**.

### MESI Protocol: The Full Picture
Each cache line can be in **one of four states**:
#### **M (Modified)**
- "I have the only valid copy, and it's different from memory"
- This cache has **exclusive ownership** and has **modified** the data
- Must write back to memory when evicted
- **Snoop response**: Invalidate other caches if they request
#### **E (Exclusive)**
- "I have the only copy, and it matches memory"
- Clean data, exclusive to this cache
- Can transition to Modified on write without informing others
- **Snoop response**: Share if requested (transition to Shared)
#### **S (Shared)**
- "I have a copy, others might too"
- Data is clean (matches memory)
- Read-only access
- **Snoop response**: No action needed for reads, invalidate on writes
#### **I (Invalid)**
- "This line doesn't contain valid data"
- Must fetch from another cache or memory on access


# TLB (Translation Lookaside Buffer)

## **What it is:**
A **small cache inside the CPU** that remembers recent virtual-to-physical memory address translations.
## **Where it is:**
Inside the **MMU (Memory Management Unit)** - part of the CPU core, closer than L1 cache.
## **Why important for HFT:**
Every memory access needs address translation. **TLB miss = 10-30 cycle penalty**. In HFT, this can add microseconds of latency when processing millions of messages.
## **HFT Impact:**
If your trading strategies jump around memory randomly (poor locality), you get **TLB thrashing** → constant translation misses → death by a thousand paper cuts in latency.