conf_data = configuration_data()
foreach a: ['prefix', 'bindir', 'datadir', 'localedir', 'sysconfdir']
  conf_data.set(a, get_option(a))
endforeach
configure_file(input: 'config.h.in',
    output: 'config.h',
    configuration: conf_data)

version_data = configuration_data()
version_data.set('version', version)
configure_file(input: 'version.h.in',
    output: 'version.h',
    configuration: version_data)

