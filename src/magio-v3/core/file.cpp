#include "magio-v3/core/file.h"

#include "magio-v3/utils/logger.h"
#include "magio-v3/core/coro_context.h"
#include "magio-v3/core/io_context.h"

#ifdef _WIN32

#elif defined (__linux__)
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace magio {

namespace detail {

IoHandle open_file(const char* path, int mode, int x) {
    IoHandle ioh{.a = kInvalidHandle};
#ifdef _WIN32
    int first = mode & 0b000111;
    if (!(first)) {
        return ioh;
    }

    DWORD desired_access = 0;
    switch (first) {
    case File::ReadOnly:
        desired_access = GENERIC_READ;
        break;
    case File::WriteOnly:
        desired_access = GENERIC_WRITE;
        break;
    case File::ReadWrite:
        desired_access = GENERIC_READ | GENERIC_WRITE;
        break;
    default:
        return ioh;
    }

    // OPEN_EXISTING must exit
    // OPEN_ALWAYS if not exit then create
    // CREATE_ALWAYS create and truncate

    DWORD createion_disposition = OPEN_EXISTING;

    if (mode & File::Create) {
        createion_disposition = OPEN_ALWAYS; // create
    }
    if (mode & File::Truncate) {
        if (mode & File::Create) {
            createion_disposition = CREATE_ALWAYS;
        } else {
            createion_disposition = TRUNCATE_EXISTING;
        }
    }
    if (mode & File::Append) {
        //
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
        return ioh;
    }

    ioh.ptr = handle;

#elif defined (__linux__)
    int flag = 0;
    int first = mode & 0b000111;
    if (!(first)) {
        return ioh;
    }
    switch (first) {
    case File::ReadOnly:
        flag = O_RDONLY;
        break;
    case File::WriteOnly:
        flag = O_WRONLY;
        break;
    case File::ReadWrite:
        flag = O_RDWR;
        break;
    default:
        return ioh;
    }

    if (mode & File::Create) {
        flag |= O_CREAT;
    }
    if (mode & File::Truncate) {
        flag |= O_TRUNC;
    }
    if (mode & File::Append) {
        flag |= O_APPEND;
    }

    int fd = ::open(path, flag, x);
    if (-1 == fd) {
        return ioh;
    }

    ioh.a = fd;
#endif

    return ioh;
}

void close_file(IoHandle ioh) {
#ifdef _WIN32
    ::CloseHandle(ioh.ptr);
#elif defined (__linux__)
    ::close(ioh.a);
#endif
}

void file_sync_all(IoHandle ioh) {
#ifdef _WIN32
    
#elif defined (__linux__)
    ::fsync(ioh.a);
#endif
}

void file_sync_data(IoHandle ioh) {
#ifdef _WIN32
    
#elif defined (__linux__)
    ::fdatasync(ioh.a);
#endif
}

}

RandomAccessFile::RandomAccessFile() {
    reset();
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

RandomAccessFile RandomAccessFile::open(const char *path, int mode, int x) {
    RandomAccessFile file;
    file.handle_ = detail::open_file(path, mode, x);
    if (mode & File::Append) {
        file.enable_app_ = true;
    }
    return file;
}

void RandomAccessFile::cancel() {
    if (handle_.a != kInvalidHandle) {
        this_context::get_service().cancel(handle_);
    } 
}

void RandomAccessFile::close() {
    if (handle_.a != kInvalidHandle) {
        detail::close_file(handle_);
        reset();
    }
}

void RandomAccessFile::sync_all() {
    if (kInvalidHandle != handle_.a) {
        detail::file_sync_all(handle_);
    }
}

void RandomAccessFile::sync_data() {
    if (kInvalidHandle != handle_.a) {
        detail::file_sync_data(handle_);
    }
}

#ifdef MAGIO_USE_CORO
Coro<Result<size_t>> RandomAccessFile::read_at(size_t offset, char *buf, size_t len) {
    attach_context();
    ResumeHandle rh;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().read_file(handle_, buf, len, offset, &rh, resume_callback);
    });

    co_return {rh.res, rh.ec};
}

Coro<Result<size_t>> RandomAccessFile::write_at(size_t offset, const char *msg, size_t len) {
    attach_context();
    ResumeHandle rh;

#ifdef _WIN32
    if (enable_app_) {
        LARGE_INTEGER large_int;
        ::GetFileSizeEx(handle_.ptr, &large_int);
        offset = large_int.QuadPart;
    }
#endif

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().write_file(handle_, msg, len, offset, &rh, resume_callback);
    });

    co_return {rh.res, rh.ec};
}
#endif

void RandomAccessFile::read_at(size_t offset, char *buf, size_t len, Functor<void (std::error_code, size_t)> &&completion_cb) {
    using Cb = Functor<void (std::error_code, size_t)>;
    attach_context();

    this_context::get_service().read_file(handle_, buf, len, offset, new Cb(std::move(completion_cb)),
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec, ioc->res);
            delete cb;
            delete ioc;
        });
}

void RandomAccessFile::write_at(size_t offset, const char *msg, size_t len, Functor<void (std::error_code, size_t)> &&completion_cb) {
    using Cb = Functor<void (std::error_code, size_t)>;
    attach_context();

#ifdef _WIN32
    if (enable_app_) {
        LARGE_INTEGER large_int;
        ::GetFileSizeEx(handle_.ptr, &large_int);
        offset = large_int.QuadPart;
    }
#endif

    this_context::get_service().write_file(handle_, msg, len, offset, new Cb(std::move(completion_cb)),
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec, ioc->res);
            delete cb;
            delete ioc;
        });
}

void RandomAccessFile::reset() {
    handle_.a = kInvalidHandle;
    attached_ = nullptr;
    enable_app_ = false;
}

void RandomAccessFile::attach_context() {
    if (kInvalidHandle == handle_.a) {
        return;
    }

    if (!attached_) {
        std::error_code ec;
        attached_ = LocalContext;
        this_context::get_service().attach(handle_, ec);
    } else if (attached_ != LocalContext) {
        M_FATAL("{}", "The file cannot be attached to different context");
    }
}

File::File() {
    reset();
}

File::~File() {
    close();
}

File::File(File&& other) noexcept
    : handle_(other.handle_)
    , attached_(other.attached_)
    , read_offset_(other.read_offset_)
    , write_offset_(other.write_offset_)
{ 
    other.reset();
}

File& File::operator=(File&& other) noexcept {
    handle_ = other.handle_;
    attached_ = other.attached_;
    read_offset_ = other.read_offset_;
    write_offset_ = other.write_offset_;
    other.reset();
    return *this;
}

File File::open(const char *path, int mode, int x) {
    File file;
    file.handle_ = detail::open_file(path, mode, x);
    if (mode & File::Append) {
#ifdef _WIN32
        LARGE_INTEGER large_int;
        ::GetFileSizeEx(file.handle_.ptr, &large_int);
        file.write_offset_ = large_int.QuadPart;
#endif
    } else {
        file.write_offset_ = 0;
    }

    file.read_offset_ = 0;
    return file;
}

void File::cancel() {
    if (kInvalidHandle != handle_.a) {
        this_context::get_service().cancel(handle_);
    }
}

void File::close() {
    if (kInvalidHandle != handle_.a) {
        detail::close_file(handle_);
        reset();
    }
}

#ifdef MAGIO_USE_CORO
Coro<Result<size_t>> File::read(char *buf, size_t len) {
    attach_context();
    ResumeHandle rh;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().read_file(handle_, buf, len, read_offset_, &rh, resume_callback);
    });

    read_offset_ += rh.res;
    co_return {rh.res, rh.ec};
}

Coro<Result<size_t>> File::write(const char *msg, size_t len) {
    attach_context();
    ResumeHandle rh;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().write_file(handle_, msg, len, write_offset_, &rh, resume_callback);
    });

    write_offset_ += rh.res;
    co_return {rh.res, rh.ec};
}
#endif

void File::read(char *buf, size_t len, Functor<void (std::error_code, size_t)> &&completion_cb) {
    using Cb = Functor<void (std::error_code, size_t)>;
    attach_context();
    struct FileResume {
        Cb cb;
        File* pfile;
    };
    auto fr = new FileResume{.cb = std::move(completion_cb), .pfile = this};

    this_context::get_service().read_file(handle_, buf, len, read_offset_, fr,
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto fr = (FileResume*)ptr;
            fr->pfile->read_offset_ += ioc->res;
            (fr->cb)(ec, ioc->res);
            delete fr;
            delete ioc;
        });
}

void File::write(const char *msg, size_t len, Functor<void (std::error_code, size_t)> &&completion_cb) {
    using Cb = Functor<void (std::error_code, size_t)>;
    attach_context();
    struct FileResume {
        Cb cb;
        File* pfile;
    };
    auto fr = new FileResume{.cb = std::move(completion_cb), .pfile = this};

    this_context::get_service().write_file(handle_, msg, len, write_offset_, fr,
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto fr = (FileResume*)ptr;
            fr->pfile->write_offset_ += ioc->res;
            (fr->cb)(ec, ioc->res);
            delete fr;
            delete ioc;
        });
}

void File::sync_all() {
    if (kInvalidHandle != handle_.a) {
        detail::file_sync_all(handle_);
    }
}

void File::sync_data() {
    if (kInvalidHandle != handle_.a) {
        detail::file_sync_data(handle_);
    }
}

void File::attach_context() {
    if (kInvalidHandle == handle_.a) {
        return;
    }

    if (!attached_) {
        std::error_code ec;
        attached_ = LocalContext;
        this_context::get_service().attach(handle_, ec);
    } else if (attached_ != LocalContext) {
        M_FATAL("{}", "The file cannot be attached to different context");
    }
}

void File::reset() {
    handle_.a = kInvalidHandle;
    attached_ = nullptr;
    read_offset_ = 0;
    write_offset_ = 0;
}

}