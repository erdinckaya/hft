#pragma once

#include <atomic>
#include <iostream>
#include <memory>

template<typename T>
class MPSCQueue {
public:
    MPSCQueue() {
        auto dummy = new Node();
        dummy->next.store(nullptr, std::memory_order_relaxed);
        head_ = dummy;
        tail_.store(dummy, std::memory_order_relaxed);
    }

    ~MPSCQueue() noexcept {
        auto current = head_;
        while (current) {
            auto tmp = current->next.load(std::memory_order_relaxed);
            delete current;
            current = tmp;
        }
    }

    bool push(const T &value) noexcept {
        auto node = new Node();
        node->data = value;
        node->next.store(nullptr, std::memory_order_relaxed);
        return push_node(node);
    }

    bool push(T &&value) noexcept {
        auto node = new Node();
        node->data = std::move(value);
        node->next.store(nullptr, std::memory_order_relaxed);
        return push_node(node);
    }

    bool pop(T &value) noexcept {
        auto next = head_->next.load(std::memory_order_acquire);
        if (!next) {
            return false;
        }
        value = std::move(next->data);
        delete head_;
        head_ = next;
        return true;
    }

    bool empty() const noexcept {
        return head_->next.load(std::memory_order_acquire) == nullptr;
    }

private:
    static constexpr size_t CACHE_LINE = std::hardware_destructive_interference_size;
    struct Node {
        std::atomic<Node*> next{nullptr};
        alignas(CACHE_LINE)T data;
    };

    alignas(CACHE_LINE) Node* head_; // Consumer reads/writes
    alignas(CACHE_LINE) std::atomic<Node*> tail_; // Producers write, consumer reads

    bool push_node(Node *node) noexcept {
        auto prev = tail_.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
        return true;
    }
};
