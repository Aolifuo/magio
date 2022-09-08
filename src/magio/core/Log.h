#pragma once

#include <iostream>
#include <format>

namespace magio {

#define _DEBUG_

#ifdef _DEBUG_
    #define DEBUG(LITERAL, ...) \
        do { \
            std::cout << std::format(LITERAL, __VA_ARGS__); \
        } while(0)
#else 
    #define DEBUG(...)
#endif

#define LOG(LITERAL, ...) \
    do { \
        std::cout << std::format(LITERAL, __VA_ARGS__); \
    } while(0)

}