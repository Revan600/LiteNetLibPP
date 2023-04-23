cmake_minimum_required(VERSION 3.12)

project(lnl CXX)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE sources
        "src/*.cpp"
        "include/*.h")

add_executable(lnl ${sources})
target_include_directories(lnl PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_compile_definitions(WIN32_LEAN_AND_MEAN)
target_link_libraries(lnl PRIVATE wsock32 ws2_32)