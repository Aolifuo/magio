#pragma once

#include <type_traits>

namespace magio {

namespace utils {

template<typename T, typename U, bool = false>
struct CompressedPair {
    CompressedPair(T val1, U val2)
        : val1_(std::move(val1))
        , val2_(std::move(val2))
    {}

    T& frist() {
        return val1_;
    }

    U& second() {
        return val2_;
    }

private:
    T val1_;
    U val2_;
};

template<typename T, typename U>
struct CompressedPair<T, U, true>: public T {
    CompressedPair(T val1, U val2)
        : T(std::move(val1))
        , val2_(std::move(val2))
    {}

    T& first() {
        return *static_cast<T*>(this);
    }

    U& second() {
        return val2_;
    }
private:
    U val2_;
};


}

}