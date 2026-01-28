## 1. What NUMA is (clean definition)

**NUMA = Non-Uniform Memory Access**

> In a NUMA system, memory access time depends on **which CPU socket** accesses **which physical memory**.

* Each CPU socket has **local memory**
* Accessing **local memory is fast**
* Accessing **remote memory (another socket’s RAM) is slower**

**Uniform Memory Access (UMA)** → all memory same latency
**NUMA** → memory latency is *non-uniform*

---

## 2. Why NUMA exists (hardware reality)

Modern servers:

* Multiple CPU sockets
* Each socket has:

  * Its own memory controller
  * Its own RAM channels
* Sockets connected via:

  * Intel UPI
  * AMD Infinity Fabric

So:

* CPU 0 → RAM 0 = fast
* CPU 0 → RAM 1 = **cross-socket hop** = slower

💡 This is **physics**, not OS design.

---

## 3. Latency numbers (interview gold)

Typical order of magnitude:

| Access type         | Latency      |
| ------------------- | ------------ |
| L1 cache            | ~1 ns        |
| L2                  | ~3–5 ns      |
| L3                  | ~10–15 ns    |
| **Local NUMA RAM**  | ~80–100 ns   |
| **Remote NUMA RAM** | ~130–200+ ns |

> Remote NUMA access can be **1.5–2× slower**.

For HFT, that’s catastrophic.

---

## 4. Why NUMA matters in HFT (this is the punchline)

HFT is:

* Microseconds → nanoseconds
* Predictability > throughput

NUMA problems cause:

* Latency spikes
* Jitter
* Unexplained tail latency
* “Fast most of the time, slow sometimes” bugs

If your hot thread:

* Runs on socket 0
* Reads memory allocated on socket 1

→ You lose.

---

## 5. How NUMA bites you (common failure modes)

### 1️⃣ OS default behavior

* Linux allocates memory on **first touch**
* If thread migrates later → memory stays put
* Suddenly all accesses are remote

### 2️⃣ Thread migration

* Scheduler moves threads across sockets
* Cache + memory locality destroyed

### 3️⃣ Shared data structures

* One socket writes
* Another socket reads
* Cache line ping-pong across sockets

---

## 6. First-touch policy (VERY important)

Linux uses **first-touch NUMA allocation**:

> Memory is allocated on the NUMA node of the CPU that **first writes to it**.

Implication:

* Initialize memory on the same core that will use it
* Don’t allocate on one thread and consume on another (unless pinned)

Interview line:

> “Allocation locality is determined by first touch, not malloc.”

---

## 7. How HFT systems handle NUMA

### CPU pinning

* Pin threads to specific cores
* Never migrate

```bash
taskset
pthread_setaffinity_np
```

### Memory pinning

* Allocate memory on specific NUMA node

```bash
numactl --membind=0 --cpubind=0
```

or via `libnuma`:

```c
numa_alloc_onnode()
```

### One socket = one strategy

* Each strategy isolated per NUMA node
* No cross-socket sharing
* Replicate read-only data

---

## 8. NUMA vs cache coherence (they love this connection)

NUMA ≠ cache coherence, but they interact.

* Multi-socket systems use **MESI-like protocols**
* Cache lines move across sockets
* Writes cause invalidations
* Remote cache line ownership = latency spikes

Bad pattern:

```cpp
std::atomic<uint64_t> shared_counter;
```

Accessed by threads on different sockets → disaster.

---

## 9. NUMA-friendly design patterns

### ✅ Good

* Per-thread data
* Per-socket sharding
* Lock-free, socket-local queues
* Read-only replication

### ❌ Bad

* Global locks
* Shared counters
* Centralized allocators
* Cross-socket ring buffers

---

## 10. How to detect NUMA problems (interview flex)

* `numactl --hardware`
* `numastat`
* `perf c2c`
* Latency histograms (p99/p999 spikes)
* Sudden slowdowns after thread migration

---

## 11. One-liners to memorize (drop these casually)

* “NUMA issues show up as tail latency, not average latency.”
* “Local memory access is critical for predictable performance.”
* “First-touch allocation determines NUMA locality.”
* “Thread pinning without memory pinning is incomplete.”
* “Cross-socket cache line bouncing is a silent killer.”

---

## 12. If interviewer asks: “Explain NUMA in 30 seconds”

Say this:

> “NUMA means memory access latency depends on which CPU socket owns the memory. Local memory is fast, remote memory is significantly slower. In low-latency systems like HFT, we pin threads and allocate memory on the same NUMA node to avoid cross-socket access and unpredictable tail latency.”

That answer alone passes.
