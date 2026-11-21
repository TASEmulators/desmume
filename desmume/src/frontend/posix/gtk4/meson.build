dep_gtk4 = dependency('gtk4', version: '>=4.10')
gnome = import('gnome')

gtk4_args = []

# `dependencies` already contains dep_x11 (added in posix/meson.build).
gtk_dependencies = dependencies + [dep_gtk4]

# Backend-specific pkg-config files are needed for gdk/wayland/gdkwayland.h and
# gdk/x11/gdkx.h.  They are optional: without them the decoration probe falls
# back to a display type-name check and the WebGPU native-surface path stays
# disabled.
dep_gtk4_wayland = dependency('gtk4-wayland', required: false)
if dep_gtk4_wayland.found()
  gtk_dependencies += [dep_gtk4_wayland]
  gtk4_args += ['-DHAVE_GTK4_WAYLAND']
endif

dep_gtk4_x11 = dependency('gtk4-x11', required: false)
if dep_gtk4_x11.found()
  gtk_dependencies += [dep_gtk4_x11]
  gtk4_args += ['-DHAVE_GTK4_X11']
endif

# menu.ui (GMenu model), shortcuts.ui and style.css are loaded from the
# resource at runtime; the same prefix as gtk/ keeps P2/P7 shader paths
# compatible when those resources are copied over.
gresource = gnome.compile_resources(
  'gresource', 'desmume.gresource.xml',
)

desmume_src = [
  # Frontend-independent RAM Watch / RAM Search document cores, shared with
  # the GTK3/Windows/Cocoa frontends and compiled straight into the target.
  '../../shared/ram_debug_core.cpp',
  '../../shared/ramwatch_core.cpp',
  '../../shared/ram_search_core.cpp',
  '../../shared/movie_info_core.cpp',
  '../../shared/memory_viewer_core.cpp',
  '../shared/sndsdl.cpp',
  'config.cpp',
  'configdialog.cpp',
  'display.cpp',
  'emulation.cpp',
  'main.cpp',
  'tools.cpp',
  'tools/movie.cpp',
  'tools/ioregs.cpp',
  'tools/ramwatch.cpp',
  'tools/ramsearch.cpp',
  'tools/cheats.cpp',
  'tools/memoryviewer.cpp',
  'tools/luaScriptConsole.cpp',
  'tools/rowmodel.cpp',
] + gresource

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
  cpp_args: gtk4_args,
  install: true,
)

# Packaging assets (P8).  Only one GTK frontend is built per Meson config, so
# the GTK4 launcher/appdata use their own app-id (org.desmume.DeSmuME.GTK4)
# and can coexist with the GTK3 files in a shared prefix.  The man page and
# icon keep the shared names: there is exactly one `desmume` binary installed
# at a time, so `man desmume` never points at two frontends.
install_data('org.desmume.DeSmuME.GTK4.desktop',
  install_dir: join_paths(get_option('datadir'), 'applications'))
install_data('org.desmume.DeSmuME.GTK4.metainfo.xml',
  install_dir: join_paths(get_option('datadir'), 'metainfo'))
install_data('../gtk/org.desmume.DeSmuME.svg',
  install_dir: join_paths(get_option('datadir'), 'icons', 'hicolor', 'scalable', 'apps'))
install_man('doc/desmume.1')
