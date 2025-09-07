configure_file(${PROJECT_SOURCE_DIR}/roxterm.1.xml.in
    ${PROJECT_BINARY_DIR}/roxterm.1.xml)
configure_file(${PROJECT_SOURCE_DIR}/roxterm-config.1.xml.in
    ${PROJECT_BINARY_DIR}/roxterm-config.1.xml)

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
