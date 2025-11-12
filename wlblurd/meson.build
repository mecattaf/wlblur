wlblurd_sources = files(
  'src/main.c',
  'src/ipc.c',
  'src/ipc_protocol.c',
  'src/client.c',
  'src/blur_node.c',
  'src/buffer_registry.c',
  'src/config.c',
)

wlblurd_deps = [
  libwlblur_dep,
]

wlblurd_includes = include_directories('include')

wlblurd = executable(
  'wlblurd',
  wlblurd_sources,
  dependencies: wlblurd_deps,
  include_directories: wlblurd_includes,
  install: true,
)
