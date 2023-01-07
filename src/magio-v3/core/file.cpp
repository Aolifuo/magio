#include "magio-v3/core/file.h"

#include "magio-v3/core/logger.h"
#include "magio-v3/core/coro_context.h"
#include "magio-v3/core/io_context.h"

#ifdef _WIN32

#elif defined (__linux__)
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace magio {

RandomAccessFile::RandomAccessFile() {
    reset();
}

RandomAccessFile::RandomAccessFile(const char *path, int mode, int x)
    : RandomAccessFile() 
{
    open(path, mode, x);
}

RandomAccessFile::~RandomAccessFile() {
    close();
}

RandomAccessFile::RandomAccessFile(RandomAccessFile&& other) noexcept
    : handle_(other.handle_)
    , enable_app_(other.enable_app_)
{
    other.reset();
}

RandomAccessFile& RandomAccessFile::operator=(RandomAccessFile&& other) noexcept {
    handle_ = other.handle_;
    enable_app_ = other.enable_app_;
    other.reset();
    return *this;
}

void RandomAccessFile::open(const char *path, int mode, int x) {
    close();

#ifdef _WIN32
    int first = mode & 0b000111;
    if (!(first)) {
        return;
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
        return;
    }

    // OPEN_EXISTING must exit
    // OPEN_ALWAYS if not exit then create
    // CREATE_ALWAYS create and truncate

    DWORD createion_disposition = OPEN_EXISTING;
    bool enable_app = false;
    if (mode & Create) {
        createion_disposition = OPEN_ALWAYS; // create
    }
    if (mode & Truncate) {
        if (mode & Create) {
            createion_disposition = CREATE_ALWAYS;
        } else {
            createion_disposition = TRUNCATE_EXISTING;
        }
    }
    if (mode & Append) {
        enable_app = true;
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
        return;
    }

    std::error_code ec;
    this_context::get_service().relate(handle, ec);
    if (ec) {
        M_SYS_ERROR("cannot add file handle to iocp: {}", ec.value());
        ::CloseHandle(handle);
        return;
    }

    handle_ = handle;
    enable_app_ = enable_app;

#elif defined (__linux__)
    int flag = 0;
    int first = mode & 0b000111;
    if (!(first)) {
        return;
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
        return;
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
        return;
    }

    handle_ = fd;
#endif
}

void RandomAccessFile::cancel() {
    if (handle_ != (Handle)-1) {
        IoContext ioc{.handle = decltype(IoContext::handle)(handle_)};
        this_context::get_service().cancel(ioc);
    } 
}

void RandomAccessFile::close() {
    if (handle_ != (Handle)-1) {
#ifdef _WIN32
        ::CloseHandle(handle_);
#elif defined (__linux__)
        ::close(handle_);
#endif
    reset();
    }
}

void RandomAccessFile::sync_all() {
#ifdef _WIN32
    
#elif defined (__linux__)
    ::fsync(handle_);
#endif
}

void RandomAccessFile::sync_data() {
#ifdef _WIN32

#elif defined (__linux__)
    ::fdatasync(handle_);
#endif
}

#ifdef MAGIO_USE_CORO
Coro<size_t> RandomAccessFile::read_at(size_t offset, char *buf, size_t len, std::error_code &ec) {
    ResumeHandle rhandle;
    IoContext ioc{
        .handle = decltype(IoContext::handle)(handle_),
        .buf = io_buf(buf, len),
        .ptr = &rhandle,
        .cb = completion_callback
    };

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().read_file(ioc, offset);
    });

    ec = rhandle.ec;
    co_return ioc.buf.len;
}

Coro<size_t> RandomAccessFile::write_at(size_t offset, const char *msg, size_t len, std::error_code &ec) {
    ResumeHandle rhandle;
    IoContext ioc{
        .handle = decltype(IoContext::handle)(handle_),
        .buf = io_buf((char*)msg, len),
        .ptr = &rhandle,
        .cb = completion_callback
    };

#ifdef _WIN32
    if (enable_app_) {
        LARGE_INTEGER large_int;
        ::GetFileSizeEx(handle_, &large_int);
        offset = large_int.QuadPart;
    }
#endif

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().write_file(ioc, offset);
    });

    ec = rhandle.ec;
    co_return ioc.buf.len;
}
#endif

void RandomAccessFile::read_at(size_t offset, char *buf, size_t len, std::function<void (std::error_code, size_t)> &&completion_cb) {
    using Cb = std::function<void (std::error_code, size_t)>;
    auto ioc = new IoContext{
        .handle = decltype(IoContext::handle)(handle_),
        .buf= io_buf(buf, len),
        .ptr = new Cb(std::move(completion_cb)),
        .cb = [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec, ioc->buf.len);
            delete ioc;
            delete cb;
        }
    };

    this_context::get_service().read_file(*ioc, offset);
}

void RandomAccessFile::write_at(size_t offset, const char *msg, size_t len, std::function<void (std::error_code, size_t)> &&completion_cb) {
    using Cb = std::function<void (std::error_code, size_t)>;
    auto ioc = new IoContext{
        .handle = decltype(IoContext::handle)(handle_),
        .buf = io_buf((char*)msg, len),
        .ptr = new Cb(std::move(completion_cb)),
        .cb = [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec, ioc->buf.len);
            delete ioc;
            delete cb;
        }
    };

#ifdef _WIN32
    if (enable_app_) {
        LARGE_INTEGER large_int;
        ::GetFileSizeEx(handle_, &large_int);
        offset = large_int.QuadPart;
    }
#endif
    this_context::get_service().write_file(*ioc, offset);
}

void RandomAccessFile::reset() {
    handle_ = (Handle)-1;
    enable_app_ = false;
}

File::File() {

}

File::File(RandomAccessFile file)
    : file_(std::move(file))
{ }

File::File(const char *path, int mode, int x)
    : file_(path, (RandomAccessFile::Openmode)mode, x) 
{ }

File::File(File&& other) noexcept
    : file_(std::move(other.file_))
    , read_offset_(other.read_offset_)
    , write_offset_(other.write_offset_)
{ }

File& File::operator=(File&& other) noexcept {
    file_ = std::move(other.file_);
    read_offset_ = other.read_offset_;
    write_offset_ = other.write_offset_;
    return *this;
}

void File::open(const char *path, int mode, int x) {
    file_.open(path, mode, x);
    read_offset_ = 0;
    write_offset_ = 0;
}

void File::cancel() {
    file_.cancel();
}

void File::close() {
    file_.close();
}

#ifdef MAGIO_USE_CORO
Coro<size_t> File::read(char *buf, size_t len, std::error_code &ec) {
    size_t rd = co_await file_.read_at(read_offset_, buf, len, ec);
    read_offset_ += rd;
    co_return rd;
}

Coro<size_t> File::write(const char *buf, size_t len, std::error_code &ec) {
    size_t wl = co_await file_.write_at(write_offset_, buf, len, ec);
    write_offset_ += wl;
    co_return wl;
}
#endif

void File::read(char *buf, size_t len, std::function<void (std::error_code, size_t)> &&completion_cb) {
    file_.read_at(read_offset_, buf, len, [cb = std::move(completion_cb), this](std::error_code ec, size_t len) {
        read_offset_ += len;
        cb(ec, len);
    });
}

void File::write(const char *buf, size_t len, std::function<void (std::error_code, size_t)> &&completion_cb) {
    file_.write_at(write_offset_, buf, len, [cb = std::move(completion_cb), this](std::error_code ec, size_t len) {
        write_offset_ += len;
        cb(ec, len);
    });
}

void File::sync_all() {
    file_.sync_all();
}

void File::sync_data() {
    file_.sync_data();
}

}