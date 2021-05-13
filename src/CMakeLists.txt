include(CheckSymbolExists)

# roxterm and roxterm-config both depend on these
pkg_check_modules(RTCOMMON REQUIRED
    gtk+-3.0>=3.22.0 dbus-1>=1.0 dbus-glib-1>=0.22)

# roxterm-config needs to export symbols so GtkBuilder can find signal handlers
pkg_check_modules(GMODULE_EXPORT REQUIRED gmodule-export-2.0)

# If VTE >= 0.50 this should include the necessary flags for libpcre
pkg_check_modules(VTE REQUIRED vte-2.91>=0.52)

set(RTLIB_CFLAGS ${RTCOMMON_CFLAGS})
if(RTLIB_CFLAG MATCHES ";")
    list(REMOVE_DUPLICATES RTLIB_CFLAG)
endif()
set(RTLIB_CFLAGS_OTHER ${RTCOMMON_CFLAGS_OTHER})
if(RTLIB_CFLAGS_OTHER MATCHES ";")
    list(REMOVE_DUPLICATES RTLIB_CFLAGS_OTHER)
endif()
set(RTLIB_INCLUDE_DIRS ${RTCOMMON_INCLUDE_DIRS})
if(RTLIB_INCLUDE_DIRS MATCHES ";")
    list(REMOVE_DUPLICATES RTLIB_INCLUDE_DIRS)
endif()
set(RTLIB_LIBRARIES ${RTCOMMON_LIBRARIES})
if(RTLIB_LIBRARIES MATCHES ";")
    list(REMOVE_DUPLICATES RTLIB_LIBRARIES)
endif()
set(RTLIB_LIBRARY_DIRS ${RTCOMMON_LIBRARY_DIRS})
if(RTLIB_LIBRARY_DIRS MATCHES ";")
    list(REMOVE_DUPLICATES RTLIB_LIBRARY_DIRS)
endif()
set(RTLIB_LDFLAGS_OTHER ${RTCOMMON_DFLAGS_OTHER})
if(RTLIB_LDFLAGS_OTHER MATCHES ";")
    list(REMOVE_DUPLICATES RTLIB_LDFLAGS_OTHER)
endif()

set(RTMAIN_CFLAGS ${RTCOMMON_CFLAGS} ${VTE_CFLAGS})
if(RTMAIN_CFLAGS MATCHES ";")
    list(REMOVE_DUPLICATES RTMAIN_CFLAGS)
endif()
set(RTMAIN_CFLAGS_OTHER ${RTCOMMON_CFLAGS_OTHER} ${VTE_CFLAGS_OTHER})
if(RTMAIN_CFLAGS_OTHER MATCHES ";")
    list(REMOVE_DUPLICATES RTMAIN_CFLAGS_OTHER)
endif()
set(RTMAIN_INCLUDE_DIRS ${RTCOMMON_INCLUDE_DIRS} ${VTE_INCLUDE_DIRS})
if(RTMAIN_INCLUDE_DIRS MATCHES ";")
    list(REMOVE_DUPLICATES RTMAIN_INCLUDE_DIRS)
endif()
set(RTMAIN_LIBRARIES ${RTCOMMON_LIBRARIES} ${VTE_LIBRARIES})
if(RTMAIN_LIBRARIES MATCHES ";")
    list(REMOVE_DUPLICATES RTMAIN_LIBRARIES)
endif()
set(RTMAIN_LIBRARY_DIRS ${RTCOMMON_LIBRARY_DIRS} ${VTE_LIBRARY_DIRS})
if(RTMAIN_LIBRARY_DIRS MATCHES ";")
    list(REMOVE_DUPLICATES RTMAIN_LIBRARY_DIRS)
endif()
set(RTMAIN_LDFLAGS_OTHER ${RTCOMMON_DFLAGS_OTHER} ${VTE_DFLAGS_OTHER})
if(RTMAIN_LDFLAGS_OTHER MATCHES ";")
    list(REMOVE_DUPLICATES RTMAIN_LDFLAGS_OTHER)
endif()

set(RTCONFIG_CFLAGS ${RTCOMMON_CFLAGS} ${GMODULE_EXPORT_CFLAGS})
if(RTCONFIG_CFLAGS MATCHES ";")
    list(REMOVE_DUPLICATES RTCONFIG_CFLAGS)
endif()
set(RTCONFIG_CFLAGS_OTHER ${RTCOMMON_CFLAGS_OTHER}
    ${GMODULE_EXPORT_CFLAGS_OTHER})
if(RTCONFIG_CFLAGS_OTHER MATCHES ";")
    list(REMOVE_DUPLICATES RTCONFIG_CFLAGS_OTHER)
endif()
set(RTCONFIG_INCLUDE_DIRS ${RTCOMMON_INCLUDE_DIRS}
    ${GMODULE_EXPORT_INCLUDE_DIRS})
if(RTCONFIG_INCLUDE_DIRS MATCHES ";")
    list(REMOVE_DUPLICATES RTCONFIG_INCLUDE_DIRS)
endif()
set(RTCONFIG_LIBRARIES ${RTCOMMON_LIBRARIES}
    ${GMODULE_EXPORT_LIBRARIES})
if(RTCONFIG_LIBRARIES MATCHES ";")
    list(REMOVE_DUPLICATES RTCONFIG_LIBRARIES)
endif()
set(RTCONFIG_LIBRARY_DIRS ${RTCOMMON_LIBRARY_DIRS}
    ${GMODULE_EXPORT_LIBRARY_DIRS})
if(RTCONFIG_LIBRARY_DIRS MATCHES ";")
    list(REMOVE_DUPLICATES RTCONFIG_LIBRARY_DIRS)
endif()
set(RTCONFIG_LDFLAGS_OTHER ${RTCOMMON_DFLAGS_OTHER}
    ${GMODULE_EXPORT_DFLAGS_OTHER})
if(RTCONFIG_LDFLAGS_OTHER MATCHES ";")
    list(REMOVE_DUPLICATES RTCONFIG_LDFLAGS_OTHER)
endif()

# set(CMAKE_REQUIRED_FLAGS "${VTE_CFLAGS_OTHER}")
# set(CMAKE_REQUIRED_INCLUDES "${VTE_INCLUDE_DIRS}")
# set(CMAKE_REQUIRED_LIBRARIES "${VTE_LIBRARIES}")
# set(CMAKE_REQUIRED_LINK_OPTIONS "${VTE_LDFLAGS}")
# check_symbol_exists(vte_terminal_set_handle_scroll vte/vte.h
#     HAVE_VTE_HANDLE_SCROLL)
pkg_get_variable(RT_VTE_LIBDIR vte-2.91 libdir)

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
    ${RTLIB_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(rtlib PRIVATE ${RTLIB_CFLAGS_OTHER})

add_executable(roxterm $<TARGET_OBJECTS:rtlib>
    about.c main.c multitab.c multitab-close-button.c
    multitab-label.c menutree.c optsdbus.c roxterm.c roxterm-regex.c search.c
    session-file.c shortcuts.c uri.c)
add_dependencies(roxterm rtlib)
target_include_directories(roxterm PRIVATE
    ${RTMAIN_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(roxterm PRIVATE ${RTMAIN_CFLAGS_OTHER})
target_link_libraries(roxterm ${RTMAIN_LIBRARIES})
target_link_directories(roxterm PRIVATE ${RTMAIN_LIBRARY_DIRS})
target_link_options(roxterm PRIVATE ${RTMAIN_LDFLAGS_OTHER})

add_executable(roxterm-config $<TARGET_OBJECTS:rtlib>
    capplet.c colourgui.c configlet.c getname.c optsdbus.c
    profilegui.c shortcuts.c)
add_dependencies(roxterm-config rtlib)
target_include_directories(roxterm-config PRIVATE
    ${RTCONFIG_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(roxterm-config PRIVATE ${RTCONFIG_CFLAGS_OTHER})
target_compile_definitions(roxterm-config PRIVATE -DROXTERM_CAPPLET)
target_link_libraries(roxterm-config ${RTCONFIG_LIBRARIES})
target_link_directories(roxterm-config PRIVATE ${RTCONFIG_LIBRARY_DIRS})
target_link_options(roxterm-config PRIVATE ${RTCONFIG_LDFLAGS_OTHER})

install(TARGETS roxterm roxterm-config
    RUNTIME DESTINATION bin)
install(FILES roxterm-config.ui
    DESTINATION share/roxterm)
