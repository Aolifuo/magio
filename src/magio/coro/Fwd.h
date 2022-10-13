#pragma once

#include <type_traits>
#include "magio/core/Fwd.h"

namespace magio {

namespace util {

template<typename T>
using VoidToUnit = std::conditional_t<std::is_void_v<T>, Unit, T>;

}

namespace detail {

struct UseCoro {};

struct UseFuture {};

struct Detached{};

}

template<typename>
class Coro;

class Awaitable;

inline detail::UseCoro use_coro;

inline detail::UseFuture use_future;

inline detail::Detached detached;



}