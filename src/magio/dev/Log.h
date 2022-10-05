#pragma once

#include <mutex>
#include <iostream>

namespace magio {

// #define MAGIO_DEBUG 1

#ifdef MAGIO_DEBUG
#define DEBUG_LOG(args...) \
    debug_log("[file: ", __FILE__, "] [line: ", __LINE__, "] [thread:", std::this_thread::get_id(), "]: ", args)
#else
#define DEBUG_LOG(args...)
#endif

inline std::mutex GLOBAL_MUTEX;

template<typename...Args>
void debug_log(Args&&...args) {
    std::lock_guard lk(GLOBAL_MUTEX);
    ((std::cout << args), ...);
    std::cout << '\n';
}

}