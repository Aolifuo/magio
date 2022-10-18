#pragma once

#ifdef _WIN32
#include "magio/core/Expected.h"
#include "magio/plat/errors.h"
#include <WinSock2.h>

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

    std::error_code wait(size_t ms, DWORD* pbytes, void** piodata, void** key) {
        bool status = ::GetQueuedCompletionStatus(
            handle_,
            pbytes, 
            (PULONG_PTR)key, 
            (LPOVERLAPPED*)piodata, 
            (ULONG)ms);
            
        if (!status) {
            if (::GetLastError() != WAIT_TIMEOUT) {
                return MAGIO_SYSTEM_ERROR;
            }
        }

        return {};
    }

    void notify_one() {
        void* key = nullptr;
        ::PostQueuedCompletionStatus(
            handle_, 
            ULONG_MAX,
            (ULONG_PTR)key, 
            NULL);
    }
private:
    HANDLE handle_ = nullptr;
};

}

}

#endif