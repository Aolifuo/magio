#include "magio-v3/net/address.h"

#include <cstring>

#ifdef _WIN32
#include <Ws2tcpip.h>
#elif defined(__linux__)
#include <arpa/inet.h>
#endif

namespace magio {

namespace net {

InetAddress::InetAddress() { }

Result<InetAddress> InetAddress::from(std::string_view ip, PortType port) {
    std::error_code ec;
    InetAddress address;
    if (ip.find('.') != std::string_view::npos) {
        //v4
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = ::htons(port);
        if (-1 == ::inet_pton(AF_INET, ip.data(), &addr.sin_addr)) {
            return {SYSTEM_ERROR_CODE};
        }
        std::memcpy(address.buf_, &addr, sizeof(addr));
        return address;
    } else {
        // v6
        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = ::htons(port);
        addr.sin6_scope_id = 0;
        if (-1 == ::inet_pton(AF_INET6, ip.data(), &addr.sin6_addr)) {
            return {SYSTEM_ERROR_CODE};
        }
        std::memcpy(address.buf_, &addr, sizeof(addr));
        return address;
    }
}

InetAddress InetAddress::from(sockaddr* paddr) {
    InetAddress address;
    socklen_t addr_len = paddr->sa_family == AF_INET
        ? sizeof(sockaddr_in)
        : sizeof(sockaddr_in6);
    std::memcpy(address.buf_, paddr, addr_len);
    return address;
}

std::string InetAddress::ip() const {
    char result[32]{};
    sockaddr* paddr = (sockaddr*)buf_;
    ::inet_ntop(
        paddr->sa_family, paddr, 
        result, sizeof(result)
    );
    return result;
}

PortType InetAddress::port() const {
    sockaddr_in* paddr = (sockaddr_in*)buf_;
    return ::ntohs(paddr->sin_port);
}

bool InetAddress::is_ipv4() const {
    sockaddr* paddr = (sockaddr*)buf_;
    return paddr->sa_family == AF_INET;
}

bool InetAddress::is_ipv6() const {
    sockaddr* paddr = (sockaddr*)buf_;
    return paddr->sa_family == AF_INET6;
}

int InetAddress::sockaddr_len() const {
    sockaddr* paddr = (sockaddr*)buf_;
    return paddr->sa_family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
}

}

}