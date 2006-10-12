SUBDIRS = . $(UI_DIR)
noinst_LIBRARIES = libdesmume.a
libdesmume_a_SOURCES = \
        armcpu.c armcpu.h \
        arm_instructions.c arm_instructions.h \
        bios.c bios.h cp15.c cp15.h \
	Disassembler.c Disassembler.h \
	FIFO.c FIFO.h \
	debug.c debug.h \
        MMU.c MMU.h NDSSystem.c NDSSystem.h \
        thumb_instructions.c thumb_instructions.h \
        mc.c mc.h \
	arm9/GPU.c arm9/GPU.h \
	arm9/ARM9.c arm9/ARM9.h \
        cflash.c cflash.h fs.h \
        saves.c saves.h
libdesmume_a_LIBADD = fs-$(desmume_arch).$(OBJEXT)
