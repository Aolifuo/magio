#pragma once

#include <utility>

namespace magio {

namespace detail {

template<typename T>
struct DefaultDeleter {
    void operator()(T* p) const {
        delete p;
    }
};

}

template<typename T, typename Deleter = detail::DefaultDeleter<T>>
class ScopeGuard {
public:
    ScopeGuard(T* p, Deleter deleter = Deleter())
        : ptr_(p)
        , deleter_(std::move(deleter)) 
    {
        
    }

    ~ScopeGuard() {
        if (ptr_) {
            deleter_(ptr_);
        }
    }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard(ScopeGuard &&) = delete;

    T* get() {
        return ptr_;
    }

    T* release() {
        auto p = ptr_;
        ptr_ = nullptr;
        return p;
    }
private:
    T* ptr_;
    Deleter deleter_;
};

template<typename T>
ScopeGuard(T*) -> ScopeGuard<T>;

template<typename T, typename D>
ScopeGuard(T*, D) -> ScopeGuard<T, D>;

}