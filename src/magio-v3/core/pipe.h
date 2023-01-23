#ifndef MAGIO_CORE_PIPE_H_
#define MAGIO_CORE_PIPE_H_

#include "magio-v3/core/error.h"
#include "magio-v3/core/common.h"
#include "magio-v3/core/functor.h"
#include "magio-v3/core/noncopyable.h"

namespace magio {

template<typename>
class Coro;

class WritablePipe;

class ReadablePipe: Noncopyable {
    friend Result<std::tuple<ReadablePipe, WritablePipe>> make_pipe();

public:
    using Handle = IoHandle;
    
    ReadablePipe() = default;

    ~ReadablePipe();

    ReadablePipe(ReadablePipe&& other) noexcept;

    ReadablePipe& operator=(ReadablePipe&& other) noexcept;

#ifdef MAGIO_USE_CORO
    [[nodiscard]]
    Coro<Result<size_t>> read(char* buf, size_t len);
#endif

    void read(char* buf, size_t len, Functor<void(std::error_code, size_t)>&& cb);

    void close();

    operator bool() {
        return handle_.a != -1;
    }

private:
    ReadablePipe(Handle);

    Handle handle_{.a = -1};
};

class WritablePipe: Noncopyable {
    friend Result<std::tuple<ReadablePipe, WritablePipe>> make_pipe();

public:
    using Handle = IoHandle;

    WritablePipe() = default;

    ~WritablePipe();

    WritablePipe(WritablePipe&& other) noexcept;

    WritablePipe& operator=(WritablePipe&& other) noexcept;

#ifdef MAGIO_USE_CORO
    [[nodiscard]]
    Coro<Result<size_t>> write(const char* msg, size_t len);
#endif

    void write(const char* msg, size_t len, Functor<void(std::error_code, size_t)>&& cb);

    void close();

    operator bool() {
        return handle_.a != -1;
    }

private:
    WritablePipe(Handle);

    Handle handle_{.a = -1};
};

Result<std::tuple<ReadablePipe, WritablePipe>> make_pipe();

}

#endif