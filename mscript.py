#!/usr/bin/env python

import errno, os, sys

from maitch import *

ctx = Context(PACKAGE = "roxterm", SRC_DIR = "${TOP_DIR}/src")


MINILIB_SOURCES = "colourscheme.c dlg.c display.c dragrcv.c dynopts.c " \
        "encodings.c globalopts.c logo.c options.c optsfile.c rtdbus.c"

CONFIG_SOURCES = "capplet.c colourgui.c configlet.c getname.c optsdbus.c " \
        "profilegui.c"

ROXTERM_SOURCES = "about.c main.c multitab.c multitab-close-button.c " \
        "multitab-label.c menutree.c optsdbus.c roxterm.c shortcuts.c " \
        "uri.c x11support.c"


if ctx.mode == 'configure' or ctx.mode == 'help':

    ctx.arg_disable('gtk-native-tab-dragging',
            "Use ROXTerm's legacy code for dragging tabs "
            "instead of GTK+'s functions")
    ctx.arg_with('gnome-default-applications',
            "Where to install GNOME Default Applications file",
            default = None)
    ctx.arg_disable('sm', "Don't enable session management")
    ctx.arg_disable('gtk3', "Use GTK+2 instead of GTK+3 and compatible "
            "version of VTE", default = None)

if ctx.mode == 'configure':

    vfile = ctx.subst(opj("${TOP_DIR}/version"))
    if os.path.exists(ctx.subst(opj("${TOP_DIR}/.git"))):
        version = ctx.prog_output(["git", "describe"])[0].strip()
        fp = open(vfile, 'w')
        fp.write(version + '\n')
        fp.close()
    else:
        fp = open(vfile, 'r')
        version = fp.read().strip()
        fp.close()
    ctx.setenv('VERSION', version)

    try:
        ctx.find_prog_env("xmlto", "XMLTOMAN")
    except MaitchNotFoundError:
        try:
            ctx.find_prog_env("xsltproc", "XMLTOMAN")
        except MaitchNotFoundError:
            sys.stderr.write(
                    "Unable to build manpages without xmlto or xsltproc\n")
            ctx.setenv("XMLTOMAN", "")
        else:
            ctx.setenv("XMLTOMAN_OPTS", "--nonet --novalid " \
                    "--param man.charmap.use.subset 0 " \
                    "http://docbook.sourceforge.net/release/xsl/" \
                    "current/manpages/docbook.xsl")
    else:
        ctx.setenv("XMLTOMAN_OPTS", "")
    
    gda = ctx.env.get("WITH_GNOME_DEFAULT_APPLICATIONS")
    if gda == None or gda == True:
        try:
            gda = ctx.prog_output("${PKG_CONFIG} "
                    "--variable=defappsdir gnome-default-applications")
        except MaitchChildError:
            if gda == True:
                raise
            else:
                gda = ""
    elif gda == False:
        gda = ""
    ctx.setenv("WITH_GNOME_DEFAULT_APPLICATIONS", gda)
        
    gtk3 = ctx.env['ENABLE_GTK3']
    if gtk3 != False:
        try:
            ctx.pkg_config('gtk+-3.0', 'GTK')
            ctx.pkg_config('vte-2.90', 'VTE')
        except MaitchChildError:
            if gtk3 == True:
                raise
            gtk3 = False
        else:
            gtk3 = True
    if not gtk3:
        ctx.pkg_config('gtk+-2.0', 'GTK', '2.18')
        ctx.pkg_config('vte', 'VTE', '0.20')
    
    sm = ctx.env['ENABLE_SM']
    if sm != False:
        try:
            ctx.pkg_config('sm ice', 'SM')
        except MaitchChildError:
            if sm == True:
                raise
            sm = False
        else:
            sm = True
    ctx.define('ENABLE_SM', sm)
    
    ctx.pkg_config('dbus-1', 'DBUS', '1.0')
    ctx.pkg_config('dbus-glib-1', 'DBUS', '0.22')
    
    for f in "get_current_dir_name g_mkdir_with_parents " \
            "gdk_window_get_display gdk_window_get_screen " \
            "gtk_widget_get_realized gtk_widget_get_mapped " \
            "gtk_combo_box_text_new gtk_rc_style_unref".split():
        ctx.check_func(f, "${CFLAGS} ${GTK_CFLAGS}", "${LIBS} ${GTK_LIBS}")
    ctx.check_func("vte_terminal_search_set_gregex",
            "${CFLAGS} ${VTE_CFLAGS}", "${LIBS} ${VTE_LIBS}")
    
    ctx.setenv('CORE_CFLAGS',
            "${CFLAGS} ${GTK_CFLAGS} ${DBUS_CFLAGS}")
    ctx.setenv('CORE_LIBS',
            "${LIBS} ${GTK_LIBS} ${DBUS_LIBS}")
    ctx.setenv('ROXTERM_CFLAGS',
            "${CFLAGS} ${VTE_CFLAGS} ${SM_CFLAGS} ${DBUS_CFLAGS}")
    ctx.setenv('ROXTERM_LIBS',
            "${LIBS} ${VTE_LIBS} ${SM_LIBS} ${DBUS_LIBS}")
    ctx.setenv('ROXTERM_CONFIG_CFLAGS',
            "${CFLAGS} ${GTK_CFLAGS} ${DBUS_CFLAGS}")
    ctx.setenv('ROXTERM_CONFIG_LIBS',
            "${LIBS} ${GTK_LIBS} ${DBUS_LIBS}")
    
    ctx.define('DO_OWN_TAB_DRAGGING',
            ctx.env.get('ENABLE_GTK_NATIVE_TAB_DRAGGING', True))
    ctx.define('SYS_CONF_DIR', ctx.env['SYSCONFDIR'])
    ctx.define('DATA_DIR', ctx.env['DATADIR'])
    ctx.define('PKG_DATA_DIR', opj(ctx.env['DATADIR'], "roxterm"))
    ctx.define('ICON_DIR',
            opj(ctx.env['DATADIR'], "icons", "hicolor", "scalable", "apps"))
    ctx.define('LOCALE_DIR', opj(ctx.env['DATADIR'], "locale"))

elif ctx.mode == 'build':

    # Private library
    ctx.add_rule(maitch.StaticLibCRule(
            cflags = "${CORE_CFLAGS}",
            prefix = "libroxterm-",
            quiet = True))
    ctx.add_rule(maitch.StaticLibRule(
            sources = change_suffix_with_prefix(MINILIB_SOURCES,
                    ".c", ".lo", "libroxterm-")
            targets = "libroxterm.la",
            cflags = "${CORE_CFLAGS}",
            libs = "${CORE_LIBS}",
            quiet = True))
    
    # roxterm
    if ctx.env['ENABLE_SM']:
        ROXTERM_SOURCES += " session.c"
    if ctx.env['HAVE_VTE_TERMINAL_SEARCH_SET_GREGEX']:
        ROXTERM_SOURCES += " search.c"
    ctx.add_rule(maitch.CRule(
            cflags = "${ROXTERM_CFLAGS}",
            prefix = "roxterm-"))
    ctx.add_rule(maitch.LibtoolProgramRule(
            sources = change_suffix_with_prefix(ROXTERM_SOURCES,
                    ".c", ".o", "roxterm-")
            targets = "roxterm",
            cflags = "${ROXTERM_CFLAGS}",
            libs = "${ROXTERM_LIBS} -llibroxterm.la",
            quiet = True))

    # roxterm-config
    ctx.add_rule(maitch.CRule(
            cflags = "${ROXTERM_CONFIG_CFLAGS}",
            prefix = "roxterm-config-"))
    ctx.add_rule(maitch.LibtoolProgramRule(
            sources = change_suffix_with_prefix(ROXTERM_CONFIG_SOURCES,
                    ".c", ".o", "roxterm-config-")
            targets = "roxterm-config",
            cflags = "${ROXTERM_CONFIG_CFLAGS}",
            libs = "${ROXTERM_CONFIG_LIBS} -llibroxterm.la",
            quiet = True))

    

ctx.run()
sys.exit(0)
    
            

if ctx.mode == 'build':

    my_builddir = opj(ctx.build_dir, "libhdvbd")
    try:
        os.mkdir(my_builddir)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise
    try:
        os.symlink(".", opj(my_builddir, "hdvb"))
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise

    # gudev flags/libs include gobject's
    cflags = "${CFLAGS} ${SQLITE_CFLAGS} ${GUDEV_CFLAGS} " + \
            mcommon.hdvbd_includes
    libs = "${LIBS} ${SQLITE_LIBS} ${GUDEV_LIBS}"
    
    # Gobs
    gobs = ctx.glob_src("*.gob", "libhdvbd")
    c_from_gobs = maitch.change_suffix(gobs, ".gob", ".c")
    # How to compile a gob
    ctx.add_rule(maitch.SuffixRule(rule = "${GOB2} -o libhdvbd ${SRC}",
            sources = ".gob", targets = ".c .h -private.h"))
    
    c_files = ctx.glob_src("*.c", "libhdvbd") + c_from_gobs
    o_files = maitch.add_prefix(
            maitch.change_suffix(c_files, ".c", ".lo"), "libhdvbd-")
    
    # How to compile the C to .lo for a shlib. Note how wdeps forces all
    # files built by gob2 to be built first
    ctx.add_rule(maitch.ShlibCRule(
            cflags = cflags,
            prefix = "libhdvbd-",
            wdeps = c_from_gobs,
            quiet = True))
    
    # The actual library
    ctx.add_rule(maitch.ShlibRule(
            sources = o_files,
            targets = "libhdvbd/libhdvbd.la",
            cflags = cflags,
            libs = libs,
            quiet = True))
        


if __name__ == '__main__':
    ctx = mcommon.init(PACKAGE = "libhdvbd",
            BUILD_DIR = opj("${MSCRIPT_REAL_DIR}", os.pardir, "build"))
    init(ctx)
    ctx.run()

