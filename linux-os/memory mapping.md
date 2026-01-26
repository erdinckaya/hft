# Memory Mapping in HFT – Interview-Ready Notes

These notes present a **correct, precise, and interview-grade** understanding of memory mapping (`mmap`) and its role in **High-Frequency Trading (HFT)** systems, tailored for a **Senior C++ Engineer**.

---

## 1. What `mmap` Actually Does

`mmap()` maps a file or anonymous memory into a process’s **virtual address space**.

- The process accesses the memory using **normal loads/stores**
- No explicit `read()` / `write()` syscalls on the hot path
- Backed by **physical pages managed by the kernel**

Important clarification:

- `mmap` does **not eliminate copying entirely**
- It avoids _explicit user-space buffer copies_
- Data still moves:
    - Disk → page cache
    - NIC → memory (unless kernel-bypass is used)

### Key Property

`mmap` lets user space directly access kernel-managed memory **without repeated syscalls**.

---

## 2. `munmap` Semantics (Correct Behavior)

`munmap()`:

- Immediately removes the virtual memory mapping
- Does **not guarantee immediate physical memory release**
- Dirty pages may be written back later (kernel-managed)

Notes:

- Write-back is **not automatic**
- `msync()` can request flushing, but persistence is usually irrelevant in HFT

---

## 3. `mmap` vs `read()` – Why Mapping Is Faster

### `read()` Path

- Syscall per read
- Kernel → user copy
- User-managed buffers

### `mmap()` Path

- One-time syscall to map
- Page faults on first access
- No per-access syscall
- No extra user-space buffer

### Sharing Advantage

- Multiple processes can map the **same file** with `MAP_SHARED`
- Different virtual addresses, **same physical pages**
- Zero-copy _inter-process sharing_

Important:

- You cannot "share an address" across processes
- You share **physical memory**, not virtual addresses

---

## 4. What `mmap` Does _Not_ Do

Common misconceptions (often tested in interviews):

- ❌ Does NOT eliminate kernel involvement
- ❌ Does NOT avoid page faults
- ❌ Does NOT make memory deterministic by itself

Reality:

- Page faults are kernel events
- TLB misses still occur
- Kernel scheduling still matters

---

## 5. Why `mmap` Is Useful in HFT

### 1. Deterministic Memory Layout

- Fixed addresses after initialization
- No allocator jitter
- Predictable memory access patterns

### 2. Reduced Syscall Frequency

- No `read()` / `write()` in the hot path
- Kernel only involved on faults or teardown

### 3. Cache & TLB Warming

- Pre-touch pages during startup
- Populate:
    - Page tables
    - TLB entries
    - Cache lines

### 4. Fast IPC

- Shared memory ring buffers
- Lock-free structures
- No kernel mediation during steady state

---

## 6. What `mmap` Is _Not_ Used For in HFT

`mmap` is **not** used for:

- Direct NIC RX/TX buffers
- Market data socket I/O
- Kernel networking fast paths

These are handled by **kernel-bypass frameworks**.

---

## 7. Correct Network I/O Model in HFT

### Kernel Networking (Avoided)

- Sockets
- Interrupts
- Context switches
- Scheduler noise

### Kernel Bypass (Preferred)

- DPDK / Solarflare Onload / RDMA
- User-space polling
- DMA directly into user-space memory

No sockets. No UDP stack. No page cache.

---

## 8. Correct Execution Chain (End-to-End)

### Startup (Cold Path)

1. Reserve hugepages
2. Initialize DPDK
3. Allocate DMA-capable memory
4. Set up RX/TX rings
5. Pin threads to isolated CPU cores
6. Disable interrupts
7. Pre-fault and warm memory

### Runtime (Hot Path)

1. NIC receives packet
2. NIC DMA writes packet into user-space RX ring
3. Polling loop detects packet
4. Decode market data
5. (Optional) Write normalized data to shared memory ring (`mmap`)
6. Strategy reads shared memory
7. Decision → order → TX ring → NIC DMA    

Kernel involvement on the hot path: **none**

---

## 9. Where `mmap` Fits in This Architecture

Typical uses:

- Shared memory between:
    - Market data handler
    - Strategy engine
    - Risk engine
- Pre-allocated control structures
- Occasionally logging or replay buffers

Not used for:

- NIC DMA regions
- Packet RX/TX    

---

## 10. Interview-Ready Summary (Memorizable)

> In HFT systems, `mmap` is primarily used for deterministic shared memory and low-latency IPC. It allows multiple processes to share physical memory pages without explicit copying or per-access syscalls. Memory is pre-allocated and pre-faulted to avoid page faults during trading. For network I/O, kernel-bypass frameworks like DPDK are used, where the NIC DMA writes directly into user-space memory backed by hugepages, eliminating kernel networking overhead and reducing latency variance.

---

## 11. Common Follow-Up Topics to Expect

- Major vs minor page faults
- TLB shootdowns
- Hugepages tradeoffs
- `MAP_SHARED` vs `MAP_PRIVATE`
- False sharing in shared memory rings
- Polling vs interrupts
- NUMA locality