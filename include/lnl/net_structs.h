#pragma once

#include <memory>

#include <lnl/net_enums.h>

namespace lnl {
    struct disconnect_info {
        DISCONNECT_REASON reason;
        uint32_t socket_error_code;
    };

    struct net_event final {
        std::unique_ptr<net_event> next;
        NET_EVENT_TYPE type;

    };
}