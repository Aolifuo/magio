#pragma once

#include <stdint.h>
#include <string>
#include "magio/Configs.h"

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


}