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

class InetAddress {
    friend class Socket;
    friend class Acceptor;
    friend class magio::IoService;

public:
    InetAddress();

    [[nodiscard]]
    static Result<InetAddress> from(std::string_view ip, PortType port);

    std::string ip() const;

    PortType port() const;

    bool is_ipv4() const;

    bool is_ipv6() const;

private:
    static InetAddress from(sockaddr*);

    int sockaddr_len() const;

    char buf_[32];
};

}

}

#endif