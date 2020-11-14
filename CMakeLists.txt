cmake_minimum_required (VERSION 3.13 FATAL_ERROR)

project(WebWatcher)

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
    LinguistTools
)

#
# CEF configuration.
#

# Specify the CEF distribution version.
set(CEF_VERSION "81.2.24+gc0b313d+chromium-81.0.4044.113")

# Determine the platform.
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
  set(CEF_PLATFORM "macosx64")
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  if(CMAKE_SIZEOF_VOID_P MATCHES 8)
    set(CEF_PLATFORM "linux64")
  else()
    set(CEF_PLATFORM "linux32")
  endif()
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  if(CMAKE_SIZEOF_VOID_P MATCHES 8)
    set(CEF_PLATFORM "windows64")
  else()
    set(CEF_PLATFORM "windows32")
  endif()
endif()

# Add this project's cmake/ directory to the module path.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Download and extract the CEF binary distribution (executes DownloadCEF.cmake).
include(DownloadCEF)
DownloadCEF("${CEF_PLATFORM}" "${CEF_VERSION}" "${CMAKE_SOURCE_DIR}/third_party/cef")

# Add the CEF binary distribution's cmake/ directory to the module path.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")

# Load the CEF configuration (executes FindCEF.cmake).
find_package(CEF REQUIRED)

# Include the libcef_dll_wrapper target (executes libcef_dll/CMakeLists.txt).
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

# Setup common binary dir
set(EXAMPLE_TARGET_OUT_DIR "${CMAKE_BINARY_DIR}")
add_subdirectory(third_party/)

add_executable(webwatcher "")
# Fill webwatcher target by source dependings
add_subdirectory(src/)

# Generate translations from .ts files and add them to .qrc file
file(GLOB TS_FILES "translations/*.ts")
qt5_add_translation(QM_FILES ${TS_FILES})
configure_file(translations/translations.qrc ${CMAKE_BINARY_DIR} COPYONLY)

target_sources(webwatcher PUBLIC
    resources.qrc
    ${CMAKE_BINARY_DIR}/translations.qrc
    ${QM_FILES}
)
target_link_libraries (webwatcher PUBLIC
    Qt5::Widgets
    Qt5::Network
    Qt5::WebEngineWidgets
    Qt5::Xml
)
