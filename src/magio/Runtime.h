#pragma once

#include "magio/plat/socket.h"

namespace magio {

class Runtime {
public:
    static Expected<> run() {
        return plat::SocketServer::instance().initialize();
    }

private:
};

}