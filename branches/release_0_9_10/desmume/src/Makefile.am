include $(top_srcdir)/src/desmume.mk

AM_CPPFLAGS += $(SDL_CFLAGS) $(GTHREAD_CFLAGS) $(X_CFLAGS) $(ALSA_CFLAGS) $(LIBAGG_CFLAGS) $(LIBSOUNDTOUCH_CFLAGS)

EXTRA_DIST = build.bat instruction_tabdef.inc thumb_tabdef.inc fs-linux.cpp fs-windows.cpp cocoa
if HAVE_GDB_STUB
SUBDIRS = . gdbstub $(UI_DIR)
else
SUBDIRS = . $(UI_DIR)
endif
DIST_SUBDIRS = . gdbstub cli gtk gtk-glade
noinst_LIBRARIES = libdesmume.a
libdesmume_a_SOURCES = \
	armcpu.cpp armcpu.h \
	arm_instructions.cpp \
	agg2d.h agg2d.inl \
	bios.cpp bios.h bits.h cp15.cpp cp15.h \
	commandline.h commandline.cpp \
	common.cpp common.h \
	debug.cpp debug.h \
	Disassembler.cpp Disassembler.h \
	emufile.h emufile.cpp emufile_types.h encrypt.h encrypt.cpp FIFO.cpp FIFO.h \
	firmware.cpp firmware.h GPU.cpp GPU.h \
	fs.h \
	GPU_osd.h \
	instructions.h \
	mem.h mc.cpp mc.h \
	path.cpp path.h \
	readwrite.cpp readwrite.h \
	wifi.cpp wifi.h \
	mic.h \
	MMU.cpp MMU.h MMU_timing.h NDSSystem.cpp NDSSystem.h registers.h \
	OGLRender.h \
	ROMReader.cpp ROMReader.h \
	render3D.cpp render3D.h \
	rtc.cpp rtc.h \
	saves.cpp saves.h \
	slot1.cpp slot1.h \
	slot2.cpp slot2.h \
	SPU.cpp SPU.h \
	matrix.cpp matrix.h \
	gfx3d.cpp gfx3d.h \
	thumb_instructions.cpp types.h \
	shaders.h \
	movie.cpp movie.h \
	PACKED.h PACKED_END.h \
	utils/advanscene.cpp utils/advanscene.h \
	utils/datetime.cpp utils/datetime.h \
	utils/ConvertUTF.c utils/ConvertUTF.h utils/guid.cpp utils/guid.h \
	utils/emufat.cpp utils/emufat.h utils/emufat_types.h \
	utils/fsnitro.cpp utils/fsnitro.h \
	utils/md5.cpp utils/md5.h utils/valuearray.h utils/xstring.cpp utils/xstring.h \
	utils/decrypt/crc.cpp utils/decrypt/crc.h utils/decrypt/decrypt.cpp \
	utils/decrypt/decrypt.h utils/decrypt/header.cpp utils/decrypt/header.h \
	utils/task.cpp utils/task.h \
	utils/vfat.h utils/vfat.cpp \
	utils/dlditool.cpp \
	utils/libfat/bit_ops.h \
	utils/libfat/cache.cpp \
	utils/libfat/cache.h \
	utils/libfat/common.h \
	utils/libfat/directory.cpp \
	utils/libfat/directory.h \
	utils/libfat/disc.cpp \
	utils/libfat/disc.h \
	utils/libfat/disc_io.h \
	utils/libfat/fat.h \
	utils/libfat/fatdir.cpp \
	utils/libfat/fatdir.h \
	utils/libfat/fatfile.cpp \
	utils/libfat/fatfile.h \
	utils/libfat/filetime.cpp \
	utils/libfat/filetime.h \
	utils/libfat/file_allocation_table.cpp \
	utils/libfat/file_allocation_table.h \
	utils/libfat/libfat.cpp \
	utils/libfat/libfat_pc.h \
	utils/libfat/libfat_public_api.cpp \
	utils/libfat/libfat_public_api.h \
	utils/libfat/lock.cpp \
	utils/libfat/lock.h \
	utils/libfat/mem_allocate.h \
	utils/libfat/partition.cpp \
	utils/libfat/partition.h \
	utils/tinyxml/tinystr.cpp \
	utils/tinyxml/tinystr.h \
	utils/tinyxml/tinyxml.cpp \
	utils/tinyxml/tinyxml.h \
	utils/tinyxml/tinyxmlerror.cpp \
	utils/tinyxml/tinyxmlparser.cpp \
	addons/slot2_auto.cpp addons/slot2_mpcf.cpp addons/slot2_paddle.cpp addons/slot2_gbagame.cpp addons/slot2_none.cpp addons/slot2_rumblepak.cpp addons/slot2_guitarGrip.cpp addons/slot2_expMemory.cpp addons/slot2_piano.cpp addons/slot2_passme.cpp addons/slot1_none.cpp addons/slot1_r4.cpp addons/slot1_retail_nand.cpp addons/slot1_retail_auto.cpp addons/slot1_retail_mcrom.cpp addons/slot1_retail_mcrom_debug.cpp addons/slot1comp_mc.cpp addons/slot1comp_mc.h addons/slot1comp_rom.h addons/slot1comp_rom.cpp addons/slot1comp_protocol.h addons/slot1comp_protocol.cpp \
	cheatSystem.cpp cheatSystem.h \
	texcache.cpp texcache.h rasterize.cpp rasterize.h \
	metaspu/metaspu.cpp metaspu/metaspu.h \
	filter/2xsai.cpp filter/bilinear.cpp filter/epx.cpp filter/filter.h \
	filter/hq2x.cpp filter/hq2x.h filter/hq4x.cpp filter/hq4x.dat filter/hq4x.h \
	filter/interp.h filter/lq2x.cpp filter/lq2x.h filter/scanline.cpp \
	filter/videofilter.cpp filter/videofilter.h \
	version.cpp version.h \
	desmume_config.cpp desmume_config.h

if HAVE_JIT
libdesmume_a_SOURCES += \
	arm_jit.cpp arm_jit.h instruction_attributes.h \
	utils/AsmJit/AsmJit.h \
	utils/AsmJit/Config.h \
	utils/AsmJit/core.h \
	utils/AsmJit/x86.h \
	utils/AsmJit/core/apibegin.h \
	utils/AsmJit/core/apiend.h \
	utils/AsmJit/core/assembler.cpp \
	utils/AsmJit/core/assembler.h \
	utils/AsmJit/core/assert.cpp \
	utils/AsmJit/core/assert.h \
	utils/AsmJit/core/buffer.cpp \
	utils/AsmJit/core/buffer.h \
	utils/AsmJit/core/build.h \
	utils/AsmJit/core/compiler.cpp \
	utils/AsmJit/core/compiler.h \
	utils/AsmJit/core/compilercontext.cpp \
	utils/AsmJit/core/compilercontext.h \
	utils/AsmJit/core/compilerfunc.cpp \
	utils/AsmJit/core/compilerfunc.h \
	utils/AsmJit/core/compileritem.cpp \
	utils/AsmJit/core/compileritem.h \
	utils/AsmJit/core/context.cpp \
	utils/AsmJit/core/context.h \
	utils/AsmJit/core/cpuinfo.cpp \
	utils/AsmJit/core/cpuinfo.h \
	utils/AsmJit/core/defs.cpp \
	utils/AsmJit/core/defs.h \
	utils/AsmJit/core/func.cpp \
	utils/AsmJit/core/func.h \
	utils/AsmJit/core/intutil.h \
	utils/AsmJit/core/lock.h \
	utils/AsmJit/core/logger.cpp \
	utils/AsmJit/core/logger.h \
	utils/AsmJit/core/memorymanager.cpp \
	utils/AsmJit/core/memorymanager.h \
	utils/AsmJit/core/memorymarker.cpp \
	utils/AsmJit/core/memorymarker.h \
	utils/AsmJit/core/operand.cpp \
	utils/AsmJit/core/operand.h \
	utils/AsmJit/core/podvector.h \
	utils/AsmJit/core/stringbuilder.cpp \
	utils/AsmJit/core/stringbuilder.h \
	utils/AsmJit/core/stringutil.cpp \
	utils/AsmJit/core/stringutil.h \
	utils/AsmJit/core/virtualmemory.cpp \
	utils/AsmJit/core/virtualmemory.h \
	utils/AsmJit/core/zonememory.cpp \
	utils/AsmJit/core/zonememory.h \
	utils/AsmJit/x86/x86assembler.cpp \
	utils/AsmJit/x86/x86assembler.h \
	utils/AsmJit/x86/x86compiler.cpp \
	utils/AsmJit/x86/x86compiler.h \
	utils/AsmJit/x86/x86compilercontext.cpp \
	utils/AsmJit/x86/x86compilercontext.h \
	utils/AsmJit/x86/x86compilerfunc.cpp \
	utils/AsmJit/x86/x86compilerfunc.h \
	utils/AsmJit/x86/x86compileritem.cpp \
	utils/AsmJit/x86/x86compileritem.h \
	utils/AsmJit/x86/x86cpuinfo.cpp \
	utils/AsmJit/x86/x86cpuinfo.h \
	utils/AsmJit/x86/x86defs.cpp \
	utils/AsmJit/x86/x86defs.h \
	utils/AsmJit/x86/x86func.cpp \
	utils/AsmJit/x86/x86func.h \
	utils/AsmJit/x86/x86operand.cpp \
	utils/AsmJit/x86/x86operand.h \
	utils/AsmJit/x86/x86util.cpp \
	utils/AsmJit/x86/x86util.h
endif

if HAVE_GL
libdesmume_a_SOURCES += OGLRender.cpp
endif

if HAVE_OPENAL
libdesmume_a_SOURCES += mic_openal.cpp
else
if HAVE_ALSA
libdesmume_a_SOURCES += mic_alsa.cpp
else
libdesmume_a_SOURCES += mic.cpp
endif
endif

if HAVE_LIBSOUNDTOUCH
libdesmume_a_SOURCES += metaspu/SndOut.cpp metaspu/SndOut.h metaspu/Timestretcher.cpp
endif

if HAVE_LIBAGG
libdesmume_a_SOURCES += aggdraw.cpp aggdraw.h GPU_osd.cpp
else
libdesmume_a_SOURCES += GPU_osd_stub.cpp
endif
if HAVE_LUA
AM_CPPFLAGS += $(LUA_CFLAGS)
libdesmume_a_SOURCES += lua-engine.cpp
endif
if HAVE_GDB_STUB
libdesmume_a_SOURCES += gdbstub.h
endif
libdesmume_a_LIBADD = fs-$(desmume_arch).$(OBJEXT)
