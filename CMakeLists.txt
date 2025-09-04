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
configure_file(${PROJECT_SOURCE_DIR}/roxterm.1.xml.in
    ${PROJECT_BINARY_DIR}/roxterm.1.xml)
configure_file(${PROJECT_SOURCE_DIR}/roxterm-config.1.xml.in
    ${PROJECT_BINARY_DIR}/roxterm-config.1.xml)

add_subdirectory(src)

install(FILES src/icons/roxterm.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

install(DIRECTORY Config
    DESTINATION ${CMAKE_INSTALL_DATADIR}/roxterm)

install(DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_DOCDIR})

find_program(XSLTPROC xsltproc)
if (NOT "${XSLTPROC}" EQUAL "XSLTPROC-NOTFOUND")
    set(XSLTPROC_OPTIONS --novalid --param man.charmap.use.subset 0
        http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl)
    add_custom_command(OUTPUT roxterm.1
        COMMAND ${XSLTPROC} -o roxterm.1 ${XSLTPROC_OPTIONS}
            ${PROJECT_BINARY_DIR}/roxterm.1.xml
        MAIN_DEPENDENCY ${PROJECT_BINARY_DIR}/roxterm.1.xml
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_command(OUTPUT roxterm-config.1
        COMMAND ${XSLTPROC} -o roxterm-config.1 ${XSLTPROC_OPTIONS}
            ${PROJECT_BINARY_DIR}/roxterm-config.1.xml
        MAIN_DEPENDENCY ${PROJECT_BINARY_DIR}/roxterm-config.1.xml
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_target(manpages ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/roxterm.1
            ${CMAKE_CURRENT_BINARY_DIR}/roxterm-config.1)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/roxterm.1
        ${CMAKE_CURRENT_BINARY_DIR}/roxterm-config.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1) 
else ()
    message(WARNING "xsltproc not found, man pages will not be generated")
endif ()

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
    mkycmflags
    NEWS
    po
    README
    README.md
    README.translations
    roxterm.1.xml.in
    roxterm.metainfo.xml.in
    roxterm-config.1.xml.in
    roxterm.desktop
    roxterm.lsm.in
    roxterm.xml
    src
    version
    version.sh)
add_custom_target(dist DEPENDS "${TARBALL_NAME}")
