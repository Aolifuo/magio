#include "magio/execution/Execution.h"
#include "magio/core/Pimpl.h"

namespace magio {

class ThreadPool {

    CLASS_PIMPL_DECLARE(ThreadPool)

public:
    explicit ThreadPool(size_t thread_num, bool start_service = false);

    void run();
    void stop();
    void wait();
    void join();
    void attach();

    AnyExecutor get_executor() const;
};

}