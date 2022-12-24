#ifndef MAGIO_CORE_EXECUTION_H_
#define MAGIO_CORE_EXECUTION_H_

#include <functional>

namespace magio {

using Task = std::function<void()>;

class Executor {
public:
    virtual ~Executor() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void execute(Task&& task) = 0;
};

}

#endif