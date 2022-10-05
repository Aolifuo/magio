#pragma once

#define CLASS_PIMPL_DECLARE(CLASS) \
public: \
    CLASS(CLASS &&) noexcept; \
    CLASS& operator=(CLASS &&) noexcept; \
    ~CLASS(); \
private: \
    struct Impl; \
    CLASS(Impl*); \
    Impl* impl; \

#define CLASS_PIMPL_IMPLEMENT(CLASS) \
CLASS::CLASS(Impl* d) { \
    impl = d; \
} \
CLASS::CLASS(CLASS&& other) noexcept { \
    impl = other.impl; \
    other.impl = nullptr; \
} \
CLASS& CLASS::operator=(CLASS&& other) noexcept { \
    impl = other.impl; \
    other.impl = nullptr; \
    return *this; \
} \
CLASS::~CLASS() { \
    if (impl) { \
        delete impl; \
    } \
}
