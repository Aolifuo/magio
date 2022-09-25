#pragma once

#include <stdint.h>
#include <string>

namespace magio {

namespace plat {

enum class TransportProtocol { TCP, UDP };

enum class IOOperation {
    Noop,
    Accept,
    Receive,
    Send,
    Connect,
};

using socket_type = uint64_t;

struct IOContext;

struct Socket;

class IOContextHelper;

class SocketHelper;

}

struct Address {
    unsigned host;
    std::string ip;

    std::string to_string() {
        return ip + ":" + std::to_string(host);
    }
};

}