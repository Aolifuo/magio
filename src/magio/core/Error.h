#pragma once

#include <format>

namespace magio {

// 24 bytes
struct Error {
    // 0 => no error
    int code = 0;
    std::string msg;

    Error(const Error&) = default;
    Error(Error&&) = default;
    Error& operator=(const Error&) = default;
    Error& operator=(Error&&) = default;

    Error() {}
    explicit Error(int code): code(code) {}
    explicit Error(std::string_view str): code(1), msg(str) {}
    explicit Error(int code, std::string_view str): code(code), msg(str) {}

    // true => has error
    operator bool() {
        return code;
    }
};

struct Unit {};

template<typename Ok = Unit, typename Err = Error>
class Expected {
public:

    Expected(Ok elem): flag_(true) {
        new (buf_) Ok(std::move(elem));
    }

    Expected(Err elem): flag_(false) {
        new (buf_) Err(std::move(elem));
    }

    Expected(const Expected& other): flag_(other.flag_) {
        if (flag_) {
            new (buf_) Ok(const_cast<Expected&>(other).template get<Ok>());
        } else {
            new (buf_) Err(const_cast<Expected&>(other).template get<Err>());
        }
    }

    Expected(Expected&& other): flag_(other.flag_) {
        if (flag_) {
            new (buf_) Ok(std::move(other.template get<Ok>()));
        } else {
            new (buf_) Err(std::move(other.template get<Err>()));
        }
    }

    Expected& operator=(Ok elem) {
        if (flag_) {
            get<Ok>() = std::move(elem);
        } else {
            get<Err>().~Err();
            new (buf_) Ok(std::move(elem));
        }

        flag_ = true;
        return *this;
    }

    Expected& operator=(Err elem) {
        if (flag_) {
            get<Ok>().~Ok();
            new (buf_) Err(std::move(elem));
        } else {
            get<Err>() = std::move(elem);
        }

        flag_ = false;
        return *this;
    }

    Expected& operator=(const Expected& other) {
        if (!flag_ && !other.flag_) {
            get<Err>() = const_cast<Expected&>(other).template get<Err>();
        } else if (flag_ && other.flag_) {
            get<Ok>() = const_cast<Expected&>(other).template get<Ok>();
        } else if (flag_ && !other.flag_) {
            get<Ok>().~Ok();
            new (buf_) Err(const_cast<Expected&>(other).template get<Err>());
        } else if (!flag_ && other.flag_) {
            get<Err>().~Err();
            new (buf_) Ok(const_cast<Expected&>(other).template get<Ok>());
        }

        flag_ = other.flag_;
        return *this;
    }

    Expected& operator=(Expected&& other) {
        if (!flag_ && !other.flag_) {
            get<Err>() = std::move(other.template get<Err>());
        } else if (flag_ && other.flag_) {
            get<Ok>() = std::move(other.template get<Ok>());
        } else if (flag_ && !other.flag_) {
            get<Ok>().~Ok();
            new (buf_) Err(std::move(other.template get<Err>()));
        } else if (!flag_ && other.flag_) {
            get<Err>().~Err();
            new (buf_) Ok(std::move(other.template get<Ok>()));
        }

        flag_ = other.flag_;
        return *this;
    }

    ~Expected() {
        if (flag_) {
            get<Ok>().~Ok();
        } else {
            get<Err>().~Err();
        }
    }

    
    Ok unwrap() {
        if (!flag_) {
            std::terminate();
        }
        return std::move(get<Ok>());
    }

    Err unwrap_err() {
        if (flag_) {
            std::terminate();
        }
        return std::move(get<Err>());
    }

    Ok except() {
        if (flag_) {
            return std::move(get<Ok>());
        }
        throw std::runtime_error(std::move(get<Err>().msg));
    }

    operator bool() {
        return flag_;
    }
private:

    template<typename T>
    T& get() {
        return *reinterpret_cast<T*>(buf_);
    }

    using LargerSize = std::conditional_t<(sizeof(Ok) >= sizeof(Err)), Ok, Err>;
    using LargerAlign = std::conditional_t<(alignof(Ok) >= alignof(Err)), Ok, Err>;

    bool flag_;
    alignas(LargerAlign) unsigned char buf_[sizeof(LargerSize)];
};

}

template<typename CharT>
struct std::formatter<magio::Error, CharT>: std::formatter<int, CharT> {
    template<typename FormatContext>
    auto format(const magio::Error& err, FormatContext& fc) {
        return std::format_to(fc.out(), "code:{}. {}", err.code, err.msg);
    }
};
