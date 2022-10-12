#pragma once

#include <mutex>
#include <iostream>
#include "magio/dev/Config.h"

namespace magio {

namespace dev {

inline std::mutex GLOBAL_MUTEX;

template<typename...Args>
inline void debug_log(Args&&...args) {
    std::lock_guard lk(GLOBAL_MUTEX);
    ((std::cout << args), ...);
    std::cout << '\n';
}

}

#ifdef MAGIO_DEBUG
#define DEBUG_LOG(...) \
    dev::debug_log("[file: ", __FILE__, "] [line: ", __LINE__, "] [thread:", std::this_thread::get_id(), "]: ", __VA_ARGS__)
#else
#define DEBUG_LOG(...) 1
#endif

}