#pragma once

#include "magio/core/Fwd.h"

namespace magio {

namespace plat {

class IOService;

}


class ExecutionContext {
    friend class AnyExecutor;

public:
    virtual void post(Handler&&) = 0;
    virtual void dispatch(Handler&&) = 0;
    virtual TimerID set_timeout(size_t ms, Handler&&) = 0;
    virtual void clear(TimerID) = 0;
    virtual ~ExecutionContext() = default;

    virtual plat::IOService& get_service() = 0;
};

class AnyExecutor {
public:
    AnyExecutor() { }

    AnyExecutor(ExecutionContext* context)
        : context_(context) 
    {}

    void post(Handler&& handler) {
        context_->post(std::move(handler));
    }

    void dispatch(Handler&& handler) {
        context_->dispatch(std::move(handler));
    }

    TimerID set_timeout(size_t ms, Handler&& handler) {
        return context_->set_timeout(ms, std::move(handler));
    }

    void clear(TimerID id) {
        context_->clear(id);
    }

    plat::IOService& get_service() {
        return context_->get_service();
    }
    
    operator bool() const {
        return context_ != nullptr;
    }
    
private:
    ExecutionContext* context_ = nullptr;
};

}