#ifndef MAGIO_CORE_FILE_H_
#define MAGIO_CORE_FILE_H_

#include "magio-v3/utils/functor.h"
#include "magio-v3/utils/noncopyable.h"
#include "magio-v3/core/error.h"
#include "magio-v3/core/common.h"

namespace magio {

class CoroContext;

template<typename>
class Coro;

class RandomAccessFile: Noncopyable {
    friend class File;

public:
    using Handle = IoHandle;

    enum Openmode {
        ReadOnly  = 0b000001, 
        WriteOnly = 0b000010,
        ReadWrite = 0b000100,

        Create    = 0b001000,
        Truncate  = 0b010000,
        Append    = 0b100000
    };

    RandomAccessFile();

    ~RandomAccessFile();

    RandomAccessFile(RandomAccessFile&& other) noexcept;

    RandomAccessFile& operator=(RandomAccessFile&& other) noexcept;

    [[nodiscard]]
    static RandomAccessFile open(const char* path, int mode, int x = 0744);

    void cancel();

    void close();

#ifdef MAGIO_USE_CORO
    [[nodiscard]]
    Coro<Result<size_t>> read_at(size_t offset, char* buf, size_t len);

    [[nodiscard]]
    Coro<Result<size_t>> write_at(size_t offset, const char* msg, size_t len);
#endif

    void read_at(size_t offset, char* buf, size_t len, Functor<void(std::error_code, size_t)>&& completion_cb);

    void write_at(size_t offset, const char* msg, size_t len, Functor<void(std::error_code, size_t)>&& completion_cb);

    void sync_all();

    void sync_data();

    void attach_context();

    operator bool() const {
        return handle_.a != kInvalidHandle;
    }

private:
    void reset();

    Handle handle_;
    CoroContext* attached_;
    // only for win
    bool enable_app_ = false;
};

class File: Noncopyable {
public:
    using Handle = IoHandle;

    enum Openmode {
        ReadOnly  = 0b000001, 
        WriteOnly = 0b000010,
        ReadWrite = 0b000100,

        Create    = 0b001000,
        Truncate  = 0b010000,
        Append    = 0b100000
    };

    File();

    ~File();

    File(File&& other) noexcept;

    File& operator=(File&& other) noexcept;

    [[nodiscard]]
    static File open(const char* path, int mode, int x = 0744);

    void cancel();

    void close();

#ifdef MAGIO_USE_CORO
    [[nodiscard]]
    Coro<Result<size_t>> read(char* buf, size_t len);
    
    [[nodiscard]]
    Coro<Result<size_t>> write(const char* msg, size_t len);
#endif

    void read(char* buf, size_t len, Functor<void(std::error_code, size_t)>&& completion_cb);
    
    void write(const char* msg, size_t len, Functor<void(std::error_code, size_t)>&& completion_cb);

    void attach_context();

    void sync_all();

    void sync_data();

    operator bool() const {
        return handle_.a != kInvalidHandle;
    }

private:
    void reset();
    
    Handle handle_;
    CoroContext* attached_;
    size_t read_offset_ = 0;
    size_t write_offset_ = 0;
};

}

#endif