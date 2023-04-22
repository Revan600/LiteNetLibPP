#pragma once

#include <lnl/net_constants.h>

#ifdef _WIN32

#include <Windows.h>

#endif

namespace lnl {
    static constexpr auto TICKS_PER_MILLISECOND = 10000;

    inline int32_t relative_sequence_number(int32_t number, int32_t expected) {
        return (number - expected + net_constants::MAX_SEQUENCE + net_constants::HALF_MAX_SEQUENCE) %
               net_constants::MAX_SEQUENCE - net_constants::HALF_MAX_SEQUENCE;
    }

    inline int64_t get_current_time() {
#ifdef _WIN32
        int64_t timestamp;
        GetSystemTimeAsFileTime((FILETIME*) &timestamp);
        return timestamp;
#else
        static_assert(false);
#endif
    }
}