#ifndef MAGIO_CORE_CORO_CONTEXT_POOL_H_
#define MAGIO_CORE_CORO_CONTEXT_POOL_H_

#include "magio-v3/core/wait_group.h"
#include "magio-v3/core/coro_context.h"

namespace magio {

class CoroContextPool: Noncopyable {
public:
    // --> singel direct
    enum State {
        Stopping, Running, PendingDestroy 
    };

    CoroContextPool(size_t num, size_t every_entries);

    ~CoroContextPool();

    void start_all();

    CoroContext& next_context();

    CoroContext& get(size_t i);

private:
    void run_in_background(size_t id);

    bool assert_in_self_thread();
    
    State state_ = Stopping;
    size_t every_entries_ = 0;
    size_t next_idx_ = 0;
    size_t thread_id_ = 0;
    WaitGroup build_ctx_wg_;
    WaitGroup start_wg_;
    std::vector<std::unique_ptr<CoroContext>> contexts_;
    std::vector<std::thread> threads_;
};

}

#endif