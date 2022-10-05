#pragma once

#include <utility>

namespace magio {

template<typename T>
class MaybeUninit {
public:
    MaybeUninit(): flag(false) {}

    MaybeUninit(T elem) {
        flag = true;
        new (buf_) T{std::move(elem)};
    }

    MaybeUninit(const MaybeUninit& other): flag(other.flag) {
        if (flag) {
            new (buf_) T{other.get()};
        }
    }

    MaybeUninit(MaybeUninit&& other) noexcept: flag(other.flag) {
        if (flag) {
            new (buf_) T{std::move(other.get())};
        }
    }

    MaybeUninit& operator=(T elem) {
        if (flag) {
            get() = std::move(elem);
        } else {
            flag = true;
            new (buf_) T{std::move(elem)};
        }
        
        return *this;
    }

    MaybeUninit& operator=(const MaybeUninit& other) {
        if (!flag && !other.flag) {

        } else if (flag && other.flag) {
            get() = other.get();
        } else if (flag && !other.flag) {
            get().~T();
        } else if (!flag && other.flag) {
            new (buf_) T{other.get()};
        }

        flag = other.flag;
        return *this;
    }

    MaybeUninit& operator=(MaybeUninit&& other) {
        if (!flag && !other.flag) {

        } else if (flag && other.flag) {
            get() = std::move(other.get());
        } else if (flag && !other.flag) {
            get().~T();
        } else if (!flag && other.flag) {
            new (buf_) T{std::move(other.get())};
        }

        flag = other.flag;
        return *this;
    }

    ~MaybeUninit() noexcept {
        if (flag) {
            get().~T();
        }
    }

    T unwrap() {
        return std::move(get());
    }

    T unwrap_or_default() {
        if (flag) {
            return std::move(get());
        }
        return {};
    }

    void reset() {
        if (flag) {
            flag = false;
            get().~T();
        }
    }

    operator bool() {
        return flag;
    }
private:

    T& get() const {
        return *reinterpret_cast<T*>(buf_);
    }

    bool flag;
    alignas(T) mutable unsigned char buf_[sizeof(T)];
};

}