cmake_minimum_required(VERSION 3.10)

project(roxterm C)

include(FindPkgConfig)
include(GNUInstallDirs)

find_package (Intl)
if (Intl_FOUND)
    message(STATUS "Internationalization (i18n) found:")
    message(STATUS " INTL_INCLUDE_DIRS: ${Intl_INCLUDE_DIRS}")
    message(STATUS " INTL_LIBRARIES: ${Intl_LIBRARIES}")
    message(STATUS " Version: ${Intl_VERSION}")
    include_directories(${Intl_INCLUDE_DIRS})
    link_directories(${Intl_LIBRARY_DIRS})
    set(ENABLE_NLS TRUE)
else ()
    message(STATUS "Internationalization (i18n) Not found!")
    set(ENABLE_NLS FALSE)
endif ()

execute_process(COMMAND ${PROJECT_SOURCE_DIR}/version.sh
    OUTPUT_VARIABLE ROXTERM_VERSION)
execute_process(COMMAND date "+%Y-%m-%d"
    OUTPUT_VARIABLE ROXTERM_DATE)

message(STATUS "ROXTERM_VERSION: ${ROXTERM_VERSION}")
message(STATUS "ROXTERM_DATE: ${ROXTERM_DATE}")

find_package(Gettext REQUIRED)
if (Gettext_FOUND)
    message(STATUS "Gettext found:")
    message(STATUS " Version: ${GETTEXT_VERSION_STRING}")
    add_subdirectory(po)
else ()
    message(STATUS "Gettext Not found!")
endif ()

set(APPINFO_VERSION "${ROXTERM_VERSION} ${ROXTERM_DATE}")
configure_file(${PROJECT_SOURCE_DIR}/AppInfo.xml.in
    ${PROJECT_SOURCE_DIR}/AppInfo.xml)
# metainfo should really be processed by i18n
configure_file(${PROJECT_SOURCE_DIR}/roxterm.metainfo.xml.in
    ${PROJECT_BINARY_DIR}/roxterm.metainfo.xml)

add_subdirectory(src)
add_subdirectory(man)

install(FILES src/icons/roxterm.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

install(DIRECTORY Config
    DESTINATION ${CMAKE_INSTALL_DATADIR}/roxterm)

install(DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_DOCDIR})

install(FILES roxterm.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/roxterm.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)


# It would be nice if we could use cmake -E tar here, but it doesn't support
# any of the -- options needed here.
set(ROXTERM_PACKAGE_NAME "${CMAKE_PROJECT_NAME}-${ROXTERM_VERSION}")
set(TARBALL_NAME "${ROXTERM_PACKAGE_NAME}.tar.xz")
add_custom_command(OUTPUT "${TARBALL_NAME}"
    COMMAND tar cfJ
    "${TARBALL_NAME}"
    "--directory=${PROJECT_SOURCE_DIR}"
    "--transform=s,^,${CMAKE_PROJECT_NAME}-${ROXTERM_VERSION}/,"
    "--exclude=*~"
    AppInfo.xml
    AppInfo.xml.in
    AppRun
    AUTHORS
    CMakeLists.txt
    Config
    COPYING
    COPYING-LGPL
    docs
    INSTALL
    INSTALL.Debian
    man
    mkycmflags
    NEWS
    po
    README
    README.md
    roxterm.metainfo.xml.in
    roxterm.desktop
    roxterm.lsm.in
    roxterm.xml
    src
    version
    version.sh)
add_custom_target(dist DEPENDS "${TARBALL_NAME}")
