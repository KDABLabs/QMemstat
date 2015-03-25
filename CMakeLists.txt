cmake_minimum_required(VERSION 2.8.12)
project(memstat)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -D_LARGEFILE64_SOURCE")

add_executable(memstat
               memstat.cpp
               processinfo.cpp
               pageinfo.cpp)

find_package(Qt4)
if (Qt4_FOUND)
    set(QT_USE_QTNETWORK TRUE)
    include(${QT_USE_FILE})

    add_definitions(${QT_DEFINITIONS})
    qt4_wrap_cpp(qmemstat_MOC mosaicwidget.h mainwindow.h)
    add_executable(qmemstat
                qmemstat.cpp
                processinfo.cpp
                pageinfo.cpp
                mosaicwidget.cpp
                mainwindow.cpp
                ${qmemstat_MOC})
    target_link_libraries(qmemstat ${QT_LIBRARIES})
endif()
