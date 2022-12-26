#ifndef MAGIO_CORE_FILE_H_
#define MAGIO_CORE_FILE_H_

#include "magio-v3/core/noncopyable.h"
#include <system_error>

namespace magio {

template<typename>
class Coro;

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

    File() = default;

    ~File();

    File(File&& other) noexcept;

    File& operator=(File&& other) noexcept; 

    static File open(const char* path, int mode, int x = 0744);

    Coro<size_t> read(char* buf, size_t len, std::error_code& ec);

    Coro<size_t> write(const char* msg, size_t len, std::error_code& ec);

    void sync_all();

    void sync_data();

    operator bool() const {
        return data_ != nullptr;
    }
private:
    struct Data;

    File(Data* p);

    Data* data_ = nullptr;
};


}


#endif