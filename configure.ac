#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([roxterm], m4_esyscmd([./version.sh]), [h@realh.co.uk])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/roxterm.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_HEADERS([src/version.h])
#AC_CONFIG_MACRO_DIRS([m4])

# Configure options
AC_ARG_WITH([gnome-default-applications-dir],
    AS_HELP_STRING([--with-gnome-default-applications-dir],
                   [Override location of GNOME default applications directory]),
    [],
    [with_gnome_default_applications_dir=yes])

# Checks for programs.
AC_PROG_C
PKG_PROG_PKG_CONFIG
AC_PATH_PROG([INKSCAPE], [inkscape])
AC_PATH_PROGS([RSVG], [rsvg rsvg-convert])
AC_PATH_PROG([XSLTPROC], [xsltproc])

# Checks for libraries.
PKG_CHECK_MODULES([gtk],
                  [gtk+-3.0 >= 3.20.0 dbus-1 >= 1.0 dbus-glib-1 >= 0.22])
PKG_CHECK_MODULE([vte], [vte-2.91 >= 0.50], [
    PKG_CHECK_MODULE([pcre], [libpcre2-8])
], [
    PKG_CHECK_MODULE([vte], [vte-2.91 >= 0.48])
    pcre_CFLAGS=
    pcre_LIBS=
])
PKG_CHECK_MODULE([gmodule], [gmodule-export-2.0])

# Checks for header files.
AC_CHECK_HEADERS([stdio.h stdlib.h string.h])

# Checks for library functions.

# Checks for typedefs, structures, and compiler characteristics.

# Other checks
AS_CASE([x$with_gnome_default_applications_dir],
    [xno], [AC_SUBST([GNOME_DEFAULT_APPLICATIONS_DIR],[])],
    [x|xyes], [PKG_CHECK_VAR([GNOME_DEFAULT_APPLICATIONS_DIR],
                             [gnome-default-applications],
                             [defappsdir])],
    [AC_SUBST([GNOME_DEFAULT_APPLICATIONS_DIR],
              [$with_gnome_default_applications_dir])])
    
# roxterm.appdata.xml is actually supposed to be generated during translation
AC_CONFIG_FILES([Makefile src/Makefile roxterm.appdata.xml])

AC_OUTPUT
