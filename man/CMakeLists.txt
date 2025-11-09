find_program(XSLTPROC xsltproc)
if (NOT "${XSLTPROC}" EQUAL "XSLTPROC-NOTFOUND")
    set(XSLTPROC_OPTIONS --novalid --param man.charmap.use.subset 0
        http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl)

    set(ALL_MANPAGES)
    set(LANGS "" pt_BR)
    set(MAN_NAMES roxterm roxterm-config)

    foreach(LANG ${LANGS})
        set(MANPAGES_FOR_LANG)
        if(LANG)
            set(LANG_PATH "${LANG}/")
            set(INSTALL_DEST "${CMAKE_INSTALL_MANDIR}/${LANG}/man1")
        else()
            set(LANG_PATH "")
            set(INSTALL_DEST "${CMAKE_INSTALL_MANDIR}/man1")
        endif()

        foreach(MAN_NAME ${MAN_NAMES})
            set(XML_IN "${CMAKE_CURRENT_SOURCE_DIR}/${LANG_PATH}${MAN_NAME}.1.xml.in")
            set(XML_OUT "${CMAKE_CURRENT_BINARY_DIR}/${LANG_PATH}${MAN_NAME}.1.xml")
            set(MAN_OUT_REL "${LANG_PATH}${MAN_NAME}.1")
            set(MAN_OUT_ABS "${CMAKE_CURRENT_BINARY_DIR}/${MAN_OUT_REL}")

            configure_file("${XML_IN}" "${XML_OUT}")

            add_custom_command(OUTPUT "${MAN_OUT_REL}"
                COMMAND ${XSLTPROC} -o "${MAN_OUT_REL}" ${XSLTPROC_OPTIONS}
                    "${XML_OUT}"
                MAIN_DEPENDENCY "${XML_OUT}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
            list(APPEND MANPAGES_FOR_LANG "${MAN_OUT_ABS}")
        endforeach()

        install(FILES ${MANPAGES_FOR_LANG} DESTINATION "${INSTALL_DEST}")
        list(APPEND ALL_MANPAGES ${MANPAGES_FOR_LANG})
    endforeach()

    add_custom_target(manpages ALL DEPENDS ${ALL_MANPAGES})
else ()
    message(WARNING "xsltproc not found, man pages will not be generated")
endif ()
