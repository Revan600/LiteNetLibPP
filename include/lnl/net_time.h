#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <cstdlib>

#include <lnl/net_enums.h>

namespace lnl {
    //partial reimplementation of C# System.DateTime class
    class net_time final {
        static constexpr std::array<uint32_t, 13> DAYS_TO_MONTH_365{
                0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
        };

        static constexpr std::array<uint32_t, 13> DAYS_TO_MONTH_366{
                0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366
        };

        uint64_t m_ticks;
    public:
        static constexpr uint64_t TICKS_MASK = 0x3FFFFFFFFFFFFFFF;
        static constexpr uint64_t FLAGS_MASK = 0xC000000000000000;

        static constexpr int64_t MICROSECONDS_PER_MILLISECOND = 1000;
        static constexpr int64_t TICKS_PER_MICROSECOND = 10;
        static constexpr int64_t TICKS_PER_MILLISECOND = TICKS_PER_MICROSECOND * MICROSECONDS_PER_MILLISECOND;
        static constexpr int64_t TICKS_PER_SECOND = TICKS_PER_MILLISECOND * 1000;
        static constexpr int64_t TICKS_PER_MINUTE = TICKS_PER_SECOND * 60;
        static constexpr int64_t TICKS_PER_HOUR = TICKS_PER_MINUTE * 60;
        static constexpr uint64_t TICKS_PER_6HOURS = TICKS_PER_HOUR * 6;
        static constexpr int64_t TICKS_PER_DAY = TICKS_PER_HOUR * 24;

        static constexpr int32_t MILLIS_PER_SECOND = 1000;
        static constexpr int32_t MILLIS_PER_MINUTE = MILLIS_PER_SECOND * 60;
        static constexpr int32_t MILLIS_PER_HOUR = MILLIS_PER_MINUTE * 60;
        static constexpr int32_t MILLIS_PER_DAY = MILLIS_PER_HOUR * 24;

        static constexpr int32_t DAYS_PER_YEAR = 365;
        static constexpr int32_t DAYS_PER_4_YEARS = DAYS_PER_YEAR * 4 + 1;
        static constexpr int32_t DAYS_PER_100_YEARS = DAYS_PER_4_YEARS * 25 - 1;
        static constexpr int32_t DAYS_PER_400_YEARS = DAYS_PER_100_YEARS * 4 + 1;
        static constexpr int32_t DAYS_TO_1601 = DAYS_PER_400_YEARS * 4;
        static constexpr int32_t DAYS_TO_1899 = DAYS_PER_400_YEARS * 4 + DAYS_PER_100_YEARS * 3 - 367;
        static constexpr int32_t DAYS_TO_1970 = DAYS_PER_400_YEARS * 4 +
                                                DAYS_PER_100_YEARS * 3 +
                                                DAYS_PER_4_YEARS * 17 +
                                                DAYS_PER_YEAR;
        static constexpr int32_t DAYS_TO_10000 = DAYS_PER_400_YEARS * 25 - 366;
        static constexpr int32_t MARCH1_BASED_DAY_OF_NEW_YEAR = 306;

        static constexpr int64_t MIN_TICKS = 0;
        static constexpr int64_t MAX_TICKS = DAYS_TO_10000 * TICKS_PER_DAY - 1;

        static constexpr int64_t MAX_MILLIS = (int64_t) DAYS_TO_10000 * MILLIS_PER_DAY;

        static constexpr uint64_t KIND_UNSPECIFIED = 0x0000000000000000;
        static constexpr uint64_t KIND_UTC = 0x4000000000000000;
        static constexpr uint64_t KIND_LOCAL = 0x8000000000000000;
        static constexpr uint64_t KIND_LOCAL_AMBIGUOUS_DST = 0xC000000000000000;
        static constexpr int32_t KIND_SHIFT = 62;

        static constexpr uint32_t EAF_MULTIPLIER = (uint32_t) ((((uint64_t) 1 << 32) + DAYS_PER_4_YEARS - 1) /
                                                               DAYS_PER_4_YEARS);
        static constexpr uint32_t EAF_DIVIDER = EAF_MULTIPLIER * 4;

        static constexpr uint64_t FILE_TIME_OFFSET = DAYS_TO_1601 * TICKS_PER_DAY;
        static constexpr uint64_t UNIX_EPOCH_TICKS = DAYS_TO_1970 * TICKS_PER_DAY;

        static constexpr int64_t SECS_TO_100NS = 10000000; /* 10^7 */
        static constexpr int64_t MICROSECONDS_TO_100NS = 10; /* 1000 / 100 */

        static net_time utc_now();
        static net_time now();

        static std::optional<net_time> from_ticks(uint64_t ticks) {
            if (ticks > MAX_TICKS) {
                return {};
            }

            return net_time(ticks);
        }

        static std::optional<net_time> from_ticks(uint64_t ticks, DATE_TIME_KIND kind) {
            if (ticks > MAX_TICKS) {
                return {};
            }

            return net_time(ticks | ((uint64_t) (uint32_t) kind << KIND_SHIFT));
        }

        static std::optional<net_time> from_ticks(uint64_t ticks, DATE_TIME_KIND kind, bool isAmbiguousDst) {
            if (ticks > MAX_TICKS || kind != DATE_TIME_KIND::LOCAL) {
                return {};
            }

            return net_time(ticks | (isAmbiguousDst ? KIND_LOCAL_AMBIGUOUS_DST : KIND_LOCAL));
        }

        static std::optional<net_time> from_date(int32_t year, int32_t month, int32_t day) {
            auto ticks = date_to_ticks(year, month, day);
            return net_time(ticks);
        }

        static std::optional<net_time> from_date(int32_t year, int32_t month, int32_t day,
                                                 int32_t hour, int32_t minute, int32_t second) {
            if (second != 60 || !is_leap_seconds_supported()) {
                auto ticks = date_to_ticks(year, month, day) + time_to_ticks(hour, minute, second);
                return net_time(ticks);
            }

            auto result = from_date(year, month, day, hour, minute, 59);

            if (result.has_value() && !result->validate_leap_second(result->kind())) {
                return {};
            }

            return result;
        }

        [[nodiscard]] int32_t year() const {
            auto value = (uint32_t) (uticks() / TICKS_PER_6HOURS) | 3U;
            uint32_t y100 = value / DAYS_PER_400_YEARS;
            uint32_t r1 = value % DAYS_PER_400_YEARS;

            auto v1 = 100 * y100;
            auto v2 = (r1 | 3) / DAYS_PER_4_YEARS;

            return 1 + (int) (v1 + v2);
        }

        [[nodiscard]] int32_t month() const {
            uint32_t r1 = (((uint32_t) (uticks() / TICKS_PER_6HOURS) | 3U) + 1224) % DAYS_PER_400_YEARS;
            uint64_t u2 = (uint64_t) EAF_MULTIPLIER * (uint64_t) (r1 | 3);
            auto daySinceMarch1 = (uint16_t) ((uint32_t) u2 / EAF_DIVIDER);
            int n3 = 2141 * daySinceMarch1 + 197913;
            return (uint16_t) (n3 >> 16) - (daySinceMarch1 >= MARCH1_BASED_DAY_OF_NEW_YEAR ? 12 : 0);
        }

        [[nodiscard]] int32_t day() const {
            uint32_t r1 = (((uint32_t) (uticks() / TICKS_PER_6HOURS) | 3U) + 1224) % DAYS_PER_400_YEARS;
            uint64_t u2 = (uint64_t) EAF_MULTIPLIER * (uint64_t) (r1 | 3);
            auto daySinceMarch1 = (uint16_t) ((uint32_t) u2 / EAF_DIVIDER);
            int32_t n3 = 2141 * daySinceMarch1 + 197913;
            return (uint16_t) n3 / 2141 + 1;
        }

        [[nodiscard]] int32_t hour() const {
            return (int32_t) ((uint32_t) (uticks() / TICKS_PER_HOUR) % 24);
        }

        [[nodiscard]] int32_t minute() const {
            return (int32_t) ((uticks() / TICKS_PER_MINUTE) % 60);
        }

        [[nodiscard]] int32_t second() const {
            return (int32_t) ((uticks() / TICKS_PER_SECOND) % 60);
        }

        [[nodiscard]] int32_t millisecond() const {
            return (int32_t) ((uticks() / TICKS_PER_MILLISECOND) % 1000);
        }

        [[nodiscard]] int32_t microsecond() const {
            return (int32_t) ((uticks() / TICKS_PER_MICROSECOND) % 1000);
        }

        [[nodiscard]] int32_t nanosecond() const {
            return (int32_t) ((uticks() / TICKS_PER_MICROSECOND) % 100);
        }

        [[nodiscard]] uint64_t ticks() const {
            return uticks();
        }

        [[nodiscard]] DATE_TIME_KIND kind() const {
            switch (m_ticks & FLAGS_MASK) {
                case KIND_UTC: {
                    return DATE_TIME_KIND::UTC;
                }
                case KIND_UNSPECIFIED: {
                    return DATE_TIME_KIND::UNSPECIFIED;
                }
                default: {
                    return DATE_TIME_KIND::LOCAL;
                }
            }
        }

    private:
        explicit net_time(uint64_t ticks) : m_ticks(ticks) {}

        [[nodiscard]] uint64_t uticks() const {
            return m_ticks & TICKS_MASK;
        }

        static uint64_t time_to_ticks(int32_t hour, int32_t minute, int32_t second) {
            if ((uint32_t) hour >= 24 || (uint32_t) minute >= 60 || (uint32_t) second >= 60) {
                return MAX_TICKS + 1;
            }

            int totalSeconds = hour * 3600 + minute * 60 + second;
            return (uint32_t) totalSeconds * (uint64_t) TICKS_PER_SECOND;
        }

        static uint64_t date_to_ticks(int32_t year, int32_t month, int32_t day) {
            if (year < 1 || year > 9999 || month < 1 || month > 12 || day < 1) {
                return MAX_TICKS + 1;
            }

            auto& days = is_leap_year(year) ? DAYS_TO_MONTH_366 : DAYS_TO_MONTH_365;

            if ((uint32_t) day > days[month] - days[month - 1]) {
                return MAX_TICKS + 1;
            }

            uint32_t n = days_to_year((uint32_t) year) + days[month - 1] + (uint32_t) day - 1;
            return n * (uint64_t) TICKS_PER_DAY;
        }

        static uint32_t days_to_year(uint32_t year) {
            auto y = year - 1;
            auto cent = y / 100;
            return y * (365 * 4 + 1) / 4 - cent + cent / 4;
        }

        static bool is_leap_year(int32_t year) {
            if ((year & 3) != 0) {
                return false;
            }

            if ((year & 15) == 0) {
                return true;
            }

            return (uint32_t) year % 25 != 0;
        }

        bool validate_leap_second(DATE_TIME_KIND kind) const;

        static bool is_leap_seconds_supported();
    };
}