#ifndef MAGIO_CORE_ERROR_H_
#define MAGIO_CORE_ERROR_H_

#include <system_error>

namespace magio {

class SocketSystemError: public std::error_category {
public:
    const char* name() const noexcept override {
        return "socket system error";
    }

    std::string message(int code) const override;

    static std::error_category& get() {
        static SocketSystemError error;
        return error;
    }
};

std::error_code make_socket_error_code(int code);

#ifdef _WIN32
#define SOCKET_ERROR_CODE make_socket_error_code(::GetLastError())
#elif defined(__linux__)
#define SOCKET_ERROR_CODE make_socket_error_code(errno)
#endif

}



#endif