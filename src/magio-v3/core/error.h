#ifndef MAGIO_CORE_ERROR_H_
#define MAGIO_CORE_ERROR_H_

#include <system_error>

#include "magio-v3/core/unit.h"

namespace magio {

namespace detail {

class GetErrorCode {
public:
    GetErrorCode(std::error_code& ec)
        : ec_(ec) { }
    
    void set_code(std::error_code& ec) {
        ec_ = ec;
    }

private:
    std::error_code& ec_;
};

class ThrowOnError { };

class PanicOnError { };

}

class SystemError: public std::error_category {
public:
    const char* name() const noexcept override {
        return "system error";
    }

    std::string message(int code) const override;

    static std::error_category& get() {
        static SystemError error;
        return error;
    }
};

std::error_code make_system_error_code(int code);

#ifdef _WIN32
#define SYSTEM_ERROR_CODE make_system_error_code(::GetLastError())
#elif defined(__linux__)
#define SYSTEM_ERROR_CODE make_system_error_code(errno)
#endif

template<typename T = void>
class Result;

template<typename T>
class Result {
    template<typename U>
    friend U operator| (Result<U>&& result, detail::GetErrorCode);
    template<typename U>
    friend U operator| (Result<U>&& result, detail::ThrowOnError);
    template<typename U>
    friend U operator| (Result<U>&& result, detail::PanicOnError);

public:
    Result() { }

    Result(T& lvalue, std::error_code ec = {})
        : value_(lvalue), ec_(ec)
    { }

    Result(T&& rvalue, std::error_code ec = {}) 
        : value_(std::move(rvalue)), ec_(ec)
    { }

    Result(std::error_code ec) 
        : ec_(ec)
    { }

private:
    T value_;
    std::error_code ec_;
};

template<>
class Result<void> {
    template<typename U>
    friend U operator| (Result<U>&& result, detail::GetErrorCode);
    template<typename U>
    friend U operator| (Result<U>&& result, detail::ThrowOnError);
    template<typename U>
    friend U operator| (Result<U>&& result, detail::PanicOnError);

public:
    Result() { }

    Result(std::error_code ec) 
        : ec_(ec)
    { }

private:
    std::error_code ec_;
};

template<typename T>
inline T operator| (Result<T>&& result, detail::GetErrorCode gec) {
    gec.set_code(result.ec_);
    if constexpr (!std::is_void_v<T>) {
        return std::move(result.value_);
    }
}

template<typename T>
inline T operator| (Result<T>&& result, detail::ThrowOnError) {
    if (result.ec_) {
        throw std::system_error(result.ec_);
    }
    if constexpr (!std::is_void_v<T>) {
        return std::move(result.value_);
    }
}

template<typename T>
inline T operator| (Result<T>&& result, detail::PanicOnError) {
    if (result.ec_) {
        printf("panic: %s", result.ec_.message().c_str());
    }
    if constexpr (!std::is_void_v<T>) {
        return std::move(result.value_);
    }
}

inline detail::GetErrorCode get_err(std::error_code& ec) {
    return {ec};
}

inline detail::ThrowOnError throw_on_err;

inline detail::PanicOnError panic_on_err;

inline void try_rethrow(const std::exception_ptr& eptr) {
    if (eptr) {
        std::rethrow_exception(eptr);
    }
}

}

#endif