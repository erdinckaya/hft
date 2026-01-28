
# Big Picture (what interviewers actually test)

They’re not checking if you know definitions. They want to know:

* **Latency vs reliability trade-offs**
* **Kernel/network stack behavior**
* **When TCP is a bad idea**
* **Why multicast exists**
* **How packet loss is handled in real trading systems**
* **What breaks at scale**

If you can reason about *why* things are used, you’ll stand out.

---

# TCP (Transmission Control Protocol)

### Why TCP exists

* Reliable, ordered, congestion-controlled byte stream
* Guarantees **delivery**, **ordering**, **no duplication**

### How TCP works (must-know internals)

* **3-way handshake**: SYN → SYN-ACK → ACK
  → Adds latency before first byte
* **Sequence numbers + ACKs**
* **Retransmissions**
* **Flow control** (receiver window)
* **Congestion control** (cwnd)

### TCP latency killers (HFT red flags)

| Feature               | Why it’s bad                      |
| --------------------- | --------------------------------- |
| Nagle’s Algorithm     | Buffers small packets             |
| Delayed ACKs          | Waits before ACK                  |
| Retransmissions       | Latency spikes                    |
| Head-of-Line Blocking | One lost packet blocks everything |
| Kernel buffering      | Adds jitter                       |

👉 **HFT fix**:

```cpp
setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, ...)
```

### Why TCP is still used in HFT

* **Order entry**
* **Risk management**
* **Trade confirmations**

Because:

* Losing an order is worse than being slow
* Exchanges often **require TCP**

### Interview killer line

> “TCP optimizes for throughput and fairness, not tail latency. In HFT, tail latency is often more important than average latency.”

---

# UDP (User Datagram Protocol)

### What UDP really is

* No connection
* No ordering
* No retransmission
* No congestion control
* Message-oriented (datagrams)

### Why HFT loves UDP

* **Lowest possible latency**
* No head-of-line blocking
* No kernel retries
* One packet = one message

### The dark side of UDP

| Problem         | Impact             |
| --------------- | ------------------ |
| Packet loss     | You must handle it |
| Reordering      | Happens at scale   |
| Duplication     | Rare but real      |
| No backpressure | Receiver can drop  |

### How HFT systems handle UDP loss

* **Sequence numbers in payload**
* **Gap detection**
* **Request retransmit via TCP**
* **Snapshot + incremental feed**

> Market data feeds almost always combine **UDP multicast + TCP recovery**

### Interview killer line

> “UDP trades correctness for determinism. In trading, we prefer knowing *now* that data is missing rather than finding out later.”

---

# Multicast (the secret sauce)

### What multicast is

* One sender → many receivers
* Implemented using **UDP**
* Network replicates packets (not sender)

### Why exchanges use multicast

* Market data fan-out
* Same packet, same time, same latency
* Fair dissemination

### Multicast vs Unicast UDP

|            | Multicast | Unicast   |
| ---------- | --------- | --------- |
| Bandwidth  | Efficient | Expensive |
| Fairness   | High      | Low       |
| Complexity | Higher    | Lower     |

### Multicast gotchas (very interviewable)

* **Packet loss is normal**
* **No built-in recovery**
* **Receiver joins late → misses data**
* **NIC / switch buffer overruns**

### Real HFT multicast architecture

```
[Exchange]
   |
UDP Multicast
   |
[Feed Handler]
   |
TCP Snapshot / Recovery
   |
Order Book Builder
```

### Interview killer line

> “Multicast gives fairness but not reliability. Reliability is added *at the application layer*, not the transport layer.”

---

# Kernel & OS Stuff (this is where senior candidates win)

### Socket buffers

```bash
net.core.rmem_max
net.core.wmem_max
```

* Too small → drops
* Too big → cache misses

### Busy polling / kernel bypass

* `SO_BUSY_POLL`
* DPDK / Solarflare Onload / Mellanox VMA
* User-space networking

> Expect questions like: *“How do you reduce kernel jitter?”*

### NUMA awareness

* NIC IRQ affinity
* Thread pinning
* Memory locality

### Timestamping

* HW vs SW timestamps
* PTP (Precision Time Protocol)

---

# TCP vs UDP vs Multicast (HFT summary table)

| Use case       | Protocol      | Why                 |
| -------------- | ------------- | ------------------- |
| Market data    | UDP Multicast | Fan-out + fairness  |
| Order entry    | TCP           | Reliability         |
| Recovery       | TCP           | Guaranteed delivery |
| Internal feeds | UDP           | Low latency         |
| Risk checks    | TCP           | Correctness > speed |

---

# Common Interview Traps ⚠️

**Q:** Why not use TCP for market data?
**A:** Head-of-line blocking and retransmissions cause unpredictable latency spikes.

**Q:** Why not use UDP for orders?
**A:** Losing an order is catastrophic; retransmissions must be reliable.

**Q:** What happens if multicast packet is lost?
**A:** Detect via sequence numbers → request snapshot or gap fill via TCP.

**Q:** How do exchanges ensure fairness?
**A:** Multicast + deterministic packet timing.

---

# One-Liners You Can Drop Casually 💣

* “Tail latency matters more than mean latency in trading.”
* “TCP is optimized for throughput, not determinism.”
* “Multicast gives fairness; TCP gives correctness.”
* “We handle packet loss explicitly instead of hiding it.”

---

# TCP Handshake (in real detail)

TCP is **stateful**. Before any data flows, both sides must agree on:

* Sequence numbers
* Window sizes
* Capabilities (timestamps, SACK, etc.)

## 1️⃣ Three-Way Handshake

### Step 1: SYN

Client → Server

```
SYN, seq = x
```

Client says:

> “I want to talk, and I’ll start counting bytes from x.”

What’s happening internally:

* Socket moves from `CLOSED` → `SYN_SENT`
* Initial Sequence Number (ISN) chosen (randomized for security)
* TCP options sent:

  * MSS
  * Window Scale
  * SACK permitted
  * Timestamps (optional)

---

### Step 2: SYN-ACK

Server → Client

```
SYN, ACK = x+1, seq = y
```

Server says:

> “I heard you. I’ll start at y, and I expect x+1 next.”

Internals:

* Server allocates **connection state**
* Moves socket to `SYN_RECEIVED`
* Half-open connection exists

⚠️ **SYN flood vulnerability** lives here.

---

### Step 3: ACK

Client → Server

```
ACK = y+1
```

Client says:

> “We’re synced. Let’s send data.”

Both sides:

* Move to `ESTABLISHED`
* Data can now flow

---

## 🔥 Why this handshake hurts in HFT

### Latency cost

* Minimum **1 RTT before first byte**
* Worse over WAN or congested networks

### State cost

* Kernel allocates buffers
* TCP control blocks created
* Cache pollution

### Failure impact

* If SYN or SYN-ACK is dropped → handshake retries
* Adds jitter and long tail latency

---

## TCP Fast Open (TFO)

Some systems allow:

```
SYN + DATA
```

But:

* Not universally supported
* Still stateful
* Rare in exchange connectivity

👉 Don’t oversell TFO in interviews.

---

# TCP Teardown (often overlooked)

Connection close also has a handshake:

```
FIN → ACK → FIN → ACK
```

Problems:

* `TIME_WAIT`
* Port exhaustion
* Delays reconnects

💡 HFT systems:

* Keep TCP connections **long-lived**
* Avoid reconnects during trading hours

---

# UDP “Handshake” (or lack thereof)

### Key point

👉 **UDP has no handshake at transport layer**

You can send data immediately:

```
sendto()
```

No:

* SYN
* ACK
* State
* RTT wait

### What UDP really does

* Wraps payload in IP packet
* Best-effort delivery

That’s it.

---

## But… HFT Still Adds “Handshakes” on top of UDP

Interviewers LOVE this nuance.

### Application-level handshake (common pattern)

Example:

```
Client → Server: LOGIN (seq=0)
Server → Client: LOGIN_ACK
Client → Server: SUBSCRIBE_FEED
```

Why?

* Identity
* Permissions
* Sequence alignment
* Recovery starting point

This is **not UDP doing it**, but the **application protocol**.

---

# Multicast Handshake (IGMP)

Multicast does have a *network-level* handshake:

### IGMP Join

Receiver → Network

```
IGMP JOIN group 239.x.x.x
```

Switch/router:

* Starts forwarding multicast traffic

But:

* Sender is unaware
* No reliability
* No confirmation

💡 This is why:

> “Joining late means missing data forever unless recovery exists.”

---

# Comparing Handshakes (Interview Table)

| Protocol  | Handshake | State     | RTT Cost | Reliability |
| --------- | --------- | --------- | -------- | ----------- |
| TCP       | 3-way     | Yes       | ≥1 RTT   | Built-in    |
| UDP       | None      | No        | 0        | None        |
| UDP + App | Custom    | App-level | Optional | App-defined |
| Multicast | IGMP      | Network   | Minimal  | None        |

---

# Packet Loss During Handshake (Great Interview Angle)

### TCP

* SYN lost → retransmit
* SYN-ACK lost → retransmit
* Exponential backoff
* Latency spike

### UDP

* No retries
* App must detect silence
* Timeouts are **explicit**

👉 HFT prefers **knowing immediately** that something failed.

---

# One-Liners to Drop 💣

* “TCP handshake establishes shared state; UDP avoids shared state entirely.”
* “UDP removes the RTT tax.”
* “Multicast handshakes are about group membership, not reliability.”
* “HFT systems shift reliability from transport to application layer.”

---

# Common Follow-Up Questions They May Ask

* Can TCP send data before handshake completes?
* How does SYN flood affect latency?
* Why is TIME_WAIT a problem?
* How do you resync a UDP feed?
* What happens if IGMP join is slow?
