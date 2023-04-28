include(FetchContent)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.11.0
)
FetchContent_MakeAvailable(googletest)
add_library(GTest::GTest INTERFACE IMPORTED)
target_link_libraries(GTest::GTest INTERFACE gtest_main)

file(GLOB_RECURSE sources
        "*.cpp")

add_executable(lnl_test ${sources})

target_link_libraries(lnl_test
        PRIVATE
        GTest::GTest
        lnl)

add_test(lnl_gtests lnl_test)