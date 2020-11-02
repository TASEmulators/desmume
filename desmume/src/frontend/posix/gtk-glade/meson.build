dep_gtk2 = dependency('gtk+-2.0')
dep_glade = dependency('libglade-2.0')
dep_gmodule = dependency('gmodule-2.0')
dep_gtkglext = dependency('gtkglext-1.0')
dep_x11 = dependency('x11')

gtk_dependencies = dependencies + [dep_gtk2, dep_glade, dep_gmodule, dep_gtkglext, dep_x11]

desmume_src = [
  'glade-xml.cpp',
  'callbacks.cpp',
  'callbacks_IO.cpp',
  'desmume.cpp',
  'keyval_names.cpp',
  'main.cpp',
  'gdk_gl.cpp',
  'dTools/callbacks_1_ioregs.cpp',
  'dTools/callbacks_2_memview.cpp',
  'dTools/callbacks_3_palview.cpp',
  'dTools/callbacks_4_tileview.cpp',
  'gdk_3Demu.cpp',
  '../shared/sndsdl.cpp',
  '../shared/ctrlssdl.cpp',
  '../../../driver.cpp',
]

# TODO: why do we have to redeclare it here with one more fs level?
includes = include_directories(
  '../../../../src',
  '../../../../src/libretro-common/include',
  '../../../../src/frontend',
)

executable('desmume-glade',
  desmume_src,
  cpp_args: ['-DGETTEXT_PACKAGE="desmume"', '-DLOCALEDIR="' + get_option('localedir') + '"', '-DVERSION="Unknown version"', '-DGTKGLEXT_AVAILABLE'],
  dependencies: gtk_dependencies,
  include_directories: includes,
  link_with: libdesmume,
  install: true,
)

install_data('desmume-glade.desktop', install_dir: join_paths(get_option('datadir'), 'applications'))
install_data('glade/DeSmuME.xpm', install_dir: join_paths(get_option('datadir'), 'pixmaps'))
install_data('doc/desmume-glade.1', install_dir: join_paths(get_option('datadir'), 'man', 'man1'))
install_data(['glade/DeSmuMe_Dtools.glade', 'glade/DeSmuMe.glade', 'glade/DeSmuME.xpm'], install_dir: join_paths(get_option('datadir'), 'glade'))
