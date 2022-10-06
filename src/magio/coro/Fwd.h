#pragma once

#include <type_traits>
#include "magio/core/Fwd.h"

namespace magio {

namespace util {

template<typename T>
using VoidToUnit = std::conditional_t<std::is_void_v<T>, Unit, T>;

}

template<typename>
struct Coro;

struct UseCoro {};

struct Detached{};

inline UseCoro use_coro;

inline Detached detached;

}