SUBDIRS = . $(UI_DIR)
noinst_LIBRARIES = libdesmume.a
libdesmume_a_SOURCES = \
	armcpu.cpp armcpu.hpp \
	arm_instructions.cpp arm_instructions.hpp \
	bios.cpp bios.hpp cp15.cpp cp15.hpp \
	Disassembler.c Disassembler.h \
	FIFO.c FIFO.h \
	debug.c debug.h \
	MMU.cpp MMU.hpp NDSSystem.cpp NDSSystem.hpp \
	thumb_instructions.cpp thumb_instructions.hpp \
	arm7/fw.c arm7/fw.h \
	arm7/spi.c arm7/spi.h \
	arm9/GPU.c arm9/GPU.h \
	arm9/ARM9.c arm9/ARM9.h \
	windows/cflash.cpp windows/cflash.h fs.h
libdesmume_a_LIBADD = fs-$(desmume_arch).$(OBJEXT)
