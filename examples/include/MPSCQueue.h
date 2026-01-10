#pragma once

#include <atomic>
#include <iostream>
#include <memory>

/*
 * MPSCQueue<T>
 * --------------
 * A lock-free Multiple-Producer, Single-Consumer queue.
 *
 *  - Multiple threads may call push()
 *  - Exactly one thread may call pop()
 *
 * The queue is implemented as a linked list with:
 *  - a dummy head node for simpler edge-case handling
 *  - an atomic tail pointer used by producers
 *  - a non-atomic head pointer used only by the consumer
 *
 * Memory ordering is carefully chosen to ensure correctness while remaining fast.
 */
template<typename T>
class MPSCQueue {
public:
    /*
     * Constructor:
     * Initializes the queue with a single dummy node.
     * Both head_ and tail_ initially point to this node.
     */
    MPSCQueue() {
        auto dummy = new Node();
        dummy->next.store(nullptr, std::memory_order_relaxed);
        head_ = dummy;
        tail_.store(dummy, std::memory_order_relaxed);
    }

    /*
     * Destructor:
     * Walks the linked list and deletes all nodes.
     * Must only be called when no producers/consumer are running.
     */
    ~MPSCQueue() noexcept {
        auto current = head_;
        while (current) {
            auto tmp = current->next.load(std::memory_order_relaxed);
            delete current;
            current = tmp;
        }
    }

    /*
     * Push a copy of a value into the queue.
     * Safe to call from multiple producer threads concurrently.
     */
    bool push(const T &value) noexcept {
        auto node = new Node();
        node->data = value;
        node->next.store(nullptr, std::memory_order_relaxed);
        return push_node(node);
    }

    /*
     * Push a moved value into the queue.
     * Also safe for multiple producers.
     */
    bool push(T &&value) noexcept {
        auto node = new Node();
        node->data = std::move(value);
        node->next.store(nullptr, std::memory_order_relaxed);
        return push_node(node);
    }

    /*
     * Pop a value from the queue.
     * Must only be called by the single consumer thread.
     *
     * Returns false if the queue is empty.
     */
    bool pop(T &value) noexcept {
        // Load the node after head_
        auto next = head_->next.load(std::memory_order_acquire);
        if (!next) {
            return false;  // queue empty
        }

        // Extract the data
        value = std::move(next->data);

        // Delete the old dummy node and advance head_
        delete head_;
        head_ = next;
        return true;
    }

    /*
     * Returns true if the queue is empty.
     * Only safe to call from the consumer thread.
     */
    bool empty() const noexcept {
        return head_->next.load(std::memory_order_acquire) == nullptr;
    }

private:
    // Cache line size to prevent false sharing between threads
    static constexpr size_t CACHE_LINE = std::hardware_destructive_interference_size;

    /*
     * Queue node.
     * next is atomic because producers and the consumer touch it concurrently.
     * data is aligned to a cache line to reduce contention.
     */
    struct Node {
        std::atomic<Node*> next{nullptr};
        alignas(CACHE_LINE) T data;
    };

    // head_ is only accessed by the consumer
    alignas(CACHE_LINE) Node* head_;

    // tail_ is updated by all producers and read by the consumer
    alignas(CACHE_LINE) std::atomic<Node*> tail_;

    /*
     * Core enqueue logic.
     *
     * tail_.exchange(node) atomically:
     *   - sets the new tail
     *   - returns the previous tail
     *
     * We then link the previous tail's next pointer to the new node.
     */
    bool push_node(Node *node) noexcept {
        // Atomically swap in the new tail
        auto prev = tail_.exchange(node, std::memory_order_acq_rel);

        // Link the old tail to the new node
        prev->next.store(node, std::memory_order_release);

        return true;
    }
};
