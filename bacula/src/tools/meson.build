# Build tools - starting with bsmtp
# Note: This assumes libbac_dep is available from src/lib/meson.build

# Dependencies
math_dep = meson.get_compiler('cpp').find_library('m', required: false)
openssl_dep = dependency('openssl', required: false)

# bsmtp tool
# Note: bsmtp.c needs to be compiled as C++ (includes C++ headers)
bsmtp_wrapper = configure_file(
  input: 'bsmtp.c',
  output: 'bsmtp_wrapper.cpp',
  copy: true,
)

bsmtp = executable('bsmtp',
  bsmtp_wrapper,
  include_directories: [src_inc, top_inc],
  dependencies: [
    libbac_dep,
    math_dep,
    openssl_dep,
  ],
  install: false,
)

