#ifndef MAGIO_CORE_BUFFER_H_
#define MAGIO_CORE_BUFFER_H_

#include <string>

#include "fmt/core.h"

namespace magio {

constexpr size_t kFormatBufferSize = 500;
constexpr size_t kSmallBufferSize = 4000;
constexpr size_t kLargeBufferSize = 4000 * 1000;

template<size_t Size>
class StaticBuffer {
public:
    size_t append(std::string_view msg) {
        size_t cplen = std::min(rest(), msg.length());
        std::memcpy(buf_ + size_, msg.data(), cplen);
        size_ += cplen;
        return cplen;
    }

    size_t append_format(std::string_view fmt, fmt::format_args args) {
        auto sv = fmt::vformat_to_n(buf_ + size_, rest(), fmt, args);
        size_ += sv.size;
        return sv.size;
    }

    void clear() {
        size_ = 0;
    }
    
    size_t size() {
        return size_;
    }
    
    size_t rest() {
        return Size - size_;
    }

    std::string_view slice(size_t pos = 0) {
        return slice(pos, size());
    }

    std::string_view slice(size_t pos, size_t count) {
        if (pos >= size_) {
            return "";
        }
        count = std::min(count, size_ - pos);
        return {buf_ + pos, count};
    }

private:
    char buf_[Size];
    size_t size_ = 0;
};


template<typename Alloc = std::allocator<char>>
class Buffer {
    using AllocTraits = std::allocator_traits<Alloc>;

public:
    Buffer() { }

    Buffer(size_t init_cap, Alloc alloc = Alloc{})
        : data_({
            .alloc = std::move(alloc),
            .cap = init_cap
        })
    { 
        data_.buf = AllocTraits::allocate(data_.alloc, init_cap);
    }

    ~Buffer() {
        if (data_.buf) {
            reset();
        }
    }

    Buffer(const Buffer& other)
        : data_(other.data_)
    { 
        data_.buf = AllocTraits::allocate(data_.alloc, data_.cap);
        std::memcpy(data_.buf + data_.head, other.data_.buf + data_.head, size());
    }

    Buffer(Buffer&& other) noexcept 
        : data_(std::move(other.data_))
    {
        other.bzero();
    }

    Buffer& operator=(const Buffer& other) {
        if (this == &other) {
            return *this;
        }
        reset();
        data_ = other.data_;
        data_.buf = AllocTraits::allocate(data_.alloc, data_.cap);
        std::memcpy(data_.buf + data_.head, other.data_.buf + data_.head, size());
        return *this;
    }

    Buffer& operator=(Buffer&& other) noexcept {
        data_ = std::move(other.data_);
        other.bzero();
        return *this;
    };

    void append(std::string_view msg) {
        size_t diff = msg.length() - (data_.cap - data_.tail);
        if (diff > 0) {
            grow(diff);
        }
        std::memcpy(data_.buf + data_.tail, msg.data(), msg.length());
        data_.tail += msg.length();
    }

    void consume(size_t len) {
        size_t real = std::min(len, size());
        data_.head += real;
        if (data_.head == data_.tail) {
            clear();
        }
    }

    std::string_view slice(size_t pos = 0) {
        // pos -> buf + head + pos
        return slice(pos, size());
    }

    std::string_view slice(size_t pos, size_t count) {
        if (data_.head + pos >= data_.tail) {
            return "";
        }
        count = std::min(count, data_.tail - (data_.head + pos));
        return {data_.buf + data_.head + pos, count};
    }

    void clear() {
        data_.head_ = data_.tail_ = 0;
    }

    void reserve(size_t cap) {
        if (cap <= data_.cap) {
            return;
        }

        char* new_buf = AllocTraits::allocate(data_.alloc, cap);
        std::memcpy(new_buf + data_.head, data_.buf + data_.head, size());
        AllocTraits::deallocate(data_.alloc, data_.buf, data_.cap);
        data_.buf = new_buf;
        data_.cap = cap;
    }
    
    void reset() {
        AllocTraits::deallocate(data_.alloc, data_.buf, data_.cap);
        bzero();
    }

    void swap(const Buffer& other) noexcept {
        std::swap(data_, other.data_);
    }
    
    size_t size() {
        return data_.tail - data_.head;
    }

    size_t capacity() {
        return data_.cap;
    }

private:
    struct Data {
        Alloc alloc = Alloc{};
        size_t head = 0;
        size_t tail = 0;
        size_t cap = 0;
        char* buf = nullptr;
    };

    void grow(size_t least) {
       size_t new_cap = (data_.cap + least) * 1.5;
       reserve(new_cap);
    }

    void bzero() {
        data_.head = data_.tail = data_.cap = 0;
        data_.buf = nullptr;
    }

    Data data_;
};

namespace detail {

using FormatBuffer = StaticBuffer<kFormatBufferSize>;
using SmallBuffer = StaticBuffer<kSmallBufferSize>;
using LargeBuffer = StaticBuffer<kLargeBufferSize>;

}

}

#endif