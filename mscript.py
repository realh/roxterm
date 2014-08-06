#!/usr/bin/env python

import errno, os, re, sys, time

from maitch import *

ctx = Context(PACKAGE = "roxterm", SRC_DIR = "${TOP_DIR}/src",
        MCFLAGS = "${CPPFLAGS} -I. -I${SRC_DIR} -D_GNU_SOURCE -DHAVE_CONFIG_H")


MINILIB_SOURCES = "colourscheme.c dlg.c display.c dragrcv.c dynopts.c " \
        "encodings.c globalopts.c logo.c options.c optsfile.c rtdbus.c"

ROXTERM_CONFIG_SOURCES = "capplet.c colourgui.c configlet.c getname.c " \
        "optsdbus.c profilegui.c shortcuts.c"

ROXTERM_SOURCES = "about.c main.c multitab.c multitab-close-button.c " \
        "multitab-label.c menutree.c optsdbus.c roxterm.c shortcuts.c " \
        "uri.c x11support.c"

ROXTERM_HTML_BASENAMES = "guide index installation news".split()

LOGO_PNG = "${TOP_DIR}/Help/lib/roxterm_logo.png"
FAVICON = "${TOP_DIR}/Help/lib/favicon.ico"
TEXT_LOGO = "${TOP_DIR}/Help/lib/logo_text.png"
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
    ctx.arg_disable('nls', "Disable all translations",
            default = None)
    ctx.arg_disable('translations',
            "Disable all translations (same as --disable-nls)", default = None)
    ctx.arg_disable('git', "Assume this is a release tarball: "
            "don't attempt to generate changelogs, pixmaps etc")
    ctx.arg_enable("rox-locales",
            "Make symlinks so ROX app can load translations")

if ctx.mode == 'configure':

    ctx.find_prog_env("sed")
    try:
        ctx.find_prog_env("gpg")
    except MaitchNotFoundError:
        mprint("gpg not found, not signing tarball")
        ctx.setenv('SIGN_DIST', False)
    else:
        ctx.setenv('SIGN_DIST', True)

    vfile = ctx.subst(VFILE)
    if ctx.env['ENABLE_GIT'] != False:
        git = os.path.exists(ctx.subst(opj("${TOP_DIR}", ".git")))
        try:
            ctx.find_prog_env("git")
        except MaitchNotFoundError:
            git = False
    else:
        git = False
    if git:
        # Might have an obsolete version.h from earlier build
        ctx.delete("${SRC_DIR}/version.h")
        version = ctx.prog_output(["${GIT}", "describe",
                "--match", "[0-9]*"])[0].strip()
        version = version.replace('-', '.', 1).replace('-', '~', 1)
        ctx.save_if_different(vfile, version + '\n')
        gitlog = ctx.prog_output(["/bin/sh", os.path.abspath(
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

    if ctx.env['ENABLE_GIT'] != False:
        try:
            ctx.find_prog_env("convert")
            ctx.find_prog_env("composite")
            try:
                ctx.find_prog_env("rsvg-convert")
            except:
                ctx.find_prog_env("rsvg")
        except:
            mprint("WARNING: ImageMagick and/or rsvg binaries appear " \
                    "not to be installed.\n" \
                    "This will cause errors later unless the generated " \
                    "pixmaps are already present,\neg supplied with a " \
                    "release tarball.",
                    file = sys.stderr)
            ctx.setenv("CONVERT", "")
            ctx.setenv("COMPOSITE", "")
    else:
        ctx.setenv("CONVERT", "")
        ctx.setenv("COMPOSITE", "")

    ctx.setenv('BUG_TRACKER', "http://sourceforge.net/tracker/?group_id=124080")

    trans = ctx.env['ENABLE_TRANSLATIONS'] and ctx.env['ENABLE_NLS']
    if trans != False:
        try:
            ctx.find_prog_env("xgettext")
            ctx.find_prog_env("msgcat")
            ctx.find_prog_env("msgmerge")
            ctx.find_prog_env("msgfmt")
        except MaitchNotFoundError:
            if trans == True:
                raise
            else:
                mprint("WARNING: Translation tools not found, not building " \
                        " programs' translations", file = sys.stderr)
                ctx.setenv('HAVE_GETTEXT', False)
        else:
            ctx.setenv('HAVE_GETTEXT', True)
    else:
        ctx.setenv('HAVE_GETTEXT', False)

    if trans != False:
        try:
            ctx.find_prog_env("po4a-gettextize")
            ctx.find_prog_env("po4a-updatepo")
            ctx.find_prog_env("po4a-translate")
        except MaitchNotFoundError:
            if trans == True:
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
            ctx.setenv('PO4ADIR', "${ABS_TOP_DIR}/po4a")
            ctx.setenv('PO4ABUILDDIR', "${ABS_BUILD_DIR}/po4a")
    else:
        ctx.setenv('HAVE_PO4A', False)

    if trans != False and ctx.env['HAVE_GETTEXT']:
        try:
            ctx.find_prog_env("itstool")
        except MaitchNotFoundError:
            if trans == True:
                raise
            else:
                    mprint("WARNING: itstool not found, not building " \
                            "AppData file's translations", file = sys.stderr)
                    ctx.setenv('HAVE_ITSTOOL', False)
        else:
            ctx.setenv('HAVE_ITSTOOL', True)
            ctx.setenv('POXML_DIR', "${ABS_TOP_DIR}/poxml")
            ctx.setenv('POXML_BUILD_DIR', "${ABS_BUILD_DIR}/poxml")
            ctx.setenv('APPDATA_ITS', "${POXML_DIR}/appdata.its")
    else:
        ctx.setenv('HAVE_ITSTOOL', False)

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
            vte_version = ctx.prog_output("${PKG_CONFIG} --modversion vte-2.90")
            ctx.setenv('NEED_TRANSPARENCY_FIX', vte_version >= "0.34.8")
        except MaitchChildError:
            if gtk3 == True:
                raise
            gtk3 = False
        else:
            gtk3 = True
    if not gtk3:
        ctx.pkg_config('gtk+-2.0', 'GTK', '2.18')
        ctx.pkg_config('vte', 'VTE', '0.20')
        ctx.setenv('NEED_TRANSPARENCY_FIX', 0)

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
            "gtk_combo_box_text_new gtk_rc_style_unref " \
            "gtk_drag_begin_with_coordinates".split():
        ctx.check_func(f, "${CFLAGS} ${MCFLAGS} ${GTK_CFLAGS}",
                "${LIBS} ${GTK_LIBS}")
    for f in ["vte_terminal_search_set_gregex", "vte_terminal_get_pty_object"]:
        ctx.check_func(f, "${CFLAGS} ${MCFLAGS} ${VTE_CFLAGS}",
                "${LIBS} ${VTE_LIBS}")

    ctx.setenv('CORE_CFLAGS',
            "${CFLAGS} ${MCFLAGS} ${GTK_CFLAGS} ${DBUS_CFLAGS}")
    ctx.setenv('CORE_LIBS',
            "${LIBS} ${GTK_LIBS} ${DBUS_LIBS}")
    ctx.setenv('ROXTERM_CFLAGS',
            "${CFLAGS} ${MCFLAGS} ${VTE_CFLAGS} ${SM_CFLAGS} ${DBUS_CFLAGS}")
    ctx.setenv('ROXTERM_LIBS',
            "${LIBS} ${VTE_LIBS} ${SM_LIBS} ${DBUS_LIBS}")
    ctx.setenv('ROXTERM_CONFIG_CFLAGS',
            "${CFLAGS} ${MCFLAGS} ${GTK_CFLAGS} ${DBUS_CFLAGS} " \
            "${GMODULE_CFLAGS} -DROXTERM_CAPPLET")
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
    ctx.define('HTML_DIR', ctx.env['HTMLDIR'])
    ctx.setenv('htmldir', "${HTMLDIR}")
    ctx.define('BIN_DIR', ctx.env['BINDIR'])
    if ctx.env['HAVE_GETTEXT']:
        ctx.define('ENABLE_NLS', 1)
    else:
        ctx.define('ENABLE_NLS', None)
    ctx.define_from_var('LOCALEDIR')

    ctx.define_from_var('NEED_TRANSPARENCY_FIX')

    ctx.subst_file("${TOP_DIR}/roxterm.1.xml.in",
            "${TOP_DIR}/roxterm.1.xml", True)
    ctx.subst_file("${TOP_DIR}/roxterm-config.1.xml.in",
            "${TOP_DIR}/roxterm-config.1.xml", True)
    ctx.subst_file("${TOP_DIR}/roxterm.spec.in", "${TOP_DIR}/roxterm.spec")
    ctx.setenv('APPINFO_STRING', "${VERSION} (%s)" % \
            time.strftime("%Y-%m-%d", time.gmtime(time.time())))
    ctx.subst_file(APPINFO + ".in", APPINFO)
    ctx.save_if_different("version.h",
            '/* Auto-generated by mscript.py */\n' \
            '#ifndef VERSION_H\n' \
            '#define VERSION_H\n' \
            '#define VERSION "${VERSION}"\n' \
            '#endif\n')
    ctx.created_by_config['version.h'] = True

    # Make symlinks expected by ROX
    for f in "AUTHORS ChangeLog COPYING COPYING-LGPL NEWS README".split():
        if f == "ChangeLog":
            dest = "${TOP_DIR}/Help/Changes"
        else:
            dest = "${TOP_DIR}/Help/" + f
        src = "../" + f
        if subprocess.call(["ln", "-nfs", src, ctx.subst(dest)]):
            raise MaitchChildError("Failed to link '%s' Help file" % f)

elif ctx.mode == 'build':

    # Private library
    for c in MINILIB_SOURCES.split():
        ctx.add_rule(StaticLibCRule(
                sources = c,
                cflags = "${CORE_CFLAGS}",
                prefix = "libroxterm-",
                quiet = True))
    ctx.add_rule(CStaticLibRule(
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
    for c in ROXTERM_SOURCES.split():
        ctx.add_rule(LibtoolCRule(
                sources = c,
                cflags = "${ROXTERM_CFLAGS}",
                prefix = "roxterm-",
                wdeps = "version.h"))
    ctx.add_rule(LibtoolCProgramRule(
            sources = change_suffix_with_prefix(ROXTERM_SOURCES,
                    ".c", ".lo", "roxterm-"),
            targets = "roxterm",
            cflags = "${ROXTERM_CFLAGS}",
            libs = "${ROXTERM_LIBS} -lroxterm",
            deps = "libroxterm.la",
            quiet = True))

    # roxterm-config
    for c in ROXTERM_CONFIG_SOURCES.split():
        ctx.add_rule(LibtoolCRule(
                sources = c,
                cflags = "${ROXTERM_CONFIG_CFLAGS}",
                prefix = "roxterm-config-"))
    ctx.add_rule(LibtoolCProgramRule(
            sources = change_suffix_with_prefix(ROXTERM_CONFIG_SOURCES,
                    ".c", ".lo", "roxterm-config-"),
            targets = "roxterm-config",
            cflags = "${ROXTERM_CONFIG_CFLAGS}",
            libs = "${ROXTERM_CONFIG_LIBS} -lroxterm",
            deps = "libroxterm.la",
            quiet = True))

    # Stuff other than the program

    # Graphics
    if ctx.env['CONVERT']:
        ctx.add_rule(Rule(rule = "${CONVERT} -background #0000 " \
                "${SRC} -geometry 64x64 ${TGT}",
                targets = LOGO_PNG,
                sources = "roxterm.svg",
                where = TOP))

        # Note 'where' is NOWHERE for following two rules because sources
        # already start with ${TOP_DIR}.
        ctx.add_rule(Rule(rule = "${CONVERT} ${SRC} -geometry 16x16 ${TGT}",
                targets = FAVICON,
                sources = LOGO_PNG,
                where = NOWHERE))

        ctx.add_rule(Rule( \
                rule = "${COMPOSITE} -gravity SouthWest ${SRC} ${TGT}",
                targets = TEXT_LOGO,
                sources = [LOGO_PNG, "${TOP_DIR}/Help/lib/logo_text_only.png"],
                where = NOWHERE))

    # man pages
    if ctx.env['XMLTOMAN']:
        xmltomanrule = "${XMLTOMAN} ${XMLTOMAN_OPTS} ${SRC} ${XMLTOMAN_OUTPUT}"
        # Something is causing a thread to hang between calling xsltproc
        # and exiting from subprocess.Popen. Could it be trying to run two
        # at once? Making one wdep on the other should stop jobs overlapping.
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
        s = re.sub(r'requires lib="gtk\+" version=".*"',
                r'requires lib="gtk+" version="2.18"', s)
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
        podir = '${ABS_BUILD_DIR}/po'
        ctx.add_rule(Rule(rule = mkdir_rule, targets = podir))
        args = { 'copyright_holder': "(c) 2013 Tony Houghton",
                'version': "${VERSION}",
                'bugs_addr': "${BUG_TRACKER}",
                'use_shell': True,
                'dir': podir }
        code_pot = '${ABS_BUILD_DIR}/po/code.pot'
        glade_pot = '${ABS_BUILD_DIR}/po/glade.pot'
        trans_rules = PoRulesFromLinguas(ctx, **args) + \
                PotRules(ctx,
                        targets = code_pot,
                        deps = podir,
                        xgettext_opts = '-C -k_ -kN_',
                        **args)
        for r in trans_rules:
            ctx.add_rule(r)
        ctx.add_rule(PotRule(ctx,
                sources = '${SRC_DIR}/roxterm-config.ui',
                targets = glade_pot,
                deps = podir,
                xgettext_opts = '-L Glade',
                wdeps = "roxterm-config.ui-stamp",
                dir = "${ABS_TOP_DIR}/po"))
        ctx.add_rule(Rule(sources = [code_pot, glade_pot],
                targets = '${ABS_TOP_DIR}/po/${PACKAGE}.pot',
                rule = '${MSGCAT} -o ${TGT} ${SRC}',
                diffpat = gettext_diffpat))

    # Symlinks so ROX can use translations
    if ctx.env["ENABLE_ROX_LOCALES"]:
        def add_rox_locale(ctx, l, f):
            d = opj("locale", l, "LC_MESSAGES")
            ctx.add_rule(Rule(rule = mkdir_rule, targets = d))
            ctx.add_rule(Rule(rule = "ln -nfs ../../po/%s.mo ${TGT}" % l,
                    targets = opj(d, "roxterm.mo"),
                    wdeps = [d, opj("po", "%s.mo" % l)]))

        foreach_lingua(ctx, add_rox_locale)
        ctx.add_rule(Rule(rule = "ln -nfs pt_BR ${TGT}",
                targets = opj("locale", "pt"),
                wdeps = opj("locale", "pt_BR", "LC_MESSAGES")))


    # Translations (po4a)
    if ctx.env['HAVE_PO4A']:
        linguas = parse_linguas(ctx, podir = "${PO4ADIR}")
        charset_rule = "${SED} -i s/charset=CHARSET/charset=UTF-8/ ${TGT}"
        ctx.ensure_out_dir("po4a")
        if ctx.env['XMLTOMAN']:
            # Workaround for deadlock (?)
            lastmtarget = None
            for m in ["roxterm", "roxterm-config"]:
                ctx.add_rule(Rule(rule = ["${PO4A_GETTEXTIZE} ${PO4AOPTS} " \
                        "-f docbook -m ${SRC} -p ${TGT}",
                        charset_rule],
                        sources = "../%s.1.xml.in" % m,
                        targets = "%s.1.pot" % m,
                        where = NOWHERE,
                        diffpat = gettext_diffpat,
                        dir = "${PO4ADIR}",
                        use_shell = True))
                for l in linguas:
                    po = "${PO4ADIR}/%s.1.%s.po" % (m, l)
                    ctx.add_rule(Rule(rule = ["${PO4A_UPDATEPO} ${PO4AOPTS} " \
                            "-f docbook -m ${SRC} -p ${TGT}",
                            charset_rule],
                            sources = ["../%s.1.xml.in" % m, "%s.1.pot" % m],
                            targets = po,
                            diffpat = gettext_diffpat,
                            where = NOWHERE,
                            dir = "${PO4ADIR}",
                            use_shell = True))
                    ctx.add_rule(Rule(rule = "${PO4A_TRANSLATE} "
                            "${PO4ACHARSET} " \
                            "-k 0 -f docbook -m ../%s.1.xml.in " \
                            "-p ${SRC} -l ${TGT}" % m,
                            sources = po,
                            targets = "${ABS_BUILD_DIR}/po4a/%s.1.%s.xml.in" % \
                                    (m, l),
                            where = NOWHERE,
                            dir = "${PO4ADIR}",
                            use_shell = True))
                    ctx.add_rule(Rule(rule = "${SED} " \
                            "'s/@VERSION@/${VERSION}/; " \
                            "s#@htmldir@#${HTMLDIR}#' <${SRC} >${TGT}",
                            sources = "${PO4ABUILDDIR}/%s.1.%s.xml.in" % (m, l),
                            targets = "${PO4ABUILDDIR}/%s.1.%s.xml" % (m, l),
                            deps = po,
                            where = NOWHERE,
                            use_shell = True))
                    mtarget = "${PO4ABUILDDIR}/%s/%s.1" % (l, m)
                    ctx.add_rule(Rule( \
                            rule = [mk_parent_dir_rule, xmltomanrule],
                            sources = "${PO4ABUILDDIR}/%s.1.%s.xml" % (m, l),
                            targets = mtarget,
                            wdeps = lastmtarget,
                            where = NOWHERE))
                    lastmtarget = mtarget
        for h in ROXTERM_HTML_BASENAMES:
            master = "../Help/en/%s.html" % h
            pot = "%s.html.pot" % h
            ctx.add_rule(Rule(rule = ["${PO4A_GETTEXTIZE} ${PO4AOPTS} " \
                    "-f xhtml -m ${SRC} -p ${TGT}",
                    charset_rule,
                    "${SED} -i 's/SOME DESCRIPTIVE TITLE/" \
                            "Translations for roxterm docs/' ${TGT}",
                    "${SED} -i 's/Copyright (C) YEAR/" + \
                            "Copyright (C) 2010-2014/' " \
                            "${TGT}",
                    "${SED} -i 's/FIRST AUTHOR <EMAIL@ADDRESS>, YEAR/"
                            "Tony Houghton <h@realh.co.uk>, 2014/' ${TGT}"],
                    sources = master,
                    targets = "${PO4ADIR}/" + pot,
                    where = NOWHERE,
                    dir = "${PO4ADIR}",
                    use_shell = True))
            for l in linguas:
                ldir = "../Help/%s" % l
                ctx.ensure_out_dir(ldir)
                po = "${PO4ADIR}/%s.html.%s.po" % (h, l)
                ctx.add_rule(Rule(rule = ["${PO4A_UPDATEPO} ${PO4AOPTS} " \
                        "-f xhtml -m ${SRC} -p ${TGT}",
                        charset_rule],
                        sources = [master, pot],
                        targets = po,
                        where = NOWHERE,
                        dir = "${PO4ADIR}",
                        use_shell = True))
                ctx.add_rule(Rule(rule = [mk_parent_dir_rule,
                        "${PO4A_TRANSLATE} "
                        "${PO4ACHARSET} " \
                        "-k 0 -f xhtml -m %s " \
                        "-p ${SRC} -l ${TGT}" % master],
                        sources = po,
                        targets = "${ABS_TOP_DIR}/Help/%s/%s.html" % (ldir, h),
                        where = NOWHERE,
                        dir = "${PO4ADIR}",
                        use_shell = True))


    # Translations (itstool)
    if ctx.env['HAVE_ITSTOOL']:
        podir = "${POXML_DIR}"
        linguas = parse_linguas(ctx, podir = podir)
        basename = "roxterm.appdata.xml"
        xmlout = "${ABS_BUILD_DIR}/" + basename
        xmlin = "../" + basename + ".in"
        potfile = "${POXML_DIR}/roxterm.appdata.xml.pot"
        ctx.add_rule(Rule( \
                rule = ["${ITSTOOL} -i ${APPDATA_ITS} -o ${TGT} ${SRC}",
                    "${SED} -i 's/Project-Id-Version: PACKAGE VERSION/" \
                        "Project-Id-Version: roxterm ${VERSION}/' " \
                        "${TGT}"],
                sources = xmlin,
                targets = potfile,
                deps = "${APPDATA_ITS}",
                where = NOWHERE,
                dir = podir,
                use_shell = True))
        if linguas:
            for r in PoRulesFromLinguas(ctx, podir = podir,
                    modir = "${POXML_BUILD_DIR}",
                    sources = potfile):
                ctx.add_rule(r)
            sources = []
            for l in parse_linguas(ctx, podir = podir):
                sources.append(opj("${POXML_BUILD_DIR}", l + ".mo"))
            ctx.add_rule(Rule( \
                    rule = "${ITSTOOL} -i ${APPDATA_ITS} -j " + xmlin +
                            " -o ${TGT} ${SRC}",
                    sources = sources,
                    targets = xmlout,
                    dir = podir,
                    where = NOWHERE))
    else:
        linguas = None
    if not linguas:
        ctx.add_rule(Rule(rule = "cp ${SRC} ${TGT}",
                sources = "${ABS_TOP_DIR}/roxterm.appdata.xml.in",
                targets = "${ABS_BUILD_DIR}/roxterm.appdata.xml",
                where = NOWHERE))


elif ctx.mode == "install" or ctx.mode == "uninstall":

    ctx.install_bin("roxterm roxterm-config")
    ctx.install_data("roxterm-config.ui")
    ctx.install_data("roxterm.desktop", "${DATADIR}/applications")
    ctx.install_data("roxterm.appdata.xml", "${DATADIR}/appdata")
    if ctx.env['XMLTOMAN']:
        ctx.install_man("roxterm.1 roxterm-config.1")
    ctx.install_doc("AUTHORS ChangeLog README")
    ctx.install_doc(ctx.glob("*.html",
            subdir = ctx.subst("${TOP_DIR}/Help/en")),
            "${HTMLDIR}/en")
    ctx.install_doc(ctx.glob("*.png",
            subdir = ctx.subst("${TOP_DIR}/Help/lib")),
            "${HTMLDIR}/lib")
    ctx.install_doc(ctx.glob("*.css",
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
            ctx.install_data("po/%s.mo" % l,
                    "${LOCALEDIR}/%s/LC_MESSAGES/roxterm.mo" % l,
                    other_options = "-T")
        ptdir = ctx.subst("${DESTDIR}/${LOCALEDIR}/pt/LC_MESSAGES")
        ctx.ensure_out_dir(ptdir)
        call_subprocess(["ln", "-sfn",
                "../../pt_BR/LC_MESSAGES/roxterm.mo", ptdir])
    if ctx.env['HAVE_PO4A']:
        for l in linguas:
            if ctx.env['XMLTOMAN']:
                ctx.install_man("po4a/%s/roxterm.1 po4a/%s/roxterm-config.1" % \
                        (l, l), opj("${MANDIR}", l))
            ctx.install_doc( \
                    ctx.glob("*.html",
                    subdir = ctx.subst("${TOP_DIR}/Help/%s" % l)),
                    "${HTMLDIR}/%s" % l)
        ptdir = ctx.subst("${DESTDIR}/${MANDIR}/pt/man1")
        ctx.ensure_out_dir(ptdir)
        call_subprocess(["ln", "-sfn", "../../pt_BR/man1/roxterm.1", ptdir])
        call_subprocess(["ln", "-sfn", "../../pt_BR/man1/roxterm-config.1",
                ptdir])
        call_subprocess(["ln", "-sfn", "pt_BR",
                ctx.subst("${DESTDIR}/${HTMLDIR}/pt")])

elif ctx.mode == 'pristine' or ctx.mode == 'clean':

    clean = ["${TOP_DIR}/maitch.pyc"] + \
            ctx.glob("*.po~", "${TOP_DIR}", "po") + \
            ctx.glob("*.po~", "${TOP_DIR}", "po4a") + \
            ctx.glob("*.po~", "${TOP_DIR}", "poxml")

    if ctx.mode == 'pristine':
        clean += [APPINFO, VFILE, "${TOP_DIR}/ChangeLog"] + \
            ["${TOP_DIR}/Help/" + f for f in \
                "AUTHORS COPYING COPYING-LGPL Changes NEWS README".split()] + \
            ["${TOP_DIR}/Help/lib/" + f for f in \
                "favicon.ico logo_text.png roxterm_logo.png".split()] + \
            ctx.glob("*.pot", "${TOP_DIR}", "po") + \
            ctx.glob("*.pot", "${TOP_DIR}", "po4a") + \
            ctx.glob("*.pot", "${TOP_DIR}", "poxml") + \
            ["${TOP_DIR}/" + f for f in \
                "roxterm.1.xml roxterm-config.1.xml roxterm.spec".split()]
        f = open(ctx.subst("${TOP_DIR}/po4a/LINGUAS"), 'r')
        hd = ctx.subst("${TOP_DIR}/Help/")
        for d in [hd + l.strip() for l in f.readlines() + ['pt']]:
            recursively_remove(d, False, [])
        f.close()

    for f in clean:
        ctx.delete(f)

elif ctx.mode == 'dist':

    ctx.add_dist("AUTHORS Help/AUTHORS " \
            "genlog ChangeLog ChangeLog.old Config " \
            "COPYING COPYING-LGPL Help/en Help/lib/header.png " \
            "Help/lib/logo_text_only.png " \
            "Help/lib/roxterm.css Help/lib/roxterm_ie.css "
            "Help/lib/sprites.png " \
            "INSTALL INSTALL.Debian " \
            "NEWS README README.translations " \
            "roxterm.1.xml.in roxterm-config.1.xml.in " \
            "roxterm.desktop roxterm.lsm.in roxterm.spec.in " \
            "roxterm.svg roxterm.xml TODO " \
            "src/roxterm-config.glade src/roxterm-config.ui")
    ctx.add_dist([f.replace("${TOP_DIR}/", "") \
            for f in [LOGO_PNG, FAVICON, TEXT_LOGO]])
    ctx.add_dist(ctx.glob("*.[c|h]", os.curdir, "src"))
    # maitch-specific
    ctx.add_dist("version maitch.py mscript.py")
    # ROX bits
    ctx.add_dist("AppInfo.xml.in AppRun .DirIcon " \
            "Help/Changes Help/COPYING Help/COPYING-LGPL Help/NEWS Help/README")
    if os.path.exists("AppInfo.xml"):
        ctx.add_dist("AppInfo.xml")
    # Translations
    for f in ("po/LINGUAS", "po4a/LINGUAS", "poxml/LINGUAS",
            "po/POTFILES.in", "po/roxterm.pot",
            "poxml/roxterm.appdata.xml", "poxml/roxterm.appdata.xml.in",
            "poxml/appdata.its", "poxml/roxterm.appdata.xml.pot"):
        if os.path.exists(f):
            ctx.add_dist(f)
    files = ctx.glob("*.po", os.curdir, "po") + \
            ctx.glob("*.po", os.curdir, "po4a") + \
            ctx.glob("*.pot", os.curdir, "po4a") + \
            ctx.glob("*.po", os.curdir, "poxml")
    if files:
        ctx.add_dist(files)


ctx.run()

if ctx.mode == 'uninstall':
    ctx.prune_directory("${DATADIR}/icons")
    ctx.prune_directory("${PKGDATADIR}")
    ctx.prune_directory("${DOCDIR}")
    ctx.prune_directory("${HTMLDIR}")
elif ctx.mode == 'uninstall':
        basedir = self.subst("${PACKAGE}-${VERSION}")
        filename = os.path.abspath(
                self.subst("${BUILD_DIR}/%s.%s" % (basedir, suffix)))
        mprint("Creating %s '%s'" % (zname, filename))
