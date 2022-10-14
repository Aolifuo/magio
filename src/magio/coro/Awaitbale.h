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

    void try_wake() const {
        node_->wake();
    }
private:
    PromiseNode* node_ = nullptr;
};

using WakeHandler = std::function<void(AnyExecutor, Waker waker)>;

class Awaitable {
public:
    Awaitable(WakeHandler&& handle)
        : waker_handler_(std::move(handle)) 
    { }

    Awaitable(const Awaitable&) = delete;

    bool await_ready() { return false; }

    template<typename PT>
    void await_suspend(coroutine_handle<PT> prev_h) {
        waker_handler_(prev_h.promise().executor_, &prev_h.promise());
    }

    void await_resume() {

    }
private:
    WakeHandler waker_handler_;
};

}