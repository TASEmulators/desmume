include desmume.mk

AM_CPPFLAGS += $(SDL_CFLAGS) $(GTHREAD_CFLAGS) $(X_CFLAGS) $(LIBAGG_CFLAGS) $(LIBSOUNDTOUCH_CFLAGS)



# from old stuff
# EXTRA_DIST = README.LIN README.MAC README.WIN dsm.txt dsv.txt autogen.sh $(PO_FILES_IN)
# DISTCLEANFILES = $(PO_FILES)
# SUBDIRS = $(PO_DIR) src

EXTRA_DIST = instruction_tabdef.inc thumb_tabdef.inc cocoa
if HAVE_GDB_STUB
SUBDIRS = . $(UI_DIR)
else
SUBDIRS = . $(UI_DIR)
endif
#DIST_SUBDIRS = . cli gtk
SUBDIRS += $(PO_DIR)
noinst_LIBRARIES = libdesmume.a
libdesmume_a_SOURCES = \
	../../armcpu.cpp ../../armcpu.h \
	../../arm_instructions.cpp \
	../../agg2d.h ../../agg2d.inl \
	../../bios.cpp ../../bios.h ../../bits.h ../../cp15.cpp ../../cp15.h \
	../../commandline.h ../../commandline.cpp \
	../../common.cpp ../../common.h \
	../../debug.cpp ../../debug.h \
	../../driver.cpp ../../driver.h \
	../../Database.cpp ../../Database.h \
	../../emufile.h ../../emufile.cpp ../../encrypt.h ../../encrypt.cpp ../../FIFO.cpp ../../FIFO.h \
	../../firmware.cpp ../../firmware.h ../../GPU.cpp ../../GPU.h \
	../../GPU_osd.h \
	../../instructions.h \
	../../mem.h ../../mc.cpp ../../mc.h \
	../../path.cpp ../../path.h \
	../../readwrite.cpp ../../readwrite.h \
	../../wifi.cpp ../../wifi.h \
	../../mic.h \
	../../MMU.cpp ../../MMU.h ../../MMU_timing.h ../../NDSSystem.cpp ../../NDSSystem.h ../../registers.h \
	../../ROMReader.cpp ../../ROMReader.h \
	../../utils/ziparchive.cpp ../../utils/ziparchive.h \
	../../render3D.cpp ../../render3D.h \
	../../rtc.cpp ../../rtc.h \
	../../saves.cpp ../../saves.h \
	../../slot1.cpp ../../slot1.h \
	../../slot2.cpp ../../slot2.h \
	../../SPU.cpp ../../SPU.h \
	../../matrix.cpp ../../matrix.h \
	../../gfx3d.cpp ../../gfx3d.h \
	../../thumb_instructions.cpp ../../types.h \
	../../movie.cpp ../../movie.h \
	../../PACKED.h ../../PACKED_END.h \
	../../frontend/modules/Disassembler.cpp ../../frontend/modules/Disassembler.h \
	../../utils/advanscene.cpp ../../utils/advanscene.h \
	../../utils/datetime.cpp ../../utils/datetime.h \
	../../utils/guid.cpp ../../utils/guid.h \
	../../utils/emufat.cpp ../../utils/emufat.h utils/emufat_types.h \
	../../utils/fsnitro.cpp ../../utils/fsnitro.h \
	../../utils/xstring.cpp ../../utils/xstring.h \
	../../utils/decrypt/decrypt.h ../../utils/decrypt/header.cpp ../../utils/decrypt/header.h \
	../../utils/task.cpp ../../utils/task.h \
	../../utils/vfat.h ../../utils/vfat.cpp \
	../../utils/dlditool.cpp \
	../../utils/libfat/bit_ops.h \
	../../utils/libfat/cache.cpp \
	../../utils/libfat/cache.h \
	../../utils/libfat/common.h \
	../../utils/libfat/directory.cpp \
	../../utils/libfat/directory.h \
	../../utils/libfat/disc.cpp \
	../../utils/libfat/disc.h \
	../../utils/libfat/disc_io.h \
	../../utils/libfat/fat.h \
	../../utils/libfat/fatdir.cpp \
	../../utils/libfat/fatdir.h \
	../../utils/libfat/fatfile.cpp \
	../../utils/libfat/fatfile.h \
	../../utils/libfat/filetime.cpp \
	../../utils/libfat/filetime.h \
	../../utils/libfat/file_allocation_table.cpp \
	../../utils/libfat/file_allocation_table.h \
	../../utils/libfat/libfat.cpp \
	../../utils/libfat/libfat_pc.h \
	../../utils/libfat/libfat_public_api.cpp \
	../../utils/libfat/libfat_public_api.h \
	../../utils/libfat/lock.cpp \
	../../utils/libfat/lock.h \
	../../utils/libfat/mem_allocate.h \
	../../utils/libfat/partition.cpp \
	../../utils/libfat/partition.h \
	../../utils/tinyxml2/tinyxml2.cpp ../../utils/tinyxml2/tinyxml2.h \
	../../utils/glcorearb.h \
	../../utils/colorspacehandler/colorspacehandler.cpp ../../utils/colorspacehandler/colorspacehandler.h \
	../../addons/slot2_auto.cpp ../../addons/slot2_mpcf.cpp ../../addons/slot2_paddle.cpp ../../addons/slot2_gbagame.cpp ../../addons/slot2_none.cpp ../../addons/slot2_rumblepak.cpp ../../addons/slot2_guitarGrip.cpp ../../addons/slot2_hcv1000.cpp ../../addons/slot2_expMemory.cpp ../../addons/slot2_piano.cpp ../../addons/slot2_passme.cpp ../../addons/slot1_none.cpp ../../addons/slot1_r4.cpp ../../addons/slot1_retail_nand.cpp ../../addons/slot1_retail_auto.cpp ../../addons/slot1_retail_mcrom.cpp ../../addons/slot1_retail_mcrom_debug.cpp ../../addons/slot1comp_mc.cpp ../../addons/slot1comp_mc.h ../../addons/slot1comp_rom.h ../../addons/slot1comp_rom.cpp ../../addons/slot1comp_protocol.h ../../addons/slot1comp_protocol.cpp \
	../../cheatSystem.cpp ../../cheatSystem.h \
	../../texcache.cpp ../../texcache.h ../../rasterize.cpp ../../rasterize.h \
	../../metaspu/metaspu.cpp ../../metaspu/metaspu.h \
	../../filter/2xsai.cpp ../../filter/bilinear.cpp ../../filter/deposterize.cpp ../../filter/epx.cpp ../../filter/filter.h \
	../../filter/hq2x.cpp ../../filter/hq2x.h \
	../../filter/hq3x.cpp ../../filter/hq3x.dat \
	../../filter/hq4x.cpp ../../filter/hq4x.dat \
	../../filter/interp.h ../../filter/lq2x.cpp ../../filter/lq2x.h ../../filter/scanline.cpp \
	../../filter/videofilter.cpp ../../filter/videofilter.h \
	../../filter/xbrz.cpp ../../filter/xbrz.h \
	../../version.cpp ../../version.h \
	../../libretro-common/compat/compat_getopt.c \
	../../libretro-common/file/file_path.c \
	../../libretro-common/compat/compat_strl.c \
	../../libretro-common/features/features_cpu.c \
	../../libretro-common/file/retro_dirent.c \
	../../libretro-common/file/retro_stat.c \
	../../libretro-common/rthreads/async_job.c \
	../../libretro-common/rthreads/rsemaphore.c \
	../../libretro-common/rthreads/rthreads.c \
	../../libretro-common/encodings/encoding_utf.c

# WebGPU. The portable presentation core, the display wrapper and the 3D
# renderer always compile and provide stubs when wgpu-native is absent. The
# Linux native-window adapter is only built when configure finds wgpu-native
# (--enable-wgpu, plus CPPFLAGS/LDFLAGS or a non-system installation on the
# link path).
libdesmume_a_SOURCES += \
	../../WGPURender.cpp \
	../shared/wgpu_present.cpp \
	shared/wgpu_display_linux.cpp

if ENABLE_WGPU
AM_CPPFLAGS += $(WGPU_CFLAGS) -I$(top_srcdir)/../../../3rdparty/wgpu
LIBS += $(WGPU_LIBS)
libdesmume_a_SOURCES += \
	shared/wgpu_present_linux.cpp
endif

# asmjit and the ARM JIT front-end need C++20 while the rest of the core
# stays at C++14 (configure.ac).  Compile them in a convenience library with
# per-target flags, then fold its objects into libdesmume.a: core code calls
# the JIT and the JIT calls core code, so two separate archives would be
# circular.  This is the autotools equivalent of Meson's link_whole.
if HAVE_JIT
noinst_LIBRARIES += libdesmume_jit.a
libdesmume_a_LIBADD = $(libdesmume_jit_a_OBJECTS)
libdesmume_jit_a_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/../../../3rdparty -DASMJIT_STATIC
libdesmume_jit_a_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS) -std=c++20
libdesmume_jit_a_SOURCES = \
	../../arm_jit.h ../../instruction_attributes.h \
	../../../3rdparty/asmjit/arm/a64_assembler.cpp \
	../../../3rdparty/asmjit/arm/a64_builder.cpp \
	../../../3rdparty/asmjit/arm/a64_compiler.cpp \
	../../../3rdparty/asmjit/arm/a64_emit_helper.cpp \
	../../../3rdparty/asmjit/arm/a64_formatter.cpp \
	../../../3rdparty/asmjit/arm/a64_func.cpp \
	../../../3rdparty/asmjit/arm/a64_inst_api.cpp \
	../../../3rdparty/asmjit/arm/a64_inst_db.cpp \
	../../../3rdparty/asmjit/arm/a64_ra_pass.cpp \
	../../../3rdparty/asmjit/arm/arm_formatter.cpp \
	../../../3rdparty/asmjit/axl/arena.cpp \
	../../../3rdparty/asmjit/axl/arena_bit_set.cpp \
	../../../3rdparty/asmjit/axl/arena_hash.cpp \
	../../../3rdparty/asmjit/axl/arena_vector.cpp \
	../../../3rdparty/asmjit/core/arch_traits.cpp \
	../../../3rdparty/asmjit/core/assembler.cpp \
	../../../3rdparty/asmjit/core/builder.cpp \
	../../../3rdparty/asmjit/core/code_holder.cpp \
	../../../3rdparty/asmjit/core/code_writer.cpp \
	../../../3rdparty/asmjit/core/compiler.cpp \
	../../../3rdparty/asmjit/core/const_pool.cpp \
	../../../3rdparty/asmjit/core/cpu_info.cpp \
	../../../3rdparty/asmjit/core/debug_utils.cpp \
	../../../3rdparty/asmjit/core/emit_helper.cpp \
	../../../3rdparty/asmjit/core/emitter.cpp \
	../../../3rdparty/asmjit/core/emitter_utils.cpp \
	../../../3rdparty/asmjit/core/environment.cpp \
	../../../3rdparty/asmjit/core/error.cpp \
	../../../3rdparty/asmjit/core/error_handler.cpp \
	../../../3rdparty/asmjit/core/formatter.cpp \
	../../../3rdparty/asmjit/core/func.cpp \
	../../../3rdparty/asmjit/core/func_args_context.cpp \
	../../../3rdparty/asmjit/core/inst.cpp \
	../../../3rdparty/asmjit/core/inst_db.cpp \
	../../../3rdparty/asmjit/core/jit_allocator.cpp \
	../../../3rdparty/asmjit/core/jit_runtime.cpp \
	../../../3rdparty/asmjit/core/logger.cpp \
	../../../3rdparty/asmjit/core/pauth.cpp \
	../../../3rdparty/asmjit/core/ra_local.cpp \
	../../../3rdparty/asmjit/core/ra_pass.cpp \
	../../../3rdparty/asmjit/core/ra_stack.cpp \
	../../../3rdparty/asmjit/core/string.cpp \
	../../../3rdparty/asmjit/core/target.cpp \
	../../../3rdparty/asmjit/core/type.cpp \
	../../../3rdparty/asmjit/core/virt_mem.cpp \
	../../../3rdparty/asmjit/x86/x86_assembler.cpp \
	../../../3rdparty/asmjit/x86/x86_builder.cpp \
	../../../3rdparty/asmjit/x86/x86_compiler.cpp \
	../../../3rdparty/asmjit/x86/x86_emit_helper.cpp \
	../../../3rdparty/asmjit/x86/x86_formatter.cpp \
	../../../3rdparty/asmjit/x86/x86_func.cpp \
	../../../3rdparty/asmjit/x86/x86_inst_api.cpp \
	../../../3rdparty/asmjit/x86/x86_inst_db.cpp \
	../../../3rdparty/asmjit/x86/x86_ra_pass.cpp

if HOST_CPU_KIND_X86
libdesmume_jit_a_SOURCES += ../../arm_jit.cpp
else
if HOST_CPU_KIND_ARM
libdesmume_jit_a_SOURCES += ../../utils/arm_jit/arm_jit_arm.cpp
endif
endif
endif

libdesmume_a_SOURCES += shared/desmume_config.cpp shared/desmume_config.h

if ENABLE_OPENGL_ES
libdesmume_a_SOURCES += \
	../../OGLRender.h ../../OGLRender_3_2.h \
	../../OGLRender.cpp ../../OGLRender_3_2.cpp \
	../../OGLRender_ES3.h ../../OGLRender_ES3.cpp
else
if ENABLE_OPENGL_STANDARD
libdesmume_a_SOURCES += \
	../../OGLRender.h ../../OGLRender_3_2.h \
	../../OGLRender.cpp ../../OGLRender_3_2.cpp
endif
endif

# SDL3 is a hard dependency of the POSIX frontends, so microphone capture
# is always available through SDL (no ALSA/OpenAL conditionals).
libdesmume_a_SOURCES += shared/mic_sdl.cpp

if HAVE_LIBSOUNDTOUCH
libdesmume_a_SOURCES += ../../metaspu/SndOut.cpp ../../metaspu/SndOut.h ../../metaspu/Timestretcher.cpp
endif

if HAVE_LIBAGG
libdesmume_a_SOURCES += ../../frontend/modules/osd/agg/aggdraw.cpp ../../frontend/modules/osd/agg/aggdraw.h ../../frontend/modules/osd/agg/agg_osd.cpp
endif
if HAVE_LUA
AM_CPPFLAGS += -I$(top_srcdir)/../windows/lua/include
libdesmume_a_SOURCES += ../../lua-engine.cpp ../cocoa/LuaJITApi.cpp
endif
if HAVE_GDB_STUB
libdesmume_a_SOURCES += ../../gdbstub/gdbstub.cpp
libdesmume_a_SOURCES += ../../gdbstub/gdbstub_internal.h
libdesmume_a_SOURCES += ../../gdbstub.h
endif
