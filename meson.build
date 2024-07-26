project('roxterm', 'c')

gtkdep = dependency('gtk+-3.0', version: '>=3.22.0')
vtedep = dependency('vte-2.91', version: '>=0.52', required: false)
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
docdir = get_option('prefix') / get_option('datadir') / 'doc' / 'roxterm'

roxterm_version = run_command('./version.sh', check: true).stdout()
date = run_command('date', '+%Y-%m-%d', check: true).stdout()

conf_data = configuration_data()
conf_data.set('ROXTERM_VERSION', roxterm_version)
conf_data.set('APPINFO_VERSION', roxterm_version + ' ' + date)
conf_data.set('INSTALL_DOCDIR', docdir)

roxterm_man_xml = configure_file(input: 'roxterm.1.xml.in',
    output: 'roxterm.1.xml', configuration: conf_data)
roxterm_config_man_xml = configure_file(input: 'roxterm-config.1.xml.in',
    output: 'roxterm-config.1.xml', configuration: conf_data)
configure_file(input: 'AppInfo.xml.in', output: 'AppInfo.xml',
    configuration: conf_data)

meta_info = configure_file(input: 'roxterm.metainfo.xml.in',
    output: 'roxterm.metainfo.xml', configuration: conf_data)

custom_target('roxterm.1', capture: true,
    input: roxterm_man_xml, output: 'roxterm.1',
    build_by_default: true,
    install: true, install_dir: get_option('mandir') / 'man1',
    command: ['xmltoman', '@INPUT@'])
custom_target('roxterm-config.1', capture: true,
    input: roxterm_config_man_xml, output: 'roxterm-config.1',
    build_by_default: true,
    install: true, install_dir: get_option('mandir') / 'man1',
    command: ['xmltoman', '@INPUT@'])

# Stuff to be installed
dd = get_option('prefix') / get_option('datadir')
install_data('roxterm.desktop', install_dir: dd / 'applications')
install_data(meta_info, install_dir: dd / 'metainfo')
install_data('src/icons/roxterm.svg',
             install_dir: dd / 'icons/hicolor/scalabale.apps')
install_subdir('Config', install_dir: dd / '/roxterm')

# Docs
install_emptydir(docdir)
foreach a: ['AUTHORS', 'Changes', 'COPYING', 'index.html', 'index.php',
    'NEWS', 'README']
    install_data('docs' / a, install_dir: docdir)
endforeach
foreach a: ['en', 'lib']
    install_subdir('docs' / a, install_dir: docdir)
endforeach

subdir('src')
subdir('Config')
