SUBDIRS = . $(UI_DIR)
noinst_LIBRARIES = libdesmume.a
libdesmume_a_SOURCES = \
	ARM9.c ARM9.h \
	armcpu.cpp armcpu.hpp \
	arm_instructions.cpp arm_instructions.hpp \
	bios.cpp bios.hpp cp15.cpp cp15.hpp \
	Disassembler.c Disassembler.h \
	FIFO.c FIFO.h \
        GPU.cpp GPU.hpp debug.c debug.h \
	MMU.cpp MMU.hpp NDSSystem.cpp NDSSystem.hpp \
	thumb_instructions.cpp thumb_instructions.hpp \
	windows/cflash.cpp windows/cflash.h fs.h
libdesmume_a_LIBADD = fs-$(desmume_arch).$(OBJEXT)
