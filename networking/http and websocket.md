
## 1. Big Picture (What interviewers want you to say)

> **HTTP is request/response, stateless, higher overhead.
> WebSocket is persistent, full-duplex, low-latency streaming.
> In HFT, HTTP is mostly for control/REST; WebSocket (or raw TCP/UDP) is for market data & order flow.**

If you can say that confidently and then *go deep*, you’re already ahead.

---

## 2. HTTP — what matters for HFT (not web dev)

### Core characteristics

* **Stateless**
* **Client → request → server → response**
* Usually **short-lived connections** (unless keep-alive)
* Text-heavy headers (esp. HTTP/1.1)

### HTTP versions (VERY interview-relevant)

#### HTTP/1.1

* Head-of-line blocking
* One request at a time per connection
* High header overhead
* ❌ Bad for low latency

#### HTTP/2

* Multiplexing over single TCP connection
* Binary framing
* Header compression (HPACK)
* Still **TCP HOL blocking**
* ⚠️ Better, but still not ideal for market data

#### HTTP/3

* QUIC over UDP
* Avoids TCP HOL blocking
* Rare in HFT today (complexity, jitter)

💡 **HFT Insight**
Most HFT shops:

* Use HTTP **only** for:

  * Authentication
  * Configuration
  * Risk limits
  * Non-latency-critical control paths

---

### Latency costs in HTTP (you should mention these)

* TCP handshake
* TLS handshake
* Header parsing
* Memory allocations
* Kernel ↔ user space transitions

> Interview line they love:
> “HTTP is fine when latency is milliseconds; HFT cares about microseconds or nanoseconds.”

---

## 3. WebSocket — why HFT likes it (but still isn’t perfect)

### What WebSocket really is

* Starts as **HTTP handshake**
* Upgrades to **persistent TCP connection**
* **Full-duplex**
* Message-based (frames)

### Key advantages

* No repeated handshakes
* Server can push data
* Lower per-message overhead
* Better for **streaming market data**

### Latency profile

* Still TCP
* Still kernel networking
* Still congestion control
* But **much lower application-layer overhead** than HTTP

### Why exchanges offer WebSockets

* Easier than FIX/TCP
* Browser compatible
* Reasonable performance for most clients

---

## 4. The elephant in the room (say this to sound senior)

> **Serious HFT does NOT rely on WebSocket for the fastest paths.**

They often use:

* Raw TCP
* FIX over TCP
* Proprietary binary protocols
* UDP multicast (market data)
* Kernel bypass (Solarflare, Mellanox)
* DPDK / RDMA / AF_XDP

WebSocket is:

* For **retail / mid-frequency**
* For **less latency-sensitive feeds**
* For **convenience, not ultimate speed**

If you say this, interviewers will nod.

---

## 5. WebSocket internals (common interview questions)

### Framing

* Small header (2–14 bytes)
* Masking (client → server)
* Opcode (text/binary/ping/pong)

### Binary vs text

* **Always binary for trading**
* No JSON if latency matters
* Pre-allocated buffers
* Zero-copy parsing

### Backpressure

* If consumer is slow → buffers grow → latency explodes
* Need:

  * Drop policies
  * Ring buffers
  * Lock-free queues

---

## 6. TCP issues you MUST mention

### Nagle’s Algorithm

* Coalesces small packets
* ❌ Kills latency
* **Disable with `TCP_NODELAY`**

### Delayed ACKs

* Adds ~40ms in worst cases
* Interaction with Nagle is deadly

### Congestion control

* TCP assumes fairness, not speed
* Packet loss = latency spikes

> Good line:
> “TCP guarantees delivery, not latency.”

---

## 7. C++-specific interview angles (VERY important)

### Memory management

* No dynamic allocation on hot path
* Object pools
* Pre-allocated buffers
* Cache-line alignment

### Parsing

* Avoid string parsing
* Avoid virtual calls
* Avoid exceptions
* Branch-predictable code

### Threading model

* One thread per socket (often pinned)
* CPU affinity
* Busy polling instead of blocking
* No mutexes on hot path

### Kernel interaction

* `epoll` vs busy-poll
* `SO_REUSEPORT`
* Huge pages
* NIC offloading awareness

---

## 8. Sample interview questions & killer answers

### Q: Why not HTTP for market data?

**A:**
“HTTP is request-response and stateless, which adds unnecessary latency and overhead. Market data is a continuous stream, so persistent, push-based protocols like WebSocket or raw TCP are better.”

---

### Q: Is WebSocket fast enough for HFT?

**A:**
“It’s faster than HTTP, but still limited by TCP and kernel overhead. For true HFT, firms use binary protocols over TCP or UDP multicast with kernel bypass.”

---

### Q: Biggest latency bottleneck in WebSocket?

**A:**
“TCP congestion control, kernel transitions, memory allocation, and backpressure in the application layer.”

---

### Q: How do you optimize a WebSocket client in C++?

**A:**

* Binary frames only
* Pre-allocated buffers
* Disable Nagle
* CPU pinning
* Zero-copy parsing
* Lock-free queues

That answer alone is gold.

---

## 9. One-minute mental model (memorize this)

* **HTTP** → control plane
* **WebSocket** → streaming, mid-freq
* **TCP/UDP binary** → real HFT
* **Latency killers** → allocations, syscalls, locks
* **Latency savers** → persistence, pre-allocation, kernel bypass
