gnome = import('gnome')

conf_data = configuration_data()
foreach a: ['prefix', 'bindir', 'datadir', 'localedir', 'sysconfdir']
    conf_data.set(a, get_option(a))
endforeach
conf_data.set('vte_libdir', vtedep.get_variable('libdir'))
configure_file(input: 'config.h.in',
    output: 'config.h',
    configuration: conf_data)

version_data = configuration_data()
version_data.set('ROXTERM_VERSION', roxterm_version)
configure_file(input: 'version.h.in',
    output: 'version.h',
    configuration: version_data)

common_deps = [gtkdep, dbusdep, gdbusdep]

gresources = gnome.compile_resources('gresources', 'gresources.xml',
    c_name: 'roxterm_resource')

rtlib = static_library('roxterm', ['colourscheme.c', 'dlg.c', 'dragrcv.c',
    'dynopts.c', 'globalopts.c', 'options.c', 'optsfile.c', 'resources.c',
    'rtdbus.c', gresources],
    dependencies: common_deps,
    install: false)

rtconfig = executable('roxterm-config', ['capplet.c', 'colourgui.c',
    'configlet.c', 'getname.c', 'optsdbus.c', 'profilegui.c', 'shortcuts.c'],
    dependencies: common_deps + [gmoddep],
    link_with: rtlib, c_args: '-DROXTERM_CAPPLET')

roxterm = executable('roxterm', ['about.c', 'main.c', 'multitab.c',
    'multitab-close-button.c', 'multitab-label.c', 'menutree.c', 'optsdbus.c',
    'roxterm.c', 'roxterm-regex.c', 'search.c', 'session-file.c',
    'shortcuts.c', 'uri.c'],
    dependencies: common_deps + [vtedep, pcredep],
    link_with: rtlib)
