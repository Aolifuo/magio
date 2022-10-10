#pragma once

#include <system_error>
#include "magio/execution/Execution.h"
#include "magio/core/Pimpl.h"

namespace magio {

class EventLoop {

    CLASS_PIMPL_DECLARE(EventLoop)
    
public:
    EventLoop(size_t worker_threads);

    void post(Handler&& handler);
    void dispatch(Handler&& handler);
    TimerID set_timeout(size_t ms, Handler&& handler);
    void clear(TimerID id);
    bool poll();
    void run();
    void stop();

    IOService get_service();

    AnyExecutor get_executor() const;
private:
};

}