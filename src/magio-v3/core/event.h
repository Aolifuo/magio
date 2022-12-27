#ifndef MAGIO_CORE_EVENT_H_
#define MAGIO_CORE_EVENT_H_

#include <system_error>
#include "magio-v3/core/noncopyable.h"

namespace magio {

template<typename>
class Coro;

struct IoContext;

class CoroContext;

// only for linux, TODO win
// 1:1 coroutine, thread safe
class SingleEvent: Noncopyable {
public:
    using Handle =
#ifdef _WIN32
    void*;
#elif defined (__linux__)
    int;
#endif
    SingleEvent();

    ~SingleEvent();

    // never wait
    Coro<void> send(std::error_code& ec);

    Coro<void> receive(std::error_code& ec);

private:
    CoroContext* ctx_;

    Handle handle_;
};

}

#endif