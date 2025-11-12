# Examples are optional
if get_option('examples').enabled()
  blur_png = executable('blur-png',
    'blur-png.c',
    dependencies: [libwlblur_dep],
  )

  ipc_example = executable('ipc-client-example',
    'ipc-client-example.c',
  )
endif
