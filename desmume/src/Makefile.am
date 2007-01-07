SUBDIRS = . $(UI_DIR)
noinst_LIBRARIES = libdesmume.a
libdesmume_a_SOURCES = \
        armcpu.c armcpu.h ARM9.h \
        arm_instructions.c arm_instructions.h \
        bios.c bios.h cp15.c cp15.h \
        cflash.c cflash.h fs.h \
	debug.c debug.h \
	Disassembler.c Disassembler.h \
	FIFO.c FIFO.h \
	GPU.c GPU.h \
        mc.c mc.h \
        MMU.c MMU.h NDSSystem.c NDSSystem.h registers.h \
	ROMReader.c ROMReader.h \
        saves.c saves.h \
        SPU.c SPU.h \
        thumb_instructions.c thumb_instructions.h
libdesmume_a_LIBADD = fs-$(desmume_arch).$(OBJEXT)
