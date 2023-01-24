#ifndef MAGIO_CORE_COMMON_H_
#define MAGIO_CORE_COMMON_H_

namespace magio {

using SocketHandle = 
#ifdef _WIN32
    size_t;
#elif defined (__linux__)
    int;
#endif

struct IoHandle {
    union {
        SocketHandle a;
        void* ptr;
    };
};

constexpr SocketHandle kInvalidHandle = (SocketHandle)-1;

}

#endif