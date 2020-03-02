plugin_sources = [
    'iirequalizer.c',
    'iirequalizernbands.c'
]

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: true)

plugin_deps = [
    dependency('gstreamer-1.0'),
    dependency('gstreamer-base-1.0'),
    # dependency('gstreamer-controller-1.0'),
    dependency('gstreamer-audio-1.0'),
    m_dep
]

library(
    'iirequealizer',
    plugin_sources,
    include_directories: [include_dir, config_h_dir],
    dependencies: plugin_deps,
    install: true,
    install_dir: plugins_install_dir,
    cpp_args: plugins_cxx_args
)
