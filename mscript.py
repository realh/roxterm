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

ROXTERM_HTML_BASENAMES = "guide index installation news".split()

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
    ctx.arg_disable('gettext', "Disable translation of program strings with " \
            "gettext", default = None)
    ctx.arg_disable('po4a', "Disable translation of documentation with po4a",
            default = None)
    ctx.arg_disable('translations', "Disable all translations", default = None)

if ctx.mode == 'configure':

    ctx.find_prog_env("sed")

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
        gitlog = ctx.prog_output([os.path.abspath(
                ctx.subst("${TOP_DIR}/genlog"))])[0].lstrip()
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
        ctx.setenv("XMLTOMAN_OPTS", "-o ${TGT} --nonet --novalid " \
                "--param man.charmap.use.subset 0 " \
                "http://docbook.sourceforge.net/release/xsl/" \
                "current/manpages/docbook.xsl")
        ctx.setenv("XMLTOMAN_OUTPUT", "")
    
    ctx.find_prog_env("convert")
    ctx.find_prog_env("composite")
    
    ctx.setenv('BUG_TRACKER', "http://sourceforge.net/tracker/?group_id=124080")
    
    gt = ctx.env['ENABLE_GETTEXT']
    po4a = ctx.env['ENABLE_PO4A']
    trans = ctx.env['ENABLE_TRANSLATIONS']
    if trans != False and gt != False:
        try:
            ctx.find_prog_env("xgettext")
            ctx.find_prog_env("msgmerge")
            ctx.find_prog_env("msgfmt")
        except MaitchNotFoundError:
            if trans == True or gt == True:
                raise
            else:
                mprint("WARNING: gettext tools not found, not building " \
                        "programs' translations", file = sys.stderr)
                ctx.setenv('HAVE_GETTEXT', False)
        else:
            ctx.setenv('HAVE_GETTEXT', True)
    else:
        ctx.setenv('HAVE_GETTEXT', False)
    
    if trans != False and po4a != False:
        try:
            ctx.find_prog_env("po4a-gettextize")
            ctx.find_prog_env("po4a-updatepo")
            ctx.find_prog_env("po4a-translate")
        except MaitchNotFoundError:
            if trans == True or po4a == True:
                raise
            else:
                    mprint("WARNING: po4a tools not found, not building " \
                            "documentation's translations", file = sys.stderr)
                    ctx.setenv('HAVE_PO4A', False)
        else:
            ctx.setenv('HAVE_PO4A', True)
            ctx.setenv('PO4ACHARSET', "-M UTF-8")
            ctx.setenv('PO4AOPTS', "${PO4ACHARSET} --package-name=${PACKAGE} " \
                    "--package-version=${VERSION} " \
                    "--copyright-holder='Tony Houghton' " \
                    "--msgid-bugs-address=${BUG_TRACKER}")
            ctx.setenv('PO4ADIR', "${TOP_DIR}/po4a")
    else:
        ctx.setenv('HAVE_PO4A', False)
    
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
    ctx.setenv('htmldir', "${HTMLDIR}")
    ctx.define('BIN_DIR', ctx.env['BINDIR'])
    
    ctx.subst_file("${TOP_DIR}/roxterm.1.xml.in", "roxterm.1.xml", True)
    ctx.subst_file("${TOP_DIR}/roxterm-config.1.xml.in", "roxterm-config.1.xml",
            True)
    ctx.subst_file("${TOP_DIR}/roxterm.spec.in", "roxterm.spec")
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
        xmltomanrule = "${XMLTOMAN} ${XMLTOMAN_OPTS} ${SRC} ${XMLTOMAN_OUTPUT}"
        # Something is causing a thread to hang between calling xsltproc
        # and exiting from subprocess.Popen. Could it be trying to run two
        # at once. Making one wdep on the other should stop jobs overlapping.
        ctx.add_rule(Rule(
                rule = xmltomanrule,
                targets = "roxterm.1",
                sources = "roxterm.1.xml",
                where = TOP))
        ctx.add_rule(Rule(
                rule = xmltomanrule,
                targets = "roxterm-config.1",
                sources = "roxterm-config.1.xml",
                wdeps = "roxterm.1",
                where = TOP))
        #ctx.add_rule(SuffixRule(
        #        rule = xmltomanrule,
        #        targets = ".1",
        #        sources = ".1.xml",
        #        where = TOP))
        # Force invocation of above suffix rule
        #ctx.add_rule(TouchRule(
        #        targets = "manpages",
        #        sources = "roxterm.1 roxterm-config.1"))
    
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
    
    # Translations (gettext)
    if ctx.env['HAVE_GETTEXT']:
        trans_rules = StandardTranslationRules(ctx,
                copyright_holder = "(c) 2011 Tony Houghton",
                version = "${VERSION}",
                bugs_addr = "${BUG_TRACKER}",
                diffpat = '^"Project-Id-Version:',
                use_shell = True)
        for r in trans_rules:
            ctx.add_rule(r)
    
    # Translations (po4a)
    linguas = parse_linguas(ctx)
    charset_rule = "${SED} -i s/charset=CHARSET/charset=UTF-8/ ${TGT}"
    if ctx.env['HAVE_PO4A']:
        ctx.ensure_out_dir("po4a")
        if ctx.env['XMLTOMAN']:
            lastmtarget = None
            for m in ["roxterm", "roxterm-config"]:
                ctx.add_rule(Rule(rule = ["${PO4A_GETTEXTIZE} ${PO4AOPTS} " \
                        "-f docbook -m ${SRC} -p ${TGT}",
                        charset_rule],
                        sources = "${TOP_DIR}/%s.1.xml.in" % m,
                        targets = "${PO4ADIR}/%s.1.pot" % m,
                        where = NOWHERE,
                        diffpat = ['^"Project-Id-Version:',
                                '^"POT-Creation-Date:'],
                        use_shell = True))
                for l in linguas:
                    ctx.add_rule(Rule(rule = ["${PO4A_UPDATEPO} ${PO4AOPTS} " \
                            "-f docbook -m ${SRC} -p ${TGT}",
                            charset_rule],
                            sources = ["${TOP_DIR}/%s.1.xml.in" % m,
                                    "${PO4ADIR}/%s.1.pot" % m],
                            targets = "${PO4ADIR}/%s.1.%s.po" % (m, l),
                            diffpat = '^"POT-Creation-Date:',
                            where = NOWHERE,
                            use_shell = True))
                    ctx.add_rule(Rule(rule = "${PO4A_TRANSLATE} "
                            "${PO4ACHARSET} " \
                            "-k 0 -f docbook -m ${TOP_DIR}/%s.1.xml.in " \
                            "-p ${SRC} -l ${TGT}" % m,
                            sources = "${PO4ADIR}/%s.1.%s.po" % (m, l),
                            targets = "po4a/%s.1.%s.xml.in" % (m, l),
                            where = NOWHERE,
                            use_shell = True))
                    ctx.add_rule(Rule(rule = "${SED} " \
                            "'s/@VERSION@/${VERSION}/; " \
                            "s#@htmldir@#${HTMLDIR}#' <${SRC} >${TGT}",
                            sources = "po4a/%s.1.%s.xml.in" % (m, l),
                            targets = "po4a/%s.1.%s.xml" % (m, l),
                            where = NOWHERE,
                            use_shell = True))
                    mtarget = "po4a/%s/%s.1" % (l, m)
                    ctx.add_rule(Rule( \
                            rule = [mkdir_rule, xmltomanrule],
                            sources = "po4a/%s.1.%s.xml" % (m, l),
                            targets = mtarget,
                            wdeps = lastmtarget,
                            where = NOWHERE))
                    lastmtarget = mtarget
        for h in ROXTERM_HTML_BASENAMES:
            master = "${TOP_DIR}/Help/en/%s.html" % h
            pot = "${PO4ADIR}/%s.html.pot" % h
            ctx.add_rule(Rule(rule = ["${PO4A_GETTEXTIZE} ${PO4AOPTS} " \
                    "-f xhtml -m ${SRC} -p ${TGT}",
                    charset_rule,
                    "${SED} -i 's/SOME DESCRIPTIVE TITLE/" \
                            "Translations for roxterm docs/' ${TGT}",
                    "${SED} -i 's/Copyright (C) YEAR/Copyright (C) 2010/' " \
                            "${TGT}",
                    "${SED} -i 's/FIRST AUTHOR <EMAIL@ADDRESS>, YEAR/"
                            "Tony Houghton <h@realh.co.uk>, 2010/' ${TGT}"],
                    sources = master,
                    targets = pot,
                    where = NOWHERE,
                    use_shell = True))
            for l in linguas:
                ldir = "${TOP_DIR}/Help/%s" % l
                ctx.ensure_out_dir(ldir)
                po = "${PO4ADIR}/%s.html.%s.po" % (h, l)
                ctx.add_rule(Rule(rule = ["${PO4A_UPDATEPO} ${PO4AOPTS} " \
                        "-f xhtml -m ${SRC} -p ${TGT}",
                        charset_rule],
                        sources = [master, pot],
                        targets = po,
                        where = NOWHERE,
                        use_shell = True))
                ctx.add_rule(Rule(rule = [mkdir_rule,
                        "${PO4A_TRANSLATE} "
                        "${PO4ACHARSET} " \
                        "-k 0 -f xhtml -m %s " \
                        "-p ${SRC} -l ${TGT}" % master],
                        sources = po,
                        targets = "%s/%s.html" % (ldir, h),
                        where = NOWHERE,
                        use_shell = True))
            

elif ctx.mode == "install" or ctx.mode == "uninstall":

    ctx.install_bin("roxterm roxterm-config")
    ctx.install_data("roxterm-config.ui")
    ctx.install_data("roxterm.desktop", "${DATADIR}/applications")
    if ctx.env['XMLTOMAN']:
        ctx.install_man("roxterm.1 roxterm-config.1")
    ctx.install_doc("AUTHORS ChangeLog COPYING README")
    ctx.install_doc(ctx.glob("*.html",
            subdir = ctx.subst("${TOP_DIR}/Help/en")),
            "${HTMLDIR}/en")
    ctx.install_doc(ctx.glob("*",
            subdir = ctx.subst("${TOP_DIR}/Help/lib")),
            "${HTMLDIR}/lib")
    ctx.install_data("roxterm.svg", "${DATADIR}/icons/hicolor/scalable/apps")
    ctx.install_data(["Config/Colours/Tango", "Config/Colours/GTK"],
            "${PKGDATADIR}/Config/Colours")
    ctx.install_data("Config/Shortcuts/Default",
            "${PKGDATADIR}/Config/Shortcuts")
    gda = ctx.env['WITH_GNOME_DEFAULT_APPLICATIONS']
    if gda:
        ctx.install_data("roxterm.xml", gda)
    
    linguas = parse_linguas(ctx)
    if ctx.env['HAVE_GETTEXT']:
        for l in linguas:
            ctx.install_data("po/%s.mo" % l, "${LOCALEDIR}/%s/LC_MESSAGES" % l)
    if ctx.env['HAVE_PO4A']:
        for l in linguas:
            if ctx.env['XMLTOMAN']:
                ctx.install_man("po4a/%s/roxterm.1 po4a/%s/roxterm-config.1" % \
                        (l, l), opj("${MANDIR}", l))
            ctx.install_doc( \
                    ctx.glob("*.html",
                    subdir = ctx.subst("${TOP_DIR}/Help/%s" % l)),
                    "${HTMLDIR}/%s" % l)

elif ctx.mode == 'distclean' or ctx.mode == 'clean':
    
    clean = [LOGO_PNG, TEXT_LOGO, FAVICON, APPINFO]
    if ctx.mode == 'distclean':
        clean += [VFILE, DCH]
    for f in clean:
        ctx.delete(f)

elif ctx.mode == 'dist':
    
    ctx.add_dist("AUTHORS Help/AUTHORS " \
            "ChangeLog ChangeLog.old Config " \
            "COPYING Help/en Help/lib/header.png " \
            "Help/lib/logo_text_only.png " \
            "Help/lib/roxterm.css Help/lib/roxterm_ie.css "
            "Help/lib/sprites.png " \
            "INSTALL INSTALL.Debian INSTALL.ReadFirst " \
            "NEWS README README.translations " \
            "roxterm.1.xml.in roxterm-config.1.xml.in " \
            "roxterm.desktop roxterm.lsm.in roxterm.spec.in " \
            "roxterm.svg roxterm.xml TODO " \
            "src/roxterm-config.glade src/roxterm-config.ui")
    for f in [LOGO_PNG, FAVICON, TEXT_LOGO]:
        if os.path.exists(f):
            ctx.add_dist(f)
    ctx.add_dist(ctx.glob("*.[c|h]", os.curdir, "src"))
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
    if os.path.exists("AppInfo.xml"):
        ctx.add_dist("AppInfo.xml")
    # Translations
    ctx.add_dist("po/LINGUAS")
    files = ctx.glob("*.po", os.curdir, "po4a") + \
            ctx.glob("*.pot", os.curdir, "po4a")
    if files:
        ctx.add_dist(files)
    if os.path.exists("po/POTFILES.in"):
        ctx.add_dist("po/POTFILES.in")
    if os.path.exists("po/roxterm.pot"):
        ctx.add_dist("po/roxterm.pot")
    files = ctx.glob("*.po", os.curdir, "po", False)
    if files:
        ctx.add_dist(files)
    # Only needed for autotools
    files = "ABOUT-NLS po4a/LINGUAS po4a/Makefile.in po4a/Makefile.am".split()
    files += ["po/%s" % f for f in "insert-header.sin " \
                    "Makefile.in.in Rules-quot boldquot.sed en@quot.header " \
                    "en@boldquot.header Makevars remove-potcdate.sin " \
                    "stamp-po quot.sed".split()]
    # Other stuff to be removed when we no longer support autotools
    files += "acinclude.m4 bootstrap.sh configure.ac.in " \
            "Makefile.am src/Makefile.am update-locales".split()
    files += "m4 aclocal.m4 " \
                "compile config.h.in " \
                "config.rpath configure configure.ac " \
                "depcomp install-sh intl ltmain.sh " \
                "Makefile.in src/Makefile.in missing".split()
    for f in files:
        if os.path.exists(f):
            ctx.add_dist(f)
    for f in ["config.sub", "config.guess"]:
        if os.path.exists(f):
            ctx.add_dist(os.path.realpath(f), arcname = f)
            

ctx.run()

if ctx.mode == 'uninstall':
    ctx.prune_directory("${DATADIR}/icons")
    ctx.prune_directory("${PKGDATADIR}")
    ctx.prune_directory("${DOCDIR}")
    ctx.prune_directory("${HTMLDIR}")
