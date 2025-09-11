find_program(GETTEXT_XGETTEXT_EXECUTABLE xgettext)
find_program(GETTEXT_MSGMERGE_EXECUTABLE msgmerge)
find_program(GETTEXT_MSGFMT_EXECUTABLE msgfmt)

set(POT_FILE "${CMAKE_CURRENT_BINARY_DIR}/messages.pot")
file(GLOB PO_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.po)
message(TRACE " PO_FILES: ${PO_FILES}")
set(LANGUAGES)
string(REGEX REPLACE "\\.po" "" LANGUAGES "${PO_FILES}")
message(STATUS " LANGUAGES: ${LANGUAGES}")

if (GETTEXT_XGETTEXT_EXECUTABLE)
    file(GLOB_RECURSE POT_SOURCES
        RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        "../src/*.c" "../src/*.h"
    )
    set(POT_UI "../src/roxterm-config.ui")
    set(POT_DEPS ${POT_SOURCES} ${POT_UI})

    set(XGETTEXT_ARGS
        --from-code=UTF-8
        --output=${POT_FILE}
        --package-name=roxterm
        --package-version=${ROXTERM_VERSION}
        --msgid-bugs-address="https://github.com/realh/roxterm/issues"
    )

    # CMake docs claim that the Gettext module creates a target called
    # potfile. This is a lie, rendering the alleged target useless, but
    # we'd better avoid possibly clashing with it.
    add_custom_target(pot
        DEPENDS ${POT_DEPS}
        COMMENT "Regenerating ${POT_FILE}"
    )
    add_custom_command(
        TARGET pot
        PRE_BUILD
        COMMAND
            xgettext
            ${XGETTEXT_ARGS}
            --language=C
            --keyword=_
            ${POT_SOURCES}
        COMMAND
            xgettext
            ${XGETTEXT_ARGS}
            --language=C
            --keyword=N_
            --join-existing
            ${POT_SOURCES}
        COMMAND
            xgettext
            ${XGETTEXT_ARGS}
            --language=Glade
            --join-existing
            ${POT_UI}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif (GETTEXT_XGETTEXT_EXECUTABLE)

if (GETTEXT_MSGMERGE_EXECUTABLE)
    add_custom_target(
        pot-merge
        DEPENDS pot
    )
    foreach(PO_FILE IN ITEMS ${PO_FILES})
        message(TRACE " Enabling msgmerge for: ${PO_FILE}")
        add_custom_command(
            TARGET pot-merge
            PRE_BUILD
            COMMAND
                ${CMAKE_COMMAND} -E echo "Updating ${PO_FILE}"
            COMMAND
                ${GETTEXT_MSGMERGE_EXECUTABLE}
                --update --no-fuzzy-matching --backup=off
                --output-file=${CMAKE_CURRENT_SOURCE_DIR}/${PO_FILE}
                ${CMAKE_CURRENT_SOURCE_DIR}/${PO_FILE}
                ${POT_FILE}
        )
    endforeach()
endif (GETTEXT_MSGMERGE_EXECUTABLE)

if (GETTEXT_MSGFMT_EXECUTABLE)
    add_custom_target(
        po-compile
        ALL
        DEPENDS ${PO_FILES}
    )
    message(TRACE " Enabling msgfmt for ${LANGUAGES}")
    add_custom_command(
        TARGET po-compile
        PRE_BUILD
        COMMAND
        ${CMAKE_COMMAND} -E echo Compiling '${PO_FILES}'
    )
    foreach(LANG IN ITEMS ${LANGUAGES})
        set(MO_DIR "${CMAKE_CURRENT_BINARY_DIR}/${LANG}/LC_MESSAGES")
        set(MO_FILE "${MO_DIR}/roxterm.mo")
        set(PO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${LANG}.po)
        file(MAKE_DIRECTORY ${MO_DIR})
        add_custom_command(
            TARGET po-compile
            PRE_BUILD
            COMMAND
                ${GETTEXT_MSGFMT_EXECUTABLE}
                --output-file=${MO_FILE}
                ${PO_FILE}
            COMMENT
        )
        install(
            FILES "${MO_FILE}"
            DESTINATION
                "${CMAKE_INSTALL_FULL_LOCALEDIR}/${LANG}/LC_MESSAGES"
            COMPONENT
                po
        )
    endforeach()
endif (GETTEXT_MSGFMT_EXECUTABLE)
