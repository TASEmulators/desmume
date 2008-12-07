include $(top_srcdir)/src/desmume.mk

EXTRA_DIST = build.bat instruction_tabdef.inc thumb_tabdef.inc fs-linux.cpp fs-windows.cpp
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
	debug.cpp debug.h \
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
	matrix.cpp matrix.h \
	gfx3d.cpp gfx3d.h \
        thumb_instructions.cpp thumb_instructions.h types.h
if HAVE_GDB_STUB
libdesmume_a_SOURCES += gdbstub.h
endif
libdesmume_a_LIBADD = fs-$(desmume_arch).$(OBJEXT)
