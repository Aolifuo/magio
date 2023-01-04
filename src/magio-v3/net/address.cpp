#include "magio-v3/net/address.h"

#include "magio-v3/core/error.h"

#ifdef _WIN32
#include <Ws2tcpip.h>
#elif defined(__linux__)
#include <arpa/inet.h>
#endif

namespace magio {

namespace net {

IpAddress make_address(std::string_view str, std::error_code& ec) {
    // judge v4 v6
    IpAddress address;
    if (str.find('.') != std::string_view::npos) {
        //v4
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        if (-1 == ::inet_pton(AF_INET, str.data(), &addr.sin_addr)) {
            ec = SYSTEM_ERROR_CODE;
            return {};
        }
        std::memcpy(address.addr_in_, &addr, sizeof(addr));
        address.ip_ = str;
        address.level_ = Ip::v4;
        return address;
    } else {
        // v6
        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_scope_id = 0;
        if (-1 == ::inet_pton(AF_INET6, str.data(), &addr.sin6_addr)) {
            ec = SYSTEM_ERROR_CODE;
            return {};
        }
        std::memcpy(address.addr_in_, &addr, sizeof(addr));
        address.ip_ = str;
        address.level_ = Ip::v6;
        return address;
    }
}

EndPoint::EndPoint(IpAddress address, PortType port) {
    port_ = port;
    if (address.is_v4()) {
        sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(address.addr_in_);
        addr->sin_port = ::htons(port);
    } else {
        sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(address.addr_in_);
        addr->sin6_port = ::htons(port);
    }
    address_ = address;
}

}

}