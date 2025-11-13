libwlblur_sources = files(
  'src/blur_kawase.c',
  'src/blur_context.c',
  'src/blur_params.c',
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

libwlblur_includes = include_directories('include', '.')

libwlblur = library(
  'wlblur',
  libwlblur_sources,
  dependencies: libwlblur_deps,
  include_directories: libwlblur_includes,
  link_args: ['-lm'],
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

# Verify all required shaders exist at build time
fs = import('fs')
required_shaders = [
  'kawase_downsample.frag.glsl',
  'kawase_upsample.frag.glsl',
  'blur_finish.frag.glsl',
  'vibrancy.frag.glsl',
  'common.glsl'
]

foreach shader : required_shaders
  shader_path = 'shaders' / shader
  if not fs.exists(shader_path)
    error('Missing required shader file: ' + shader_path)
  endif
endforeach

# Install shaders to system
install_subdir('shaders',
  install_dir: get_option('datadir') / 'wlblur',
  exclude_files: ['README.md']
)

# Declare dependency for internal use
libwlblur_dep = declare_dependency(
  link_with: libwlblur,
  include_directories: libwlblur_includes,
)
