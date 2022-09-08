#pragma once

#include <stdint.h>

namespace magio {

namespace plat {

enum class TransportProtocol { TCP, UDP };

enum class IOOperation {
    Noop,
    Accept,
    Receive,
    Send,
};

using socket_type = uint64_t;

struct IOContext;

struct Socket;

class IOContextHelper;

class SocketHelper;


}


}