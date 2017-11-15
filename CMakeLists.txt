cmake_minimum_required(VERSION 3.0)
project(memstat)

include(FeatureSummary)

set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -D_LARGEFILE64_SOURCE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.7)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wpedantic")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic")
endif()

find_package(Qt5Core) # for qmemstat
set_package_properties(Qt5Core PROPERTIES TYPE RECOMMENDED PURPOSE "Qt5 libraries. Required for the qmemstat GUI executable." URL "https://www.qt.io/")

add_subdirectory(src)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
