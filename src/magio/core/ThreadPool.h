#include <functional>
#include <thread>
#include <future>
#include "magio/core/Queue.h"

namespace magio {

namespace utils {

template<typename>
struct IsRefWrapper: std::false_type {};

template<typename T>
struct IsRefWrapper<std::reference_wrapper<T>>: std::true_type {};

}

class ThreadPool {
public:
    explicit ThreadPool(size_t thread_num) {
        threads_.reserve(thread_num);
    }

    ~ThreadPool() {
        task_queue_.stop();
    }

    void start() noexcept {
        std::call_once(start_flag_, [this] {
            for (size_t i = 0; i < threads_.capacity(); ++i) {
                threads_.emplace_back(&ThreadPool::worker, this);
            }
        });
    }

    template<typename Fn, typename ...Args>
    requires std::is_invocable_v<Fn, Args...>
    void execute(Fn&& fn, Args&&...args) {
        task_queue_.push([fn = std::forward<Fn>(fn), 
                        targ = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            using Tuple = decltype(targ);
            ([&]<size_t...Idx>(std::index_sequence<Idx...>) mutable {
                //fn(std::move(std::get<Idx>(targ))...);
                fn(static_cast<std::conditional_t<
                    utils::IsRefWrapper<std::tuple_element_t<Idx, Tuple>>::value,
                    std::tuple_element_t<Idx, Tuple>,
                    std::tuple_element_t<Idx, Tuple>&&
                >>(std::get<Idx>(targ))...);
            })(std::make_index_sequence<sizeof...(Args)>{});
        });

    }

    template<typename Fn, typename ...Args>
    requires std::is_invocable_v<Fn, Args...>
    auto submit(Fn&& fn, Args&&...args) {
        
        std::packaged_task pack_task{std::forward<Fn>(fn)};
        auto ret_fut = pack_task.get_future();

        task_queue_.push([task_ptr = std::make_shared<decltype(pack_task)>(std::move(pack_task)), 
                            targ = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            using Tuple = decltype(targ);
            ([&]<size_t...Idx>(std::index_sequence<Idx...>) mutable {
                //pack_task(std::move(std::get<Idx>(targ))...);
                (*task_ptr)(static_cast<std::conditional_t<
                    utils::IsRefWrapper<std::tuple_element_t<Idx, Tuple>>::value,
                    std::tuple_element_t<Idx, Tuple>,
                    std::tuple_element_t<Idx, Tuple>&&
                >>(std::get<Idx>(targ))...);
            })(std::make_index_sequence<sizeof...(Args)>{});
        });
        return ret_fut;
    }
private:
    void worker() {
        for (; ;) {
            auto task = task_queue_.try_take();
            if (!task) {
                return;
            }
            (task.unwrap())();
        }
    }

    NotificationQueue<std::function<void()>> task_queue_;
    std::vector<std::jthread> threads_;
    std::once_flag start_flag_;
};

}