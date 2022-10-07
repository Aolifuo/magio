#pragma once

#ifdef __linux__

#include <system_error>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<fcntl.h>
#include "magio/net/Address.h"

namespace magio {

namespace plat {

Address local_address(int fd) {
    ::sockaddr_in addr{};
    ::socklen_t len = sizeof(sockaddr_in);
    ::getsockname(fd, (::sockaddr*)&addr, &len);
    return {::inet_ntoa(addr.sin_addr), ntohs(addr.sin_port)};
}

Address remote_address(int fd) {
    ::sockaddr_in addr{};
    ::socklen_t len = sizeof(sockaddr_in);
    ::getpeername(fd, (::sockaddr*)&addr, &len);
    return {::inet_ntoa(addr.sin_addr), ntohs(addr.sin_port)};
}

std::error_code set_nonblock(int fd) {
    int oldflag = ::fcntl(fd, F_GETFL, 0);
    int newflag = oldflag | O_NONBLOCK;
    if (-1 == ::fcntl(fd, F_SETFL, newflag)) {
        return std::make_error_code((std::errc)errno);
    }
    return {};
}

}

}

#endif