#pragma once

#include "magio/EventLoop.h"
#include "magio/coro/CoSpawn.h"
#include "magio/plat/runtime.h"

namespace magio {

namespace detail {

class GlobalLoop {
public:
    static EventLoop& ins() {
        static EventLoop loop;
        return loop;
    }
private:
};

}

namespace this_context {

template<typename Ret, typename Handler = detail::Detached>
inline auto spawn(Coro<Ret>&& coro, Handler handler = Handler{}) {
    return co_spawn(detail::GlobalLoop::ins().get_executor(), std::move(coro), std::move(handler));
}

inline void spawn_blocking(std::function<void()>&& task) {
    plat::Runtime::ins().post(std::move(task));
}

inline void run() {
    detail::GlobalLoop::ins().run();
}

template<typename Ret, typename Handler = detail::Detached>
inline void run(Coro<Ret>&& coro, Handler handler = Handler{}) {
    co_spawn(detail::GlobalLoop::ins().get_executor(), std::move(coro), std::move(handler));
    run();
}

inline void stop() {
    detail::GlobalLoop::ins().stop();
}

inline AnyExecutor get_executor() {
    return detail::GlobalLoop::ins().get_executor();
}

}

}