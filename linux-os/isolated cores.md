# Isolated CPU Cores for High-Frequency Trading (HFT)

## What Are Isolated Cores?
Isolated cores (also called core isolation or CPU shielding) involve dedicating specific CPU cores exclusively to critical processes by removing them from the general Linux kernel scheduler. In HFT systems, this typically means:
- **Core Pinning**: Locking specific applications to specific CPU cores
- **IRQ Isolation**: Preventing interrupts from reaching isolated cores
- **CPU Affinity**: Ensuring only designated processes run on specific cores
- **Kernel Boot Parameters**: Using `isolcpus` to exclude cores from general scheduling

## Primary Usages in HFT

### 1. **Low-Latency Market Data Processing**
- Dedicated cores for parsing exchange feeds (FIX/FAST binary protocols)
- Minimizing jitter when processing market-by-order or market-by-price data

### 2. **Strategy Execution Engines**
- Exclusive cores for alpha models and signal generation
- Running co-located strategies near exchange matching engines

### 3. **Order Management Systems**
- Dedicated cores for order routing and execution
- Ensuring predictable response times for fill processing

### 4. **Network Stack Processing**
- Isolating cores for NIC polling (DPDK-style packet processing)
- Handling TCP/UDP sockets with minimal context switching

## Importance in HFT

### **Latency Reduction**
- Eliminates context switching overhead (saving 1-100 microseconds)
- Reduces CPU cache thrashing (L1/L2/L3 cache stays hot)
- Minimizes pipeline stalls from unpredictable interruptions

### **Deterministic Performance**
- Provides consistent execution timing (critical for sub-microsecond strategies)
- Eliminates "noisy neighbor" problems in multi-tenant environments
- Ensures predictable worst-case execution times

### **Tail Latency Improvement**
- Reduces latency spikes (99.9th and 99.99th percentiles)
- Critical for strategies where consistency matters more than average speed

## Trade-Offs and Considerations

### **1. Resource Utilization**
- **Con**: Underutilization of expensive hardware (dedicated cores sit idle during low activity)
- **Pro**: Predictable performance during peak load spikes

### **2. System Complexity**
- **Con**: Requires specialized kernel tuning and monitoring
- **Con**: Complicates load balancing and failover scenarios
- **Pro**: Once tuned, provides stable, reproducible performance

### **3. Cost vs. Benefit**
- **Cost**: Need 20-50% more cores for same throughput
- **Benefit**: Latency improvements of 2-10x in worst-case scenarios
- **ROI Calculation**: Must justify hardware cost against potential alpha gains

### **4. Operational Overhead**
- **Con**: Requires dedicated system administrators with low-latency expertise
- **Con**: Difficult to maintain across kernel updates and hardware changes
- **Pro**: Competitive advantage for firms with specialized expertise

### **5. Hardware Limitations**
- **NUMA Effects**: Must consider memory locality and PCIe lane allocation
- **Thermal Throttling**: Isolated cores can't share thermal load effectively
- **Hyper-Threading Conflicts**: Must disable HT on isolated cores

## Implementation Patterns

### **Common Configurations:**
```
# Typical 16-core HFT server setup:
- Cores 0-1: OS/system tasks
- Cores 2-5: Market data processing (isolated)
- Cores 6-9: Strategy engines (isolated) 
- Cores 10-11: Order management (isolated)
- Cores 12-15: Backup/reserve cores
```

### **Kernel Boot Parameters Example:**
```
isolcpus=2-11 nohz_full=2-11 rcu_nocbs=2-11
```

## When Isolation Matters Most

### **Prioritize Isolation When:**
- Trading latency-sensitive strategies (arbitrage, market-making)
- Operating in highly competitive venues (CME, EUREX, major equity exchanges)
- Running strategies sensitive to microsecond-level jitter

### **Consider Alternatives When:**
- Running statistical/strategic arbitrage with longer holding periods
- Trading in less competitive markets
- Budget-constrained or running lower-frequency strategies

## Future Considerations
- **Custom silicon**: Some firms moving toward FPGA/ASIC solutions
- **SmartNICs**: Offloading network processing to specialized hardware
- **Kernel bypass**: Using technologies like Solarflare's OpenOnload or Arista's Latency Analyzer

Core isolation remains a fundamental technique in the HFT arms race, providing predictable low-latency performance despite its resource inefficiency. The trade-off essentially boils down to: **pay more for hardware to ensure you never miss opportunities due to system unpredictability.**


---

# 1. What Is a CPU Core? (Very Basic)

A **CPU (processor)** has multiple **cores**.

- A **core** is a physical unit that can execute instructions
    
- The operating system (Linux) schedules programs (“threads”) onto cores
    
- By default, **all programs share all cores**
    

This sharing causes problems for **low-latency programs**.

---

# 2. What Problem Are Isolated Cores Solving?

In HFT, programs must respond in **microseconds**.

Problems with shared cores:

- The OS may pause your program to run another task
    
- Hardware interrupts can interrupt your program
    
- Other programs can evict data from the CPU’s cache
    

These cause **unpredictable delays**, even if they’re short.

💡 **Key idea**:

> Isolated cores are cores that only your critical program is allowed to run on.

---

# 3. What Is an “Isolated Core” (Plain English)

An **isolated core** is:

- A CPU core
    
- Reserved only for one important program
    
- The operating system avoids using it for other work
    

The goal is **predictability**, not speed.

---

# 4. Why HFT Interviews Care About This

HFT firms care about:

- **Worst-case delay**, not average speed
    
- Missing one market event can cost money
    

So they ask:

> “How do you prevent the OS from interfering with your program?”

---

# 5. What Interferes With a Core?

Let’s name the sources of interference clearly.

---

## 5.1 The OS Scheduler

The **scheduler** decides:

- Which program runs
    
- On which core
    
- For how long
    

Normally:

- Your program runs
    
- Then gets paused
    
- Another program runs
    
- Then your program resumes
    

This pause is called a **context switch**.

Even a short pause can be bad.

---

## 5.2 Interrupts (Very Important)

An **interrupt** is when hardware says:

> “Stop what you’re doing, handle this now”

Examples:

- Network packet arrives
    
- Timer fires
    
- Disk operation finishes
    

Interrupts can:

- Stop your program
    
- Run kernel code
    
- Resume your program later
    

This adds **latency spikes**.

---

## 5.3 CPU Cache (Simple Explanation)

Each core has **small, very fast memory** called cache.

If another program runs:

- It may overwrite cache data
    
- Your program must reload data
    
- This costs time
    

---

# 6. How Linux Isolates a Core (Step by Step)

Now we talk about _how_ isolation is done.

---

## 6.1 Telling Linux “Don’t Use These Cores”

Linux can be told at boot time:

> “Do not schedule normal programs on these cores.”

This is called **CPU isolation**.

Effect:

- Normal OS work goes to other cores
    
- Your critical program gets exclusive use
    

---

## 6.2 Pinning Your Program to a Core

You explicitly say:

> “This program runs only on core 2.”

This is called **CPU pinning**.

Result:

- Your program never moves between cores
    
- Cache stays warm
    
- Behavior is predictable
    

---

## 7. What Is a NIC? (You Asked)

**NIC = Network Interface Card**

- The hardware that sends and receives network packets
    
- Market data arrives through the NIC
    
- The NIC generates interrupts when packets arrive
    

Important:

- NIC interrupts can run on any core unless controlled
    
- You must keep them **away from isolated cores**
    

---

## 8. What Is NUMA? (You Asked)

**NUMA = Non-Uniform Memory Access**

On large servers:

- The CPU is split into **sockets**
    
- Each socket has its **own memory**
    
- Accessing local memory is fast
    
- Accessing memory from another socket is slower
    

Think of it as:

> “Some memory is closer than other memory”

Bad setup:

- Program runs on one socket
    
- Memory is on another socket  
    → extra delay
    

Good setup:

- Program, memory, and NIC all on the same socket
    

---

## 9. What Is SMT? (You Asked)

**SMT = Simultaneous Multi-Threading**  
(also called **Hyper-Threading** by Intel)

One physical core pretends to be **two logical cores**.

They share:

- Execution units
    
- Caches
    

Problem:

- Another thread can steal resources
    
- Causes unpredictable timing
    

HFT practice:

- Disable SMT
    
- Or never use the sibling thread
    

---

# 10. What an Interviewer Expects at Junior/Mid Level

You do **not** need kernel-developer knowledge.

You should be able to say:

> “We isolate cores so the OS, interrupts, and other programs do not interfere with latency-critical code. We pin the program to those cores and keep network interrupts and memory close to them.”

That alone is a **good answer**.

---

# 11. Simple Mental Model (Remember This)

Imagine:

- One core = one race car track
    
- No pedestrians
    
- No traffic lights
    
- No other cars
    

That’s an isolated core.

---
