#pragma once

#include "magio/core/Fwd.h"

namespace magio {

class ExecutionContext {
public:
    virtual void post(CompletionHandler) = 0;
    virtual void waiting(WaitingCompletionHandler) = 0;
    virtual TimerID set_timeout(size_t ms, CompletionHandler) = 0;
    virtual void clear(TimerID) = 0;
    virtual bool poll() = 0;
    virtual ~ExecutionContext() = default;

private:
};

class AnyExecutor {
    
public:
    AnyExecutor() { }

    AnyExecutor(ExecutionContext* context)
        : context_(context) 
    {}

    void post(CompletionHandler handler) {
        context_->post(std::move(handler));
    }

    void waiting(WaitingCompletionHandler handler) {
        context_->waiting(std::move(handler));
    }

    TimerID set_timeout(size_t ms, CompletionHandler handler) {
        return context_->set_timeout(ms, std::move(handler));
    }

    void clear(TimerID id) {
        context_->clear(id);
    } 

    bool poll() {
        return context_->poll();
    }
private:
    ExecutionContext* context_ = nullptr;
};

}