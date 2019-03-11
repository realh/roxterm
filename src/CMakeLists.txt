# Find libraries

set(ROXTERM_PC_LIBS "vte-2.91>=0.52 gtk+-3.0>=3.20.0")

# Check for vim support
if(ENABLE_VIM)
    string(APPEND ROXTERM_PC_LIBS " msgpack")
    set(ENABLE_VIM 1)
else(ENABLE_VIM)
    set(ENABLE_VIM 0)
endif(ENABLE_VIM)

SEPARATE_ARGUMENTS(ROXTERM_PC_LIBS)
# If VTE >= 0.50 this should include the necessary flags for libpcre.
# gmodule is required so GtkBuilder can find signal handlers
pkg_check_modules(ROXTERM REQUIRED ${ROXTERM_PC_LIBS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h)

add_executable(roxterm4
    multitext-geometry-provider.c multitext-notebook.c multitext-window.c
    roxterm-application.c roxterm-launch-params.c roxterm-profile.c
    roxterm-rgba.c roxterm-vte.c roxterm-window.c)
target_include_directories(roxterm4 PRIVATE
    ${ROXTERM_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(roxterm4 PRIVATE ${ROXTERM_CFLAGS_OTHER})
target_link_libraries(roxterm4
    ${ROXTERM_LDFLAGS_OTHER} ${ROXTERM_LINK_LIBRARIES})

install(TARGETS roxterm4 RUNTIME DESTINATION bin)
#install(FILES roxterm-config.ui DESTINATION share/roxterm)
