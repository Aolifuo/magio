#ifndef MAGIO_CORE_CURRENT_THREAD_H_
#define MAGIO_CORE_CURRENT_THREAD_H_

#include <thread>
#include <sstream>

namespace magio {

class CurrentThread {
public:
    static size_t get_id() {
        static thread_local size_t id = [] {
            std::stringstream ss;
            size_t id;
            std::string str;
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