#include "magio-v3/core/event.h"

#include "magio-v3/core/coro_context.h"
#include "magio-v3/core/io_context.h"

#ifdef _WIN32

#elif defined (__linux__)
#include <unistd.h>
#include <sys/eventfd.h>
#endif

namespace magio {

SingleEvent::SingleEvent() {
    handle_ =
#ifdef _WIN32
    nullptr;
#elif defined (__linux__)
    ::eventfd(0, 0);
#endif
}

SingleEvent::~SingleEvent() {
#ifdef _WIN32
    
#elif defined (__linux__)
    ::close(handle_);
#endif
}

Coro<void> SingleEvent::send(std::error_code& ec) {
    uint64_t msg = 1;
    ResumeHandle rh;
    IoContext ioc;
    ioc.handle = decltype(IoContext::handle)(handle_);
    ioc.buf.buf = (char*)&msg;
    ioc.buf.len = sizeof(msg);
    ioc.ptr = &rh;
    ioc.cb = completion_callback;
    
    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().write_file(ioc, 0);
    });

    ec = rh.ec;
    if (ec) {
        co_return;
    }

    co_return;
}

Coro<void> SingleEvent::receive(std::error_code& ec) {
    uint64_t msg;
    ResumeHandle rh;
    IoContext ioc;
    ioc.handle = decltype(IoContext::handle)(handle_);
    ioc.buf.buf = (char*)&msg;
    ioc.buf.len = sizeof(msg);
    ioc.ptr = &rh;
    ioc.cb = completion_callback;
    
    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().read_file(ioc, 0);
    });

    ec = rh.ec;
    if (ec) {
        co_return;
    }

    co_return;
}

}