#pragma once

#include <functional>

namespace magio {

using TimerID = size_t;

using Handler = std::function<void()>;

using TimerHandler = std::function<void(bool)>;

struct Unit {};

inline constexpr TimerID MAGIO_INVALID_TIMERID = std::numeric_limits<TimerID>::max();

}