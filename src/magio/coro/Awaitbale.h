#pragma once

#include "magio/coro/PromiseBase.h"
#include "magio/execution/Execution.h"

namespace magio {

class Waker {
public:
    Waker() = default;

    Waker(PromiseNode* node)
        : node_(node)
    { }

    void wake() const {
        node_->wake();
    }

    PromiseNode* node_ = nullptr;
};

template<typename Fn>
class Awaitable {
public:
    Awaitable(Fn&& handle)
        : waker_handler_(std::move(handle)) 
    { }

    Awaitable(const Awaitable&) = delete;

    bool await_ready() { return false; }

    template<typename PT>
    void await_suspend(coroutine_handle<PT> prev_h) {
        waker_handler_(prev_h.promise().executor_, &prev_h.promise(), prev_h.promise().timeout_);
    }

    void await_resume() {

    }
private:
    Fn waker_handler_;
};

template<>
class Awaitable<void> {
    Awaitable(const Awaitable&) = delete;

    bool await_ready() { return false; }

    void await_suspend(coroutine_handle<> prev_h) {
        prev_h.resume();
    }

    void await_resume() {

    }
};

template<typename Fn>
Awaitable(Fn&& fn) -> Awaitable<Fn>;

}