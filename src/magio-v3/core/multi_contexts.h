#ifndef MAGIO_CORE_MULTI_CONTEXT_H_
#define MAGIO_CORE_MULTI_CONTEXT_H_

#include "magio-v3/core/coro_context.h"

namespace magio {

class MultithreadedContexts: Noncopyable {
public:
    // --> singel direct
    enum State {
        Stopping, Running, PendingDestroy 
    };

    MultithreadedContexts(size_t num, size_t every_entries);

    ~MultithreadedContexts();

    void start_all();

    CoroContext& next_context();

private:
    void run_in_background(size_t id);

    bool assert_in_self_thread();

    State state_ = Stopping;
    size_t every_entries_ = 0;
    size_t next_idx_ = 0;
    size_t thread_id_ = 0;
    std::vector<std::unique_ptr<CoroContext>> contexts_;
    std::vector<std::thread> threads_;
};

}

#endif