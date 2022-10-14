#pragma once

#include <system_error>
#include <cstring>
#include "magio/dev/Log.h"

#ifdef _WIN32
#include <atlconv.h>
#endif

namespace magio {

// -1 EOF
// -2 Unkown error

#ifdef _WIN32

class WinSystemError: std::error_category {
public:
    const char* name() const noexcept override {
        return "windows system error";
    }

    std::string message(int code) const override {
        switch (code) {
        case -1:
            return "EOF";
        case -2:
            return "IO loop Not start";
        }

        LPTSTR lp_buffer = nullptr;
        if (0 == FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&lp_buffer,
            0,
            NULL)) 
        {
            return "Unknown error";
        } else {
            std::string str = lp_buffer;
            LocalFree(lp_buffer);
            return str;
        }
    }
    
    static std::error_category& get() {
        static WinSystemError win_sys_error;
        return win_sys_error;
    }
private:
};

inline std::error_code make_win_system_error(int code) {
    return std::error_code(code, WinSystemError::get());
}

#define MAGIO_SYSTEM_ERROR \
    (DEBUG_LOG("make error ", ::GetLastError()), make_win_system_error(::GetLastError()))

#define MAGIO_NO_DEBUG_SYSTEM_ERROR \
    ((make_win_system_error(::GetLastError())))

#define MAGIO_THROW_SYSTEM_ERROR \
    do { DEBUG_LOG("throw error"); throw std::system_error(MAGIO_SYSTEM_ERROR); } while(0)


#endif

#ifdef __linux__

class LinuxSystemError: std::error_category {
public:
    const char* name() const noexcept override {
        return "windows system error";
    }

    std::string message(int code) const override {
        switch (code) {
        case -1:
            return "EOF";
        case -2:
            return "Accept error";
        }

        return strerror(code);
    }
    
    static std::error_category& get() {
        static LinuxSystemError linux_sys_error;
        return linux_sys_error;
    }
private:
};

inline std::error_code make_linux_system_error(int code) {
    return std::error_code(code, LinuxSystemError::get());
}

#define MAGIO_SYSTEM_ERROR \
    (DEBUG_LOG("make error ", errno), make_linux_system_error(errno))

#define MAGIO_THROW_SYSTEM_ERROR \
    do { DEBUG_LOG("throw error"); throw std::system_error(MAGIO_SYSTEM_ERROR); } while(0)

#endif

}

