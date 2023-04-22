#pragma once

#include <fmt/format.h>

namespace lnl {
    struct net_logger final {
        template <typename... T>
        void log(const std::string& str, T&& ... args) {
            auto line = fmt::format(str, std::forward<T>(args)...);
            printf("%s\n", line.c_str());
        }
    };
}