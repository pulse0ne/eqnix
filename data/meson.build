conf_glade = configuration_data()
conf_glade.set('PROJECT_VERSION', meson.project_version())

gresources = gnome_mod.compile_resources(
    'eqnix-resources',
    'eqnix.gresource.xml',
    c_name: meson.project_name(),
    dependencies: []
)

# dbusconf = configuration_data()
# dbusconf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))

# configure_file()
