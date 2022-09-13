#include <future>
#include "magio/core/Execution.h"
#include "magio/core/Pimpl.h"

namespace magio {

class ThreadPool {

    CLASS_PIMPL_DECLARE(ThreadPool)

public:
    explicit ThreadPool(size_t thread_num);

    void post(CompletionHandler handler);
    void waiting(WaitingCompletionHandler handler);
    TimerID set_timeout(size_t ms, CompletionHandler handler);
    void clear(TimerID id);
    void start();
    void stop();
    void wait();
    void join();
    void detach();

    AnyExecutor get_executor();

    template<typename Fn, typename...Args>
    auto get_future(Fn&& fn, Args&&...args) {
        std::packaged_task task(std::forward<Fn>(fn));
        auto fut = task.get_future();

        post(
            [task_ptr = std::make_shared<decltype(task)>(std::move(task)), 
            arg_tup = std::make_tuple(args...)] () mutable
            {
                std::apply([task_ptr]<typename...Ts>(Ts&&...args) {
                    (*task_ptr)(std::forward<Ts>(args)...);
                }, arg_tup);
            }
        );

        return fut;
    }
};

}