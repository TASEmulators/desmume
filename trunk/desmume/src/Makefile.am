include $(top_srcdir)/src/desmume.mk

EXTRA_DIST = build.bat instruction_tabdef.inc thumb_tabdef.inc fs-linux.cpp fs-windows.cpp
DIST_SUBDIRS = . gdbstub cli cocoa gtk gtk-glade windows
SUBDIRS = . gdbstub $(UI_DIR)
noinst_LIBRARIES = libdesmume.a
libdesmume_a_SOURCES = \
        armcpu.cpp armcpu.h ARM9.h \
        arm_instructions.cpp arm_instructions.h \
        bios.cpp bios.h bits.h cp15.cpp cp15.h \
        cflash.cpp cflash.h config.h fs.h \
	debug.cpp debug.h \
	Disassembler.cpp Disassembler.h \
	dscard.h fat.h FIFO.cpp FIFO.h \
	GPU.cpp GPU.h \
	gl_vertex.cpp gl_vertex.h \
        mem.h mc.cpp mc.h \
	wifi.cpp wifi.h \
        MMU.cpp MMU.h NDSSystem.cpp NDSSystem.h registers.h \
	ROMReader.cpp ROMReader.h \
	render3D.cpp render3D.h \
        saves.cpp saves.h \
        SPU.cpp SPU.h \
	gdbstub.h \
	matrix.cpp matrix.h \
	gfx3d.cpp gfx3d.h \
        thumb_instructions.cpp thumb_instructions.h types.h
libdesmume_a_LIBADD = fs-$(desmume_arch).$(OBJEXT)
