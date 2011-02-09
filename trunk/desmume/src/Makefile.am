include $(top_srcdir)/src/desmume.mk

AM_CPPFLAGS += $(SDL_CFLAGS) $(GTHREAD_CFLAGS) $(X_CFLAGS) $(ALSA_CFLAGS) $(LIBAGG_CFLAGS)

EXTRA_DIST = build.bat instruction_tabdef.inc thumb_tabdef.inc fs-linux.cpp fs-windows.cpp
if HAVE_GDB_STUB
SUBDIRS = . gdbstub $(UI_DIR)
else
SUBDIRS = . $(UI_DIR)
endif
DIST_SUBDIRS = . gdbstub cli cocoa gtk gtk-glade wx
noinst_LIBRARIES = libdesmume.a
libdesmume_a_SOURCES = \
	armcpu.cpp armcpu.h \
	arm_instructions.cpp arm_instructions.h \
	agg2d.h agg2d.inl \
	bios.cpp bios.h bits.h cp15.cpp cp15.h \
	commandline.h commandline.cpp \
	common.cpp common.h \
	debug.cpp debug.h \
	Disassembler.cpp Disassembler.h \
	emufile.h emufile.cpp emufile_types.h fat.h FIFO.cpp FIFO.h \
	firmware.cpp firmware.h GPU.cpp GPU.h \
	fs.h \
	GPU_osd.h \
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
	SPU.cpp SPU.h \
	matrix.cpp matrix.h \
	gfx3d.cpp gfx3d.h \
	texcache.cpp texcache.h \
	thumb_instructions.cpp thumb_instructions.h types.h \
	shaders.h \
	movie.cpp movie.h \
	PACKED.h PACKED_END.h \
	utils/datetime.cpp utils/datetime.h \
	utils/ConvertUTF.c utils/ConvertUTF.h utils/guid.cpp utils/guid.h \
	utils/emufat.cpp utils/emufat.h utils/emufat_types.h \
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
	addons.cpp addons.h \
	addons/slot2_mpcf.cpp addons/slot2_paddle.cpp addons/slot2_gbagame.cpp addons/slot2_none.cpp addons/slot2_rumblepak.cpp addons/slot2_guitarGrip.cpp addons/slot2_expMemory.cpp addons/slot2_piano.cpp addons/slot1_none.cpp addons/slot1_r4.cpp addons/slot1_retail.cpp \
	cheatSystem.cpp cheatSystem.h \
	texcache.cpp texcache.h rasterize.cpp rasterize.h \
	metaspu/metaspu.cpp metaspu/metaspu.h \
	version.cpp version.h \
	desmume_config.cpp desmume_config.h

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
