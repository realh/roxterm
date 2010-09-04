
SUBDIRS = intl po src

nobase_pkgdata_DATA = Config/Colours/GTK \
		Config/Colours/Tango \
		Config/Shortcuts/Default

LOGO_PNG = $(top_srcdir)/Help/lib/roxterm_logo.png
FAVICON = $(top_srcdir)/Help/favicon.ico
TEXT_LOGO = $(top_srcdir)/Help/lib/logo_text.png
LOGO_TEXT_ONLY = $(top_srcdir)/Help/lib/logo_text_only.png

HELPIMGS =  $(LOGO_PNG) $(TEXT_LOGO) \
		$(top_srcdir)/Help/lib/header.png \
		$(top_srcdir)/Help/lib/sprites.png
		
HTMLFILES = $(top_srcdir)/Help/index.html \
	$(top_srcdir)/Help/guide.html \
	$(top_srcdir)/Help/installation.html \
	$(top_srcdir)/Help/news.html \
	$(top_srcdir)/Help/roxterm.css \
	$(top_srcdir)/Help/roxterm_ie.css \
	$(FAVICON)

HELPOTHER = \
		$(top_srcdir)/ChangeLog \
		$(top_srcdir)/NEWS

desktopdir = $(datadir)/applications
desktop_DATA = roxterm.desktop

if BUILD_MANPAGES

man_MANS = roxterm.1 roxterm-config.1

roxterm.1: roxterm.1.xml
	@XMLTOMAN@ $<

roxterm-config.1: roxterm-config.1.xml
	@XMLTOMAN@ $<

$(LOGO_PNG): roxterm.svg
	convert -background '#0000' roxterm.svg -geometry 64x64 $(LOGO_PNG)

$(FAVICON): $(LOGO_PNG)
	convert $(LOGO_PNG) -geometry 16x16 $(FAVICON)

$(TEXT_LOGO): $(LOGO_PNG) $(LOGO_TEXT_ONLY)
	composite -gravity SouthWest $(LOGO_PNG) $(LOGO_TEXT_ONLY) $(TEXT_LOGO)

DISTCLEANFILES = $(man_MANS)

endif

if INSTALL_DEFAULT_APPS
defaultappsdir = $(DEFAULTAPPS_DIR)
defaultapps_DATA = roxterm.xml
endif

EXTRA_DIST = config.rpath m4 roxterm.svg \
		.DirIcon AppRun AppInfo.xml AppInfo.xml.in \
		roxterm.desktop $(nobase_pkgdata_DATA) $(HTMLFILES) \
		$(LOGO_TEXT_ONLY) \
		AUTHORS COPYING README $(HELPOTHER) $(HELPIMGS) \
		update-locales roxterm.xml \
		Help/AUTHORS Help/COPYING Help/Changes Help/README \
		builddeb INSTALL.Debian \
		debian/changelog     debian/dirs             \
		debian/compat        debian/menu             \
		debian/control       debian/roxterm.doc-base \
		debian/copyright     debian/roxterm.postinst \
		debian/roxterm.prerm debian/roxterm.xpm      \
		debian/rules         debian/watch

Help/AUTHORS:
	ln -nfs ../AUTHORS Help/AUTHORS

Help/COPYING:
	ln -nfs ../COPYING Help/COPYING

Help/Changes:
	ln -nfs ../ChangeLog Help/Changes

Help/NEWS:
	ln -nfs ../NEWS Help/NEWS

Help/README:
	ln -nfs ../README Help/README

dist-hook:
	git log > $(top_srcdir)/ChangeLog && \
		cat $(top_srcdir)/ChangeLog.old >> $(top_srcdir)/ChangeLog

install-data-local:
	$(install_sh) -m 0644 -d $(DESTDIR)/$(docdir) $(DESTDIR)/$(htmldir)/lib
	$(install_sh) -m 0644 -t $(DESTDIR)/$(htmldir) $(HTMLFILES)
	$(install_sh) -m 0644 -t $(DESTDIR)/$(htmldir)/lib $(HELPIMGS)
	$(install_sh) -m 0644 $(top_srcdir)/roxterm.svg \
		$(DESTDIR)/$(pkgicondir)/

uninstall-local:
	rm -rf $(DESTDIR)/$(pkgdatadir) \
		$(DESTDIR)/$(docdir) \
		$(DESTDIR)/$(htmldir) \
		$(DESTDIR)/$(pkgicondir)/roxterm.svg

ACLOCAL_AMFLAGS = -I m4

deb:
	cd $(top_srcdir) && ./builddeb
