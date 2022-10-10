#pragma once

#include <atomic>
#include "magio/dev/Config.h"
#include "magio/dev/Log.h"

namespace magio {

namespace dev {

inline std::atomic_int SOCKET_NUMS = 0;
inline std::atomic_int BUFFER_NUMS = 0;

#ifdef MAGIO_DEBUG
#define MAGIO_MAKE_SOCKET (++dev::SOCKET_NUMS)
#define MAGIO_CLOSE_SOCKET (--dev::SOCKET_NUMS)
#define MAGIO_CHECK_RESOURCE \
    do { \
        if (magio::dev::SOCKET_NUMS > 0) { \
            DEBUG_LOG("socket leak ", magio::dev::SOCKET_NUMS); \
        } \
        DEBUG_LOG("check end"); \
    } while(0)

#else
#define MAGIO_MAKE_SOCKET 
#define MAGIO_CLOSE_SOCKET 
#define MAGIO_GET_BUFFER 
#define MAGIO_PUT_BUFFER
#define MAGIO_CHECK_RESOURCE
#endif

}


}