set (WEBSCANNER_HEADERS
    mainwindow.hpp
    webwatcher.hpp
    timeunit.hpp
    applicationsettings.hpp
    applicationlanguage.hpp
    watcherinputdialog.hpp
    logging.hpp
    cefenginewrapper.hpp
)

set (WEBSCANNER_SOURCES
    main.cpp
    mainwindow.cpp
    webwatcher.cpp
    watcherinputdialog.cpp
    timeunit.cpp
    applicationsettings.cpp
    applicationlanguage.cpp
    logging.cpp
    cefenginewrapper.cpp
)

source_group (TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX Headers FILES ${WEBSCANNER_HEADERS})
source_group (TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX Sources FILES ${WEBSCANNER_SOURCES})

target_sources(webwatcher PUBLIC ${WEBSCANNER_HEADERS})
target_sources(webwatcher PRIVATE
    ${WEBSCANNER_SOURCES}
    mainwindow.ui
    watcherinputdialog.ui
    languagedialog.ui
)

add_subdirectory(cefengine/)
