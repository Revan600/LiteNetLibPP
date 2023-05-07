#include <gtest/gtest.h>

#include <lnl/net_time.h>

#include <chrono>

TEST(net_time, should_have_correct_values) {
    auto time = lnl::net_time::from_date(1991, 8, 24, 23, 59, 59);
    ASSERT_TRUE(time);
    ASSERT_EQ(time->ticks(), 628186751990000000);
    ASSERT_EQ(time->year(), 1991);
    ASSERT_EQ(time->month(), 8);
    ASSERT_EQ(time->day(), 24);
    ASSERT_EQ(time->hour(), 23);
    ASSERT_EQ(time->minute(), 59);
    ASSERT_EQ(time->second(), 59);
}

TEST(net_time, utc_now_returns_correct_date) {
    using namespace std::chrono;

    auto utc = lnl::net_time::utc_now();
    auto now_c = time(nullptr);
    tm parts{};

#if _WIN32
    gmtime_s(&parts, &now_c);
#elif __linux__
    parts = *gmtime(&now_c);
#endif

    ASSERT_EQ(utc.kind(), lnl::DATE_TIME_KIND::UTC);
    ASSERT_EQ(utc.year(), parts.tm_year + 1900);
    ASSERT_EQ(utc.month(), parts.tm_mon + 1);
    ASSERT_EQ(utc.day(), parts.tm_mday);
    ASSERT_EQ(utc.hour(), parts.tm_hour);
    ASSERT_EQ(utc.minute(), parts.tm_min);
    ASSERT_EQ(utc.second(), parts.tm_sec);
}

TEST(net_time, now_returns_correct_date) {
    using namespace std::chrono;

    auto now = lnl::net_time::now();
    auto now_c = time(nullptr);
    tm parts{};

#if _WIN32
    localtime_s(&parts, &now_c);
#elif __linux__
    parts = *localtime(&now_c);
#endif

    ASSERT_EQ(now.kind(), lnl::DATE_TIME_KIND::LOCAL);
    ASSERT_EQ(now.year(), parts.tm_year + 1900);
    ASSERT_EQ(now.month(), parts.tm_mon + 1);
    ASSERT_EQ(now.day(), parts.tm_mday);
    ASSERT_EQ(now.hour(), parts.tm_hour);
    ASSERT_EQ(now.minute(), parts.tm_min);
    ASSERT_EQ(now.second(), parts.tm_sec);
}