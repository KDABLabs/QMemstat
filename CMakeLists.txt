cmake_minimum_required(VERSION 2.8.12)
project(memstat)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -D_LARGEFILE64_SOURCE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-error=unused-result -Wno-error=sign-conversion")
if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.7)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wpedantic")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic")
endif()

add_executable(memstat
               memstat.cpp
               processinfo.cpp
               pageinfo.cpp)

find_package(Qt4)
if (Qt4_FOUND)
    set(QT_USE_QTNETWORK TRUE)
    include(${QT_USE_FILE})

    add_definitions(${QT_DEFINITIONS})
    qt4_wrap_cpp(qmemstat_MOC flagsmodel.h mosaicwidget.h mainwindow.h)
    add_executable(qmemstat
                qmemstat.cpp
                processinfo.cpp
                pageinfo.cpp
                flagsmodel.cpp
                mosaicwidget.cpp
                mainwindow.cpp
                ${qmemstat_MOC})
    target_link_libraries(qmemstat ${QT_LIBRARIES})
endif()
