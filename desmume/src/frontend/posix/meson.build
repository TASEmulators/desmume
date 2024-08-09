project('desmume',
  ['c', 'cpp'],
  version: '2.7.10',
  meson_version: '>=0.45',
  default_options: [
    'warning_level=1',
    'c_std=c11',
    'cpp_std=c++14',
  ],
  license: 'GPL2+',
)

dep_glib2 = dependency('glib-2.0')
dep_sdl = dependency('sdl2')
dep_pcap = dependency('pcap')
dep_zlib = dependency('zlib')
dep_threads = dependency('threads')
dep_gl = dependency('gl', required: false)
dep_gles = dependency('glesv2', required: false)
dep_openal = dependency('openal', required: get_option('openal'))
dep_alsa = dependency('alsa', required: false)
dep_soundtouch = dependency('soundtouch', required: false)
dep_agg = dependency('libagg', required: false)
dep_fontconfig = dependency('fontconfig', required: false)
dep_osmesa = dependency('osmesa', required: false)
dep_egl = dependency('egl', required: false)

# XXX: something wrong with this one.
#dep_lua = dependency('lua-5.1', required: false)

if get_option('wifi')
  add_global_arguments('-DEXPERIMENTAL_WIFI_COMM', language: ['c', 'cpp'])
endif

dependencies = [dep_glib2, dep_sdl, dep_pcap, dep_zlib, dep_threads]

# Determine the CPU architecture of the target.
target_cpu_64bit = false
target_cpu_kind_x86 = false
target_cpu_kind_arm = false
target_cpu_kind_powerpc = false
target_cpu_kind_unknown = false

if target_machine.cpu() == 'x86_64' or \
   target_machine.cpu() == 'amd64'
  target_cpu_kind_x86 = true
  target_cpu_64bit = true
elif target_machine.cpu() == 'x86' or \
     target_machine.cpu() == 'i686' or \
     target_machine.cpu() == 'i586' or \
     target_machine.cpu() == 'i486' or \
     target_machine.cpu() == 'i386'
  target_cpu_kind_x86 = true
elif target_machine.cpu() == 'aarch64' or \
     target_machine.cpu() == 'arm64'
  target_cpu_kind_arm = true
  target_cpu_64bit = true
elif target_machine.cpu() == 'arm'
  target_cpu_kind_arm = true
elif target_machine.cpu() == 'ppc64'
  target_cpu_kind_powerpc = true
  target_cpu_64bit = true
elif target_machine.cpu() == 'powerpc'
  target_cpu_kind_powerpc = true
else
  target_cpu_kind_unknown = true
endif

if target_cpu_64bit
  add_global_arguments('-DHOST_64', language: ['c', 'cpp'])
else
  add_global_arguments('-DHOST_32', language: ['c', 'cpp'])
endif

# Enable JIT for supported architectures.
if target_cpu_kind_x86 or target_cpu_kind_arm
  have_jit = true
  add_global_arguments('-DHAVE_JIT', language: ['c', 'cpp'])
else
  have_jit = false
endif

includes = include_directories(
  '../../../src',
  '../../../src/libretro-common/include',
  '../../../src/frontend',
)

libdesmume_src = [
  '../../armcpu.cpp',
  '../../arm_instructions.cpp',
  '../../bios.cpp',
  '../../cp15.cpp',
  '../../commandline.cpp',
  '../../common.cpp',
  '../../debug.cpp',
  '../../driver.cpp',
  '../../Database.cpp',
  '../../emufile.cpp', '../../encrypt.cpp', '../../FIFO.cpp',
  '../../firmware.cpp', '../../GPU.cpp',
  '../../mc.cpp',
  '../../path.cpp',
  '../../readwrite.cpp',
  '../../wifi.cpp',
  '../../MMU.cpp', '../../NDSSystem.cpp',
  '../../ROMReader.cpp',
  '../../render3D.cpp',
  '../../rtc.cpp',
  '../../saves.cpp',
  '../../slot1.cpp',
  '../../slot2.cpp',
  '../../SPU.cpp',
  '../../matrix.cpp',
  '../../gfx3d.cpp',
  '../../thumb_instructions.cpp',
  '../../movie.cpp',
  '../../frontend/modules/Disassembler.cpp',
  '../../utils/advanscene.cpp',
  '../../utils/datetime.cpp',
  '../../utils/guid.cpp',
  '../../utils/emufat.cpp',
  '../../utils/fsnitro.cpp',
  '../../utils/xstring.cpp',
  '../../utils/decrypt/crc.cpp', '../../utils/decrypt/decrypt.cpp',
  '../../utils/decrypt/header.cpp',
  '../../utils/task.cpp',
  '../../utils/vfat.cpp',
  '../../utils/dlditool.cpp',
  '../../utils/libfat/cache.cpp',
  '../../utils/libfat/directory.cpp',
  '../../utils/libfat/disc.cpp',
  '../../utils/libfat/fatdir.cpp',
  '../../utils/libfat/fatfile.cpp',
  '../../utils/libfat/filetime.cpp',
  '../../utils/libfat/file_allocation_table.cpp',
  '../../utils/libfat/libfat.cpp',
  '../../utils/libfat/libfat_public_api.cpp',
  '../../utils/libfat/lock.cpp',
  '../../utils/libfat/partition.cpp',
  '../../utils/tinyxml/tinystr.cpp',
  '../../utils/tinyxml/tinyxml.cpp',
  '../../utils/tinyxml/tinyxmlerror.cpp',
  '../../utils/tinyxml/tinyxmlparser.cpp',
  '../../utils/colorspacehandler/colorspacehandler.cpp',
  '../../addons/slot2_auto.cpp', '../../addons/slot2_mpcf.cpp', '../../addons/slot2_paddle.cpp', '../../addons/slot2_gbagame.cpp', '../../addons/slot2_none.cpp', '../../addons/slot2_rumblepak.cpp', '../../addons/slot2_guitarGrip.cpp', '../../addons/slot2_hcv1000.cpp', '../../addons/slot2_expMemory.cpp', '../../addons/slot2_piano.cpp', '../../addons/slot2_passme.cpp', '../../addons/slot1_none.cpp', '../../addons/slot1_r4.cpp', '../../addons/slot1_retail_nand.cpp', '../../addons/slot1_retail_auto.cpp', '../../addons/slot1_retail_mcrom.cpp', '../../addons/slot1_retail_mcrom_debug.cpp', '../../addons/slot1comp_mc.cpp', '../../addons/slot1comp_rom.cpp', '../../addons/slot1comp_protocol.cpp',
  '../../cheatSystem.cpp',
  '../../texcache.cpp', '../../rasterize.cpp',
  '../../metaspu/metaspu.cpp',
  '../../filter/2xsai.cpp', '../../filter/bilinear.cpp', '../../filter/deposterize.cpp', '../../filter/epx.cpp',
  '../../filter/hq2x.cpp',
  '../../filter/hq3x.cpp',
  '../../filter/hq4x.cpp',
  '../../filter/lq2x.cpp', '../../filter/scanline.cpp',
  '../../filter/videofilter.cpp',
  '../../filter/xbrz.cpp',
  '../../version.cpp',
  '../../libretro-common/compat/compat_getopt.c',
  '../../libretro-common/file/file_path.c',
  '../../libretro-common/compat/compat_strl.c',
  '../../libretro-common/features/features_cpu.c',
  '../../libretro-common/file/retro_dirent.c',
  '../../libretro-common/file/retro_stat.c',
  '../../libretro-common/rthreads/async_job.c',
  '../../libretro-common/rthreads/rsemaphore.c',
  '../../libretro-common/rthreads/rthreads.c',
  '../../libretro-common/encodings/encoding_utf.c',
]

if have_jit
  if target_cpu_kind_x86
    libdesmume_src += [
      '../../arm_jit.cpp',
      '../../utils/AsmJit/core/assembler.cpp',
      '../../utils/AsmJit/core/assert.cpp',
      '../../utils/AsmJit/core/buffer.cpp',
      '../../utils/AsmJit/core/compiler.cpp',
      '../../utils/AsmJit/core/compilercontext.cpp',
      '../../utils/AsmJit/core/compilerfunc.cpp',
      '../../utils/AsmJit/core/compileritem.cpp',
      '../../utils/AsmJit/core/context.cpp',
      '../../utils/AsmJit/core/cpuinfo.cpp',
      '../../utils/AsmJit/core/defs.cpp',
      '../../utils/AsmJit/core/func.cpp',
      '../../utils/AsmJit/core/logger.cpp',
      '../../utils/AsmJit/core/memorymanager.cpp',
      '../../utils/AsmJit/core/memorymarker.cpp',
      '../../utils/AsmJit/core/operand.cpp',
      '../../utils/AsmJit/core/stringbuilder.cpp',
      '../../utils/AsmJit/core/stringutil.cpp',
      '../../utils/AsmJit/core/virtualmemory.cpp',
      '../../utils/AsmJit/core/zonememory.cpp',
      '../../utils/AsmJit/x86/x86assembler.cpp',
      '../../utils/AsmJit/x86/x86compiler.cpp',
      '../../utils/AsmJit/x86/x86compilercontext.cpp',
      '../../utils/AsmJit/x86/x86compilerfunc.cpp',
      '../../utils/AsmJit/x86/x86compileritem.cpp',
      '../../utils/AsmJit/x86/x86cpuinfo.cpp',
      '../../utils/AsmJit/x86/x86defs.cpp',
      '../../utils/AsmJit/x86/x86func.cpp',
      '../../utils/AsmJit/x86/x86operand.cpp',
      '../../utils/AsmJit/x86/x86util.cpp',
    ]
  elif target_cpu_kind_arm
    libdesmume_src += [
      '../../utils/arm_jit/arm_jit_arm.cpp',
    ]
  endif
endif

libdesmume_src += [
  'shared/desmume_config.cpp',
]

if get_option('opengles')
  if dep_gles.found()
    add_global_arguments('-DENABLE_OPENGL_ES', language: ['c', 'cpp'])
    dependencies += dep_gles
    libdesmume_src += [
      '../../OGLRender.cpp',
      '../../OGLRender_3_2.cpp',
      '../../OGLRender_ES3.cpp',
    ]
  else
    message('Cannot build with OpenGL ES -- glesv2 library not found.')
  endif
elif get_option('opengl')
  if dep_gl.found()
    add_global_arguments('-DENABLE_OPENGL_STANDARD', language: ['c', 'cpp'])
    dependencies += dep_gl
    libdesmume_src += [
      '../../OGLRender.cpp',
      '../../OGLRender_3_2.cpp',
    ]
  else
    message('Cannot build with OpenGL -- gl library not found.')
  endif
endif

if get_option('glx')
  if dep_gl.found()
    if get_option('opengl')
      add_global_arguments('-DENABLE_GLX', language: ['c', 'cpp'])
    elif get_option('opengles')
      message('GLX contexts are incompatible with OpenGL ES -- cancelling the use of GLX.')
    else
      message('GLX contexts are only compatible with standard OpenGL -- cancelling the use of GLX.')
    endif
  else
    message('Cannot use GLX -- gl library not found.')
  endif
elif get_option('osmesa')
  if dep_osmesa.found()
    if get_option('opengl') and dep_gl.found()
      message('OSMesa contexts are deprecated.')
      add_global_arguments('-DENABLE_OSMESA', language: ['c', 'cpp'])
      dependencies += dep_osmesa
    elif get_option('opengles')
      message('OSMesa contexts are incompatible with OpenGL ES -- cancelling the use of OSMesa.')
    else
      message('OSMesa contexts are only compatible with standard OpenGL -- cancelling the use of OSMesa.')
    endif
  else
    message('Cannot use OSMesa -- osmesa library not found.')
  endif
elif get_option('egl')
  if dep_egl.found()
    add_global_arguments('-DENABLE_EGL', language: ['c', 'cpp'])
    dependencies += dep_egl
  else
    message('Cannot use EGL -- egl library not found.')
  endif
endif

if dep_openal.found()
  dependencies += dep_openal
  libdesmume_src += [
    'shared/mic_openal.cpp',
  ]
elif dep_alsa.found()
  dependencies += dep_alsa
  libdesmume_src += [
    'shared/mic_alsa.cpp',
  ]
else
  libdesmume_src += [
    '../../mic.cpp',
  ]
endif

if dep_soundtouch.found()
  dependencies += dep_soundtouch
  add_global_arguments('-DHAVE_LIBSOUNDTOUCH', language: ['c', 'cpp'])
  libdesmume_src += [
    '../../metaspu/SndOut.cpp',
    '../../metaspu/Timestretcher.cpp',
  ]
endif

if dep_agg.found()
  dependencies += dep_agg
  add_global_arguments('-DHAVE_LIBAGG', language: ['c', 'cpp'])
  if dep_fontconfig.found()
    dependencies += dep_fontconfig
    add_global_arguments('-DAGG2D_USE_VECTORFONTS', language: ['c', 'cpp'])
    add_global_link_arguments('-laggfontfreetype', language: ['c', 'cpp'])
  endif
  libdesmume_src += [
    '../modules/osd/agg/aggdraw.cpp',
    '../modules/osd/agg/agg_osd.cpp',
  ]
endif

# TODO: fix Lua on ¬Windows.
#if dep_lua.found()
#  dependencies += dep_lua
#  libdesmume_src += [
#    '../../lua-engine.cpp',
#  ]
#endif

if get_option('gdb-stub')
  add_global_arguments('-DGDB_STUB', language: ['c', 'cpp'])
  libdesmume_src += [
    '../../gdbstub/gdbstub.cpp',
  ]
endif

libdesmume = static_library('desmume',
  libdesmume_src,
  dependencies: dependencies,
  include_directories: includes,
)

if get_option('frontend-cli')
  subdir('cli')
endif
if get_option('frontend-gtk')
  subdir('gtk')
endif
