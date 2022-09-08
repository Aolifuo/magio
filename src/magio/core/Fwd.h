#pragma once


#include <functional>

namespace magio {

using TimerID = size_t;

using CompletionHandler = std::function<void()>;

}