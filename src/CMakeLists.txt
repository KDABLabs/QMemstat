add_executable(memstat
               memstat.cpp
               processinfo.cpp
               pageinfo.cpp)
install(TARGETS memstat RUNTIME DESTINATION bin)

if (Qt5Core_FOUND)
    find_package(Qt5 CONFIG REQUIRED COMPONENTS Gui Widgets Network)
    add_executable(qmemstat
                qmemstat.cpp
                processinfo.cpp
                pageinfo.cpp
                flagsmodel.cpp
                mosaicwidget.cpp
                mainwindow.cpp)
    target_link_libraries(qmemstat Qt5::Widgets Qt5::Network)
    install(TARGETS qmemstat RUNTIME DESTINATION bin)
endif()
