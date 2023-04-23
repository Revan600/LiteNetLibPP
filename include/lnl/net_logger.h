#pragma once

#include <lnl/net_utils.h>

namespace lnl {
    struct net_logger final {
        template <typename... T>
        void log(const std::string& str, T&& ... args) {
            auto line = string_format(str, std::forward<T>(args)...);
            printf("%s\n", line.c_str());
        }
    };
}