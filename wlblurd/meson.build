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

# TOML parsing library
toml_dep = dependency('toml', required: false)
if not toml_dep.found()
  toml_dep = dependency('tomlc99', required: false)
endif
if not toml_dep.found()
  # Fallback: use pkg-config to find toml
  toml_dep = dependency('toml', method: 'pkg-config', required: true)
endif

wlblurd_deps = [
  libwlblur_dep,
  toml_dep,
]

wlblurd_includes = include_directories('include')

wlblurd = executable(
  'wlblurd',
  wlblurd_sources,
  dependencies: wlblurd_deps,
  include_directories: wlblurd_includes,
  install: true,
)
