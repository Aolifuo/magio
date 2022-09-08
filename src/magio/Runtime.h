#pragma once

#include "magio/plat/socket.h"

namespace magio {

class Runtime {
public:
    static Excepted<> run() {
        return plat::SocketServer::instance().initialize();
    }

private:
};

}