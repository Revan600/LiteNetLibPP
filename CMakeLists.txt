cmake_minimum_required(VERSION 3.12)

project(lnl CXX)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE sources
        "src/*.cpp"
        "include/*.h")

option(BUILD_EXAMPLE "Build the example app" ON)

add_library(lnl STATIC ${sources})
target_include_directories(lnl PRIVATE ${CMAKE_SOURCE_DIR}/include)

if (WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
    target_link_libraries(lnl PRIVATE wsock32 ws2_32)
elseif (UNIX)
    target_link_libraries(lnl PRIVATE pthread)
endif ()

if (BUILD_EXAMPLE)
    add_executable(lnl_example_server example/server.cpp)
    target_include_directories(lnl_example_server PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(lnl_example_server PRIVATE lnl)

    add_executable(lnl_example_client example/client.cpp)
    target_include_directories(lnl_example_client PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(lnl_example_client PRIVATE lnl)
endif ()