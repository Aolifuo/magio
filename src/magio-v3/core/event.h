#ifndef MAGIO_CORE_EVENT_H_
#define MAGIO_CORE_EVENT_H_

#include <system_error>
#include "magio-v3/core/noncopyable.h"

namespace magio {

template<typename>
class Coro;

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

    Coro<void> send(std::error_code& ec);

    Coro<void> receive(std::error_code& ec);

private:
    Handle handle_;
};

}

#endif