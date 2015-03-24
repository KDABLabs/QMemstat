cmake_minimum_required(VERSION 2.8.12)
project(memstat)

find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -D_LARGEFILE64_SOURCE")

qt4_wrap_cpp(memstat_MOC mosaicwindow.h)

add_executable(memstat
               memstat.cpp
               processinfo.cpp
               pageinfo.cpp
               mosaicwindow.cpp
               ${memstat_MOC})

target_link_libraries(memstat ${QT_LIBRARIES})
