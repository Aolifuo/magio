#include "magio-v3/core/file.h"

#include "magio-v3/core/logger.h"
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

struct File::Data {
#ifdef _WIN32
    HANDLE handle;
#elif defined (__linux__)
    int handle;
#endif
    size_t read_offset = 0;
};

File::File(Data* p) {
    data_ = p;
}

File::~File() {
    if (data_) {
#ifdef _WIN32
        ::CloseHandle(data_->handle);
#elif defined (__linux__)
        ::close(data_->handle);
#endif
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

#ifdef _WIN32
File File::open(const char *path, int mode, int x) {
    int first = mode & 0b000111;
    if (!(first)) {
        return {};
    }

    DWORD desired_access = 0;
    switch (first) {
    case ReadOnly:
        desired_access = GENERIC_READ;
        break;
    case WriteOnly:
        desired_access = GENERIC_WRITE;
        break;
    case ReadWrite:
        desired_access = GENERIC_READ | GENERIC_WRITE;
        break;
    default:
        return {};
    }

    DWORD createion_disposition = OPEN_EXISTING;
    if (mode & Create) {
        if (desired_access & GENERIC_WRITE) {
            createion_disposition = CREATE_ALWAYS; // create | truncate
        } else {
            createion_disposition = OPEN_ALWAYS; // create
        }
    }
    if (mode & Append) {
        if (mode & Create) {
            createion_disposition = OPEN_ALWAYS; // create
        } // else must exit
    }
    if (mode & Truncate) {
        if (mode & Create) {
            createion_disposition = CREATE_ALWAYS;
        } else {
            createion_disposition = TRUNCATE_EXISTING;
        }
    }

    LPCTSTR ppath = TEXT(path);
    HANDLE handle = CreateFile(
        ppath, 
        desired_access, 
        FILE_SHARE_READ,
        NULL, 
        createion_disposition, 
        FILE_FLAG_OVERLAPPED, 
        NULL
    );

    if (INVALID_HANDLE_VALUE == handle) {
        return {};
    }

    std::error_code ec;
    this_context::get_service().relate(handle, ec);
    if (ec) {
        M_SYS_ERROR("cannot add file handle to iocp: {}", ec.value());
        ::CloseHandle(handle);
        return {};
    }

    return {new Data{handle}};
}

#elif defined (__linux__)
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
#endif

Coro<size_t> File::read(char *buf, size_t len, std::error_code &ec) {
    detail::ResumeHandle rhandle;
    IoContext ioc;
    ioc.handle = decltype(IoContext::handle)(data_->handle);
    ioc.buf.buf = buf;
    ioc.buf.len = len;
    ioc.ptr = &rhandle;
    ioc.cb = detail::completion_callback;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().read_file(ioc, data_->read_offset);
    });

    ec = rhandle.ec;
    if (ec) {
        co_return {};
    }
    data_->read_offset += ioc.buf.len;
    co_return ioc.buf.len;
}

Coro<size_t> File::write(const char *msg, size_t len, std::error_code &ec) {
    detail::ResumeHandle rhandle;
    IoContext ioc;
    ioc.handle = decltype(IoContext::handle)(data_->handle);
    ioc.buf.buf = (char*)msg;
    ioc.buf.len = len;
    ioc.ptr = &rhandle;
    ioc.cb = detail::completion_callback;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().write_file(ioc, 0);
    });

    ec = rhandle.ec;
    if (ec) {
        co_return {};
    }

    co_return ioc.buf.len;
}


}