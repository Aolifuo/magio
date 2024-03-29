#ifndef MAGIO_NET_SOCKET_H_
#define MAGIO_NET_SOCKET_H_

#include <cstring>

#include "magio-v3/utils/functor.h"
#include "magio-v3/utils/noncopyable.h"
#include "magio-v3/core/error.h"
#include "magio-v3/core/common.h"
#include "magio-v3/net/protocal.h"

namespace magio {

class CoroContext;

template<typename>
class Coro;

namespace net {

class InetAddress;

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
    static Result<Socket> open(Ip ip, Transport tp);

    [[nodiscard]]
    Result<> bind(const InetAddress& address);

#ifdef MAGIO_USE_CORO
    [[nodiscard]]
    Coro<Result<>> connect(const InetAddress& address);

    [[nodiscard]]
    Coro<Result<size_t>> send(const char* msg, size_t len);

    [[nodiscard]]
    Coro<Result<size_t>> receive(char* buf, size_t len);

    [[nodiscard]]
    Coro<Result<size_t>> send_to(const char* msg, size_t len, const InetAddress& address);

    [[nodiscard]]
    Coro<Result<std::pair<size_t, InetAddress>>> receive_from(char* buf, size_t len);
#endif

    void connect(const InetAddress& address, Functor<void(std::error_code)>&& completion_cb);

    void send(const char* msg, size_t len, Functor<void(std::error_code, size_t)>&& completion_cb);

    void receive(char* buf, size_t len, Functor<void(std::error_code, size_t)>&& completion_cb);

    void send_to(const char* msg, size_t len, const InetAddress& address, Functor<void(std::error_code, size_t)>&& completion_cb);

    void receive_from(char* buf, size_t len, Functor<void(std::error_code ec, size_t, InetAddress)>&& completion_cb);

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
        return handle_ != kInvalidHandle;
    }

private:
    Socket(Handle handle, Ip ip, Transport tp);

    void reset();

    Handle handle_ = kInvalidHandle;

    CoroContext* attached_;
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