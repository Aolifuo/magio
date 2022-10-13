#pragma once

#ifdef _WIN32
#include <WinSock2.h>
#include "magio/core/Error.h"
#include "magio/plat/errors.h"

namespace magio {

namespace plat {

class IOCompletionPort{
public:
    IOCompletionPort() = default;

    IOCompletionPort(const IOCompletionPort&) = delete;

    std::error_code open() {
        if (handle_) {
            return {};
        }

        HANDLE handle = ::CreateIoCompletionPort(
            INVALID_HANDLE_VALUE, 
            NULL, 
            0, 
            0);
        
        if (!handle) {
            return MAGIO_SYSTEM_ERROR;
        }

        handle_ = handle;
        return {};
    }

    void close() {
        if (handle_) {
            ::CloseHandle(handle_);
        }
    }

    std::error_code relate(SOCKET sock) {
        HANDLE handle = ::CreateIoCompletionPort(
            (HANDLE)sock,
            handle_, 
            NULL,
            0);

        if (!handle) {
            return MAGIO_SYSTEM_ERROR;
        }

        return {};
    }

    std::error_code wait(size_t ms, DWORD* pbytes, void** piodata) {
        void* key;
        bool status = ::GetQueuedCompletionStatus(
            handle_,
            pbytes, 
            (PULONG_PTR)&key, 
            (LPOVERLAPPED*)piodata, 
            (ULONG)ms);
            
        if (!status) {
            if (::GetLastError() != WAIT_TIMEOUT) {
                return MAGIO_SYSTEM_ERROR;
            }
        }

        return {};
    }
private:
    HANDLE handle_ = nullptr;
};

}

}

#endif