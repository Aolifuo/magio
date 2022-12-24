#ifndef MAGIO_NET_ADDRESS_H_
#define MAGIO_NET_ADDRESS_H_

#include <string>
#include <system_error>

#include "magio-v3/net/protocal.h"

namespace magio {

namespace net {

using PortType = uint_least16_t;

class IpAddress {
    friend class Socket;
    friend class EndPoint;
    friend class Acceptor;
    friend IpAddress make_address(std::string_view str, std::error_code& ec);

    template<typename Addr>
    IpAddress(Addr addr, std::string_view str, Ip level)
        : ip_(str), level_(level) 
    { 
        std::memcpy(addr_in_, &addr, sizeof(addr));
    }

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
    char addr_in_[32];
    std::string ip_;
    Ip level_;
};

// v4 v6
IpAddress make_address(std::string_view str, std::error_code& ec);

class EndPoint {
public:
    EndPoint() = default;

    EndPoint(IpAddress addr, PortType port);

    IpAddress address() const {
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