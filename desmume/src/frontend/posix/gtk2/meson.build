dep_gtk3 = dependency('gtk+-3.0')
dep_x11 = dependency('x11')

gtk_dependencies = dependencies + [dep_gtk3, dep_x11]

desmume_src = [
  'avout_pipe_base.cpp',
  'avout_x264.cpp',
  'avout_flac.cpp',
  'config.cpp',
  'desmume.cpp',
  'dToolsList.cpp',
  'tools/ioregsView.cpp',
  '../shared/sndsdl.cpp',
  '../shared/ctrlssdl.cpp',
  'osmesa_3Demu.cpp',
  'sdl_3Demu.cpp',
  'cheatsGTK.cpp',
  'main.cpp',
]

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
