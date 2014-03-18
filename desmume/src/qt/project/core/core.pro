QT -= core gui
TEMPLATE = lib

CONFIG += staticlib
CONFIG -= app_bundle
CONFIG -= qt

TARGET = desmume

DEFINES += DESMUME_QT

win32 {
	!win32-g++: DEFINES += NOMINMAX \
		_CRT_SECURE_NO_DEPRECATE \
		WIN32 \
		HAVE_LIBZ

	DEFINES -= UNICODE

	INCLUDEPATH += $$PWD/../../../windows

	SOURCES += \
		../../../fs-windows.cpp \
		../../../metaspu/SoundTouch/3dnow_win.cpp \
		../../../metaspu/SoundTouch/cpu_detect_x86_win.cpp

	# Windows glib
	contains(QMAKE_TARGET.arch, x86_64) {
		LIBS += -L$$PWD/../../../windows/.libs/ -lglib-vc8-x64
	} else {
		LIBS += -L$$PWD/../../../windows/.libs/ -lglib-vc8-Win32
	}
	LIBS += -lole32 -lshell32 -ladvapi32
	INCLUDEPATH += $$PWD/../../../windows/glib-2.20.1/build/glib $$PWD/../../../windows/glib-2.20.1/build
	DEPENDPATH += $$PWD/../../../windows/glib-2.20.1/build/glib

	# Windows zlib
	contains(QMAKE_TARGET.arch, x86_64) {
		LIBS += -L$$PWD/../../../windows/zlib123/ -lzlib-vc8-x64
	} else {
		LIBS += -L$$PWD/../../../windows/zlib123/ -lzlib-vc8-Win32
	}
	INCLUDEPATH += $$PWD/../../../windows/zlib123
	DEPENDPATH += $$PWD/../../../windows/zlib123

	# Windows libagg
	contains(QMAKE_TARGET.arch, x86_64) {
		LIBS += -L$$PWD/../../../windows/agg/ -lagg-2.5-x64
	} else {
		LIBS += -L$$PWD/../../../windows/agg/ -lagg-2.5
	}
	INCLUDEPATH += $$PWD/../../../windows/agg/include
	DEPENDPATH += $$PWD/../../../windows/agg/include

	svnrev.target = userconfig/svnrev.h
	svnrev.commands = cd $$PWD & svnrev.cmd
	QMAKE_EXTRA_TARGETS += svnrev
	PRE_TARGETDEPS += userconfig/svnrev.h
} else:unix:!macx {
	DESMUME_ARCH = $$QMAKE_HOST.arch
	linux-*-64: DESMUME_ARCH = x86_64
	linux-*-32: DESMUME_ARCH = x86

	DEFINES += \
		HOST_LINUX=1 \
		HAVE_LIBZ=1 \
		HAVE_LIBAGG=1 \
		HAVE_JIT=1 \
		_GNU_SOURCE=1 \
		_REENTRANT

	SOURCES += \
		../../../fs-linux.cpp \
		../../../metaspu/SoundTouch/cpu_detect_x86_gcc.cpp

	QMAKE_CXXFLAGS += -pthread

	# Note: If you plan to distribute the binary, remove -march=native
	QMAKE_CXXFLAGS_RELEASE += -O3 -flto=4 -fuse-linker-plugin -funroll-loops -march=native -minline-all-stringops
	QMAKE_LFLAGS_RELEASE += -O3 -flto=4 -fuse-linker-plugin -funroll-loops -march=native -minline-all-stringops

	contains(DESMUME_ARCH, x86_64) {
		message("Build target decided as x86_64")
		DEFINES += HOST_64=1
	} else {
		message("Build target decided as x86")
		DEFINES += HOST_32=1
	}

	CONFIG += link_pkgconfig
	PKGCONFIG += glib-2.0 libagg zlib

	svnrev.target = userconfig/svnrev.h
	svnrev.commands = cd $$PWD; sh svnrev.sh
	QMAKE_EXTRA_TARGETS += svnrev
	PRE_TARGETDEPS += userconfig/svnrev.h
} else:macx {
	error("Mac OS X not supported")
}

INCLUDEPATH += userconfig/ defaultconfig/ ../../../../src

SOURCES += \
    ../../../armcpu.cpp \
    ../../../arm_instructions.cpp \
    ../../../agg2d.inl \
    ../../../bios.cpp \
    ../../../commandline.cpp \
    ../../../common.cpp \
    ../../../cp15.cpp \
    ../../../debug.cpp \
    ../../../Disassembler.cpp \
    ../../../emufile.cpp \
    ../../../encrypt.cpp \
    ../../../FIFO.cpp \
    ../../../firmware.cpp \
    ../../../GPU.cpp \
    ../../../GPU_osd.cpp \
    ../../../matrix.cpp \
    ../../../mc.cpp \
    ../../../MMU.cpp \
    ../../../NDSSystem.cpp \
    ../../../ROMReader.cpp \
    ../../../render3D.cpp \
    ../../../rtc.cpp \
    ../../../saves.cpp \
    ../../../slot1.cpp \
    ../../../slot2.cpp \
    ../../../SPU.cpp \
    ../../../gfx3d.cpp \
    ../../../thumb_instructions.cpp \
    ../../../movie.cpp \
    ../../../utils/advanscene.cpp \
    ../../../utils/datetime.cpp \
    ../../../utils/guid.cpp \
    ../../../utils/emufat.cpp \
    ../../../utils/fsnitro.cpp \
    ../../../utils/md5.cpp \
    ../../../utils/xstring.cpp \
    ../../../utils/vfat.cpp \
    ../../../utils/task.cpp \
    ../../../utils/dlditool.cpp \
    ../../../utils/ConvertUTF.c \
    ../../../utils/decrypt/crc.cpp \
    ../../../utils/decrypt/decrypt.cpp \
    ../../../utils/decrypt/header.cpp \
    ../../../utils/libfat/cache.cpp \
    ../../../utils/libfat/directory.cpp \
    ../../../utils/libfat/disc.cpp \
    ../../../utils/libfat/fatdir.cpp \
    ../../../utils/libfat/fatfile.cpp \
    ../../../utils/libfat/file_allocation_table.cpp \
    ../../../utils/libfat/filetime.cpp \
    ../../../utils/libfat/libfat_public_api.cpp \
    ../../../utils/libfat/libfat.cpp \
    ../../../utils/libfat/lock.cpp \
    ../../../utils/libfat/partition.cpp \
    ../../../utils/tinyxml/tinystr.cpp \
    ../../../utils/tinyxml/tinyxml.cpp \
    ../../../utils/tinyxml/tinyxmlerror.cpp \
    ../../../utils/tinyxml/tinyxmlparser.cpp \
    ../../../addons/slot1_none.cpp \
    ../../../addons/slot1_r4.cpp \
    ../../../addons/slot1_retail_auto.cpp \
    ../../../addons/slot1_retail_mcrom_debug.cpp \
    ../../../addons/slot1_retail_mcrom.cpp \
    ../../../addons/slot1_retail_nand.cpp \
    ../../../addons/slot1comp_mc.cpp \
    ../../../addons/slot1comp_protocol.cpp \
    ../../../addons/slot1comp_rom.cpp \
    ../../../addons/slot2_auto.cpp \
    ../../../addons/slot2_expMemory.cpp \
    ../../../addons/slot2_gbagame.cpp \
    ../../../addons/slot2_guitarGrip.cpp \
    ../../../addons/slot2_mpcf.cpp \
    ../../../addons/slot2_none.cpp \
    ../../../addons/slot2_paddle.cpp \
    ../../../addons/slot2_passme.cpp \
    ../../../addons/slot2_piano.cpp \
    ../../../addons/slot2_rumblepak.cpp \
    ../../../cheatSystem.cpp \
    ../../../texcache.cpp \
    ../../../rasterize.cpp \
    ../../../metaspu/metaspu.cpp \
    ../../../filter/2xsai.cpp \
    ../../../filter/bilinear.cpp \
    ../../../filter/epx.cpp \
    ../../../filter/hq2x.cpp \
    ../../../filter/hq4x.cpp \
    ../../../filter/lq2x.cpp \
    ../../../filter/scanline.cpp \
    ../../../filter/videofilter.cpp \
    ../../../filter/xbrz.cpp \
    ../../../version.cpp \
    ../../../utils/AsmJit/core/assembler.cpp \
    ../../../utils/AsmJit/core/assert.cpp \
    ../../../utils/AsmJit/core/buffer.cpp \
    ../../../utils/AsmJit/core/compiler.cpp \
    ../../../utils/AsmJit/core/compilercontext.cpp \
    ../../../utils/AsmJit/core/compilerfunc.cpp \
    ../../../utils/AsmJit/core/compileritem.cpp \
    ../../../utils/AsmJit/core/context.cpp \
    ../../../utils/AsmJit/core/cpuinfo.cpp \
    ../../../utils/AsmJit/core/defs.cpp \
    ../../../utils/AsmJit/core/func.cpp \
    ../../../utils/AsmJit/core/logger.cpp \
    ../../../utils/AsmJit/core/memorymanager.cpp \
    ../../../utils/AsmJit/core/memorymarker.cpp \
    ../../../utils/AsmJit/core/operand.cpp \
    ../../../utils/AsmJit/core/stringbuilder.cpp \
    ../../../utils/AsmJit/core/stringutil.cpp \
    ../../../utils/AsmJit/core/virtualmemory.cpp \
    ../../../utils/AsmJit/core/zonememory.cpp \
    ../../../utils/AsmJit/x86/x86assembler.cpp \
    ../../../utils/AsmJit/x86/x86compiler.cpp \
    ../../../utils/AsmJit/x86/x86compilercontext.cpp \
    ../../../utils/AsmJit/x86/x86compilerfunc.cpp \
    ../../../utils/AsmJit/x86/x86compileritem.cpp \
    ../../../utils/AsmJit/x86/x86cpuinfo.cpp \
    ../../../utils/AsmJit/x86/x86defs.cpp \
    ../../../utils/AsmJit/x86/x86func.cpp \
    ../../../utils/AsmJit/x86/x86operand.cpp \
    ../../../utils/AsmJit/x86/x86util.cpp \
    ../../../mic.cpp \
    ../../../aggdraw.cpp \
    ../../../wifi.cpp \
    ../../../arm_jit.cpp \
    ../../../readwrite.cpp \
    ../../../driver.cpp \
    ../../../path.cpp \
    ../../../OGLRender.cpp \
    ../../../metaspu/SoundTouch/AAFilter.cpp \
    ../../../metaspu/SoundTouch/FIFOSampleBuffer.cpp \
    ../../../metaspu/SoundTouch/FIRFilter.cpp \
    ../../../metaspu/SoundTouch/mmx_optimized.cpp \
    ../../../metaspu/SoundTouch/RateTransposer.cpp \
    ../../../metaspu/SoundTouch/SoundTouch.cpp \
    ../../../metaspu/SoundTouch/sse_optimized.cpp \
    ../../../metaspu/SoundTouch/TDStretch.cpp \
    ../../../metaspu/SoundTouch/WavFile.cpp \
    ../../../metaspu/SndOut.cpp \
    ../../../metaspu/Timestretcher.cpp

HEADERS += \
    ../../../armcpu.h \
    ../../../agg2d.h \
    ../../../bios.h \
    ../../../bits.h \
    ../../../commandline.h \
    ../../../common.h \
    ../../../cp15.h \
    ../../../debug.h \
    ../../../Disassembler.h \
    ../../../emufile.h \
    ../../../emufile_types.h \
    ../../../encrypt.h \
    ../../../FIFO.h \
    ../../../firmware.h \
    ../../../fs.h \
    ../../../GPU.h \
    ../../../GPU_osd.h \
    ../../../instructions.h \
    ../../../matrix.h \
    ../../../mem.h \
    ../../../mc.h \
    ../../../mic.h \
    ../../../MMU_timing.h \
    ../../../MMU.h \
    ../../../NDSSystem.h \
    ../../../OGLRender.h \
    ../../../ROMReader.h \
    ../../../render3D.h \
    ../../../rtc.h \
    ../../../saves.h \
    ../../../slot1.h \
    ../../../slot2.h \
    ../../../SPU.h \
    ../../../gfx3d.h \
    ../../../types.h \
    ../../../shaders.h \
    ../../../movie.h \
    ../../../PACKED.h \
    ../../../PACKED_END.h \
    ../../../utils/advanscene.h \
    ../../../utils/datetime.h \
    ../../../utils/ConvertUTF.h \
    ../../../utils/guid.h \
    ../../../utils/emufat.h \
    ../../../utils/emufat_types.h \
    ../../../utils/fsnitro.h \
    ../../../utils/md5.h \
    ../../../utils/valuearray.h \
    ../../../utils/xstring.h \
    ../../../utils/vfat.h \
    ../../../utils/task.h \
    ../../../utils/decrypt/crc.h \
    ../../../utils/decrypt/decrypt.h \
    ../../../utils/decrypt/header.h \
    ../../../utils/libfat/bit_ops.h \
    ../../../utils/libfat/cache.h \
    ../../../utils/libfat/common.h \
    ../../../utils/libfat/directory.h \
    ../../../utils/libfat/disc_io.h \
    ../../../utils/libfat/disc.h \
    ../../../utils/libfat/fat.h \
    ../../../utils/libfat/fatdir.h \
    ../../../utils/libfat/fatfile.h \
    ../../../utils/libfat/file_allocation_table.h \
    ../../../utils/libfat/filetime.h \
    ../../../utils/libfat/libfat_pc.h \
    ../../../utils/libfat/libfat_public_api.h \
    ../../../utils/libfat/lock.h \
    ../../../utils/libfat/mem_allocate.h \
    ../../../utils/libfat/partition.h \
    ../../../utils/tinyxml/tinystr.h \
    ../../../utils/tinyxml/tinyxml.h \
    ../../../addons/slot1comp_mc.h \
    ../../../addons/slot1comp_protocol.h \
    ../../../addons/slot1comp_rom.h \
    ../../../cheatSystem.h \
    ../../../texcache.h \
    ../../../rasterize.h \
    ../../../metaspu/metaspu.h \
    ../../../filter/filter.h \
    ../../../filter/hq2x.h \
    ../../../filter/hq4x.h \
    ../../../filter/interp.h \
    ../../../filter/lq2x.h \
    ../../../filter/videofilter.h \
    ../../../filter/xbrz.h \
    ../../../version.h \
    ../../../utils/AsmJit/AsmJit.h \
    ../../../utils/AsmJit/Config.h \
    ../../../utils/AsmJit/core.h \
    ../../../utils/AsmJit/x86.h \
    ../../../utils/AsmJit/core/apibegin.h \
    ../../../utils/AsmJit/core/apiend.h \
    ../../../utils/AsmJit/core/assembler.h \
    ../../../utils/AsmJit/core/assert.h \
    ../../../utils/AsmJit/core/buffer.h \
    ../../../utils/AsmJit/core/build.h \
    ../../../utils/AsmJit/core/compiler.h \
    ../../../utils/AsmJit/core/compilercontext.h \
    ../../../utils/AsmJit/core/compilerfunc.h \
    ../../../utils/AsmJit/core/compileritem.h \
    ../../../utils/AsmJit/core/context.h \
    ../../../utils/AsmJit/core/cpuinfo.h \
    ../../../utils/AsmJit/core/defs.h \
    ../../../utils/AsmJit/core/func.h \
    ../../../utils/AsmJit/core/intutil.h \
    ../../../utils/AsmJit/core/lock.h \
    ../../../utils/AsmJit/core/logger.h \
    ../../../utils/AsmJit/core/memorymanager.h \
    ../../../utils/AsmJit/core/memorymarker.h \
    ../../../utils/AsmJit/core/operand.h \
    ../../../utils/AsmJit/core/podvector.h \
    ../../../utils/AsmJit/core/stringbuilder.h \
    ../../../utils/AsmJit/core/stringutil.h \
    ../../../utils/AsmJit/core/virtualmemory.h \
    ../../../utils/AsmJit/core/zonememory.h \
    ../../../utils/AsmJit/x86/x86assembler.h \
    ../../../utils/AsmJit/x86/x86compiler.h \
    ../../../utils/AsmJit/x86/x86compilercontext.h \
    ../../../utils/AsmJit/x86/x86compilerfunc.h \
    ../../../utils/AsmJit/x86/x86compileritem.h \
    ../../../utils/AsmJit/x86/x86cpuinfo.h \
    ../../../utils/AsmJit/x86/x86defs.h \
    ../../../utils/AsmJit/x86/x86func.h \
    ../../../utils/AsmJit/x86/x86operand.h \
    ../../../utils/AsmJit/x86/x86util.h \
    ../../../aggdraw.h \
    ../../../wifi.h \
    ../../../arm_jit.h \
    ../../../instruction_attributes.h \
    ../../../readwrite.h \
    ../../../driver.h \
    ../../../path.h \
    ../../../metaspu/SoundTouch/AAFilter.h \
    ../../../metaspu/SoundTouch/BPMDetect.h \
    ../../../metaspu/SoundTouch/cpu_detect.h \
    ../../../metaspu/SoundTouch/FIFOSampleBuffer.h \
    ../../../metaspu/SoundTouch/FIFOSamplePipe.h \
    ../../../metaspu/SoundTouch/FIRFilter.h \
    ../../../metaspu/SoundTouch/RateTransposer.h \
    ../../../metaspu/SoundTouch/SoundTouch.h \
    ../../../metaspu/SoundTouch/STTypes.h \
    ../../../metaspu/SoundTouch/TDStretch.h \
    ../../../metaspu/SoundTouch/WavFile.h \
    ../../../metaspu/SndOut.h \
    defaultconfig/svnrev.h
