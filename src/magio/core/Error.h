#pragma once

#include <stdexcept>
#include <system_error>

namespace magio {

template<typename Ok, typename Err>
class Expected;

namespace detail {

template<typename>
struct IsExpected: std::false_type {};

template<typename Ok, typename Err>
struct IsExpected<Expected<Ok, Err>>: std::true_type { };

template<typename>
struct ExpectedTraits;

template<typename Ok, typename Err>
struct ExpectedTraits<Expected<Ok, Err>> {
    using OkType = Ok;
    using ErrType = Err;
};

}

struct Unit {};

template<typename Ok = Unit, typename Err = std::error_code>
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
            new (buf_) Ok(other.template get<Ok>());
        } else {
            new (buf_) Err(other.template get<Err>());
        }
    }

    Expected(Expected&& other) noexcept: flag_(other.flag_) {
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

    Expected& operator=(const Expected& other) noexcept {
        if (!flag_ && !other.flag_) {
            get<Err>() = other.template get<Err>();
        } else if (flag_ && other.flag_) {
            get<Ok>() = other.template get<Ok>();
        } else if (flag_ && !other.flag_) {
            get<Ok>().~Ok();
            new (buf_) Err(other.template get<Err>());
        } else if (!flag_ && other.flag_) {
            get<Err>().~Err();
            new (buf_) Ok(other.template get<Ok>());
        }

        flag_ = other.flag_;
        return *this;
    }

    Expected& operator=(Expected&& other) noexcept {
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

    Ok unwrap_or(Ok ok) {
        if (flag_) {
            return std::move(get<Ok>());
        }
        return ok;
    }

    Ok except() {
        if (flag_) {
            return std::move(get<Ok>());
        }
        throw std::runtime_error(std::move(get<Err>().msg));
    }

    // Error type must be the same
    template<typename Fn>
    requires detail::IsExpected<std::invoke_result_t<Fn, Ok>>::value
        &&   std::is_invocable_v<Fn, Ok>
        &&   std::is_same_v<Err, typename detail::ExpectedTraits<std::invoke_result_t<Fn, Ok>>::ErrType>
    std::invoke_result_t<Fn, Ok> and_then(Fn fn) {
        if (!flag_) {
            return std::move(get<Err>());
        }
        
        return fn(std::move(get<Ok>()));
    }

    // Ok type must be the same
    template<typename Fn>
    requires detail::IsExpected<std::invoke_result_t<Fn, Err>>::value
        &&   std::is_invocable_v<Fn, Err>
        &&   std::is_same_v<Ok, typename detail::ExpectedTraits<std::invoke_result_t<Fn, Err>>::OkType>
    std::invoke_result_t<Fn, Err> or_else(Fn fn) {
        if (flag_) {
            return std::move(get<Ok>());
        }

        return fn(std::move(get<Err>()));
    }

    template<typename Fn>
    requires std::is_invocable_v<Fn, Ok>
    Expected<std::invoke_result_t<Fn, Ok>, Err> map(Fn fn) {
        if (!flag_) {
            return std::move(get<Err>());
        } 

        return fn(std::move(get<Ok>()));
    }

    operator bool() {
        return flag_;
    }
private:

    template<typename T>
    T& get() const {
        return *reinterpret_cast<T*>(buf_);
    }

    using LargerSize = std::conditional_t<(sizeof(Ok) >= sizeof(Err)), Ok, Err>;
    using LargerAlign = std::conditional_t<(alignof(Ok) >= alignof(Err)), Ok, Err>;

    bool flag_;
    alignas(LargerAlign) mutable unsigned char buf_[sizeof(LargerSize)];
};

}

