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
dep_wayland = dependency('wayland-client')
dep_x11 = dependency('x11')

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

if dep_soundtouch.found()
  dependencies += dep_soundtouch
  add_global_arguments('-DHAVE_LIBSOUNDTOUCH', language: ['c', 'cpp'])
  libdesmume_src += [
    '../../metaspu/SndOut.cpp',
    '../../metaspu/Timestretcher.cpp',
  ]
endif

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
add_global_arguments('-DDESMUME_WGPU_OSD', language: ['c', 'cpp'])

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
# and shared/wgpu_display_linux.*. wgpu-native is a hard dependency of the POSIX
# build: it is taken from the checked-in 3rdparty copy, or from the directory
# passed with -Dwgpu-native-path=/path/to/wgpu-linux-<arch>-release.
wgpu_native_path = get_option('wgpu-native-path')
wgpu_native_lib_dir = meson.current_source_dir() / '../../../3rdparty/libs' / ('linux-' + host_machine.cpu_family())
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

# The portable core, the display wrapper and the WebGPU 3D renderer are always
# built. The X11 and Wayland adapters are part of the same build; the frontend
# simply picks the one that matches the running GDK backend.
libdesmume_src += [
  '../../WGPURender.cpp',
  '../shared/wgpu_present.cpp',
  'shared/wgpu_display_linux.cpp',
  'shared/wgpu_present_linux.cpp',
  'shared/wgpu_upscaler_linux.cpp',
]

add_global_arguments(['-DENABLE_WGPU'], language: ['c', 'cpp'])

dep_wgpu = declare_dependency(
  dependencies: dep_wgpu_lib,
  include_directories: wgpu_native_include,
  link_args: ['-Wl,-rpath,' + wgpu_native_lib_dir],
)
dependencies += [dep_wgpu, dep_wayland, dep_x11]

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
