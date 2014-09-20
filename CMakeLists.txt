cmake_minimum_required(VERSION 2.8.12)
project(memstat)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_executable(memstat memstat.cpp processinfo.cpp pageinfo.cpp)
