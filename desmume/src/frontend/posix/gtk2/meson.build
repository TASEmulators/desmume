dep_gtk3 = dependency('gtk+-3.0')
dep_x11 = dependency('x11')

gtk_dependencies = dependencies + [dep_gtk3, dep_x11]

desmume_src = [
  '../shared/avout_flac.cpp',
  '../shared/avout_pipe_base.cpp',
  '../shared/avout_x264.cpp',
  '../shared/ctrlssdl.cpp',
  '../shared/sndsdl.cpp',
  'config.cpp',
  'desmume.cpp',
  'dToolsList.cpp',
  'tools/ioregsView.cpp',
  'cheatsGTK.cpp',
  'main.cpp',
]

if get_option('glx') and dep_gl.found()
  desmume_src += [
    '../shared/glx_3Demu.cpp',
  ]
elif get_option('osmesa') and dep_osmesa.found()
  desmume_src += [
    '../shared/osmesa_3Demu.cpp',
  ]
elif get_option('egl') and dep_egl.found()
  desmume_src += [
    '../shared/egl_3Demu.cpp',
  ]
else
  desmume_src += [
    '../shared/sdl_3Demu.cpp',
  ]
endif

# TODO: why do we have to redeclare it here with one more fs level?
includes = include_directories(
  '../../../../src',
  '../../../../src/libretro-common/include',
  '../../../../src/frontend',
)

executable('desmume',
  desmume_src,
  dependencies: gtk_dependencies,
  include_directories: includes,
  link_with: libdesmume,
  install: true,
)

install_data('desmume.desktop', install_dir: join_paths(get_option('datadir'), 'applications'))
install_data('org.desmume.DeSmuME.metainfo.xml', install_dir: join_paths(get_option('datadir'), 'metainfo'))
install_data('DeSmuME.xpm', install_dir: join_paths(get_option('datadir'), 'pixmaps'))
install_man('doc/desmume.1')
