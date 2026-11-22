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

fs = import('fs')
cxx = meson.get_compiler('cpp')

# Which bundled wgpu-native to use. Termux runs a native Android toolchain, so
# uname -s still reports Linux; uname -o is the reliable signal there. An
# NDK cross-build can set -Dwgpu-platform=android explicitly instead.
wgpu_platform = get_option('wgpu-platform')
if wgpu_platform == 'auto'
  if host_machine.system() == 'android'
    wgpu_platform = 'android'
  else
    uname_os = run_command('uname', '-o', check: false).stdout()
    if uname_os.contains('Android') or uname_os.contains('android')
      wgpu_platform = 'android'
    else
      wgpu_platform = 'linux'
    endif
  endif
endif
if (wgpu_platform == 'android') and (host_machine.cpu_family() != 'aarch64')
  warning('The bundled Android wgpu-native is aarch64 only, but the host cpu family is ' + host_machine.cpu_family())
endif
message('wgpu platform: ' + wgpu_platform)

dep_glib2 = dependency('glib-2.0')
dep_sdl = dependency('sdl3')
dep_pcap = dependency('pcap')
dep_zlib = dependency('zlib')
dep_threads = dependency('threads')
dep_gl = dependency('gl', required: false)
dep_gles = dependency('glesv2', required: false)
dep_soundtouch = dependency('soundtouch', required: false)
# AGG is compiled from the bundled Windows sources below. Distributions have
# dropped libagg (Fedora no longer ships it), and this is the exact AGG version
# the OSD was written against. freetype2 is only used for vector fonts.
dep_freetype = dependency('freetype2', required: false)
dep_fontconfig = dependency('fontconfig', required: false)
dep_egl = dependency('egl', required: false)
# wayland-client is only linked by the WebGPU surface adapter. Keep it
# optional when WebGPU is disabled so a minimal build does not need it.
dep_wayland = dependency('wayland-client', required: get_option('wgpu'))
dep_x11 = dependency('x11')

# The FFmpeg recorder is optional. When any of the libav* modules is missing,
# or -Dffmpeg-recording=false is passed, the feature is compiled out and the
# old x264/FLAC recorders remain the only recording path.
dep_libavcodec    = dependency('libavcodec',    version: '>=59.37', required: false)
dep_libavformat   = dependency('libavformat',                        required: false)
dep_libavutil     = dependency('libavutil',     version: '>=57.28', required: false)
dep_libswscale    = dependency('libswscale',                         required: false)
dep_libswresample = dependency('libswresample',                      required: false)

ffmpeg_recording = get_option('ffmpeg-recording') \
  and dep_libavcodec.found() \
  and dep_libavformat.found() \
  and dep_libavutil.found() \
  and dep_libswscale.found() \
  and dep_libswresample.found()

if ffmpeg_recording
  add_global_arguments('-DHAVE_FFMPEG_RECORD', language: ['c', 'cpp'])
  message('FFmpeg video recording: enabled')
else
  message('FFmpeg video recording: disabled (missing libav* or -Dffmpeg-recording=false)')
endif

# ELF hosts where the linker provides __start_/__stop_ symbols for a
# C-identifier section. macOS/Mach-O is deliberately excluded.
osd_elf_font_systems = ['android', 'dragonfly', 'freebsd', 'gnu', 'linux',
                        'netbsd', 'openbsd', 'solaris']

# Lua scripting is resolved with dlopen() at runtime, exactly like the macOS
# frontend.  Nothing links against Lua at build time and no system development
# package is needed: the local LuaJIT headers describe the Lua 5.1 ABI, and
# LuaJIT (preferred) or a Lua 5.1 shared library is loaded when a script starts.
# The loader lives in frontend/cocoa/LuaJITApi.cpp so both POSIX and Cocoa share
# the same implementation.
add_global_arguments('-DHAVE_LUA=1', '-DDESMUME_LUA_DYNAMIC=1',
                     language: ['c', 'cpp'])

# dlopen() lives in libdl on older glibc.  On modern glibc and macOS it is part
# of libc, so a missing libdl is not an error.
dep_dl = cxx.find_library('dl', required: false)

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
  '../windows/agg/include',
  '../windows/lua/include',
  '../windows/agg/font_freetype',
)

# The bundled AGG rasterizer, built from the same sources the Windows project
# uses. This is the software OSD renderer; the WebGPU OSD replaces only the
# drawing backend and keeps the same AggDrawTarget interface.
agg_sources = files(
  '../windows/agg/src/agg_arc.cpp',
  '../windows/agg/src/agg_arrowhead.cpp',
  '../windows/agg/src/agg_bezier_arc.cpp',
  '../windows/agg/src/agg_bspline.cpp',
  '../windows/agg/src/agg_curves.cpp',
  '../windows/agg/src/agg_embedded_raster_fonts.cpp',
  '../windows/agg/src/agg_gsv_text.cpp',
  '../windows/agg/src/agg_image_filters.cpp',
  '../windows/agg/src/agg_line_aa_basics.cpp',
  '../windows/agg/src/agg_line_profile_aa.cpp',
  '../windows/agg/src/agg_rounded_rect.cpp',
  '../windows/agg/src/agg_sqrt_tables.cpp',
  '../windows/agg/src/agg_trans_affine.cpp',
  '../windows/agg/src/agg_trans_double_path.cpp',
  '../windows/agg/src/agg_trans_single_path.cpp',
  '../windows/agg/src/agg_trans_warp_magnifier.cpp',
  '../windows/agg/src/agg_vcgen_bspline.cpp',
  '../windows/agg/src/agg_vcgen_contour.cpp',
  '../windows/agg/src/agg_vcgen_dash.cpp',
  '../windows/agg/src/agg_vcgen_markers_term.cpp',
  '../windows/agg/src/agg_vcgen_smooth_poly1.cpp',
  '../windows/agg/src/agg_vcgen_stroke.cpp',
  '../windows/agg/src/agg_vpgen_clip_polygon.cpp',
  '../windows/agg/src/agg_vpgen_clip_polyline.cpp',
  '../windows/agg/src/agg_vpgen_segmentator.cpp',
)

libdesmume_src = agg_sources + [
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
elif get_option('egl')
  if dep_egl.found()
    add_global_arguments('-DENABLE_EGL', language: ['c', 'cpp'])
    dependencies += dep_egl
  else
    message('Cannot use EGL -- egl library not found.')
  endif
endif

# SDL3 is a hard dependency, so microphone capture is always available
# through SDL (no ALSA/OpenAL conditionals or optional dependencies).
libdesmume_src += [
  'shared/mic_sdl.cpp',
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

# OSD: AggDraw over the bundled AGG rasterizer. HAVE_LIBAGG gates the OSD in
# the core and all frontends, so it is unconditional now that AGG is compiled
# in rather than looked up with pkg-config.
add_global_arguments('-DHAVE_LIBAGG', language: ['c', 'cpp'])
libdesmume_src += [
  '../modules/osd/agg/aggdraw.cpp',
  '../modules/osd/agg/agg_osd.cpp',
  '../modules/osd/wgpu/wgpu_draw.cpp',
  '../modules/osd/wgpu/wgpu_osd_draw.cpp',
]
# Stage 2 of the WebGPU OSD migration: WGPUOSDDrawTarget records the
# primitives agg_osd.cpp uses instead of rasterizing them with AGG.  The
# recorder implements the AggDrawTarget interface without instantiating Agg2D,
# so both object files are free of undefined agg::* symbols (wgpu-agg.md 20).
# Only the WebGPU frontend path uses the recorder.
if get_option('wgpu')
  add_global_arguments('-DDESMUME_WGPU_OSD', language: ['c', 'cpp'])
endif

# Vector fonts need FreeType; the GTK frontends also use fontconfig to find a
# font file, so only advertise AGG2D_USE_VECTORFONTS when both are present.
if dep_freetype.found()
  dependencies += dep_freetype
  libdesmume_src += [
    '../windows/agg/font_freetype/agg_font_freetype.cpp',
  ]
  if dep_fontconfig.found()
    dependencies += dep_fontconfig
    add_global_arguments('-DAGG2D_USE_VECTORFONTS', language: ['c', 'cpp'])
  endif
endif

if get_option('osd-elf-fonts') and osd_elf_font_systems.contains(host_machine.system())
  python3 = import('python').find_installation('python3')
  osd_font_bins = files(
    '../windows/osd_fonts/fonts/gse4x6.bin',
    '../windows/osd_fonts/fonts/gse4x8.bin',
    '../windows/osd_fonts/fonts/gse5x7.bin',
    '../windows/osd_fonts/fonts/gse5x9.bin',
    '../windows/osd_fonts/fonts/gse6x9.bin',
    '../windows/osd_fonts/fonts/gse6x12.bin',
    '../windows/osd_fonts/fonts/gse7x11.bin',
    '../windows/osd_fonts/fonts/gse7x11_bold.bin',
    '../windows/osd_fonts/fonts/gse7x15.bin',
    '../windows/osd_fonts/fonts/gse7x15_bold.bin',
    '../windows/osd_fonts/fonts/gse8x16.bin',
    '../windows/osd_fonts/fonts/gse8x16_bold.bin',
    '../windows/osd_fonts/fonts/mcs11_prop.bin',
    '../windows/osd_fonts/fonts/mcs11_prop_condensed.bin',
    '../windows/osd_fonts/fonts/mcs12_prop.bin',
    '../windows/osd_fonts/fonts/mcs13_prop.bin',
    '../windows/osd_fonts/fonts/mcs5x10_mono.bin',
    '../windows/osd_fonts/fonts/mcs5x11_mono.bin',
    '../windows/osd_fonts/fonts/mcs6x10_mono.bin',
    '../windows/osd_fonts/fonts/mcs6x11_mono.bin',
    '../windows/osd_fonts/fonts/mcs7x12_mono_high.bin',
    '../windows/osd_fonts/fonts/mcs7x12_mono_low.bin',
    '../windows/osd_fonts/fonts/verdana12.bin',
    '../windows/osd_fonts/fonts/verdana12_bold.bin',
    '../windows/osd_fonts/fonts/verdana13.bin',
    '../windows/osd_fonts/fonts/verdana13_bold.bin',
    '../windows/osd_fonts/fonts/verdana14.bin',
    '../windows/osd_fonts/fonts/verdana14_bold.bin',
    '../windows/osd_fonts/fonts/verdana16.bin',
    '../windows/osd_fonts/fonts/verdana16_bold.bin',
    '../windows/osd_fonts/fonts/verdana17.bin',
    '../windows/osd_fonts/fonts/verdana17_bold.bin',
    '../windows/osd_fonts/fonts/verdana18.bin',
    '../windows/osd_fonts/fonts/verdana18_bold.bin',
  )
  osd_fonts_section = custom_target('osd-fonts-section',
    output: 'osd_fonts_section.S',
    command: [
      python3,
      files('../windows/osd_fonts/generate_elf_font_section.py'),
      '@OUTPUT@',
    ],
    depend_files: osd_font_bins,
    build_by_default: true,
  )
  libdesmume_src += [
    '../modules/osd/agg/osd_fonts_section.cpp',
    osd_fonts_section,
  ]
  add_global_arguments('-DDESMUME_ELF_FONTS', language: ['c', 'cpp'])
endif

# Lua engine.  The sources are always built: the loader makes the emulator
# start even when no LuaJIT/Lua 5.1 shared library is installed, and the script
# console then reports why scripting is unavailable.
if dep_dl.found()
  dependencies += dep_dl
endif
libdesmume_src += [
  '../../lua-engine.cpp',
  '../cocoa/LuaJITApi.cpp',
]

if get_option('gdb-stub')
  add_global_arguments('-DGDB_STUB', language: ['c', 'cpp'])
  libdesmume_src += [
    '../../gdbstub/gdbstub.cpp',
  ]
endif

# WebGPU presentation layer. The portable core (src/frontend/shared) is used by
# both Windows and Linux; the Linux adapters live in shared/wgpu_present_linux.*
# and shared/wgpu_display_linux.*.
#
# The renderer and display wrappers always provide stub implementations, so the
# frontend can stay linked when -Dwgpu=false is used. Only the wgpu-native
# dependency, the real Linux surface adapter and the ENABLE_WGPU define are
# conditional.
libdesmume_src += [
  '../../WGPURender.cpp',
  '../shared/wgpu_present.cpp',
  'shared/wgpu_display_linux.cpp',
  'shared/wgpu_upscaler_linux.cpp',
]

if get_option('wgpu')
  # wgpu-native is taken from the checked-in 3rdparty copy, or from the
  # directory passed with -Dwgpu-native-path=/path/to/wgpu-<platform>-<arch>-release.
  wgpu_native_path = get_option('wgpu-native-path')
  wgpu_native_lib_dir = meson.current_source_dir() / '../../../3rdparty/libs' / (wgpu_platform + '-' + host_machine.cpu_family())
  wgpu_native_include = include_directories('../../../3rdparty/wgpu')

  if wgpu_native_path != ''
    wgpu_native_path_abs = wgpu_native_path
    if not fs.is_absolute(wgpu_native_path)
      wgpu_native_path_abs = join_paths(meson.current_source_dir(), wgpu_native_path)
    endif

    if fs.exists(join_paths(wgpu_native_path_abs, 'lib', 'libwgpu_native.so'))
      wgpu_native_lib_dir = join_paths(wgpu_native_path_abs, 'lib')
      wgpu_native_include = include_directories(join_paths(wgpu_native_path_abs, 'include', 'webgpu'))
    else
      wgpu_native_lib_dir = wgpu_native_path_abs
    endif
  endif

  dep_wgpu_lib = cxx.find_library('wgpu_native', dirs: wgpu_native_lib_dir, required: true)

  libdesmume_src += [
    'shared/wgpu_present_linux.cpp',
  ]

  add_global_arguments(['-DENABLE_WGPU'], language: ['c', 'cpp'])

  dep_wgpu = declare_dependency(
    dependencies: dep_wgpu_lib,
    include_directories: wgpu_native_include,
    link_args: ['-Wl,-rpath,' + wgpu_native_lib_dir],
  )
  dependencies += [dep_wgpu, dep_wayland]
endif

dependencies += [dep_x11]

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

  if target_cpu_kind_x86
    jit_src = ['../../arm_jit.cpp']
  else
    jit_src = ['../../utils/arm_jit/arm_jit_arm.cpp']
  endif
  libdesmume_jit = static_library('desmume_jit',
    jit_src,
    include_directories: [includes, asmjit_inc],
    cpp_args: asmjit_args,
    override_options: asmjit_std,
    dependencies: dep_threads,
  )

  jit_link_whole = [libdesmume_jit, libasmjit]
endif

libdesmume = static_library('desmume',
  libdesmume_src,
  dependencies: dependencies,
  include_directories: [includes] + soundtouch_includes,
  link_whole: jit_link_whole,
)

if get_option('frontend-cli')
  subdir('cli')
endif

# GTK frontend discovery.
#
# A single option drives all three GTK frontends.  'auto' lets the build pick
# whatever the machine ships, preferring the newest toolkit: gtk4, then gtk3,
# then gtk2.  Naming a version explicitly pins it and fails loudly instead of
# silently downgrading.  Only one GTK frontend is built per configuration; the
# GTK3 and GTK4 targets have distinct names and live in separate build dirs.
gtk_frontend = get_option('frontend-gtk')

dep_gtk2 = dependency('gtk+-2.0', version: '>=2.24', required: false)
dep_gtk3 = dependency('gtk+-3.0', version: '>=3.24', required: false)
dep_gtk4 = dependency('gtk4', version: '>=4.10', required: false)

if gtk_frontend == 'auto'
  if dep_gtk4.found()
    gtk_frontend = 'gtk4'
  elif dep_gtk3.found()
    gtk_frontend = 'gtk3'
  elif dep_gtk2.found()
    gtk_frontend = 'gtk2'
  else
    error('No GTK development files found. Install libgtk-4-dev, libgtk-3-dev or libgtk2.0-dev, or pass -Dfrontend-gtk=none to build without a GTK frontend.')
  endif
endif

# A forced choice must be honoured: probe results are cached, so the subdir
# dependency() call is free, but the failure has to look like a user error.
if gtk_frontend == 'gtk4' and not dep_gtk4.found()
  error('frontend-gtk=gtk4 requires GTK 4.10 or newer (pkg-config "gtk4" not found).')
elif gtk_frontend == 'gtk3' and not dep_gtk3.found()
  error('frontend-gtk=gtk3 requires GTK 3.24 or newer (pkg-config "gtk+-3.0" not found).')
elif gtk_frontend == 'gtk2' and not dep_gtk2.found()
  error('frontend-gtk=gtk2 requires GTK 2.24 or newer (pkg-config "gtk+-2.0" not found).')
endif

message('GTK frontend: @0@'.format(gtk_frontend))

if gtk_frontend == 'gtk2'
  subdir('gtk2')
elif gtk_frontend == 'gtk3'
  subdir('gtk')
elif gtk_frontend == 'gtk4'
  subdir('gtk4')
endif
