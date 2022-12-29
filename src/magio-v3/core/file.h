#ifndef MAGIO_CORE_FILE_H_
#define MAGIO_CORE_FILE_H_

#include "magio-v3/core/noncopyable.h"
#include <system_error>

namespace magio {

template<typename>
class Coro;

class RandomAccessFile: Noncopyable {
public:
    using Handle =
#ifdef  _WIN32
    void*;
#elif defined (__linux__)
    int;
#endif

    enum Openmode {
        ReadOnly  = 0b000001, 
        WriteOnly = 0b000010,
        ReadWrite = 0b000100,

        Create    = 0b001000,
        Truncate  = 0b010000,
        Append    = 0b100000
    };

    RandomAccessFile();

    RandomAccessFile(const char* path, int mode, int x = 0744);

    ~RandomAccessFile();

    RandomAccessFile(RandomAccessFile&& other) noexcept;

    RandomAccessFile& operator=(RandomAccessFile&& other) noexcept;

    void open(const char* path, int mode, int x = 0744);

    void close();

    Coro<size_t> read_at(size_t offset, char* buf, size_t len, std::error_code& ec);

    Coro<size_t> write_at( size_t offset, const char* msg, size_t len, std::error_code& ec);

    void sync_all();

    void sync_data();

    operator bool() const {
        return handle_ != (Handle)-1;
    }

private:
    void reset();

    Handle handle_;
    // only for win
    bool enable_app_ = false;
    size_t size_ = 0;
};

class File: Noncopyable {
public:
    enum Openmode {
        ReadOnly  = 0b000001, 
        WriteOnly = 0b000010,
        ReadWrite = 0b000100,

        Create    = 0b001000,
        Truncate  = 0b010000,
        Append    = 0b100000
    };

    File();

    File(const char* path, int mode, int x = 0744);

    File(File&& other) noexcept;

    File& operator=(File&& other) noexcept;

    void open(const char* path, int mode, int x = 0744);

    void close();

    Coro<size_t> read(char* buf, size_t len, std::error_code& ec);

    Coro<size_t> write(const char* buf, size_t len, std::error_code& ec);

    void sync_all();

    void sync_data();

    operator bool() const {
        return file_;
    }

private:
    File(RandomAccessFile file);

    RandomAccessFile file_;
    size_t read_offset_ = 0;
};

}

#endif