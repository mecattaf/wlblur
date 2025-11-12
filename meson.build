project('wlblur', 'c',
  version: '0.1.0',
  license: 'MIT',
  default_options: [
    'c_std=c99',
    'warning_level=2',
    'werror=false',
  ],
)

# Project information
project_description = 'Compositor-agnostic blur daemon for Wayland'
project_url = 'https://github.com/mecattaf/wlblur'

# Dependencies (check availability only, don't fail)
egl_dep = dependency('egl', required: false)
glesv2_dep = dependency('glesv2', required: false)
libdrm_dep = dependency('libdrm', required: false)

if egl_dep.found() and glesv2_dep.found() and libdrm_dep.found()
  message('All dependencies found - build will succeed')
else
  message('Some dependencies missing - build will fail (expected for skeleton)')
endif

# Subdirectories
subdir('libwlblur')
subdir('wlblurd')
subdir('examples')
subdir('tests')

# Summary
summary({
  'prefix': get_option('prefix'),
  'libdir': get_option('libdir'),
  'bindir': get_option('bindir'),
}, section: 'Directories')

summary({
  'EGL': egl_dep.found(),
  'GLESv2': glesv2_dep.found(),
  'libdrm': libdrm_dep.found(),
}, section: 'Dependencies')
