cmake_minimum_required (VERSION 3.10 FATAL_ERROR)

project(WebWatcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)
set (CMAKE_AUTOUIC ON)

find_package(Qt5 5.6.0 CONFIG REQUIRED
    Widgets
    Network
    WebEngineWidgets
    Xml
)

set (WEBSCANNER_HEADERS
    src/mainwindow.h
    src/webwatcher.h
    src/timeunit.h
    src/watcherinputdialog.h
    src/notificator.h
)

set (WEBSCANNER_SOURCES
    src/mainwindow.cpp
    src/webwatcher.cpp
    src/watcherinputdialog.cpp
    src/timeunit.cpp
    src/notificator.cpp
    src/main.cpp
)

source_group (TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX Headers FILES ${WEBSCANNER_HEADERS})
source_group (TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX Sources FILES ${WEBSCANNER_SOURCES})

add_executable (webwatcher
    ${WEBSCANNER_HEADERS}
    ${WEBSCANNER_SOURCES}
    src/mainwindow.ui
    src/watcherinputdialog.ui
    src/notification.ui
    resources.qrc
)
target_link_libraries (webwatcher PUBLIC
    Qt5::Widgets
    Qt5::Network
    Qt5::WebEngineWidgets
    Qt5::Xml
)
