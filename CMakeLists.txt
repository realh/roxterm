cmake_minimum_required(VERSION 3.0)

include(FindPkgConfig)

project(ROXTerm)
execute_process(COMMAND ${PROJECT_SOURCE_DIR}/version.sh
    OUTPUT_VARIABLE ROXTERM_VERSION)

add_subdirectory(src)
