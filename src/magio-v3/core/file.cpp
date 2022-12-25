#include "magio-v3/core/file.h"

#include "magio-v3/core/coro_context.h"
#include "magio-v3/core/io_context.h"
#include "magio-v3/core/detail/completion_callback.h"

#ifdef _WIN32

#elif defined (__linux__)
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace magio {

#ifdef _WIN32

#elif defined (__linux__)

struct File::Data {
    int handle;
};

File::File(Data* p) {
    data_ = p;
}

File::~File() {
    if (data_) {
        ::close(data_->handle);
        delete data_;
        data_ = nullptr;
    }
}

File::File(File&& other) noexcept {
    data_ = other.data_;
    other.data_ = nullptr;
}

File& File::operator=(File&& other) noexcept {
    data_ = other.data_;
    other.data_ = nullptr;
    return *this;
}

File File::open(const char *path, int mode, int x) {
    int flag = 0;
    int first = mode & 0b000111;
    if (!(first)) {
        return {};
    }
    switch (first) {
    case ReadOnly:
        flag = O_RDONLY;
        break;
    case WriteOnly:
        flag = O_WRONLY;
        break;
    case ReadWrite:
        flag = O_RDWR;
        break;
    default:
        return {};
    }

    if (mode & Create) {
        flag |= O_CREAT;
    }
    if (mode & Truncate) {
        flag |= O_TRUNC;
    }
    if (mode & Append) {
        flag |= O_APPEND;
    }

    int fd = ::open(path, flag, x);
    if (-1 == fd) {
        return {};
    }

    Data* p = new Data{fd};
    return {p};
}

Coro<size_t> File::read(char *buf, size_t len, std::error_code &ec) {
    detail::ResumeHandle rhandle;
    IoContext ioc;
    ioc.handle = data_->handle;
    ioc.buf.buf = buf;
    ioc.buf.len = len;
    ioc.ptr = &rhandle;
    ioc.cb = detail::completion_callback;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().read_file(ioc);
    });

    ec = rhandle.ec;
    if (ec) {
        co_return {};
    }

    co_return ioc.buf.len;
}

Coro<size_t> File::write(const char *msg, size_t len, std::error_code &ec) {
    detail::ResumeHandle rhandle;
    IoContext ioc;
    ioc.handle = data_->handle;
    ioc.buf.buf = (char*)msg;
    ioc.buf.len = len;
    ioc.ptr = &rhandle;
    ioc.cb = detail::completion_callback;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().write_file(ioc);
    });

    ec = rhandle.ec;
    if (ec) {
        co_return {};
    }

    co_return ioc.buf.len;
}

#endif

}