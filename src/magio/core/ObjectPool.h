#pragma once

#include <cassert>
#include <mutex>
#include <deque>
#include <list>

namespace magio {

template<typename T>
struct DefaultGenerator {
    constexpr T operator()() const {
        return {};
    }
};

template<typename T, typename Gen = DefaultGenerator<T>>
class SyncObjectPool {
public:
    SyncObjectPool(size_t init, size_t max, Gen gen = Gen{})
        : gen_(std::move(gen)) 
    {
        assert(init >= 1 && max >= 1);

        for (size_t i = 0; i < init; ++i) {
            free_que_.emplace_back(std::addressof(fixed_list_.emplace_front(gen_())));
        }
    }

    T* get() {
        std::lock_guard lk(m_);
        if (free_que_.empty()) {
            grow(fixed_list_.size() / 2 + 1);
        }

        T* ret = free_que_.front();
        free_que_.pop_front();
        return ret;
    }

    void put(T* ptr) {
        std::lock_guard lk(m_);
        free_que_.emplace_back(ptr);
    }

    size_t free_size() {
        return fixed_list_.size();
    }
private:
    void grow(size_t num) {
        for (size_t i = 0; i < num; ++i) {
            free_que_.emplace_back(std::addressof(fixed_list_.emplace_front(gen_())));
        }
    }

    void shrink() {

    }
    Gen gen_;

    std::deque<T*> free_que_;
    std::list<T> fixed_list_;

    std::mutex m_;
};

}