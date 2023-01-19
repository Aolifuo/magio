#ifndef MAGIO_NET_SOCKET_H_
#define MAGIO_NET_SOCKET_H_

#include <cstring>
#include <functional>
#include <system_error>

#include "magio-v3/core/common.h"
#include "magio-v3/core/noncopyable.h"
#include "magio-v3/net/protocal.h"

namespace magio {

template<typename>
class Coro;

namespace net {

class EndPoint;

class Socket: Noncopyable {
    friend class Acceptor;

public:
    enum Shutdown {
        Read = 0, Write, Both
    };

    using Handle = SocketHandle;

    Socket();

    ~Socket();

    Socket(Socket&& other) noexcept;

    Socket& operator=(Socket&& other) noexcept;

    [[nodiscard]]
    static Socket open(Ip ip, Transport tp, std::error_code& ec);

    void bind(const EndPoint& ep, std::error_code& ec);

#ifdef MAGIO_USE_CORO
    [[nodiscard]]
    Coro<void> connect(const EndPoint& ep, std::error_code& ec);

    [[nodiscard]]
    Coro<size_t> send(const char* msg, size_t len, std::error_code& ec);

    [[nodiscard]]
    Coro<size_t> receive(char* buf, size_t len, std::error_code& ec);

    [[nodiscard]]
    Coro<size_t> send_to(const char* msg, size_t len, const EndPoint& ep, std::error_code& ec);

    [[nodiscard]]
    Coro<std::pair<size_t, EndPoint>> receive_from(char* buf, size_t len, std::error_code& ec);
#endif

    void connect(const EndPoint& ep, std::function<void(std::error_code)>&& completion_cb);

    void send(const char* msg, size_t len, std::function<void(std::error_code, size_t)>&& completion_cb);

    void receive(char* buf, size_t len, std::function<void(std::error_code, size_t)>&& completion_cb);

    void send_to(const char* msg, size_t len, const EndPoint& ep, std::function<void(std::error_code, size_t)>&& completion_cb);

    void receive_from(char* buf, size_t len, std::function<void(std::error_code ec, size_t, EndPoint)>&& completion_cb);

    void cancel();

    void shutdown(Shutdown type);
    
    void close();

    void attach_context();

    Handle handle() const {
        return handle_;
    }

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

    Handle handle_ = -1;

    bool is_attached_ = false;
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