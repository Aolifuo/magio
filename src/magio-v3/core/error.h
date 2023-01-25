#ifndef MAGIO_CORE_ERROR_H_
#define MAGIO_CORE_ERROR_H_

#include <system_error>

#include "magio-v3/core/unit.h"

namespace magio {

namespace detail {

class RedirectError {
public:
    RedirectError(std::error_code& ec)
        : ec_(ec) { }
    
    void set_code(std::error_code& ec) {
        ec_ = ec;
    }

private:
    std::error_code& ec_;
};

struct AsTuple { };

struct ThrowOnError { };

struct PanicOnError { 
    const char* prefix = nullptr;
};

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
    friend U operator| (Result<U>&& result, detail::RedirectError);
    template<typename U>
    friend U operator| (Result<U>&& result, detail::ThrowOnError);
    template<typename U>
    friend U operator| (Result<U>&& result, detail::PanicOnError);
    template<typename U>
    friend std::tuple<U, std::error_code> operator| (Result<U>&& result, detail::AsTuple);

    static_assert(std::is_default_constructible_v<T>, "T requires default constructor");

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
    friend U operator| (Result<U>&& result, detail::RedirectError);
    template<typename U>
    friend U operator| (Result<U>&& result, detail::ThrowOnError);
    template<typename U>
    friend U operator| (Result<U>&& result, detail::PanicOnError);
    friend std::tuple<std::error_code> operator| (Result<>&& result, detail::AsTuple);

public:
    Result() { }

    Result(std::error_code ec) 
        : ec_(ec)
    { }

private:
    std::error_code ec_;
};

template<typename T>
[[nodiscard]]
inline T operator| (Result<T>&& result, detail::RedirectError re) {
    re.set_code(result.ec_);
    if constexpr (!std::is_void_v<T>) {
        return std::move(result.value_);
    }
}

template<typename T>
[[nodiscard]]
inline std::tuple<T, std::error_code> operator| (Result<T>&& result, detail::AsTuple) {
    return {std::move(result.value_), result.ec_};
}

[[nodiscard]]
inline std::tuple<std::error_code> operator| (Result<>&& result, detail::AsTuple) {
    return {result.ec_};
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
inline T operator| (Result<T>&& result, detail::PanicOnError poe) {
    if (result.ec_) {
        if (poe.prefix) {
            printf("%s\npanic: %s\n", poe.prefix, result.ec_.message().c_str());
        } else {
            printf("panic: %s\n", result.ec_.message().c_str());
        }
        std::terminate();
    }
    if constexpr (!std::is_void_v<T>) {
        return std::move(result.value_);
    }
}

inline detail::RedirectError redirect_err(std::error_code& ec) {
    return {ec};
}

inline detail::AsTuple as_tuple;

inline detail::ThrowOnError throw_on_err;

inline detail::PanicOnError panic_on_err;

inline detail::PanicOnError expect(const char* msg) {
    return {msg};
}

inline void try_rethrow(const std::exception_ptr& eptr) {
    if (eptr) {
        std::rethrow_exception(eptr);
    }
}

}

#endif