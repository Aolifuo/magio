#ifndef MAGIO_CORE_QUEUE_H_
#define MAGIO_CORE_QUEUE_H_

#include <atomic>
#include <optional>

#include "magio-v3/core/noncopyable.h"

namespace magio {

template<typename T>
class LockFreeQueue: Noncopyable {
public:
    struct Node {
        T value;
        std::atomic<Node*> next;
    };

    LockFreeQueue() { 
        auto dummy = new Node{{}, nullptr};
        head_ = tail_ = dummy;
    }

    ~LockFreeQueue() {
        Node* dest = head_.load(std::memory_order_relaxed);
        Node* prev = dest;
        size_t i = 0;
        while (dest) {
            dest = dest->next.load(std::memory_order_relaxed);
            delete prev;
            ++i;
            prev = dest;
        }
    }

    void push(T& lval) {
        emplace(lval);
    }

    void push(T&& rval) {
        emplace(std::move(rval));
    }

    template<typename...Args>
    void emplace(Args&&...args) {
        auto node = new Node{{std::forward<Args>(args)...}, nullptr};
        Node* old_tail = nullptr;
        Node* tail_next = tail_.load(std::memory_order_relaxed);
        do {
            old_tail = tail_next;
            tail_next = nullptr;
        } while (!old_tail->next.compare_exchange_weak(
            tail_next, node,
            std::memory_order_release,
            std::memory_order_relaxed
        )); 
        tail_.exchange(node, std::memory_order_relaxed);
    }

    std::optional<T> try_pop_front() {
        auto head = head_.load(std::memory_order_relaxed);
        Node* head_next = nullptr;
        Node* next_next = nullptr;
        do {
            head_next = head->next.load(std::memory_order_relaxed);
            if (!head_next) {
                return {};
            }
            next_next = head_next->next.load(std::memory_order_relaxed);
        } while (!head->next.compare_exchange_weak(
            head_next, next_next, 
            std::memory_order_release, std::memory_order_relaxed
        ));

        std::optional<T> result(std::move(head_next->value));
        delete head_next;
        return result;
    }

    void for_each() {
        Node* dest = head_.load()->next.load();
        for (; dest; dest = dest->next.load()) {
            printf("%d\n", dest->value);
        }
    }

private:
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
};

}

#endif