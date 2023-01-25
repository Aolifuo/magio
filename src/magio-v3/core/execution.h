#ifndef MAGIO_CORE_EXECUTION_H_
#define MAGIO_CORE_EXECUTION_H_

#include "magio-v3/utils/functor.h"

namespace magio {

using Task = Functor<void()>;

class Executor {
public:
    virtual ~Executor() = default;

    virtual void execute(Task&& task) = 0;
};

}

#endif