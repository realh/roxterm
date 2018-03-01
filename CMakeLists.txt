cmake_minimum_required(VERSION 3.0)

project(roxterm C)

include(FindPkgConfig)
include(GNUInstallDirs)

execute_process(COMMAND ${PROJECT_SOURCE_DIR}/version.sh
    OUTPUT_VARIABLE ROXTERM_VERSION)
execute_process(COMMAND date "+%Y-%m-%d"
    OUTPUT_VARIABLE ROXTERM_DATE)

set(APPINFO_VERSION "$ROXTERM_VERSION $ROXTERM_DATE")
configure_file(${PROJECT_SOURCE_DIR}/AppInfo.xml.in
    ${PROJECT_SOURCE_DIR}/AppInfo.xml)
# appdata should really be processed by i18n
configure_file(${PROJECT_SOURCE_DIR}/roxterm.appdata.xml.in
    ${PROJECT_BINARY_DIR}/roxterm.appdata.xml)
configure_file(${PROJECT_SOURCE_DIR}/roxterm.spec.in
    ${PROJECT_SOURCE_DIR}/roxterm.spec)
configure_file(${PROJECT_SOURCE_DIR}/roxterm.1.xml.in
    ${PROJECT_BINARY_DIR}/roxterm.1.xml)
configure_file(${PROJECT_SOURCE_DIR}/roxterm-config.1.xml.in
    ${PROJECT_BINARY_DIR}/roxterm-config.1.xml)

add_subdirectory(src)

install(FILES roxterm.svg
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

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/roxterm.appdata.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/appdata)


# It would be nice if we could use cmake -E tar here, but it doesn't support
# any of the -- options needed here.
add_custom_target(dist COMMAND tar cfvJ
    "${CMAKE_PROJECT_NAME}-${ROXTERM_VERSION}.tar.xz"
    "--directory=${PROJECT_SOURCE_DIR}"
    "--transform=s,^,${CMAKE_PROJECT_NAME}-${ROXTERM_VERSION}/,"
    "--exclude=*~"
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
    README
    README.md
    README.translations
    roxterm.1.xml.in
    roxterm.appdata.xml.in
    roxterm-config.1.xml.in
    roxterm.desktop
    roxterm.lsm.in
    roxterm.spec.in
    roxterm.svg
    roxterm.xml
    src
    version
    version.sh)
