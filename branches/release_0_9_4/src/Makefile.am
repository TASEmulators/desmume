include $(top_srcdir)/src/desmume.mk

AM_CPPFLAGS += $(SDL_CFLAGS) $(GTK_CFLAGS) $(GTHREAD_CFLAGS) $(X_CFLAGS)

EXTRA_DIST = build.bat instruction_tabdef.inc thumb_tabdef.inc fs-linux.cpp fs-windows.cpp \
	matrix_sse2-x64.asm matrix_sse2-x86.asm
if HAVE_GDB_STUB
SUBDIRS = . gdbstub $(UI_DIR)
else
SUBDIRS = . $(UI_DIR)
endif
DIST_SUBDIRS = . gdbstub cli cocoa gtk gtk-glade windows
noinst_LIBRARIES = libdesmume.a
libdesmume_a_SOURCES = \
        armcpu.cpp armcpu.h ARM9.h \
        arm_instructions.cpp arm_instructions.h \
        bios.cpp bios.h bits.h cp15.cpp cp15.h \
        cflash.cpp cflash.h fs.h \
	common.cpp common.h \
	debug.cpp debug.h driver.h \
	Disassembler.cpp Disassembler.h \
	dscard.h fat.h FIFO.cpp FIFO.h \
	GPU_osd.cpp GPU_osd.h \
	GPU.cpp GPU.h \
	GPU_osd.cpp GPU_osd.h \
        mem.h mc.cpp mc.h \
	memorystream.h \
	readwrite.cpp readwrite.h \
	wifi.cpp wifi.h \
        MMU.cpp MMU.h NDSSystem.cpp NDSSystem.h registers.h \
	OGLRender.cpp OGLRender.h \
	ROMReader.cpp ROMReader.h \
	render3D.cpp render3D.h \
	rtc.cpp rtc.h \
        saves.cpp saves.h \
        SPU.cpp SPU.h \
	softrender.cpp softrender.h softrender_config.h \
	softrender_v3sysfont.h softrender_desmumefont.h \
	matrix.cpp matrix.h \
	gfx3d.cpp gfx3d.h \
	texcache.cpp texcache.h \
        thumb_instructions.cpp thumb_instructions.h types.h \
	shaders.h \
	movie.cpp movie.h \
	PACKED.h PACKED_END.h \
	utils/ConvertUTF.c utils/ConvertUTF.h utils/guid.cpp utils/guid.h \
	utils/md5.cpp utils/md5.h utils/valuearray.h utils/xstring.cpp utils/xstring.h \
	utils/decrypt/crc.cpp utils/decrypt/crc.h utils/decrypt/decrypt.cpp \
	utils/decrypt/decrypt.h utils/decrypt/header.cpp utils/decrypt/header.h \
	addons.cpp addons.h \
	addons/compactFlash.cpp addons/gbagame.cpp addons/none.cpp addons/rumblepak.cpp \
	mic.cpp mic.h \
	cheatSystem.cpp cheatSystem.h \
	texcache.cpp texcache.h rasterize.cpp rasterize.h \
	version.h
if HAVE_GLIB
libdesmume_a_SOURCES += commandline.h commandline.cpp
endif
if HAVE_GDB_STUB
libdesmume_a_SOURCES += gdbstub.h
endif
libdesmume_a_LIBADD = fs-$(desmume_arch).$(OBJEXT)
