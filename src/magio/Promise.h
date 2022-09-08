#pragma once

#include <memory>
#include "magio/core/Execution.h"
#include "magio/core/Queue.h"

namespace magio {

class Promise;

using PromisePtr = std::shared_ptr<Promise>;

class Promise: public std::enable_shared_from_this<Promise> {
public:
    Promise(AnyExecutor executor);

    PromisePtr then() {

    }
private:
    void resolve() {

    }

    void reject() {
        
    }

    AnyExecutor executor_;
    
};


}