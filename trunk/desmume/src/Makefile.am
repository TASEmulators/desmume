EXTRA_DIST = build.bat instruction_tabdef.inc thumb_tabdef.inc
SUBDIRS = . $(UI_DIR)
noinst_LIBRARIES = libdesmume.a
libdesmume_a_SOURCES = \
        armcpu.c armcpu.h ARM9.h \
        arm_instructions.c arm_instructions.h \
        bios.c bios.h bits.h cp15.c cp15.h \
        cflash.c cflash.h config.h fs.h \
	debug.c debug.h \
	Disassembler.c Disassembler.h \
	dscard.h fat.h FIFO.c FIFO.h \
	GPU.c GPU.h \
        mem.h mc.c mc.h \
	wifi.c wifi.h \
        MMU.c MMU.h NDSSystem.c NDSSystem.h registers.h \
	ROMReader.c ROMReader.h \
        saves.c saves.h \
        SPU.c SPU.h \
        thumb_instructions.c thumb_instructions.h types.h
libdesmume_a_LIBADD = fs-$(desmume_arch).$(OBJEXT)
