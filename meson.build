project('roxterm', 'c')

gtkdep = dependency('gtk+-3.0', version: '>=3.20.0')
vtedep = dependency('vte-2.91', version: '>=0.50', required: false)
if vtedep.found()
  pcredep = dependency('libpcre2-8')
else
  vtedep = dependency('vte-2.91', version: '>=0.46')
  pcredep = dependency('', required: false)
endif
dbusdep = dependency('dbus-1', version: '>=1.0')
gdbusdep = dependency('dbus-glib-1', version: '>=0.22')

version = run_command('./version.sh').stdout()

subdir('src')
