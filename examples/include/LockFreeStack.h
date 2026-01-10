#pragma once
#include <atomic>
#include <cstdint>
#include <algorithm>

/**
 * @brief A thread-safe, lock-free stack implementation using atomic operations.
 *
 * This stack supports multiple producers and multiple consumers (MPMC) without locks.
 * It uses tagged pointers to prevent the ABA problem and is optimized for high
 * contention scenarios with proper cache line alignment.
 *
 * @tparam T The type of elements stored in the stack.
 *
 * Features:
 * - Lock-free: No mutexes or blocking operations
 * - ABA-safe: Uses tagged pointers to prevent ABA problem
 * - Cache-friendly: Variables aligned to cache lines to prevent false sharing
 * - Exception-safe: Proper cleanup in destructor
 * - Move-aware: Supports move semantics for efficient transfers
 */
template<typename T>
class LockFreeStack {
public:
    /**
     * @brief Destructor that cleans up all remaining nodes.
     *
     * Note: Assumes no other threads are accessing the stack during destruction.
     * For concurrent destruction scenarios, consider using shared_ptr or RCU.
     */
    ~LockFreeStack() noexcept {
        // Traverse the entire list and delete all nodes
        auto head = mHead.load(std::memory_order_relaxed).ptr;
        while (head) {
            auto next = head->next;  // Save next pointer before deletion
            delete head;             // Delete current node
            head = next;             // Move to next node
        }
    }

    /**
     * @brief Push a value onto the stack (copy version).
     *
     * @param value The value to push.
     *
     * Thread-safe: Multiple threads can push simultaneously.
     * Time complexity: O(1) amortized (retry on contention).
     */
    void push(const T& value) noexcept{
        push_node(new Node(value));
    }

    /**
     * @brief Push a value onto the stack (move version).
     *
     * @param value The value to move onto the stack.
     *
     * More efficient than copy for types with expensive copy operations.
     */
    void push(T&& value) noexcept{
        push_node(new Node(std::move(value)));
    }

    /**
     * @brief Try to pop a value from the stack.
     *
     * @param value Reference to store the popped value.
     * @return true if a value was successfully popped.
     * @return false if the stack was empty.
     *
     * Thread-safe: Multiple threads can pop simultaneously.
     * The popped value is moved out of the node for efficiency.
     *
     * Memory ordering:
     * - acquire load: Ensures we see all previous pushes
     * - acq_rel CAS: Synchronizes with other push/pop operations
     * - relaxed size update: Size counter is approximate anyway
     */
    bool pop(T& value) noexcept {
        auto head = mHead.load(std::memory_order_acquire);

        // Retry loop: CAS might fail due to concurrent operations
        while (true) {
            // Check if stack is empty
            if (!head.ptr) {
                return false;
            }

            // Prepare new head (next node with incremented counter)
            TaggedPtr newNode{head.ptr->next, head.counter + 1};

            // Attempt to atomically update the head
            if (mHead.compare_exchange_weak(head, newNode,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
                // Success: We now own the node
                value = std::move(head.ptr->data);  // Move data out
                delete head.ptr;                    // Safe to delete now
                mSize.fetch_sub(1, std::memory_order_relaxed);  // Update approximate size
                return true;
            }
            // CAS failed: Another thread modified the stack
            // 'head' was updated with current value, retry
        }
    }

    /**
     * @brief Check if the stack is empty.
     *
     * @return true if the stack appears empty.
     * @return false if the stack appears non-empty.
     *
     * Note: This is approximate due to concurrent modifications.
     * A false result doesn't guarantee subsequent pop() will succeed.
     */
    bool empty() const noexcept {
        return mSize.load(std::memory_order_acquire) == 0;
    }

    /**
     * @brief Get the approximate size of the stack.
     *
     * @return size_t Approximate number of elements in the stack.
     *
     * Note: This is approximate due to concurrent modifications.
     * The actual size at any moment may differ.
     */
    size_t size() const noexcept {
        return mSize.load(std::memory_order_acquire);
    }

private:
    /**
     * @brief Internal node structure for the linked list.
     */
    struct Node {
        T data;         ///< The stored value
        Node* next;     ///< Pointer to the next node in the stack

        /**
         * @brief Construct a node with copied data.
         */
        explicit Node(const T& val) : data(val), next(nullptr) {}

        /**
         * @brief Construct a node with moved data.
         */
        explicit Node(T&& val) : data(std::move(val)), next(nullptr) {}
    };

    /**
     * @brief Tagged pointer combining a node pointer with a modification counter.
     *
     * The counter prevents the ABA problem: even if a pointer is reused,
     * the counter will be different, causing CAS to fail.
     *
     * Alignment to 16 bytes is required for double-width CAS (CMPXCHG16B)
     * on x86_64 architectures.
     */
    struct alignas(16) TaggedPtr {
        Node* ptr;          ///< Pointer to the node
        uintptr_t counter;  ///< Modification counter for ABA prevention
    };

    /**
     * @brief Internal helper to push a pre-allocated node.
     *
     * @param node The node to push onto the stack.
     *
     * Implements the lock-free push algorithm:
     * 1. Link node to current head
     * 2. Attempt CAS to make node the new head
     * 3. Retry on contention
     */
    void push_node(Node* node) noexcept {
        auto head = mHead.load(std::memory_order_acquire);

        // Retry loop for lock-free insertion
        while (true) {
            // Link new node to current head (LIFO order)
            node->next = head.ptr;

            // Prepare new tagged pointer with incremented counter
            TaggedPtr newNode{node, head.counter + 1};

            // Attempt atomic update
            if (mHead.compare_exchange_weak(head, newNode,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
                // Success: Update approximate size
                mSize.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            // CAS failed: Another thread modified the stack
            // 'head' was updated with current value, retry
        }
    }

    // Cache line size for alignment (typically 64 bytes on modern CPUs)
    static constexpr size_t CACHE_LINE = std::hardware_destructive_interference_size;

    /**
     * @brief Atomic head pointer with modification counter.
     *
     * Aligned to cache line boundary to prevent false sharing.
     * False sharing occurs when variables on the same cache line
     * are accessed by different CPU cores, causing cache invalidations.
     */
    alignas(CACHE_LINE) std::atomic<TaggedPtr> mHead{TaggedPtr{nullptr, 0}};

    /**
     * @brief Approximate size counter.
     *
     * Separated by cache line from mHead to prevent false sharing.
     * Updated with relaxed ordering as it's only approximate.
     */
    alignas(CACHE_LINE) std::atomic<std::size_t> mSize{0};

    // Note: Copy constructor and assignment operator are implicitly deleted
    // because std::atomic is not copyable. This is intentional for thread safety.
};