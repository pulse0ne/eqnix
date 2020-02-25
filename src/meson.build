eqnix_sources = [
    'main.cpp',
    'application.cpp',
    'application_ui.cpp',
    'fr_plot.cpp',
    'util.cpp',
    'logger.cpp',
    'pa_manager.cpp',
    'pipeline.cpp',
    'equalizer.cpp',
    gresources
]

eqnix_deps = [
    dependency('libpulse'),
    dependency('glib-2.0', version: '>=2.56'),
    dependency('glibmm-2.4', version: '>=2.56'),
    dependency('gtk+-3.0', version: '>=3.20'),
    dependency('gtkmm-3.0', version: '>=3.20'),
    dependency('sigc++-2.0', version: ['>=2.10', '<3']),
    dependency('gstreamer-1.0', version: '>=1.10.4'),
    # dependency('sndfile'),
    dependency('threads')
]

executable(
    meson.project_name(),
    eqnix_sources,
    include_directories: [include_dir, config_h_dir],
    dependencies: eqnix_deps,
    install: true,
    native: true
)
