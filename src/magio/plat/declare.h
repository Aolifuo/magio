#pragma once

#include <cstddef>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace magio {

namespace plat {

enum class Protocol { TCP, UDP };

enum class IOOP {
    Noop,
    Accept,
    Receive,
    Send,
    Connect,
};

constexpr size_t MAGIO_INFINITE = 0xFFFFFFFF;

#ifdef __linux__
using socket_type = int;
constexpr int MAGIO_INVALID_SOCKET = -1;
constexpr size_t MAGIO_LIBURING_ENTRIES = 512;
#endif
#ifdef _WIN32
using socket_type = SOCKET;
constexpr auto MAGIO_INVALID_SOCKET = INVALID_SOCKET;
#endif

}


}