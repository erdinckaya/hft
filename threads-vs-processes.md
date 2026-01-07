### **1. The Core Conceptual Difference**

**Process** is an **instance of a running program** with:
- Its own **virtual address space** (memory isolation from other processes)
- System resources (file handles, sockets, etc.)
- At least **one thread of execution**
- Protected by OS: One process cannot directly access another's memory (except through IPC)

**Thread** is a **unit of execution within a process**:
- Shares the process's memory space (code, data, heap)
- Has its own stack, register set, and program counter
- Lightweight compared to processes
``` text
Process A (Browser)              Process B (Text Editor)
├── Memory Space A               ├── Memory Space B
├── Resources A                  ├── Resources B
├── Thread 1 (UI)                ├── Thread 1 (Auto-save)
├── Thread 2 (Network)           └── Thread 2 (Spell check)
└── Thread 3 (JavaScript Engine)
```

### **3. Creation & Context Switching Overhead**
Process creation:
- **Costly:** Duplicates entire memory space (copy-on-write optimization helps)
- **Time:** ~1ms for a moderately sized process
- **Memory:** Significant duplication (though COW reduces actual physical memory use)
Thread creation:
- **Inexpensive:** Only allocates stack (~8MB default) and thread control block
- **Time:** ~100μs (10x faster than process creation)
- **Memory:** Only stack overhead

**Context Switching Comparison:**

| Aspect             | Process Switch                       | Thread Switch                       |
| ------------------ | ------------------------------------ | ----------------------------------- |
| **TLB Flush**      | Usually required (new address space) | Not required (same address space)   |
| **Cache Impact**   | High (new working set)               | Moderate (shared working set)       |
| **OS Involvement** | Full kernel mode switch              | Can be lighter (user-level threads) |
| **Typical Cost**   | 1-10μs                               | 0.1-2μs                             |
### **4. Communication & Synchronization**

**Between Processes (IPC):**

- **Pipes:** `pipe()`, `mkfifo()`
- **Shared Memory:** `shmget()`, `mmap()`
- **Message Queues:** `mq_open()`, `msgget()`
- **Sockets:** Unix domain sockets, network sockets
- **Signals:** `kill()`, `sigaction()`
- **Files:** Lock files, memory-mapped files

**Between Threads:**

- **Shared Memory:** Direct pointer access (dangerous!)
- **Mutexes:** `pthread_mutex_t`, `std::mutex`
- **Condition Variables:** `pthread_cond_t`, `std::condition_variable`
- **Semaphores:** Counting/binary semaphores
- **Atomic Operations:** `std::atomic`, CAS operations
- **Barriers:** `pthread_barrier_t`, `std::barrier` (C++20)

### **5. Practical Considerations & Trade-offs**

**When to use Processes:**

- Need memory isolation (security/stability)
- Running independent applications
- Leveraging multi-CPU systems with independent workloads
- Fault tolerance (one process crash doesn't affect others)
    

**When to use Threads:**

- Need to share data efficiently
- I/O-bound applications (handling many connections)
- Parallel computation on shared data structures
- Responsive UI applications


**Common Threading Bugs:**

1. **Data Races:** Multiple threads access shared data without synchronization
2. **Deadlocks:** Circular wait for resources
3. **Livelocks:** Threads keep changing state but make no progress
4. **Priority Inversion:** Low-priority thread holds lock needed by high-priority thread
5. **False Sharing:** Threads modifying different variables on same cache line

###  **Best Practices for Seasoned Engineers**

1. **Minimize Shared State:** Design for minimal data sharing between threads
2. **Use Higher-Level Abstractions:** Prefer `std::async`, thread pools, parallel algorithms over raw threads
3. **Think in Tasks, Not Threads:** Focus on work to be done, not thread management
4. **Profile Before Optimizing:** Threading bugs are expensive - measure first
5. **Test Under Load:** Many threading issues only appear under heavy concurrency



| Senaryo                                | Mümkün mü?               | Nasıl?                | Örnek               |
| -------------------------------------- | ------------------------ | --------------------- | ------------------- |
| **1 core'da 2 thread AYNI ANDA**       | ✅ Evet (Hyper-Threading) | Pipeline interleaving | Intel i7, AMD Ryzen |
| **1 core'da 2 thread SIRAYLA**         | ✅ Evet (time slicing)    | Context switching     | Tüm CPU'lar         |
| **1 core'da 2 process**                | ✅ Evet                   | Process switching     | Tüm OS'ler          |
| **1 core'da 2 process AYNI ANDA**      | ❌ Hayır                  | -                     | -                   |
| **Thread farklı core'lara geçebilir**  | ✅ Evet                   | Migration             | Load balancing      |
| **Process farklı core'lara geçebilir** | ✅ Evet                   | Migration             | OS scheduler        |


![[thread.cpp]]