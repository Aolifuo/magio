#ifndef MAGIO_CORE_CURRENT_THREAD_H_
#define MAGIO_CORE_CURRENT_THREAD_H_

#include <thread>
#include <sstream>

namespace magio {

class CurrentThread {
public:
    static unsigned get_id() {
        static thread_local unsigned id = [] {
            std::stringstream ss;
            unsigned id;
            ss << std::this_thread::get_id();
            ss >> id;
            return id;
        }();
        return id;
    }

private:
};

}

#endif