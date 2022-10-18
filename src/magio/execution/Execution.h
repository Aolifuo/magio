#pragma once

#include "magio/core/Fwd.h"

namespace magio {

namespace plat {

class IOService;

}

class Waker;

class ExecutionContext {
    friend class AnyExecutor;

public:
    virtual void async_wake(Waker) = 0;
    virtual TimerID invoke_after(TimerHandler&&, size_t) = 0;
    virtual bool cancel(TimerID) = 0;
    virtual plat::IOService& get_service() = 0;
    virtual ~ExecutionContext() = default;
};

class AnyExecutor {
public:
    AnyExecutor() { }

    AnyExecutor(ExecutionContext* context)
        : context_(context) 
    {}

    void async_wake(Waker waker) const;

    TimerID invoke_after(TimerHandler&& handler, size_t ms) const;

    bool cancel(TimerID id) const {
        return context_->cancel(id);
    }

    plat::IOService& get_service() const {
        return context_->get_service();
    }
    
    operator bool() const {
        return context_ != nullptr;
    }
    
private:
    ExecutionContext* context_ = nullptr;
};

}