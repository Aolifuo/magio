#ifndef MAGIO_UTILS_NONCOPYABLE_H_
#define MAGIO_UTILS_NONCOPYABLE_H_

namespace magio {

class Noncopyable {
public:
    Noncopyable() = default;
    ~Noncopyable() = default;

    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator=(const Noncopyable&) = delete;
};

}

#endif