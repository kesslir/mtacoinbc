cmake_minimum_required(VERSION 3.16)
project(Server)

set(CMAKE_CXX_STANDARD 17)

find_package(ZLIB REQUIRED)

add_executable(Server.out main.cpp ../block.cpp)
target_include_directories(Server.out PRIVATE ..)
target_link_libraries(Server.out ZLIB::ZLIB)
