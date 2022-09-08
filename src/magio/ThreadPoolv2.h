#include "magio/core/Execution.h"
#include "magio/core/Pimpl.h"

namespace magio {

class ThreadPoolv2 {

    CLASS_PIMPL_DECLARE(ThreadPoolv2)

public:
    explicit ThreadPoolv2(size_t thread_num);

    void post(CompletionHandler handler);
    TimerID set_timeout(size_t ms, CompletionHandler handler);
    void clear(TimerID id);
    void start();
    void stop();
    void wait();
    void join();
    void detach();

    AnyExecutor get_executor();

};

}