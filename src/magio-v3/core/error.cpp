#include "magio-v3/core/error.h"

#ifdef _WIN32
#include <atlconv.h>
#endif

namespace magio {

std::string SystemError::message(int code) const {
#ifdef _WIN32
    LPTSTR pbuf = nullptr;

    DWORD size = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER 
        | FORMAT_MESSAGE_FROM_SYSTEM 
        | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        code,
        MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
        (LPTSTR)&pbuf,
        0,
        NULL
    );

    if (0 == size) {
        return "Unknown error";
    }

    std::string result(pbuf, size);
    LocalFree(pbuf);
    return result;
#elif defined (__linux__)
    return std::strerror(code);
#endif
}

std::error_code make_system_error_code(int code) {
    return {code, SystemError::get()};
}

}