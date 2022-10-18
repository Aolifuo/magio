#pragma once

#include <system_error>
#include "magio/execution/Execution.h"
#include "magio/core/Pimpl.h"

namespace magio {

class Magico {

    CLASS_PIMPL_DECLARE(Magico)
    
public:
    Magico(size_t worker_threads = 1);

    void run();
    void stop();

    AnyExecutor get_executor() const;
private:
};

}