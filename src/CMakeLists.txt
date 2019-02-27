# Find libraries

set(ROXTERM_PC_LIBS "vte-2.91>=0.52 gtk+-3.0>=3.20.0")

# Check for vim support
if(ENABLE_VIM STREQUAL CHECK)
    pkg_check_modules(MSGPACK msgpack)
    if(MSGPACK_FOUND)
        set(ENABLE_VIM "ON")
    else(MSGPACK_FOUND)
        set(ENABLE_VIM "OFF")
    endif(MSGPACK_FOUND)
endif(ENABLE_VIM STREQUAL CHECK)
if(ENABLE_VIM)
    string(APPEND ROXTERM_PC_LIBS " msgpack")

SEPARATE_ARGUMENTS(ROXTERM_PC_LIBS)
# If VTE >= 0.50 this should include the necessary flags for libpcre.
# gmodule is required so GtkBuilder can find signal handlers
pkg_check_modules(ROXTERM REQUIRED ROXTERM_PC_LIBS)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h)

add_executable(roxterm
    roxterm-application.c roxterm-window.c)
target_include_directories(roxterm PRIVATE
    ${ROXTERM_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(roxterm PRIVATE ${ROXTERM_CFLAGS_OTHER})
target_link_libraries(roxterm
    ${ROXTERM_LDFLAGS_OTHER} ${ROXTERM_LINK_LIBRARIES})

install(TARGETS roxterm RUNTIME DESTINATION bin)
#install(FILES roxterm-config.ui DESTINATION share/roxterm)
