#ifndef MAGIO_CORE_DETAIL_COMPLETION_CALLBACK_H_
#define MAGIO_CORE_DETAIL_COMPLETION_CALLBACK_H_

#include <system_error>
#include "magio-v3/core/coroutine.h"

namespace magio::detail {

struct ResumeHandle {
    std::error_code ec;
    std::coroutine_handle<> handle;
};

inline void completion_callback(std::error_code ec, void* ptr) {
    auto* h = static_cast<ResumeHandle*>(ptr);
    h->ec = ec;
    // this_context::wake_in_context(h->handle);
    h->handle.resume();
}

}

#endif