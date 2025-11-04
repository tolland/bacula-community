# Build libbacfind library
# This library is needed by bacula-sd

# All source files for libbacfind (from Makefile.in LIBBACFIND_SRCS)
findlib_sources = [
  'find.c',
  'match.c',
  'find_one.c',
  'attribs.c',
  'create_file.c',
  'bfile.c',
  'drivetype.c',
  'enable_priv.c',
  'fstype.c',
  'mkpath.c',
  'savecwd.c',
  'namedpipe.c',
  'win32filter.c',
]

# Create wrapper files for C sources (they include C++ headers)
findlib_wrappers = []
foreach src : findlib_sources
  wrapper = configure_file(
    input: src,
    output: '@BASENAME@_wrapper.cpp',
    copy: true,
  )
  findlib_wrappers += wrapper
endforeach

libbacfind = library('bacfind',
  findlib_wrappers,
  include_directories: [src_inc, top_inc],
  dependencies: [
    libbac_dep,
  ],
  install: false,
)

libbacfind_dep = declare_dependency(
  link_with: libbacfind,
  include_directories: [src_inc, top_inc],
)

