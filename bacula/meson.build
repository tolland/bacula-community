project('bacula', 'c', 'cpp',
  version: '15.0.3',
  license: 'AGPLv3',
  meson_version: '>= 0.60.0',
  default_options: [
    'c_std=c99',
    'cpp_std=c++11',
    'warning_level=1',
  ]
)

# Include directories
src_inc = include_directories('src')
top_inc = include_directories('.')

# Minimal configuration header
config_h = configuration_data()
config_h.set_quoted('VERSION', meson.project_version())

configure_file(
  output: 'config.h',
  configuration: config_h,
  install: false
)

# Subdirectories
subdir('src/lib')

