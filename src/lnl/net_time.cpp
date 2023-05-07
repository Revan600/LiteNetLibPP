#include <lnl/net_time.h>

#ifdef _WIN32

#include <Windows.h>
#include <winternl.h>
#include <timezoneapi.h>

#pragma comment(lib, "ntdll.lib")

#elif __linux__

#include <sys/time.h>

#endif

#ifdef _WIN32
typedef struct _SYSTEM_LEAP_SECOND_INFORMATION {
    BOOLEAN Enabled;
    ULONG Flags;
} SYSTEM_LEAP_SECOND_INFORMATION;

//https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/api/ntexapi/system_information_class.htm
#define SystemLeapSecondInformation (SYSTEM_INFORMATION_CLASS)0xCE
#define STATUS_SUCCESS 0x00000000

#elif __linux__
#endif

bool lnl::net_time::is_leap_seconds_supported() {
#ifdef _WIN32
    SYSTEM_LEAP_SECOND_INFORMATION systemLeapSecondInformation{};
    auto result = NtQuerySystemInformation(SystemLeapSecondInformation,
                                           (void*) &systemLeapSecondInformation,
                                           sizeof(SYSTEM_LEAP_SECOND_INFORMATION),
                                           nullptr);

    if (result != STATUS_SUCCESS) {
        return false;
    }

    return systemLeapSecondInformation.Enabled;
#elif __linux__
    return false;
#endif
}

bool lnl::net_time::validate_leap_second(DATE_TIME_KIND kind) const {
#if _WIN32
    SYSTEMTIME time;
    time.wYear = (uint16_t) year();
    time.wMonth = (uint16_t) month();
    time.wDayOfWeek = 0;
    time.wDay = (uint16_t) day();
    time.wHour = (uint16_t) hour();
    time.wMinute = (uint16_t) minute();
    time.wSecond = 60;
    time.wMilliseconds = 0;

    if (kind != DATE_TIME_KIND::UTC) {
        SYSTEMTIME st{};

        if (TzSpecificLocalTimeToSystemTime(nullptr, &time, &st) != FALSE) {
            return true;
        }
    }

    if (kind != DATE_TIME_KIND::LOCAL) {
        FILETIME ft{};

        if (SystemTimeToFileTime(&time, &ft) != FALSE) {
            return true;
        }
    }

    return false;
#elif __linux__
    return false;
#endif
}

lnl::net_time lnl::net_time::utc_now() {
#ifdef _WIN32
    uint64_t timestamp;
    GetSystemTimeAsFileTime((PFILETIME) &timestamp);

    return net_time(timestamp + (FILE_TIME_OFFSET | KIND_UTC));
#elif __linux__
    static net_time dummy(0);

    struct timeval time{};

    if (gettimeofday(&time, nullptr) != 0) {
        // in failure we return 00:00 01 January 1970 UTC (Unix epoch)
        return dummy;
    }

    auto timestamp = (uint64_t) time.tv_sec * SECS_TO_100NS +
                     time.tv_usec * MICROSECONDS_TO_100NS;

    return net_time(timestamp + (UNIX_EPOCH_TICKS | KIND_UTC));
#endif
}

lnl::net_time lnl::net_time::now() {
#ifdef _WIN32
    uint64_t utc;
    GetSystemTimeAsFileTime((PFILETIME) &utc);

    uint64_t timestamp;
    FileTimeToLocalFileTime((PFILETIME) &utc, (LPFILETIME) &timestamp);

    return net_time(timestamp + (FILE_TIME_OFFSET | KIND_LOCAL));
#elif __linux__
    static net_time dummy(0);

    struct timeval tv{};

    if (gettimeofday(&tv, nullptr) != 0) {
        return dummy;
    }

    auto tm = *localtime(&tv.tv_sec);

    auto timestamp = (uint64_t) (tv.tv_sec + tm.tm_gmtoff) * SECS_TO_100NS +
                     tv.tv_usec * MICROSECONDS_TO_100NS;

    return net_time(timestamp + (UNIX_EPOCH_TICKS | KIND_LOCAL));
#endif
}
