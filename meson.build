project('roxterm', 'c')

gtkdep = dependency('gtk+-3.0', version: '>=3.20.0')
vtedep = dependency('vte-2.91', version: '>=0.50', required: false)
if vtedep.found()
  pcredep = dependency('libpcre2-8')
else
  vtedep = dependency('vte-2.91', version: '>=0.48')
  pcredep = dependency('', required: false)
endif
dbusdep = dependency('dbus-1', version: '>=1.0')
gdbusdep = dependency('dbus-glib-1', version: '>=0.22')
# GtkBuilder needs this:
gmoddep = dependency('gmodule-export-2.0')

# Configure-time substitutions
doc_srcdir = '@srcdir@/Help'
doc_installdir = '@prefix@/@datadir@/doc/roxterm'

version = run_command('./version.sh').stdout()
date = run_command('date', '+%Y-%m-%d').stdout()

conf_data = configuration_data()
conf_data.set('VERSION', version)
conf_data.set('APPINFO_VERSION', version + ' ' + date)
conf_data.set('htmldir', doc_installdir)
configure_file(input: 'roxterm.1.xml.in', output: 'roxterm.1.xml',
    configuration: conf_data)
configure_file(input: 'roxterm-config.1.xml.in', output: 'roxterm.1.xml',
    configuration: conf_data)
configure_file(input: 'AppInfo.xml.in', output: 'AppInfo.xml',
    configuration: conf_data)
# Kludge, appdata should be handled by i18n
configure_file(input: 'roxterm.appdata.xml.in', output: 'roxterm.appdata.xml',
    configuration: conf_data)

# Ensure we have generated images etc which meson can't really do
run_command('./gendist.sh')

# Stuff to be installed
install_man('roxterm.1', 'roxterm-config.1')
install_data('roxterm.desktop', install_dir: '@prefix@/@datadir@/applications')
install_data('roxterm.appdata.xml', install_dir: '@prefix@/@datadir@/appdata')
install_data('roxterm.svg',
             install_dir: '@prefix@/@datadir@/icons/hicolor/scalabale.apps')
install_subdir('Config', install_dir: '@prefix@/@datadir@/roxterm')
gdadir = get_option('gnome-default-applications-dir')
if not gdadir
  gdadir = run_command('pkg-config', '--variable=defappsdir',
                       'gnome-default-applications')
endif
install_data('roxterm.xml', install_dir: gdadir)

subdir('src')
subdir('Config')
subdir('Help')
