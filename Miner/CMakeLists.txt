cmake_minimum_required(VERSION 3.16)
project(Miner)

set(CMAKE_CXX_STANDARD 17)

find_package(ZLIB REQUIRED)

add_executable(Miner.out main.cpp ../block.cpp)
target_include_directories(Server.out PRIVATE ..)
target_link_libraries(Miner.out ZLIB::ZLIB)
