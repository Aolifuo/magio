#pragma once

#include <unordered_map>
#include <system_error>
#ifdef _WIN32
#include "winerror.h"

#define SYSTEM_ERROR_MAP() \
    XX(ERROR_INVALID_HANDLE, "The handle is invalid") \
    XX(ERROR_DUP_NAME, "You were not connected because a duplicate name exists on the network. If joining a domain, go to System in Control Panel to change the computer name and try again. If joining a workgroup, choose another workgroup name.") \
    XX(ERROR_NETNAME_DELETED, "The specified network name is no longer available") \
    XX(WAIT_TIMEOUT, "The wait operation timed out") \
    XX(ERROR_ABANDONED_WAIT_0, "Iocp service is interrupted") \
    XX(ERROR_OPERATION_ABORTED, "The I/O operation has been aborted because of either a thread exit or an application request") \
    XX(ERROR_CONNECTION_REFUSED, "The remote computer refused the network connection") \
    XX(ERROR_CONNECTION_ABORTED, "The network connection was aborted by the local system") \
    XX(WSAENOTSOCK, "An operation was attempted on something that is not a socket.")
#endif

namespace magio {

#ifdef _WIN32

inline const char* system_error_str(int code) {
#define XX(CODE, STRING) {CODE, STRING},
    static std::unordered_map<int, const char*> system_error_map
    {
        SYSTEM_ERROR_MAP()
    };
#undef XX
    if (auto it = system_error_map.find(code); it != system_error_map.end()) {
        return it->second;
    }
    std::printf("system_error %d\n", code);
    return "Unknown error";
}

#endif

}

