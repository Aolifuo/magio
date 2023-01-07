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
    other.handle_ = (Handle)-1;
}

ReadablePipe& ReadablePipe::operator=(ReadablePipe &&other) noexcept {
    handle_ = other.handle_;
    other.handle_ = (Handle)-1;
    return *this;
}

#ifdef MAGIO_USE_CORO
Coro<size_t> ReadablePipe::read(char *buf, size_t len, std::error_code &ec) {
    ResumeHandle rhandle;
    IoContext ioc{
        .handle = decltype(IoContext::handle)(handle_),
        .buf = io_buf(buf, len),
        .ptr = &rhandle,
        .cb = completion_callback
    };

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().read_file(ioc, 0);
    });

    ec = rhandle.ec;
    co_return ioc.buf.len;
}
#endif

void ReadablePipe::read(char *buf, size_t len, std::function<void (std::error_code, size_t)>&& cb) {
    using Cb = std::function<void (std::error_code, size_t)>;
    auto ioc = new IoContext{
        .handle = decltype(IoContext::handle)(handle_),
        .buf = io_buf(buf, len),
        .ptr = new Cb(std::move(cb)),
        .cb = [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec, ioc->buf.len);
            delete cb;
            delete ioc;
        }
    };

    this_context::get_service().read_file(*ioc, 0);
}

void ReadablePipe::close() {
    if (handle_ != (Handle)-1) {
#ifdef _WIN32
        ::CloseHandle(handle_);
#elif defined (__linux__)
        ::close(handle_);
#endif
        handle_ = (Handle)-1;
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
    other.handle_ = (Handle)-1;
}

WritablePipe& WritablePipe::operator=(WritablePipe &&other) noexcept {
    handle_ = other.handle_;
    other.handle_ = (Handle)-1;
    return *this;
}

#ifdef MAGIO_USE_CORO
Coro<size_t> WritablePipe::write(const char *msg, size_t len, std::error_code &ec) {
    ResumeHandle rhandle;
    IoContext ioc{
        .handle = decltype(IoContext::handle)(handle_),
        .buf = io_buf((char*)msg, len),
        .ptr = &rhandle,
        .cb = completion_callback
    };

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().write_file(ioc, 0);
    });

    ec = rhandle.ec;
    co_return ioc.buf.len;
}
#endif

void WritablePipe::write(const char *msg, size_t len, std::function<void (std::error_code, size_t)> &&cb) {
    using Cb = std::function<void (std::error_code, size_t)>;
    auto ioc = new IoContext{
        .handle = decltype(IoContext::handle)(handle_),
        .buf = io_buf((char*)msg, len),
        .ptr = new Cb(std::move(cb)),
        .cb = [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec, ioc->buf.len);
            delete cb;
            delete ioc;
        }
    };

    this_context::get_service().write_file(*ioc, 0);
}

void WritablePipe::close() {
    if (handle_ != (Handle)-1) {
#ifdef _WIN32
        ::CloseHandle(handle_);
#elif defined (__linux__)
        ::close(handle_);
#endif
        handle_ = (Handle)-1;
    }
}

std::tuple<ReadablePipe, WritablePipe> make_pipe(std::error_code& ec) {
#ifdef _WIN32
    HANDLE read_h;
    HANDLE write_h;
    
    // ?
    BOOL status = ::CreatePipe(
        &read_h, 
        &write_h, 
        NULL, 
        0
    );

    if (!status) {
        ec = SYSTEM_ERROR_CODE;
    }

    this_context::get_service().relate(read_h, ec); // ?
    this_context::get_service().relate(write_h, ec); // ?

    return {ReadablePipe(read_h), WritablePipe(write_h)};
#elif defined (__linux__)
    int pipefd[2];
    if (-1 == ::pipe(pipefd)) {
        ec = SYSTEM_ERROR_CODE;
    }

    return {ReadablePipe(pipefd[0]), WritablePipe(pipefd[1])};
#endif
}

}