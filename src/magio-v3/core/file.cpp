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

struct RandomAccessFile::Data {
#ifdef _WIN32
    HANDLE handle;
#elif defined (__linux__)
    int handle;
#endif
    // only for win
    bool enable_app = false;
    size_t size = 0;
};

RandomAccessFile::RandomAccessFile(Data* p) {
    data_ = p;
}

RandomAccessFile::~RandomAccessFile() {
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

RandomAccessFile::RandomAccessFile(RandomAccessFile&& other) noexcept {
    data_ = other.data_;
    other.data_ = nullptr;
}

RandomAccessFile& RandomAccessFile::operator=(RandomAccessFile&& other) noexcept {
    data_ = other.data_;
    other.data_ = nullptr;
    return *this;
}

#ifdef _WIN32
RandomAccessFile RandomAccessFile::open(const char *path, int mode, int x) {
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
        return {};
    }

    std::error_code ec;
    this_context::get_service().relate(handle, ec);
    if (ec) {
        M_SYS_ERROR("cannot add file handle to iocp: {}", ec.value());
        ::CloseHandle(handle);
        return {};
    }

    size_t size = 0;
    LARGE_INTEGER large_int;
    ::GetFileSizeEx(handle, &large_int);
    size = large_int.QuadPart;

    return {new Data{handle, enable_app, size}};
}

void RandomAccessFile::sync_all() {
    
}

void RandomAccessFile::sync_data() {
    
}

#elif defined (__linux__)
RandomAccessFile RandomAccessFile::open(const char *path, int mode, int x) {
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

void RandomAccessFile::sync_all() {
    ::fsync(data_->handle);
}

void RandomAccessFile::sync_data() {
    ::fdatasync(data_->handle);
}

#endif

Coro<size_t> RandomAccessFile::read_at(size_t offset, char *buf, size_t len, std::error_code &ec) {
    ResumeHandle rhandle;
    IoContext ioc;
    ioc.handle = decltype(IoContext::handle)(data_->handle);
    ioc.buf.buf = buf;
    ioc.buf.len = len;
    ioc.ptr = &rhandle;
    ioc.cb = completion_callback;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().read_file(ioc, offset);
    });

    ec = rhandle.ec;

    co_return ioc.buf.len;
}

Coro<size_t> RandomAccessFile::write_at(size_t offset, const char *msg, size_t len, std::error_code &ec) {
    ResumeHandle rhandle;
    IoContext ioc;
    ioc.handle = decltype(IoContext::handle)(data_->handle);
    ioc.buf.buf = (char*)msg;
    ioc.buf.len = len;
    ioc.ptr = &rhandle;
    ioc.cb = completion_callback;

    if (data_->enable_app) {
        offset = data_->size;
    }

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().write_file(ioc, offset);
    });

    ec = rhandle.ec;
    data_->size += ioc.buf.len;

    co_return ioc.buf.len;
}

File::File() {

}

File::File(RandomAccessFile file)
    : file_(std::move(file))
{ }

File::File(File&& other) noexcept
    : file_(std::move(other.file_))
    , read_offset_(other.read_offset_)
{ }

File& File::operator=(File&& other) noexcept {
    file_ = std::move(other.file_);
    read_offset_ = other.read_offset_;
    return *this;
}

File File::open(const char *path, int mode, int x) {
    auto rafile = RandomAccessFile::open(path, (RandomAccessFile::Openmode)mode, x);
    if (!rafile) {
        return {};
    }
    return {std::move(rafile)};
}

Coro<size_t> File::read(char *buf, size_t len, std::error_code &ec) {
    size_t rd = co_await file_.read_at(read_offset_, buf, len, ec);
    read_offset_ += rd;
    co_return rd;
}

Coro<size_t> File::write(char *buf, size_t len, std::error_code &ec) {
    size_t wl = co_await file_.write_at(0, buf, len, ec);
    co_return wl;
}

void File::sync_all() {
    file_.sync_all();
}

void File::sync_data() {
    file_.sync_data();
}

}