#ifndef MAGIO_CORE_COROUTINE_H_
#define MAGIO_CORE_COROUTINE_H_

#ifdef MAGIO_USE_CORO
#ifdef __cpp_impl_coroutine
#include <coroutine>
#else
#include <experimental/coroutine>
namespace std {
    using std::experimental::suspend_always;
    using std::experimental::suspend_never;
    using std::experimental::noop_coroutine;
    using std::experimental::coroutine_handle;
}
#endif
#endif
#endif