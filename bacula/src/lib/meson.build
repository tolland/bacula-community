# Minimal subset - just a few simple utility files
# This is a minimal experiment to get something building
# Note: These .c files include C++ headers, so we need to compile them as C++
# Workaround: create wrapper .cpp files that include the .c files

# Create wrapper files that include the C sources
base64_wrapper = configure_file(
  input: 'base64.c',
  output: 'base64_wrapper.cpp',
  copy: true,
)

md5_wrapper = configure_file(
  input: 'md5.c',
  output: 'md5_wrapper.cpp',
  copy: true,
)

bsnprintf_wrapper = configure_file(
  input: 'bsnprintf.c',
  output: 'bsnprintf_wrapper.cpp',
  copy: true,
)

libbac = library('bac',
  base64_wrapper, md5_wrapper, bsnprintf_wrapper,
  include_directories: [src_inc, top_inc],
  install: false,
)

libbac_dep = declare_dependency(
  link_with: libbac,
  include_directories: [src_inc, top_inc],
)

