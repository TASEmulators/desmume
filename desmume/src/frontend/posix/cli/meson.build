cli_src = [
  'main.cpp',
  '../shared/sndsdl.cpp',
  '../shared/ctrlssdl.cpp',
]

# TODO: why do we have to redeclare it here with one more fs level?
includes = include_directories(
  '../../../../src',
  '../../../../src/libretro-common/include',
  '../../../../src/frontend',
)

executable('desmume-cli',
  cli_src,
  dependencies: dependencies,
  include_directories: includes,
  link_with: libdesmume,
  install: true,
)

install_man('doc/desmume-cli.1')
