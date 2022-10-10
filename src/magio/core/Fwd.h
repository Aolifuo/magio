#pragma once

#include <functional>

namespace magio {

using TimerID = size_t;

using Handler = std::function<void()>;

using WaitingHandler = std::function<bool()>;

struct Unit {};

}