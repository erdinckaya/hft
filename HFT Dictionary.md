
# HFT Systems Glossary (Plain English)

---

## CPU & EXECUTION

### **CPU (Central Processing Unit)**

The main processor that runs programs.

**Why it matters:**  
All latency ultimately comes from how fast the CPU can execute instructions.

---

### **Core**

A physical processing unit inside the CPU that runs instructions.

**Why it matters:**  
HFT systems often dedicate (isolate) cores for critical work.

---

### **Thread**

A single execution flow inside a program.

**Why it matters:**  
Threads are what get scheduled onto cores.

---

### **Process**

A running program, which can have one or more threads.

---

### **Context Switch**

When the OS pauses one thread and runs another.

**Why it matters:**  
Context switches introduce unpredictable delays.

**Interview signal:**  
You want fewer or zero context switches on critical threads.

---

### **Instruction Pipeline**

The CPU processes instructions in stages.

**Why it matters:**  
Stalls in the pipeline cause latency.

---

## CPU SHARING & ISOLATION

### **Isolated Core**

A CPU core reserved exclusively for a latency-critical thread.

**Why it matters:**  
Prevents OS and other programs from interfering.

---

### **CPU Pinning (Affinity)**

Forcing a thread to run only on specific cores.

**Why it matters:**  
Keeps cache warm and behavior predictable.

---

### **Scheduler**

The OS component that decides which thread runs on which core.

**Why it matters:**  
The scheduler introduces jitter (random delays).

---

## MEMORY & CACHES

### **Memory (RAM)**

Main system memory.

**Why it matters:**  
Accessing memory is much slower than accessing cache.

---

### **Cache**

Small, very fast memory close to the CPU.

Types:

- **L1**: Fastest, smallest
    
- **L2**
    
- **L3**: Shared between cores
    

**Why it matters:**  
Cache misses cause latency spikes.

---

### **Cache Miss**

When data is not in cache and must be fetched from memory.

---

### **False Sharing**

Two threads modify different variables that live on the same cache line.

**Why it matters:**  
Causes unnecessary cache invalidations.

---

### **Cache Line**

The smallest unit of data transferred between memory and cache (usually 64 bytes).

---

### **TLB (Translation Lookaside Buffer)**

A cache for virtual-to-physical memory address translations.

**Why it matters:**  
TLB misses slow down memory access.

---

### **Huge Pages**

Larger memory pages to reduce TLB misses.

---

## OPERATING SYSTEM

### **Kernel**

The core of the OS that manages hardware.

---

### **User Space**

Where normal programs run.

---

### **Kernel Space**

Where the OS runs.

**Why it matters:**  
Transitions between user and kernel space are expensive.

---

### **System Call**

When a program asks the kernel to do something.

**Why it matters:**  
System calls add latency.

---

### **Interrupt**

A signal that forces the CPU to stop current work and handle an event.

Examples:

- Network packet arrival
    
- Timer tick
    

**Why it matters:**  
Interrupts cause unpredictable pauses.

---

### **Interrupt Affinity**

Choosing which cores handle interrupts.

**Why it matters:**  
Interrupts must stay off isolated cores.

---

### **Timer Tick**

A periodic interrupt used by the OS.

**Why it matters:**  
Adds regular jitter if not disabled.

---

### **Busy Polling**

Actively checking for events instead of waiting for interrupts.

**Why it matters:**  
Lower latency, higher CPU usage.

---

## NETWORKING

### **NIC (Network Interface Card)**

The hardware that sends and receives network packets.

**Why it matters:**  
Market data and orders go through the NIC.

---

### **Packet**

A chunk of network data.

---

### **Kernel Bypass**

Sending/receiving packets without going through the OS kernel.

**Why it matters:**  
Avoids interrupts and system calls.

Examples:

- DPDK
    
- Solarflare Onload
    

---

### **Polling vs Interrupts**

- **Interrupts**: CPU is notified when data arrives
    
- **Polling**: CPU repeatedly checks for data
    

HFT prefers polling for predictability.

---

## MULTI-CPU SYSTEMS

### **Socket**

A physical CPU package on the motherboard.

---

### **NUMA (Non-Uniform Memory Access)**

Each CPU socket has its own local memory.

**Why it matters:**  
Accessing remote memory is slower.

---

### **NUMA Node**

A group of cores and memory that are close together.

---

### **NUMA Local Memory**

Memory attached to the same socket as the core.

---

## CPU HARDWARE FEATURES

### **SMT (Simultaneous Multi-Threading)**

One physical core appears as two logical cores.

(Also called Hyper-Threading)

**Why it matters:**  
Threads compete for shared resources.

---

### **Logical Core**

What the OS sees as a core (may be shared).

---

### **Physical Core**

Actual hardware execution unit.

---

### **C-States**

CPU sleep states.

**Why it matters:**  
Waking up causes latency.

---

### **P-States**

CPU frequency scaling states.

**Why it matters:**  
Frequency changes affect predictability.

---

## PERFORMANCE & LATENCY

### **Latency**

Time it takes to respond to an event.

---

### **Throughput**

How much work is done per unit time.

---

### **Tail Latency**

Worst-case latency (e.g., 99.9th percentile).

**Why it matters:**  
HFT cares more about tail latency than averages.

---

### **Jitter**

Variation in latency.

---

### **Deterministic Performance**

Predictable timing behavior.

---

## INTERVIEW-IMPORTANT CONCEPTS

### **Warm Cache**

Data already present in cache.

---

### **Cold Cache**

Cache needs to be filled again.

---

### **Busy Core**

A core running a loop continuously (no sleeping).

---

### **Noise**

Any unexpected work that delays your thread.

Examples:

- OS tasks
    
- Interrupts
    
- Other programs
    

---

## GOLDEN INTERVIEW PHRASES (MEMORIZE)

- “We optimize for **tail latency**, not average.”
    
- “Isolation is about **predictability**.”
    
- “Interrupts and OS noise are the biggest sources of jitter.”
    
- “We pin threads and memory to reduce movement.”
    
