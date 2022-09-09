#pragma once

#include "magio/core/Execution.h"
#include "magio/core/Pimpl.h"

namespace magio {

class EventLoop {

    CLASS_PIMPL_DECLARE(EventLoop)
    
public:
    EventLoop();

    void post(CompletionHandler handler);
    void waiting(WaitingCompletionHandler handler);
    TimerID set_timeout(size_t ms, CompletionHandler handler);
    void clear(TimerID id);
    bool poll();
    void run();
    void stop();
    void restart();

    AnyExecutor get_executor();
private:
};

}