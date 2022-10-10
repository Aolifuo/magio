#include "magio/net/IOService.h"

#include "magio/plat/runtime.h"
#include "magio/plat/loop.h"

namespace magio {

// open only once
std::error_code IOService::open() {
    std::error_code ec;
    std::call_once(*once_f_, [&] {
        ec = loop_->open();
        if (ec) {
            return;
        }

        for (size_t i = 0; i < runtime_->workers(); ++i) {
            runtime_->post([loop = loop_] {
                for (; ;) {
                    if (auto ec = loop->poll2(plat::MAGIO_INFINITE)) {
                        DEBUG_LOG("loop error: ", ec.message());
                        return;
                    }
                }
            });
        }
    });

    return {};
}


}