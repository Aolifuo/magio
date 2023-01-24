#ifndef MAGIO_NET_ADDRESS_H_
#define MAGIO_NET_ADDRESS_H_

#include <string>

#include "magio-v3/core/error.h"
#include "magio-v3/net/protocal.h"

struct sockaddr;

namespace magio {

class IoService;

namespace net {

using PortType = uint_least16_t;

class IpAddress {
    friend class EndPoint;
    friend class Socket;
    friend class Acceptor;
    friend class magio::IoService;
    
    friend Result<IpAddress> make_address(std::string_view str);
    friend IpAddress _make_address(sockaddr* paddr);

public:
    IpAddress() = default;

    std::string to_string() const {
        return ip_;
    }

    Ip ip() const {
        return level_;
    }
    
    bool is_v4() const {
        return level_ == Ip::v4;
    }

    bool is_v6() const {
        return level_ == Ip::v6;
    }

private:
    int addr_len() const;

    char addr_in_[32];
    std::string ip_;
    Ip level_;
};

// v4 v6
[[nodiscard]]
Result<IpAddress> make_address(std::string_view str);

IpAddress _make_address(sockaddr* paddr);

class EndPoint {
public:
    EndPoint() = default;

    EndPoint(IpAddress addr, PortType port);

    const IpAddress& address() const {
        return address_;
    }

    PortType port() const {
        return port_;
    }

private:
    IpAddress address_;
    PortType port_;
};

}

}

#endif