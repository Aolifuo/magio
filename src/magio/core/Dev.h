#pragma once

#include <atomic>

// #define MAGIO_DEBUG 

namespace magio {

namespace dev {

inline std::atomic_int SOCKET_NUMS = 0;
inline std::atomic_int CORO_NUMS = 0;

#ifdef MAGIO_DEBUG
#define MAGIO_MAKE_SOCKET (++dev::SOCKET_NUMS)
#define MAGIO_CLOSE_SOCKET (--dev::SOCKET_NUMS)
#define MAGIO_CREATE_CORO (++dev::CORO_NUMS)
#define MAGIO_DESTROY_CORO (--dev::CORO_NUMS)
#define MAGIO_CHECK_RESOURCE \
    do { \
        if (magio::dev::SOCKET_NUMS > 0) { \
            M_ERROR("socket leak {}", magio::dev::SOCKET_NUMS); \
        } \
        if (magio::dev::CORO_NUMS > 0) { \
            M_ERROR("coro leak {}", magio::dev::CORO_NUMS); \
        } \
        M_INFO("check over"); \
    } while(0)

#else
#define MAGIO_MAKE_SOCKET 
#define MAGIO_CLOSE_SOCKET 
#define MAGIO_CREATE_CORO
#define MAGIO_DESTROY_CORO
#define MAGIO_CHECK_RESOURCE
#endif

}

}