cmake_minimum_required(VERSION 3.0)

project(roxterm C)

include(FindPkgConfig)
include(GNUInstallDirs)

option(ENABLE_VIM "Enable neovim integration" ON)

execute_process(COMMAND ${PROJECT_SOURCE_DIR}/version.sh
    OUTPUT_VARIABLE ROXTERM_VERSION)
execute_process(COMMAND date "+%Y-%m-%d"
    OUTPUT_VARIABLE ROXTERM_DATE)

add_subdirectory(src)

install(FILES roxterm.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

install(FILES roxterm.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)