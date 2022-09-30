#pragma once

#include <mutex>

#include "magio/core/MaybeUninit.h"

namespace magio {

template<typename T, typename Alloc = std::allocator<T>>
class RingQueue {

    using AllocTraits = std::allocator_traits<Alloc>; 

public:
    explicit RingQueue(Alloc alloc = Alloc{})
        : alloc_(alloc) {
        owned_ = AllocTraits::allocate(alloc_, 10 + 1);
        cap_ = 10 + 1;
    }

    explicit RingQueue(size_t cap, Alloc alloc = Alloc{})
        : alloc_(alloc) {
        // assert(cap > 0);
        owned_ = AllocTraits::allocate(alloc_, cap + 1);
        cap_ = cap + 1;
    }

    RingQueue(RingQueue&& other)
        : owned_(other.owned_)
        , cap_(other.cap_)
        , front_(other.len_)
        , rear_(other.rear_)
        , alloc_(std::move(other.alloc_)) 
    {
        other.owned_ = nullptr;
        other.cap_ = 0;
        other.front_ = 0;
        other.rear_ = 0;
    }

    RingQueue& operator=(RingQueue&& other) {
        owned_ = other.owned_;
        cap_ = other.cap_;
        front_ = other.front_;
        rear_ = other.rear_;
        alloc_ = std::move(other.alloc_);
        other.owned_ = nullptr;
        other.cap_ = 0;
        other.front_ = 0;
        other.rear_ = 0;
        return *this;
    }

    ~RingQueue() {
        if (owned_) {
            for (; front_ != rear_; front_ = next(front_)) {
                AllocTraits::destroy(alloc_, owned_ + front_);
            }
            AllocTraits::deallocate(alloc_, owned_, cap_);
        }
    }

    T& front() {
        return owned_[front_];
    }

    template<typename...Args>
    void emplace(Args&&...args) {
        if (full()) {
            extend();
        }
        AllocTraits::construct(alloc_, owned_ + rear_, std::forward<Args>(args)...);
        rear_ = next(rear_);
    }

    void push(T elem) {
        emplace(std::move(elem));
    }

    void pop() {
        if (empty()) {
            return;
        }
        AllocTraits::destroy(alloc_, owned_ + front_);
        front_ = next(front_);
    }

    bool empty() {
        return front_ == rear_;
    }

    bool full() {
        return next(rear_) == front_;
    }

    size_t size() {
        return rear_ >= front_ ? rear_ - front_ : rear_ + cap_ - front_;
    }

    void clear() {
        for (; front_ != rear_; front_ = next(front_)) {
            AllocTraits::destroy(alloc_, owned_ + front_);
        }
    }
private:
    size_t next(size_t cur) {
        return (cur + 1) % cap_;
    }

    void extend() {
        size_t new_cap = (cap_ - 1) / 2 + cap_;
        T* new_owned = AllocTraits::allocate(alloc_, new_cap);
        size_t dest = 0;
        for (size_t i = front_; i != rear_; i = next(i)) {
            AllocTraits::construct(alloc_, new_owned + dest, std::move(owned_[i]));
            AllocTraits::destroy(alloc_, owned_ + i);
            ++dest;
        }
        AllocTraits::deallocate(alloc_, owned_, cap_);
        owned_ = new_owned;
        cap_ = new_cap;
        front_ = 0;
        rear_ = dest;
    }

    T* owned_ = nullptr;
    size_t cap_ = 0;
    size_t front_ = 0;
    size_t rear_ = 0;
    Alloc alloc_;
};


template<typename T, typename Container = RingQueue<T>>
class BlockedQueue {
public:
    explicit BlockedQueue() = default;
    explicit BlockedQueue(size_t cap)
        :queue_(cap) {}

    BlockedQueue(BlockedQueue&& other) = default;

    void push(T elem) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(elem));
    }

    MaybeUninit<T> try_take() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return {};
        }

        MaybeUninit<T> result(std::move(queue_.front()));
        queue_.pop();
        return result;
    }
private:
    Container queue_;
    std::mutex mutex_;
};


template<typename T, typename Container = RingQueue<T>>
class NotificationQueue {
public:
    explicit NotificationQueue() = default;
    explicit NotificationQueue(size_t cap)
        : queue_(cap) { }
    
    void push(T elem) {
        std::lock_guard lk(mutex_);
        queue_.push(std::move(elem));
        not_empty_cv_.notify_one();
    }

    MaybeUninit<T> try_take() {
        std::unique_lock lk(mutex_);
        // wait
        not_empty_cv_.wait(lk, [this]{ return !queue_.empty() || stop_flag_; });
        if (stop_flag_) {
            return {};
        }

        MaybeUninit<T> res{std::move(queue_.front())};
        queue_.pop();
        return res;
    }

    void stop() {
        std::lock_guard lk(mutex_);
        stop_flag_ = true;
        queue_.clear();
        not_empty_cv_.notify_all();
    }

    void start() {
        std::lock_guard lk(mutex_);
        stop_flag_ = false;
    }
private:
    Container queue_;
    std::mutex mutex_;
    std::condition_variable not_empty_cv_;
    bool stop_flag_{false};
};

}
