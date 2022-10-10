#include <future>
#include "magio/execution/Execution.h"
#include "magio/core/Pimpl.h"
#include "magio/utils/Function.h"

namespace magio {

class ThreadPool {

    CLASS_PIMPL_DECLARE(ThreadPool)

public:
    explicit ThreadPool(size_t thread_num);

    void post(Handler&& handler);
    void dispatch(Handler&& handler);
    TimerID set_timeout(size_t ms, Handler&& handler);
    void clear(TimerID id);
    void run();
    void stop();
    void wait();
    void join();
    void attach();

    AnyExecutor get_executor() const;

    template<typename Fn, typename...Args>
    auto get_future(Fn&& fn, Args&&...args) {
        std::packaged_task<typename FunctorTraits<std::remove_cvref_t<Fn>>::FunctionType> task(std::forward<Fn>(fn));
        auto fut = task.get_future();

        post(
            [task_ptr = std::make_shared<decltype(task)>(std::move(task)), 
            arg_tup = std::make_tuple(std::forward<Args>(args)...)] () mutable
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