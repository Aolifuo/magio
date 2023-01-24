#include "magio-v3/core/pipe.h"

#include "magio-v3/core/error.h"
#include "magio-v3/core/io_context.h"
#include "magio-v3/core/coro_context.h"

#ifdef _WIN32

#elif defined (__linux__)
#include <unistd.h>
#endif

namespace magio {

ReadablePipe::~ReadablePipe() {
    close();
}

ReadablePipe::ReadablePipe(Handle handle)
    : handle_(handle)
{ }

ReadablePipe::ReadablePipe(ReadablePipe&& other) noexcept
    : handle_(other.handle_)
{
    other.handle_.a = kInvalidHandle;
}

ReadablePipe& ReadablePipe::operator=(ReadablePipe &&other) noexcept {
    handle_ = other.handle_;
    other.handle_.a = kInvalidHandle;
    return *this;
}

#ifdef MAGIO_USE_CORO
Coro<Result<size_t>> ReadablePipe::read(char *buf, size_t len) {
    ResumeHandle rh;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().read_file(handle_, buf, len, 0, &rh, resume_callback);
    });

    co_return {rh.res, rh.ec};
}
#endif

void ReadablePipe::read(char *buf, size_t len, Functor<void (std::error_code, size_t)>&& cb) {
    using Cb = Functor<void (std::error_code, size_t)>;

    this_context::get_service().read_file(handle_, buf, len, 0, new Cb(std::move(cb)), 
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec, ioc->res);
            delete cb;
            delete ioc;
        });
}

void ReadablePipe::close() {
    if (handle_.a != kInvalidHandle) {
#ifdef _WIN32
        ::CloseHandle(handle_.ptr);
#elif defined (__linux__)
        ::close(handle_.a);
#endif
        handle_.a = kInvalidHandle;
    }
}

WritablePipe::WritablePipe(Handle handle)
    : handle_(handle)
{ }

WritablePipe::~WritablePipe() {
    close();
}

WritablePipe::WritablePipe(WritablePipe&& other) noexcept
    : handle_(other.handle_)
{
    other.handle_.a = kInvalidHandle;
}

WritablePipe& WritablePipe::operator=(WritablePipe &&other) noexcept {
    handle_ = other.handle_;
    other.handle_.a = kInvalidHandle;
    return *this;
}

#ifdef MAGIO_USE_CORO
Coro<Result<size_t>> WritablePipe::write(const char *msg, size_t len) {
    ResumeHandle rh;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().write_file(handle_, msg, len, 0, &rh, resume_callback);
    });

    co_return {rh.res, rh.ec};
}
#endif

void WritablePipe::write(const char *msg, size_t len, Functor<void (std::error_code, size_t)> &&cb) {
    using Cb = Functor<void (std::error_code, size_t)>;

    this_context::get_service().write_file(handle_, msg, len, 0, new Cb(std::move(cb)), 
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec, ioc->res);
            delete cb;
            delete ioc;
        });
}

void WritablePipe::close() {
    if (handle_.a != kInvalidHandle) {
#ifdef _WIN32
        ::CloseHandle(handle_.ptr);
#elif defined (__linux__)
        ::close(handle_.a);
#endif
        handle_.a = kInvalidHandle;
    }
}

Result<std::tuple<ReadablePipe, WritablePipe>> make_pipe() {
#ifdef _WIN32
    // TODO
    return {std::error_code{}};
#elif defined (__linux__)
    int pipefd[2];
    if (-1 == ::pipe(pipefd)) {
        return{SYSTEM_ERROR_CODE};
    }

    return {{ReadablePipe(IoHandle{.a = pipefd[0]}), WritablePipe(IoHandle{.a = pipefd[1]})}};
#endif
}

}