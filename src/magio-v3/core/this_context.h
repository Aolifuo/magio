#ifndef MAGIO_CORE_THIS_CONTEXT_H_
#define MAGIO_CORE_THIS_CONTEXT_H_

#include <chrono>

#include "magio-v3/core/unit.h"
#include "magio-v3/core/io_service.h"
#include "magio-v3/core/execution.h"
#include "magio-v3/core/coroutine.h"

namespace magio {

namespace detail {

template<typename T>
struct CoroCompletionHandler {
    using type = Functor<void(std::exception_ptr, T)>;
};

template<>
struct CoroCompletionHandler<void> {
    using type = Functor<void(std::exception_ptr, Unit)>;
};

struct UseCoro { };

}

template<typename>
class Coro;

class CoroContext;

class TimerHandle;

using TimerClock = std::chrono::steady_clock;

using TimerTask = Functor<void(bool)>;

template<typename T>
using CoroCompletionHandler = typename detail::CoroCompletionHandler<T>::type;

inline thread_local CoroContext* LocalContext = nullptr;

inline detail::UseCoro use_coro;

namespace this_context {

void start();

void stop();

void execute(Task&& task);

void dispatch(Task&& task);

#ifdef MAGIO_USE_CORO
template<typename T>
void spawn(Coro<T> coro);

template<typename T>
[[nodiscard]]
Coro<T> spawn(Coro<T> coro, detail::UseCoro);

template<typename T>
void spawn(Coro<T> coro, CoroCompletionHandler<T>&& handler);

void wake_in_context(std::coroutine_handle<> h);

void queue_in_context(std::coroutine_handle<> h);
#endif

template<typename Rep, typename Per>
TimerHandle expires_after(const std::chrono::duration<Rep, Per>& dur, TimerTask&& task);

TimerHandle expires_until(const TimerClock::time_point& tp, TimerTask&& task);

IoService get_service();

}

}

#endif