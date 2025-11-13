wlblurd_sources = files(
  'src/main.c',
  'src/ipc.c',
  'src/ipc_protocol.c',
  'src/client.c',
  'src/blur_node.c',
  'src/buffer_registry.c',
  'src/config.c',
  'src/presets.c',
  'src/reload.c',
)

# Bundle tomlc99 source instead of external dependency
# This eliminates dependency issues on distros where tomlc99 is not packaged
tomlc99_sources = files(
  'vendor/tomlc99/toml.c'
)

wlblurd_sources += tomlc99_sources

wlblurd_deps = [
  libwlblur_dep,
]

wlblurd_includes = [
  include_directories('include'),
  include_directories('vendor/tomlc99'),
]

wlblurd = executable(
  'wlblurd',
  wlblurd_sources,
  dependencies: wlblurd_deps,
  include_directories: wlblurd_includes,
  install: true,
)
