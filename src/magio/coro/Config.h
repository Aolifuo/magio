#pragma once

#ifdef __cpp_impl_coroutine
#include <coroutine>
#else
#include <experimental/coroutine>
#endif

namespace magio {
#ifdef __cpp_impl_coroutine
using std::coroutine_handle;
using std::coroutine_traits;
using std::suspend_always;
using std::suspend_never;
#else
using std::experimental::coroutine_handle;
using std::experimental::coroutine_traits;
using std::experimental::suspend_always;
using std::experimental::suspend_never;
#endif

}