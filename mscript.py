#!/usr/bin/env python

import errno, os, re, sys, time

from maitch import *

ctx = Context(PACKAGE = "roxterm", SRC_DIR = "${TOP_DIR}/src",
        CFLAGS = "-O2 -g -Wall -I. -I${SRC_DIR} -D_GNU_SOURCE -DHAVE_CONFIG_H")


MINILIB_SOURCES = "colourscheme.c dlg.c display.c dragrcv.c dynopts.c " \
        "encodings.c globalopts.c logo.c options.c optsfile.c rtdbus.c"

ROXTERM_CONFIG_SOURCES = "capplet.c colourgui.c configlet.c getname.c " \
        "optsdbus.c profilegui.c"

ROXTERM_SOURCES = "about.c main.c multitab.c multitab-close-button.c " \
        "multitab-label.c menutree.c optsdbus.c roxterm.c shortcuts.c " \
        "uri.c x11support.c"

LOGO_PNG = "${TOP_DIR}/Help/lib/roxterm_logo.png"
FAVICON = "${TOP_DIR}/Help/lib/favicon.ico"
TEXT_LOGO = "${TOP_DIR}/Help/lib/logo_text.png"
DCH = "${TOP_DIR}/debian/changelog"
APPINFO = "${TOP_DIR}/AppInfo.xml"
VFILE = "${TOP_DIR}/version"


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

    git = os.path.exists(ctx.subst(opj("${TOP_DIR}", ".git")))
    try:
        ctx.find_prog_env("git")
    except MaitchNotFoundError:
        git = False
    vfile = ctx.subst(VFILE)
    if git:
        # Might have an obsolete version.h from earlier build
        ctx.delete("${SRC_DIR}/version.h")
        version = ctx.prog_output(["${GIT}", "describe"])[0].strip()
        version = version.replace('-', '.', 1).replace('-', '~', 1)
        ctx.save_if_different(vfile, version + '\n')
        gitlog = ctx.prog_output(["${GIT}", "log"])[0]
        fp = open(ctx.subst("${TOP_DIR}/ChangeLog.old"), 'r')
        gitlog += fp.read()
        fp.close()
        # Can't use ctx.save_if_different because it tries to subst
        # content and fails
        save_if_different(ctx.subst("${TOP_DIR}/ChangeLog"), gitlog)
    else:
        fp = open(vfile, 'r')
        version = fp.read().strip()
        fp.close()
    ctx.setenv('VERSION', version)

    try:
        ctx.find_prog_env("xsltproc", "XMLTOMAN")
    except MaitchNotFoundError:
        mprint("Unable to build manpages without xsltproc", file = sys.stderr)
        ctx.setenv("XMLTOMAN", "")
    else:
        ctx.setenv("XMLTOMAN_OPTS", "--nonet --novalid " \
                "--param man.charmap.use.subset 0 " \
                "http://docbook.sourceforge.net/release/xsl/" \
                "current/manpages/docbook.xsl")
        ctx.setenv("XMLTOMAN_OUTPUT", "")
    
    ctx.find_prog_env("convert")
    ctx.find_prog_env("composite")
    
    try:
        ctx.find_prog_env("xgettext")
        ctx.find_prog_env("msgmerge")
    except MaitchNotFoundError:
        pass
    
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
    ctx.pkg_config('gmodule-export-2.0', 'GMODULE')
    
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
            "${CFLAGS} ${GTK_CFLAGS} ${DBUS_CFLAGS} ${GMODULE_CFLAGS} "
            "-DROXTERM_CAPPLET")
    ctx.setenv('ROXTERM_CONFIG_LIBS',
            "${LIBS} ${GTK_LIBS} ${DBUS_LIBS} ${GMODULE_LIBS}")
    
    ctx.define_from_var('PACKAGE')
    ctx.define('DO_OWN_TAB_DRAGGING',
            ctx.env.get('ENABLE_GTK_NATIVE_TAB_DRAGGING', True))
    ctx.define('SYS_CONF_DIR', ctx.env['SYSCONFDIR'])
    ctx.define('DATA_DIR', ctx.env['DATADIR'])
    ctx.define('PKG_DATA_DIR', opj(ctx.env['DATADIR'], "roxterm"))
    ctx.define('ICON_DIR',
            opj(ctx.env['DATADIR'], "icons", "hicolor", "scalable", "apps"))
    ctx.define('LOCALE_DIR', opj(ctx.env['DATADIR'], "locale"))
    ctx.define('HTML_DIR', ctx.env['HTMLDIR'])
    ctx.define('BIN_DIR', ctx.env['BINDIR'])
    
    ctx.subst_file("${TOP_DIR}/roxterm.1.xml.in", "roxterm.1.xml")
    ctx.subst_file("${TOP_DIR}/roxterm-config.1.xml.in", "roxterm-config.1.xml")
    ctx.setenv('APPINFO_STRING', "${VERSION} (%s)" % \
            time.strftime("%Y-%m-%d", time.gmtime(time.time())))
    ctx.subst_file(APPINFO + ".in", APPINFO)
    ctx.save_if_different("version.h",
            '/* Auto-generated by mscript.py */\n' \
            '#ifndef VERSION_H\n' \
            '#define VERSION_H\n' \
            '#define VERSION "${VERSION}"\n' \
            '#endif\n')
    
    src = DCH + ".in"
    if os.path.exists(ctx.subst(src)):
        ctx.subst_file(src, DCH, at = True)

elif ctx.mode == 'build':

    # Private library
    ctx.add_rule(StaticLibCRule(
            cflags = "${CORE_CFLAGS}",
            prefix = "libroxterm-",
            quiet = True))
    ctx.add_rule(StaticLibRule(
            sources = change_suffix_with_prefix(MINILIB_SOURCES,
                    ".c", ".lo", "libroxterm-"),
            targets = "libroxterm.la",
            cflags = "${CORE_CFLAGS}",
            libs = "${CORE_LIBS}",
            quiet = True))
    
    # roxterm
    if bool(ctx.env['ENABLE_SM']):
        ROXTERM_SOURCES += " session.c"
    if bool(ctx.env['HAVE_VTE_TERMINAL_SEARCH_SET_GREGEX']):
        ROXTERM_SOURCES += " search.c"
    ctx.add_rule(CRule(
            cflags = "${ROXTERM_CFLAGS}",
            prefix = "roxterm-",
            wdeps = "version.h"))
    ctx.add_rule(LibtoolProgramRule(
            sources = change_suffix_with_prefix(ROXTERM_SOURCES,
                    ".c", ".o", "roxterm-"),
            targets = "roxterm",
            cflags = "${ROXTERM_CFLAGS}",
            libs = "${ROXTERM_LIBS} -lroxterm",
            deps = "libroxterm.la",
            quiet = True))

    # roxterm-config
    ctx.add_rule(CRule(
            cflags = "${ROXTERM_CONFIG_CFLAGS}",
            prefix = "roxterm-config-"))
    ctx.add_rule(LibtoolProgramRule(
            sources = change_suffix_with_prefix(ROXTERM_CONFIG_SOURCES,
                    ".c", ".o", "roxterm-config-"),
            targets = "roxterm-config",
            cflags = "${ROXTERM_CONFIG_CFLAGS}",
            libs = "${ROXTERM_CONFIG_LIBS} -lroxterm",
            deps = "libroxterm.la",
            quiet = True))
    
    # Stuff other than the program
    
    # Graphics
    ctx.add_rule(Rule(rule = "${CONVERT} -background #0000 " \
            "${SRC} -geometry 64x64 ${TGT}",
            targets = LOGO_PNG,
            sources = "roxterm.svg",
            where = TOP))
            
    # Note 'where' is NOWHERE for following two rules because sources already
    # start with ${TOP_DIR}.
    ctx.add_rule(Rule(rule = "${CONVERT} ${SRC} -geometry 16x16 ${TGT}",
            targets = FAVICON,
            sources = LOGO_PNG,
            where = NOWHERE))
    
    ctx.add_rule(Rule(rule = "${COMPOSITE} -gravity SouthWest ${SRC} ${TGT}",
            targets = TEXT_LOGO,
            sources = [LOGO_PNG, "${TOP_DIR}/Help/lib/logo_text_only.png"],
            where = NOWHERE))
    
    # man pages
    if ctx.env['XMLTOMAN']:
        ctx.add_rule(SuffixRule(
                rule = "${XMLTOMAN} ${XMLTOMAN_OPTS} ${SRC} ${XMLTOMAN_OUTPUT}",
                targets = ".1",
                sources = ".1.xml",
                where = TOP))
        # Force invocation of above suffix rule
        ctx.add_rule(TouchRule(
                targets = "manpages",
                sources = "roxterm.1 roxterm-config.1"))
    
    # Make sure .ui file is GTK2-compatible
    def process_ui(ctx, env, targets, sources):
        fp = open(sources[0], 'r')
        s = fp.read()
        fp.close()
        s = re.sub(r'class="GtkBox" id=(.*)hbox',
                r'class="GtkHBox" id=\1hbox', s)
        s = re.sub(r'class="GtkBox" id=(.*)vbox',
                r'class="GtkVBox" id=\1vbox', s)
        save_if_different(sources[0], s)
        fp = open(targets[0], 'w')
        fp.close()
    ctx.add_rule(Rule(rule = process_ui,
            sources = "${SRC_DIR}/roxterm-config.ui",
            targets = "roxterm-config.ui-stamp"))
    
    # Translations
    if ctx.env.get('XGETTEXT') and ctx.env.get('MSGMERGE'):
        trans_rules = StandardTranslationRules(ctx,
                copyright_holder = "(c) 2011 Tony Houghton",
                version = "'${VERSION}'",
                bugs_addr = "'http://sourceforge.net/tracker/?group_id=124080'",
                use_shell = True)
        for r in trans_rules:
            ctx.add_rule(r)

elif ctx.mode == "install" or ctx.mode == "uninstall":

    ctx.install_bin("roxterm roxterm-config")
    ctx.install_data("roxterm-config.ui")
    ctx.install_data("roxterm.desktop", "${DATADIR}/applications")
    if ctx.env['XMLTOMAN']:
        ctx.install_man("roxterm.1 roxterm-config.1")
    ctx.install_doc("AUTHORS ChangeLog COPYING README")
    ctx.install_doc(ctx.glob("*", subdir = "${TOP_DIR}/Help/en"),
            "${HTMLDIR}/en")
    ctx.install_doc(ctx.glob("*", subdir = "${TOP_DIR}/Help/lib"),
            "${HTMLDIR}/lib")
    ctx.install_data("roxterm.svg", "${DATADIR}/icons/hicolor/scalable/apps")
    ctx.install_data(["Config/Colours/Tango", "Config/Colours/GTK"],
            "${PKGDATADIR}/Config/Colours")
    ctx.install_data("Config/Shortcuts/Default",
            "${PKGDATADIR}/Config/Shortcuts")
    gda = ctx.env['WITH_GNOME_DEFAULT_APPLICATIONS']
    if gda:
        ctx.install_data("roxterm.xml", gda)

elif ctx.mode == 'distclean' or ctx.mode == 'clean':
    
    clean = [LOGO_PNG, TEXT_LOGO, FAVICON, APPINFO]
    if ctx.mode == 'distclean':
        clean += [VFILE, DCH]
    for f in clean:
        ctx.delete(f)

elif ctx.mode == 'dist':
    
    ctx.add_dist("AUTHORS builddeb " \
            "ChangeLog ChangeLog.old Config " \
            "COPYING debian Help/en Help/lib/header.png " \
            "Help/lib/logo_text_only.png " \
            "Help/lib/roxterm.css Help/lib/roxterm_ie.css "
            "Help/lib/sprites.png " \
            "INSTALL INSTALL.Debian " \
            "NEWS README README.translations " \
            "roxterm.1.xml.in roxterm-config.1.xml.in " \
            "roxterm.desktop roxterm.lsm.in roxterm.spec.in " \
            "roxterm.svg roxterm.xml src TODO update-locales")
    ctx.add_dist(ctx.glob("*.[c|h]", os.curdir, "src"))
    ctx.add_dist("src/roxterm-config.ui src/roxterm-config.glade")
    # Debian files
    ctx.add_dist("builddeb")
    debfiles = "changelog changelog.in compat control copyright " \
            "dirs roxterm-common.doc-base " \
            "roxterm-common.install roxterm-gtk2.install " \
            "roxterm-gtk2.lintian-overrides roxterm-gtk2.menu " \
            "roxterm-gtk2.postinst roxterm-gtk2.prerm roxterm-gtk3.install " \
            "roxterm-gtk3.lintian-overrides roxterm-gtk3.menu " \
            "roxterm-gtk3.postinst roxterm-gtk3.prerm roxterm.xpm rules " \
            "source/format watch"
    ctx.add_dist(["debian/" + f for f in debfiles.split()])
    # maitch-specific
    ctx.add_dist("version maitch.py mscript.py")
    # ROX bits
    ctx.add_dist("AppInfo.xml.in AppRun .DirIcon " \
            "Help/Changes Help/COPYING Help/NEWS Help/README")
    # Translations (currently not properly supported)
    ctx.add_dist("ABOUT-NLS po po4a")
    # Stuff to be removed when we no longer support autotools
    ctx.add_dist("acinclude.m4 bootstrap.sh configure.ac.in " \
            "Makefile.am src/Makefile.am")
    try:
        ctx.add_dist("m4 acinclude.m4 aclocal.m4 " \
                "bootstrap.sh compile config.guess config.h.in " \
                "config.sub config.rpath configure configure.ac " \
                "depcomp install-sh intl ltmain.sh " \
                "Makefile.in src/Makefile.in missing")
    except:
        pass
            

ctx.run()

if ctx.mode == 'uninstall':
    ctx.prune_directory("${DATADIR}/icons")
    ctx.prune_directory("${PKGDATADIR}")
    ctx.prune_directory("${DOCDIR}")
    ctx.prune_directory("${HTMLDIR}")
