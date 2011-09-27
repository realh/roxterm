## Makefile.am -- Process this file with automake to produce Makefile.in


noinst_LTLIBRARIES = libroxterm.la

AM_LDFLAGS = -static

# This is to avoid building everything if just the version number changes
BUILT_SOURCES = rtconfig.h version.h
DISTCLEANFILES = $(BUILT_SOURCES)

# stamp file is added to deps here to make sure it gets built
rtconfig.h: ../config.h roxterm-config.ui.stamp
		@sed 's/ CONFIG_H/ RTCONFIG_H/;/define PACKAGE_/d;/define VERSION/d' ../config.h > rtconfig.h.tmp
		@test -f rtconfig.h || touch rtconfig.h
		@diff rtconfig.h rtconfig.h.tmp > /dev/null 2>&1 && rm rtconfig.h.tmp || true
		@test -f rtconfig.h.tmp && echo "Updating rtconfig.h" || echo "rtconfig.h is unchanged"
		@test -f rtconfig.h.tmp && rm -f rtconfig.h || true
		@test -f rtconfig.h.tmp && mv rtconfig.h.tmp rtconfig.h || true

version.h: ../config.h
		@echo '#ifndef VERSION_H' > version.h.tmp
		@echo '#define VERSION_H' >> version.h.tmp
		@echo '' >> version.h.tmp
		@grep -B1 VERSION ../config.h | sed 's/^--//' >> version.h.tmp
		@echo '' >> version.h.tmp
		@echo '#endif /* VERSION_H */' >> version.h.tmp
		@test -f version.h || touch version.h
		@diff version.h version.h.tmp > /dev/null 2>&1 && rm version.h.tmp || true
		@test -f version.h.tmp && echo "Updating version.h" || echo "version.h is unchanged"
		@test -f version.h.tmp && rm -f version.h || true
		@test -f version.h.tmp && mv version.h.tmp version.h || true

# Replace abstract bases with instantiable widgets for GTK2 compatibility
roxterm-config.ui.stamp: $(top_srcdir)/src/roxterm-config.ui
	sed -e 's/class="GtkBox" id=\(.*\)hbox/class="GtkHBox" id=\1hbox/; s/class="GtkBox" id=\(.*\)vbox/class="GtkVBox" id=\1vbox/' \
		-i $(top_srcdir)/src/roxterm-config.ui
	touch roxterm-config.ui.stamp

libroxterm_la_SOURCES = 	\
	colourscheme.c	colourscheme.h	\
				defns.h			\
	dlg.c			dlg.h			\
	display.c		display.h		\
	dragrcv.c		dragrcv.h		\
	dynopts.c		dynopts.h		\
	encodings.c		encodings.h		\
	globalopts.c		globalopts.h		\
	logo.c			logo.h			\
	options.c		options.h		\
	optsfile.c		optsfile.h		\
	rtdbus.c		rtdbus.h

bin_PROGRAMS	= roxterm roxterm-config

roxterm_SOURCES =	\
	about.c		about.h		\
	main.c				\
	multitab.c	multitab.h	\
	multitab-close-button.c		multitab-close-button.h	\
	multitab-label.c		multitab-label.h	\
	menutree.c	menutree.h	\
	optsdbus.c	optsdbus.h	\
	roxterm.c	roxterm.h	\
	shortcuts.c	shortcuts.h	\
	uri.c		uri.h		\
	x11support.c	x11support.h

if ENABLE_SM
roxterm_SOURCES += session.c    session.h
endif

if HAVE_VTE_TERMINAL_SEARCH_SET_GREGEX
roxterm_SOURCES += search.c     search.h
endif

roxterm_LDADD = @roxterm_LIBS@ libroxterm.la
roxterm_config_CFLAGS += -DROXTERM_CAPPLET

roxterm_config_SOURCES =	\
	capplet.c	capplet.h	\
	colourgui.c	colourgui.h	\
	configlet.c	configlet.h	\
	getname.c	getname.h	\
	optsdbus.c	optsdbus.h	\
	profilegui.c	profilegui.h

roxterm_config_LDADD = @roxterm_config_LIBS@ libroxterm.la

pkgdata_DATA	=	roxterm-config.ui
EXTRA_DIST	=	roxterm-config.ui       roxterm-config.glade
