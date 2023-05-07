#pragma once

#include <lnl/net_constants.h>

#include <cstdarg>
#include <string>

#ifdef _WIN32

#include <Windows.h>

#elif __linux__

#include <sys/time.h>

#endif

namespace lnl {
    static constexpr int64_t TICKS_PER_MILLISECOND = 10000;
    static constexpr int64_t TICKS_PER_SECOND = TICKS_PER_MILLISECOND * 1000;
    static constexpr int64_t TICKS_PER_MINUTE = TICKS_PER_SECOND * 60;
    static constexpr int64_t TICKS_PER_HOUR = TICKS_PER_MINUTE * 60;
    static constexpr int64_t TICKS_PER_DAY = TICKS_PER_HOUR * 24;

    static constexpr int32_t DAYS_PER_YEAR = 365;
    static constexpr int32_t DAYS_PER_4_YEARS = DAYS_PER_YEAR * 4 + 1;
    static constexpr int32_t DAYS_PER_100_YEARS = DAYS_PER_4_YEARS * 25 - 1;
    static constexpr int32_t DAYS_PER_400_YEARS = DAYS_PER_100_YEARS * 4 + 1;
    static constexpr int32_t DAYS_TO_1601 = DAYS_PER_400_YEARS * 4;

    static constexpr int64_t FILE_TIME_OFFSET = DAYS_TO_1601 * TICKS_PER_DAY;

    static constexpr int64_t SECS_TO_100NS = 10000000; /* 10^7 */
    static constexpr int64_t MICROSECONDS_TO_100NS = 10; /* 1000 / 100 */

    static constexpr uint64_t KIND_UTC = 0x4000000000000000;

    inline int32_t relative_sequence_number(int32_t number, int32_t expected) {
        return (number - expected + net_constants::MAX_SEQUENCE + net_constants::HALF_MAX_SEQUENCE) %
               net_constants::MAX_SEQUENCE - net_constants::HALF_MAX_SEQUENCE;
    }

    inline int64_t get_current_time() {
#ifdef _WIN32
        int64_t timestamp;
        GetSystemTimeAsFileTime((FILETIME*) &timestamp);
        return (int64_t) ((timestamp + (FILE_TIME_OFFSET | KIND_UTC)) & 0x3FFFFFFFFFFFFFFF);
#else
        struct timeval time{};

        if (gettimeofday(&time, nullptr) != 0) {
            // in failure we return 00:00 01 January 1970 UTC (Unix epoch)
            return 0;
        }

        return ((int64_t) time.tv_sec) * SECS_TO_100NS + (time.tv_usec * MICROSECONDS_TO_100NS);
#endif
    }

    //https://stackoverflow.com/a/8098080
    inline std::string string_format(const std::string fmt, ...) {
        int size = ((int) fmt.size()) * 2 + 50;   // Use a rubric appropriate for your code
        std::string str;
        va_list ap;
        while (true) {     // Maximum two passes on a POSIX system...
            str.resize(size);
            va_start(ap, fmt);
            int n = vsnprintf((char*) str.data(), size, fmt.c_str(), ap);
            va_end(ap);
            if (n > -1 && n < size) {  // Everything worked
                str.resize(n);
                return str;
            }
            if (n > -1)  // Needed size returned
                size = n + 1;   // For null char
            else
                size *= 2;      // Guess at a larger size (OS specific)
        }
        return str;
    }
}