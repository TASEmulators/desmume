project('desmume',
  ['c', 'cpp'],
  version: '2.7.10',
  meson_version: '>=0.54',
  default_options: [
    'warning_level=1',
    'c_std=c11',
    'cpp_std=c++14',
  ],
  license: 'GPL2+',
)

dep_glib2 = dependency('glib-2.0')
dep_sdl = dependency('sdl3')
# Distros disagree on the pkg-config name (libpcap.pc vs pcap.pc), and some
# only ship the bare library, so try all three lookups before giving up.
dep_pcap = dependency('libpcap', required: false)
if not dep_pcap.found()
  dep_pcap = dependency('pcap', required: false)
endif
if not dep_pcap.found()
  dep_pcap = meson.get_compiler('cpp').find_library('pcap', required: false)
endif
dep_zlib = dependency('zlib')
dep_threads = dependency('threads')
dep_soundtouch = dependency('soundtouch', required: false)

if host_machine.system() == 'darwin'
  dep_gl = dependency('appleframeworks', modules: 'OpenGL', required: false)
  dep_gles = dependency('appleframeworks', modules: 'OpenGLES', required: false)
else
  dep_gl = dependency('gl', required: false)
  dep_gles = dependency('glesv2', required: false)
endif

if get_option('wifi')
  add_global_arguments('-DEXPERIMENTAL_WIFI_COMM', language: ['c', 'cpp'])
endif

# libpcap is only needed for Infrastructure Wi-Fi bridging; the core still
# builds without it and Ad-hoc networking uses plain sockets.
if dep_pcap.found()
  add_global_arguments('-DHAVE_PCAP=1', language: ['c', 'cpp'])
  message('libpcap: enabled (Infrastructure Wi-Fi bridging available)')
else
  message('libpcap: disabled (Infrastructure Wi-Fi bridging unavailable)')
endif

add_global_arguments('-DTARGET_INTERFACE', language: ['c', 'cpp'])

dependencies = [dep_glib2, dep_sdl, dep_zlib, dep_threads]
if dep_pcap.found()
  dependencies += dep_pcap
endif

if target_machine.cpu() == 'x86_64'
  add_global_arguments('-DHAVE_JIT', language: ['c', 'cpp'])
  add_global_arguments('-DHOST_64', language: ['c', 'cpp'])
  have_jit = true
elif target_machine.cpu() == 'i686'
  add_global_arguments('-DHAVE_JIT', language: ['c', 'cpp'])
  add_global_arguments('-DHOST_32', language: ['c', 'cpp'])
  have_jit = true
else
  have_jit = false
endif

# Clang refuses to compile with this warning
if meson.get_compiler('cpp').get_id() == 'clang'
  add_global_arguments('-Wno-c++11-narrowing', language: ['c', 'cpp'])
endif

includes = include_directories(
  '../../../src',
  '../../../src/libretro-common/include',
  '../../../src/frontend',
)

libdesmume_src = ['interface.cpp', 'draw_sdl_window.cpp']

libdesmume_src += [
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
  '../../utils/decrypt/decrypt.cpp',
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
  '../../utils/tinyxml2/tinyxml2.cpp',
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

libdesmume_src += [
  '../posix/shared/sndsdl.cpp',
  '../posix/shared/ctrlssdl.cpp'
]

# The JIT front-end is compiled in a separate C++20 target below, together
# with vendored asmjit; it is linked back in with link_whole.

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
    message('Cannot build with OpenGL ES -- library not found.')
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
    message('Cannot build with OpenGL -- library not found.')
  endif
endif

# Microphone capture uses SDL3, which this target already depends on.
libdesmume_src += [
  '../posix/shared/mic_sdl.cpp',
]

soundtouch_includes = []
if dep_soundtouch.found()
  dependencies += dep_soundtouch
else
  # Use the bundled SoundTouch sources when the system library is unavailable.
  # This keeps the "P" synchronization method available on platforms such as
  # ARM64, where a soundtouch development package may not be installed.
  soundtouch_includes = [include_directories('../../metaspu/SoundTouch')]
  libdesmume_src += [
    '../../metaspu/SoundTouch/AAFilter.cpp',
    '../../metaspu/SoundTouch/cpu_detect_x86.cpp',
    '../../metaspu/SoundTouch/FIFOSampleBuffer.cpp',
    '../../metaspu/SoundTouch/FIRFilter.cpp',
    '../../metaspu/SoundTouch/InterpolateCubic.cpp',
    '../../metaspu/SoundTouch/InterpolateLinear.cpp',
    '../../metaspu/SoundTouch/InterpolateShannon.cpp',
    '../../metaspu/SoundTouch/mmx_optimized.cpp',
    '../../metaspu/SoundTouch/RateTransposer.cpp',
    '../../metaspu/SoundTouch/SoundTouch.cpp',
    '../../metaspu/SoundTouch/sse_optimized.cpp',
    '../../metaspu/SoundTouch/TDStretch.cpp',
  ]
endif

add_global_arguments('-DHAVE_LIBSOUNDTOUCH', language: ['c', 'cpp'])
libdesmume_src += [
  '../../metaspu/SndOut.cpp',
  '../../metaspu/Timestretcher.cpp',
]

if get_option('gdb-stub')
  add_global_arguments('-DGDB_STUB', language: ['c', 'cpp'])
  libdesmume_src += [
    '../../gdbstub/gdbstub.cpp',
  ]
endif

simd = import('unstable-simd')
cxx = meson.get_compiler('cpp')
rval = simd.check('colorspacehandler',
  sse2: '../../utils/colorspacehandler/colorspacehandler_SSE2.cpp',
  # TODO: add these two even in generic builds.
  #avx2: '../../utils/colorspacehandler/colorspacehandler_AVX2.cpp',
  #avx512: '../../utils/colorspacehandler/colorspacehandler_AVX512.cpp',
  # TODO: add support for this in meson.
  #altivec: '../../utils/colorspacehandler/colorspacehandler_AltiVec.cpp',
  compiler: cxx,
  include_directories: includes,
)
colorspacehandler_simd = rval[0]

# asmjit and the ARM JIT front-end need C++20, while the rest of the emulator
# stays on C++14.  Build them as separate archives and merge the objects into
# libdesmume with link_whole: core code calls the JIT and the JIT calls core
# code, so a plain link_with would leave a circular dependency between two
# static archives.  This has to sit after the last add_global_arguments() call.
jit_link_whole = []
if have_jit
  # Upstream includes look like <asmjit/core.h>, so the include root is
  # 3rdparty, not 3rdparty/asmjit.
  asmjit_inc = include_directories('../../../3rdparty')
  asmjit_args = ['-DASMJIT_STATIC']
  asmjit_std = ['cpp_std=c++20']

  # tools/jit_matrix/gen_asmjit_sources.py is the single source of truth for
  # the vendored source list (the MSVC and Xcode generators use it too), so
  # adding a new upstream .cpp does not require editing this file as well.
  python3 = import('python').find_installation('python3')
  asmjit_list = run_command(
    python3,
    files('../../../../tools/jit_matrix/gen_asmjit_sources.py'),
    '--format', 'txt',
    check: true,
  ).stdout().strip()

  asmjit_src = []
  foreach src : asmjit_list.split('\n')
    asmjit_src += ['../../../' + src]
  endforeach

  libasmjit = static_library('asmjit',
    asmjit_src,
    include_directories: asmjit_inc,
    cpp_args: asmjit_args,
    override_options: asmjit_std,
    dependencies: dep_threads,
  )

  libdesmume_jit = static_library('desmume_jit',
    ['../../arm_jit.cpp'],
    include_directories: [includes, asmjit_inc],
    cpp_args: asmjit_args,
    override_options: asmjit_std,
    dependencies: dep_threads,
  )

  jit_link_whole = [libdesmume_jit, libasmjit]
endif

library('desmume',
  libdesmume_src,
  dependencies: dependencies,
  include_directories: [includes] + soundtouch_includes,
  link_with: colorspacehandler_simd,
  link_whole: jit_link_whole,
)
