# Find libraries

# If VTE >= 0.50 this should include the necessary flags for libpcre.
# Need gmodule so GtkBuilder can find signal handlers
pkg_check_modules(ROXTERM REQUIRED
    vte-2.91>=0.52 gtk+-3.0>=3.20.0 gmodule-export-2.0 libpcre2-8)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h)

add_custom_command(OUTPUT gresources.c
    COMMAND glib-compile-resources
        --target=${CMAKE_CURRENT_BINARY_DIR}/gresources.c --generate-source
        --c-name=roxterm_resource ${CMAKE_CURRENT_SOURCE_DIR}/gresources.xml
        --sourcedir=${CMAKE_CURRENT_SOURCE_DIR}
        MAIN_DEPENDENCY gresources.xml
        DEPENDS icons/roxterm.svg roxterm-config.ui)

add_executable(roxterm
    about.c capplet.c colourgui.c colourscheme.c configlet.c
    dlg.c dragrcv.c dynopts.c getname.c
    geom-widget.c
    globalopts.c gresources.c
    launch-params.c menutree.c
    multitab.c multitab-close-button.c multitab-label.c
    options.c optsfile.c profilegui.c resources.c
    roxterm.c roxterm-app.c roxterm-dirs.c roxterm-regex.c search.c
    session-file.c shortcuts.c uri.c)
target_include_directories(roxterm PRIVATE
    ${ROXTERM_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(roxterm PRIVATE ${ROXTERM_CFLAGS_OTHER})
target_link_directories(roxterm PRIVATE ${ROXTERM_LIBRARY_DIRS})
target_link_libraries(roxterm PRIVATE ${ROXTERM_LIBRARIES})

# Doesn't work on Mac: https://gitlab.kitware.com/cmake/cmake/issues/19688
# Luckily it seems to be OK to just miss out the -framework libraries
#target_link_options(roxterm PUBLIC ${ROXTERM_LDFLAGS_OTHER})

install(TARGETS roxterm RUNTIME DESTINATION bin)
install(FILES roxterm-config.ui DESTINATION share/roxterm)
