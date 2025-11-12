libwlblur_sources = files(
  'src/blur_kawase.c',
  'src/blur_context.c',
  'src/egl_helpers.c',
  'src/dmabuf.c',
  'src/shaders.c',
  'src/framebuffer.c',
  'src/utils.c',
)

libwlblur_deps = [
  egl_dep,
  glesv2_dep,
  libdrm_dep,
]

libwlblur_includes = include_directories('include')

libwlblur = library(
  'wlblur',
  libwlblur_sources,
  dependencies: libwlblur_deps,
  include_directories: libwlblur_includes,
  version: meson.project_version(),
  install: true,
)

# Install headers
install_headers(
  'include/wlblur/wlblur.h',
  'include/wlblur/blur_context.h',
  'include/wlblur/blur_params.h',
  'include/wlblur/dmabuf.h',
  subdir: 'wlblur',
)

# Declare dependency for internal use
libwlblur_dep = declare_dependency(
  link_with: libwlblur,
  include_directories: libwlblur_includes,
)
