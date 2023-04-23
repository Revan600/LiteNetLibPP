#pragma once

#include <lnl/net_manager.h>

namespace lnl {
    inline void initialize() {
#ifdef WIN32
        WSADATA wsaData;

        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }
}