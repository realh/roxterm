# Find libraries

# roxterm and roxterm-config both depend on these
pkg_check_modules(RTCOMMON REQUIRED
    gtk+-3.0>=3.20.0 dbus-1>=1.0 dbus-glib-1>=0.22)

# roxterm-config needs certain flags so GtkBuilder can find signal handlers
pkg_check_modules(GMODULE REQUIRED gmodule-export-2.0)

# If VTE >= 0.50 this should include the necessary flags for libpcre
pkg_check_modules(VTE REQUIRED vte-2.91>=0.52)

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

add_library(rtlib OBJECT
    colourscheme.c dlg.c dragrcv.c dynopts.c globalopts.c
    gresources.c options.c optsfile.c resources.c rtdbus.c)
target_include_directories(rtlib PRIVATE
    ${RTCOMMON_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(rtlib PRIVATE ${RTCOMMON_CFLAGS_OTHER})

add_executable(roxterm $<TARGET_OBJECTS:rtlib>
    about.c main.c multitab.c multitab-close-button.c
    multitab-label.c menutree.c optsdbus.c roxterm.c roxterm-regex.c search.c
    session-file.c shortcuts.c uri.c)
add_dependencies(roxterm rtlib)
target_include_directories(roxterm PRIVATE
    ${RTCOMMON_INCLUDE_DIRS} ${VTE_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(roxterm PRIVATE
    ${RTCOMMON_CFLAGS_OTHER} ${VTE_CFLAGS_OTHER})
target_link_libraries(roxterm
    ${RTCOMMON_LIBRARIES} ${VTE_LIBRARIES})
target_link_directories(roxterm PRIVATE
    ${RTCOMMON_LIBRARY_DIRS} ${VTE_LIBRARY_DIRS})
target_link_options(roxterm PRIVATE
${RTCOMMON_LDFLAGS_OTHER} ${VTE_LDFLAGS_OTHER})

add_executable(roxterm-config $<TARGET_OBJECTS:rtlib>
    capplet.c colourgui.c configlet.c getname.c optsdbus.c
    profilegui.c shortcuts.c)
add_dependencies(roxterm-config rtlib)
target_include_directories(roxterm-config PRIVATE
    ${RTCOMMON_INCLUDE_DIRS} ${GMODULE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(roxterm-config PRIVATE
    ${RTCOMMON_CFLAGS_OTHER} ${GMODULE_CFLAGS_OTHER})
target_compile_definitions(roxterm-config PRIVATE -DROXTERM_CAPPLET)
target_link_libraries(roxterm-config
    ${RTCOMMON_LIBRARIES} ${GMODULE_LIBRARIES})
target_link_directories(roxterm-config PRIVATE
    ${RTCOMMON_LIBRARY_DIRS} ${GMODULE_LIBRARY_DIRS})
target_link_options(roxterm-config PRIVATE
    ${RTCOMMON_LDFLAGS_OTHER} ${GMODULE_LDFLAGS_OTHER})

install(TARGETS roxterm roxterm-config
    RUNTIME DESTINATION bin)
install(FILES roxterm-config.ui
    DESTINATION share/roxterm)
