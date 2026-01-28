
---

## 1. Why HFTs use DPDK (say this early)

If you open with this framing, you sound senior immediately.

**DPDK = user-space, polling-based, zero-copy packet processing**

Key motivations:

* Avoid kernel network stack (syscalls, context switches, interrupts)
* Deterministic latency > throughput
* Full control over NIC, queues, memory, CPU affinity

👉 Typical latency improvement: **tens of microseconds → sub-microsecond**

---

## 2. Architecture: explain this cleanly

You *will* be asked to “walk through how a packet flows”.

### Traditional Linux stack

NIC → IRQ → kernel → socket buffer → syscall → user space

### DPDK

NIC → **DMA directly into hugepage memory** → user-space poll loop

No interrupts. No syscalls. No copies.

### Core components you should name-drop

* **EAL (Environment Abstraction Layer)**

  * Initializes hugepages, cores, NUMA, PCI devices
* **PMD (Poll Mode Driver)**

  * NIC driver running in user space
* **mbuf**

  * Fixed-size packet buffer with metadata
* **rings**

  * Lockless queues (single/multi producer/consumer)

If you forget these names, you’ll look underprepared.

---

## 3. Hugepages & memory (VERY HFT-relevant)

Interviewers love memory questions.

### Why hugepages?

* Reduce TLB misses
* Physically contiguous → DMA-friendly
* Deterministic memory access

Typical sizes:

* 2MB (common)
* 1GB (used in ultra-low latency setups)

### mbuf layout

* Header (refcount, length, offloads)
* Data buffer
* Cache-line aligned

**Important point:**
DPDK favors **fixed-size buffers** → predictable allocation → no malloc/free in hot path.

Say this sentence verbatim:

> “We pre-allocate mbufs from mempools to avoid dynamic allocation on the critical path.”

---

## 4. Polling vs interrupts (classic question)

Expect:

> “Why polling? Isn’t that wasteful?”

Answer like this:

* Interrupts introduce jitter and tail latency
* Polling trades CPU for **deterministic latency**
* In HFT, CPUs are cheaper than microseconds

Bonus:

* Busy polling avoids frequency scaling and C-state transitions
* Often paired with `isolcpus`, `nohz_full`, CPU pinning

---

## 5. Multi-core, NUMA, and queue mapping

This is where seniors shine.

### Best practice

* One RX/TX queue per core
* Pin core to NIC queue
* Match memory locality (NUMA)

Say something like:

> “We bind each lcore to a NIC queue and allocate mempools from the same NUMA node to avoid cross-socket traffic.”

If they nod, you’re doing great.

---

## 6. Lock-free design (rings)

DPDK rings are:

* Single-producer/single-consumer (fastest)
* Multi-producer/multi-consumer (slower, still lock-free)

Know this:

* False sharing kills latency
* Cache-line alignment matters more than algorithmic complexity

Good soundbite:

> “In DPDK, cache misses are usually more expensive than extra instructions.”

---

## 7. Zero-copy & packet life cycle

Packet handling usually looks like:

```
rx_burst()
process()
tx_burst()
```

Key points:

* RX/TX in bursts (amortize overhead)
* Avoid touching packet data unless needed
* Free mbufs in bulk

Interview trick:
They might ask:

> “What happens if you forget to free mbufs?”

Answer:

* Memory leak
* RX stalls once mempool exhausted
* Packet drops → latency spikes

---

## 8. DPDK + C++ (important for you)

DPDK is C. You’re C++.

Be ready to explain:

* C wrappers around DPDK APIs
* No exceptions / RTTI in hot path
* `-fno-exceptions`, `-fno-rtti`
* Careful with virtual calls

Good line:

> “We treat the hot path like C with namespaces.”

---

## 9. Typical HFT use cases

Mention concrete patterns:

* Market data feed handlers
* UDP multicast decoding
* Custom TCP/UDP stacks
* Kernel bypass order gateways

And **why**:

* Predictable latency
* Control over retransmits, batching, pacing

---

## 10. Common pitfalls (interview gold)

If you mention even 2–3 of these, you look battle-tested.

* NUMA mismatch (NIC on socket 0, core on socket 1)
* Overfetching packet data into cache
* Using STL in hot path
* Logging or stats on RX thread
* Sharing cache lines between cores
* Running with power-saving BIOS settings

---

## 11. Comparison questions you may get

Be ready for quick contrasts:

### DPDK vs kernel-bypass alternatives

* **DPDK**: full control, more boilerplate
* **Solarflare / Onload**: easier, less control
* **RDMA**: lower latency but complexity, different semantics

### DPDK vs AF_XDP

* AF_XDP: still kernel-assisted
* DPDK: full user-space, lowest latency ceiling

---

## 12. One killer closing statement

If they ask:

> “When would you *not* use DPDK?”

Answer:

> “If throughput matters more than latency, or if operational complexity outweighs microsecond wins.”

That shows maturity.
