#ifndef MAGIO_CORE_IMPL_THIS_CONTEXT_H_
#define MAGIO_CORE_IMPL_THIS_CONTEXT_H_

namespace magio {
 
namespace this_context {

inline void start() {
    LocalContext->start();
}

inline void stop() {
    LocalContext->stop();
}

inline void execute(Task&& task) {
    LocalContext->execute(std::move(task));
}

inline void dispatch(Task &&task) {
    LocalContext->dispatch(std::move(task));
}

#ifdef MAGIO_USE_CORO
template<typename T>
inline void spawn(Coro<T> coro) {
    LocalContext->spawn(std::move(coro));
}

template<typename T>
inline Coro<T> spawn(Coro<T> coro, detail::UseCoro) {
    co_return co_await coro;
}

template<typename T>
inline void spawn(Coro<T> coro, CoroCompletionHandler<T>&& handler) {
    LocalContext->spawn(std::move(coro), std::move(handler));
}

inline void wake_in_context(std::coroutine_handle<> h) {
    LocalContext->wake_in_context(h);
}

inline void queue_in_context(std::coroutine_handle<> h) {
    LocalContext->queue_in_context(h);
}
#endif

template<typename Rep, typename Per>
inline TimerHandle expires_after(const std::chrono::duration<Rep, Per>& dur, TimerTask&& task) {
    return LocalContext->expires_after(dur, std::move(task));
}

inline TimerHandle expires_until(const TimerClock::time_point& tp, TimerTask&& task) {
    return LocalContext->expires_until(tp, std::move(task));
}

inline IoService get_service() {
    return LocalContext->get_service();
}

}

}

#endif