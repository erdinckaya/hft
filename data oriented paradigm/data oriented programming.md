
## The core idea (in HFT terms)

**DOP = optimize for the memory hierarchy, not the type system.**

In HFT, you’re almost never CPU-bound in the “too many instructions” sense. You’re:

- **L1/L2 miss bound**
- **TLB miss bound**
- **branch mispredict bound**

DOP attacks all three.

Object-oriented code optimizes for _human abstraction_.  
Data-oriented code optimizes for _cache lines and pipelines_.

---

## AoS vs SoA (but beyond the interview cliché)

You already know this:

```cpp
struct Order {
    int id;
    double price;
    int qty;
    char side;
};
std::vector<Order> orders; // AoS
```

vs

```cpp
struct Orders {
    std::vector<int> ids;
    std::vector<double> prices;
    std::vector<int> qtys;
    std::vector<char> sides;
};
```

### What actually matters in practice

- **Partial iteration**  
    In HFT you often touch _one or two fields_ in hot loops (e.g., price + qty).  
    SoA means fewer cache lines pulled in → lower latency variance.
- **SIMD friendliness**  
    SoA is trivial to vectorize:
    ```cpp
    prices[i] += delta;
    ```
    AoS often defeats auto-vectorization unless you manually intervene.
- **Predictable prefetching**  
    Linear arrays + fixed strides = hardware prefetcher happiness.

---

## Cache lines > objects

A common senior-level DOP move: **design structs to fit cache lines**.

```cpp
struct alignas(64) BookLevel {
    double price;
    int32_t qty;
    int32_t order_count;
    // padding intentional
};
```

Why this matters:

- False sharing is poison in multi-threaded market data pipelines.
- One book level per cache line → zero contention between threads updating adjacent levels.

Interviewers _love_ when you say “I intentionally waste memory to reduce tail latency.”

---

## Hot / cold data split (this is huge in HFT)

Instead of:

```cpp
struct Order {
    int id;
    double price;
    int qty;
    std::string client;
    std::chrono::time_point<> ts;
};
```

Do:

```cpp
struct OrderHot {
    int id;
    double price;
    int qty;
};

struct OrderCold {
    std::string client;
    uint64_t ts_ns;
};
```

- **Hot path** only sees `OrderHot`
- Cold data lives elsewhere (or even on another NUMA node)

This reduces cache pollution and branch noise.

---

## “Algorithms operate on arrays, not objects”

Classic DOP mantra.

Bad (from a DOP POV):

```cpp
for (auto& o : orders)
    o.match();
```

Better:

```cpp
match_orders(prices.data(), qtys.data(), sides.data(), n);
```

Why?

- Fewer indirect calls
- No hidden loads
- Easier to reason about memory access patterns
- Much easier to benchmark in isolation

---

## Branch elimination > clever math

DOP often replaces branches with data transforms.

Instead of:

```cpp
if (side == BUY) bid_qty += qty;
else ask_qty += qty;
```

Do:

```cpp
bid_qty += qty * is_buy;
ask_qty += qty * (1 - is_buy);
```

Why this matters:

- Branch mispredicts hurt _way more_ than a multiply
- Especially under adversarial or bursty market data

---

## Memory allocation is part of your data model

If you say “DOP” in an HFT interview and don’t mention allocators, that’s a red flag.

Typical moves:

- **Arena / bump allocators**
- **Object pools**
- No `new` / `delete` on hot paths
- Pre-fault memory on startup (`mlockall` on Linux)

Data-oriented means _you decide when memory is touched_.

---

## NUMA awareness (senior-level signal)

In real trading systems:

- Market data thread on NUMA node 0
- Strategy on node 0
- Risk on node 1 (sometimes)
- Pin threads
- Allocate memory local to the thread that uses it

DOP without NUMA awareness is incomplete at senior level.

---

## What interviewers secretly want to hear

If you want to score points:

- “I design data layouts **from the hot loop outward**.”
- “I benchmark different layouts because intuition fails.”
- “I care more about p99 latency than average throughput.”
- “I trade memory for determinism.”
- “I don’t fight the compiler; I make the data obvious.”

---

## One brutal truth

Most “data-oriented” C++ code in interviews:

- Stops at SoA
- Ignores branch prediction
- Ignores allocator behavior
- Ignores NUMA
- Ignores tail latency

If you talk about **cache lines, prefetching, branch predictors, and allocators**, you’re already playing at staff-level depth.