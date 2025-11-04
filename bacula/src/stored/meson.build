# Build stored daemon (bacula-sd)
# This is more complex as it needs libbacsd, libbaccfg, libbacfind

# LIBBACSD_SRCS - library sources
libbacsd_sources = [
  'acquire.c',
  'ansi_label.c',
  'askdir.c',
  'autochanger.c',
  'block.c',
  'block_util.c',
  'butil.c',
  'dev.c',
  'device.c',
  'ebcdic.c',
  'init_dev.c',
  'label.c',
  'lock.c',
  'match_bsr.c',
  'mount.c',
  'null_dev.c',
  'os.c',
  'parse_bsr.c',
  'read.c',
  'read_records.c',
  'record_read.c',
  'record_util.c',
  'record_write.c',
  'reserve.c',
  'scan.c',
  'sd_plugins.c',
  'spool.c',
  'tape_alert.c',
  'vol_mgr.c',
  'wait.c',
  'tape_worm.c',
  'fifo_dev.c',
  'file_dev.c',
  'tape_dev.c',
  'vtape_dev.c',
  # Additional files needed
  'org_libsd_filemedia.c',
  'org_libsd_quota.c',
  'org_libsd_sir.c',
  'org_stored_dedup.c',
  'org_stored_prepare.c',
  # Global variables needed by library (stub definitions)
  'libbacsd_globals.c',
]

# Create wrapper files for libbacsd
libbacsd_wrappers = []
foreach src : libbacsd_sources
  wrapper = configure_file(
    input: src,
    output: '@BASENAME@_wrapper.cpp',
    copy: true,
  )
  libbacsd_wrappers += wrapper
endforeach

# Build libbacsd library
libbacsd = library('bacsd',
  libbacsd_wrappers,
  include_directories: [src_inc, top_inc],
  dependencies: [
    libbac_dep,
    libbacfind_dep,
  ],
  install: false,
)

libbacsd_dep = declare_dependency(
  link_with: libbacsd,
  include_directories: [src_inc, top_inc],
)

# SDOBJS - main executable sources
sd_sources = [
  'stored.c',
  'append.c',
  'authenticate.c',
  'store_mngr.c',
  'dircmd.c',
  'fd_cmds.c',
  'job.c',
  'hello.c',
  'status.c',
  'vbackup.c',
  'sdcollect.c',
  'stored_conf.c',
  'global.c',
]

# Include cloud_dev.c and related cloud files in the main executable
sd_sources += [
  'cloud_dev.c',
  'file_driver.c',  # Needed by cloud_dev
  'cloud_parts.c',  # Part of CLOUD_COMMON_SRCS
  'cloud_transfer_mgr.c',  # Part of CLOUD_COMMON_SRCS
]

# Create wrapper files for bacula-sd
sd_wrappers = []
foreach src : sd_sources
  wrapper = configure_file(
    input: src,
    output: '@BASENAME@_wrapper.cpp',
    copy: true,
  )
  sd_wrappers += wrapper
endforeach

# Dependencies
math_dep = meson.get_compiler('cpp').find_library('m', required: false)
dl_dep = meson.get_compiler('cpp').find_library('dl', required: false)
zlib_dep = dependency('zlib', required: false)

# Build bacula-sd executable
bacula_sd = executable('bacula-sd',
  sd_wrappers,
  include_directories: [src_inc, top_inc],
  dependencies: [
    libbacsd_dep,
    libbac_dep,
    libbacfind_dep,
    math_dep,
    dl_dep,
    zlib_dep,
    dependency('openssl', required: false),
    dependency('libcap', required: false),
  ],
  install: false,
)

# Build cloud driver plugin
# CLOUD_ALL_SRCS = cloud_parts.c cloud_transfer_mgr.c cloud_dev.c file_driver.c
# This is a loadable plugin that exports BaculaSDdriver() function
cloud_driver_sources = [
  'cloud_parts.c',
  'cloud_transfer_mgr.c',
  'cloud_dev.c',
  'file_driver.c',
]

# Create wrapper files for cloud driver
cloud_driver_wrappers = []
foreach src : cloud_driver_sources
  wrapper = configure_file(
    input: src,
    output: '@BASENAME@_driver_wrapper.cpp',
    copy: true,
  )
  cloud_driver_wrappers += wrapper
endforeach

# Build the cloud driver as a shared library (plugin)
# Note: The naming convention is bacula-sd-cloud-*-driver.so
# The plugin exports BaculaSDdriver() function
bacula_sd_cloud_driver = shared_library('bacula-sd-cloud-driver',
  cloud_driver_wrappers,
  include_directories: [src_inc, top_inc],
  dependencies: [
    libbacsd_dep,
    libbac_dep,
    libbacfind_dep,
    math_dep,
    dl_dep,
    zlib_dep,
    dependency('openssl', required: false),
  ],
  install: false,
  name_prefix: '',  # Don't add 'lib' prefix
)

