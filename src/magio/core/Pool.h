#pragma once

#include <forward_list>
#include <cassert>
#include "magio/core/MaybeUninit.h"
#include "magio/core/Queue.h"

namespace magio {

namespace utils {
    template<typename Pool>
    struct PoolTraits {
        using value_type = typename Pool::value_type;
    };

}

template<typename T, typename = RingQueue<T*>>
class BlockedFixedPool;

template<typename Pool>
class Borrowed {
    using T = typename utils::PoolTraits<Pool>::value_type;
public:
    Borrowed() {

    }

    Borrowed(T* obj, Pool* pool) {
        obj_ = obj;
        pool_ = pool;
    }

    Borrowed(Borrowed&& other) {
        obj_ = other.obj_;
        pool_ = other.pool_;
        other.obj_= nullptr;
        other.pool_ = nullptr;
    }

    Borrowed& operator=(Borrowed&& other) {
        obj_ = other.obj_;
        pool_ = other.pool_;
        other.obj_= nullptr;
        other.pool_ = nullptr;
        return *this;
    }

    ~Borrowed() {
        if (obj_) {
            pool_->unchecked_return(obj_);
        }
    }

    T* operator->() {
        return obj_;
    }

    T* get() {
        return obj_;
    }

    T* unwrap() {
        T* tmp = obj_;
        obj_ = nullptr;
        return tmp;
    }
private:
    T* obj_ = nullptr;
    Pool* pool_ = nullptr;
};

template<typename T, typename Queue>
class BlockedFixedPool {
    using Self = BlockedFixedPool;
    using process_type = void(*)(T*);
public:
    using value_type = T;

    BlockedFixedPool(size_t object_size, process_type fn) noexcept
        : fixed_list_(object_size), free_queue_(object_size), process_(fn) {
        for (auto& p : fixed_list_) {
            free_queue_.push(&p);
        }
    }
    BlockedFixedPool(BlockedFixedPool&&) = default;

    MaybeUninit<Borrowed<Self>> try_borrow() {
        auto maybe_obj = free_queue_.try_take();
        if (!maybe_obj) {
            return {};
        }
        
        return {Borrowed<Self>(maybe_obj.unwrap(), this)};
    }

    void unchecked_return(T* elem) {
        if (process_) {
            process_(elem);
        }
        free_queue_.push(elem);
    }
private:
    std::forward_list<T> fixed_list_;
    BlockedQueue<T*, Queue> free_queue_;
    process_type process_ = nullptr;
};

template<typename T, typename Pool>
requires std::is_same_v<T, typename utils::PoolTraits<Pool>::value_type>
inline void unchecked_return_to_pool(T* elem, Pool* pool) {
    pool->unchecked_return(elem);
}

}