#ifndef MAGIO_CORE_UNIT_H_
#define MAGIO_CORE_UNIT_H_

#include <type_traits>

namespace magio {

struct Unit { };

template<typename T>
using VoidToUnit = std::conditional_t<std::is_void_v<T>, Unit, T>;

}

#endif