#pragma once

#include "magio/net/SocketAddress.h"
#include "magio/plat/io_service.h"
#include "magio/execution/Execution.h"

namespace magio {

using plat::Protocol;

class Socket {
    friend class TcpServer;
    friend class UdpSocket;

    Socket(AnyExecutor exe, plat::socket_type fd)
        : exe_(exe), fd_(fd)
    { 
        exe_.get_service().count->fetch_add(1, std::memory_order_acquire);
    }

public:
    Socket(const Socket&) = delete;

    Socket(Socket&& other) noexcept 
        : exe_(other.exe_), fd_(other.fd_) 
    {
        other.fd_ = plat::MAGIO_INVALID_SOCKET;
    } 

    Socket& operator=(Socket&& other) noexcept {
        exe_ = other.exe_;
        fd_ = other.fd_;
        other.fd_ = plat::MAGIO_INVALID_SOCKET;
        return *this;
    }

    ~Socket() {
        close();
    }

    static Expected<Socket> create(AnyExecutor exe, Protocol protocol) {
        auto fd = plat::make_socket(protocol);
        if (plat::MAGIO_INVALID_SOCKET == fd) {
            return MAGIO_SYSTEM_ERROR;
        }
        return Socket(exe, fd);
    }

    void close() {
        if (plat::MAGIO_INVALID_SOCKET != fd_) {
            plat::close_socket(fd_);
            fd_ = plat::MAGIO_INVALID_SOCKET;
            exe_.get_service().count->fetch_sub(1, std::memory_order_release);
        }
    }

    Expected<> bind(const SocketAddress& address) {
        if (-1 == ::bind(fd_, (sockaddr*)&address._sockaddr, sizeof(address._sockaddr))) {
            return MAGIO_SYSTEM_ERROR;
        }
        return {Unit()};
    }

    Expected<> listen() {
        if (-1 == ::listen(fd_, SOMAXCONN)) {
            return MAGIO_SYSTEM_ERROR;
        }
        return {Unit()};
    }

    plat::socket_type handle() {
        return fd_;
    }

    AnyExecutor get_executor() {
        return exe_;
    }
protected:
    AnyExecutor         exe_;
    plat::socket_type   fd_ = plat::MAGIO_INVALID_SOCKET;
};

}