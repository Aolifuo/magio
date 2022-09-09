#include <unordered_map>
#include "winerror.h"

#define SYSTEM_ERROR_MAP() \
    XX(ERROR_INVALID_HANDLE, "The handle is invalid") \
    XX(WAIT_TIMEOUT, "The wait operation timed out") \
    XX(ERROR_NETNAME_DELETED, "The specified network name is no longer available") \
    XX(ERROR_CONNECTION_ABORTED, "The network connection was aborted by the local system") \
    XX(ERROR_OPERATION_ABORTED, "The I/O operation has been aborted because of either a thread exit or an application request") \
    XX(ERROR_ABANDONED_WAIT_0, "Iocp service is interrupted") \

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