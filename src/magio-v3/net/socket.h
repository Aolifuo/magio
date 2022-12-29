#ifndef MAGIO_NET_SOCKET_H_
#define MAGIO_NET_SOCKET_H_

#include <cstring>

#include "magio-v3/core/noncopyable.h"
#include "magio-v3/net/address.h"

namespace magio {

template<typename>
class Coro;

namespace net {

class SocketOption {
public:
    static const int ReuseAddress;
    static const int ReceiveBufferSize;
    static const int SendBufferSize;
    static const int ReceiveTimeout;
    static const int SendTimeout;
};

class SmallBytes {
public:
    SmallBytes() = default;

    template<typename T>
    SmallBytes(const T& elem) {
        size_t len1 = sizeof elem;
        size_t len2 = sizeof buf_;
        size_ = len1 > len2 ? len2 : len1;
        std::memcpy(buf_, &elem, size_);
    }

    template<typename T>
    T& get() {
        return *reinterpret_cast<T*>(buf_);
    }

    size_t size() const {
        return size_;
    }

    const char* data() const {
        return buf_;
    }
private:
    char buf_[16];
    size_t size_ = 0;
};

class Socket: Noncopyable {
    friend class Acceptor;

public:
    enum Shutdown {
        Read = 0, Write, Both
    };

    using Handle =
#ifdef _WIN32
    size_t;
#elif defined(__linux__)
    int;
#endif

    Socket();

    ~Socket();

    Socket(Socket&& other) noexcept;

    Socket& operator=(Socket&& other) noexcept;

    void open(Ip ip, Transport tp, std::error_code& ec);

    void bind(const EndPoint& ep, std::error_code& ec);

    void set_option(int op, SmallBytes bytes, std::error_code& ec);

    SmallBytes get_option(int op, std::error_code& ec);

    Coro<void> connect(const EndPoint& ep, std::error_code& ec);

    Coro<size_t> write(const char* msg, size_t len, std::error_code& ec);

    Coro<size_t> read(char* buf, size_t len, std::error_code& ec);

    Coro<size_t> write_to(const char* msg, size_t len, const EndPoint& ep, std::error_code& ec);

    Coro<std::pair<size_t, EndPoint>> receive_from(char* msg, size_t len, std::error_code& ec);

    void cancel();

    void shutdown(Shutdown type);

    Handle handle() const {
        return handle_;
    }

    void close();

    Ip ip() const {
        return ip_;
    }

    Transport transport() const {
        return transport_;
    }

    operator bool() const {
        return handle_ != -1;
    }

private:
    Socket(Handle handle, Ip ip, Transport tp);

    void reset();

    void check_relation();

    bool is_related_ = false;
    Handle handle_ = -1;
    Ip ip_ = Ip::v4;
    Transport transport_ = Transport::Tcp;
};

namespace detail {

Socket::Handle open_socket(Ip ip, Transport tp, std::error_code& ec);

void close_socket(Socket::Handle handle);

}

}

}

#endif