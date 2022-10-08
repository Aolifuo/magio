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
    #define MAGIO_GET_BUFFER (++dev::BUFFER_NUMS)
    #define MAGIO_PUT_BUFFER (--dev::BUFFER_NUMS)

class ResourceChecker {
public:
    ~ResourceChecker() {
        if (SOCKET_NUMS > 0) {
            DEBUG_LOG("socket leak");
        }

        if (BUFFER_NUMS > 0) {
            DEBUG_LOG("buffer leak");
        }

        DEBUG_LOG("check end");
    }
private:
};

inline ResourceChecker resource_checker;

#else
    #define MAGIO_MAKE_SOCKET 
    #define MAGIO_CLOSE_SOCKET 
    #define MAGIO_GET_BUFFER 
    #define MAGIO_PUT_BUFFER 
#endif

}


}