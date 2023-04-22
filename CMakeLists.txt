cmake_minimum_required(VERSION 3.12)

project(lnlpp CXX)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE sources
        "src/*.cpp"
        "include/*.h")

add_executable(lnlpp ${sources})
target_include_directories(lnlpp PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_compile_definitions(WIN32_LEAN_AND_MEAN)
target_link_libraries(lnlpp PRIVATE wsock32 ws2_32)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(lnlpp PRIVATE fmt::fmt)