# Tests
if get_option('tests').enabled()
  test_kawase = executable('test_kawase',
    'test_kawase.c',
    dependencies: [libwlblur_dep],
  )
  test('kawase algorithm', test_kawase)

  test_dmabuf = executable('test_dmabuf',
    'test_dmabuf.c',
    dependencies: [libwlblur_dep],
  )
  test('dmabuf operations', test_dmabuf)
endif
