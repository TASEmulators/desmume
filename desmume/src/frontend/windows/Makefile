# makefile for mingw.

CROSS=i686-w64-mingw32-

CXX = $(CROSS)g++
CC = $(CROSS)gcc
AR = $(CROSS)ar
RANLIB = $(CROSS)ranlib
WINDRES = $(CROSS)windres

OPT=-O3 -g0
STRIP=-s

# you can override OPT and STRIP in this include file
-include config.mak

# MINGW_HAS_SECURE_API exposes funcs like sscanf_s
CPPFLAGS = -DMINGW_HAS_SECURE_API \
  -DHOST_WINDOWS -DWIN32_FRONTEND \
  -DHAVE_LIBAGG -DHAVE_JIT -DHAVE_LUA=1 \
  -DSOUNDTOUCH_DISABLE_X86_OPTIMIZATIONS \
  -D_CRT_SECURE_NO_DEPRECATE -DWIN32 -DHAVE_ZLIB_DEFLATE -DHAVE_LIBZ \
  -DNOMINMAX \
  -I../../libretro-common/include -Izlib128

CXXFLAGS = -I. -I../.. -I../../frontend/modules/osd/agg \
  -Iagg/include -Iwinpcap -Idirectx -Ilua/include \
  -fpermissive -fms-extensions -Wno-write-strings -std=gnu++11 $(OPT)

CFLAGS=$(OPT)

LIBS= \
  -lgdi32 -lvfw32 -lshell32 -lddraw -lopengl32 \
  -lvfw32 -lwinmm -lopengl32 -lglu32 -lws2_32 -luser32 -lgdi32 -lshell32 \
  -lcomdlg32 -lshlwapi -lcomctl32 -lxinput -lole32 -ldinput8 -ldinput -ldxerr8 \
  -ldxguid -ldsound -luuid

LDFLAGS=$(STRIP) -Wl,--enable-stdcall-fixup -static-libgcc -static-libstdc++ -Wl,--stack,4194304

AGG_SRCS = \
	agg/src/agg_arc.cpp \
	agg/src/agg_arrowhead.cpp \
	agg/src/agg_bezier_arc.cpp \
	agg/src/ctrl/agg_bezier_ctrl.cpp \
	agg/src/agg_bspline.cpp \
	agg/src/ctrl/agg_cbox_ctrl.cpp \
	agg/src/agg_curves.cpp \
	agg/src/agg_embedded_raster_fonts.cpp \
	agg/font_win32_tt/agg_font_win32_tt.cpp \
	agg/src/ctrl/agg_gamma_ctrl.cpp \
	agg/src/ctrl/agg_gamma_spline.cpp \
	agg/src/agg_gsv_text.cpp \
	agg/src/agg_image_filters.cpp \
	agg/src/agg_line_aa_basics.cpp \
	agg/src/agg_line_profile_aa.cpp \
	agg/src/platform/win32/agg_platform_support.cpp \
	agg/src/ctrl/agg_polygon_ctrl.cpp \
	agg/src/ctrl/agg_rbox_ctrl.cpp \
	agg/src/agg_rounded_rect.cpp \
	agg/src/ctrl/agg_scale_ctrl.cpp \
	agg/src/ctrl/agg_slider_ctrl.cpp \
	agg/src/ctrl/agg_spline_ctrl.cpp \
	agg/src/agg_sqrt_tables.cpp \
	agg/src/agg_trans_affine.cpp \
	agg/src/agg_trans_double_path.cpp \
	agg/src/agg_trans_single_path.cpp \
	agg/src/agg_trans_warp_magnifier.cpp \
	agg/src/agg_vcgen_bspline.cpp \
	agg/src/agg_vcgen_contour.cpp \
	agg/src/agg_vcgen_dash.cpp \
	agg/src/agg_vcgen_markers_term.cpp \
	agg/src/agg_vcgen_smooth_poly1.cpp \
	agg/src/agg_vcgen_stroke.cpp \
	agg/src/agg_vpgen_clip_polygon.cpp \
	agg/src/agg_vpgen_clip_polyline.cpp \
	agg/src/agg_vpgen_segmentator.cpp \
	agg/src/platform/win32/agg_win32_bmp.cpp \


SRCS = \
	AboutBox.cpp \
	CWindow.cpp \
	FEX_Interface.cpp \
	FirmConfig.cpp \
	IORegView.cpp \
	OpenArchive.cpp \
	aviout.cpp \
	cheatsWin.cpp \
	colorctrl.cpp \
	console.cpp \
	ddraw.cpp \
	disView.cpp \
	display.cpp \
	fsnitroView.cpp \
	gbaslot_config.cpp \
	ginfo.cpp \
	hotkey.cpp \
	importSave.cpp \
	inputdx.cpp \
	lightView.cpp \
	luaconsole.cpp \
	main.cpp \
	mapView.cpp \
	matrixView.cpp \
	memView.cpp \
	mic-win.cpp \
	oamView.cpp \
	ogl.cpp \
	ogl_display.cpp \
	palView.cpp \
	pathsettings.cpp \
	ram_search.cpp \
	ramwatch.cpp \
	recentroms.cpp \
	replay.cpp \
	slot1_config.cpp \
	snddx.cpp \
	sndxa2.cpp \
	soundView.cpp \
	throttle.cpp \
	tileView.cpp \
	winutil.cpp \
	../../addons/slot1comp_mc.cpp \
	../../addons/slot1comp_protocol.cpp \
	../../addons/slot1comp_rom.cpp \
	../../addons/slot1_retail_auto.cpp \
	../../addons/slot1_retail_mcrom.cpp \
	../../addons/slot1_retail_mcrom_debug.cpp \
	../../addons/slot2_auto.cpp \
	../../addons/slot2_hcv1000.cpp \
	../../addons/slot2_passme.cpp \
	../../addons/slot2_piano.cpp \
	../../addons/slot1_none.cpp \
	../../addons/slot1_r4.cpp \
	../../addons/slot1_retail_nand.cpp \
	../../addons/slot2_mpcf.cpp \
	../../addons/slot2_paddle.cpp \
	../../arm_instructions.cpp \
	../../armcpu.cpp \
	../../arm_jit.cpp \
	../../bios.cpp \
	../../cheatSystem.cpp \
	../../commandline.cpp \
	../../common.cpp \
	../../cp15.cpp \
	../../Database.cpp \
	../../debug.cpp \
	../../driver.cpp \
	../../emufile.cpp \
	../../encrypt.cpp \
	../../FIFO.cpp \
	../../filter/2xsai.cpp \
	../../filter/bilinear.cpp \
	../../filter/deposterize.cpp \
	../../filter/epx.cpp \
	../../filter/hq2x.cpp \
	../../filter/hq4x.cpp \
	../../filter/lq2x.cpp \
	../../filter/scanline.cpp \
	../../filter/xbrz.cpp \
	../../firmware.cpp \
	../../frontend/modules/ImageOut.cpp \
	../../gfx3d.cpp \
	../../GPU.cpp \
	../../lua-engine.cpp \
	../../matrix.cpp \
	../../mc.cpp \
	../../MMU.cpp \
	../../movie.cpp \
	../../NDSSystem.cpp \
	../../OGLRender.cpp \
	../../OGLRender_3_2.cpp \
	../../path.cpp \
	../../rasterize.cpp \
	../../readwrite.cpp \
	../../render3D.cpp \
	../../ROMReader.cpp \
	../../rtc.cpp \
	../../saves.cpp \
	../../slot1.cpp \
	../../slot2.cpp \
	../../SPU.cpp \
	../../texcache.cpp \
	../../thumb_instructions.cpp \
	../../utils/advanscene.cpp \
	../../utils/AsmJit/core/assembler.cpp \
	../../utils/AsmJit/core/assert.cpp \
	../../utils/AsmJit/core/buffer.cpp \
	../../utils/AsmJit/core/compiler.cpp \
	../../utils/AsmJit/core/compilercontext.cpp \
	../../utils/AsmJit/core/compilerfunc.cpp \
	../../utils/AsmJit/core/compileritem.cpp \
	../../utils/AsmJit/core/context.cpp \
	../../utils/AsmJit/core/cpuinfo.cpp \
	../../utils/AsmJit/core/defs.cpp \
	../../utils/AsmJit/core/func.cpp \
	../../utils/AsmJit/core/logger.cpp \
	../../utils/AsmJit/core/memorymanager.cpp \
	../../utils/AsmJit/core/memorymarker.cpp \
	../../utils/AsmJit/core/operand.cpp \
	../../utils/AsmJit/core/stringbuilder.cpp \
	../../utils/AsmJit/core/stringutil.cpp \
	../../utils/AsmJit/core/virtualmemory.cpp \
	../../utils/AsmJit/core/zonememory.cpp \
	../../utils/AsmJit/x86/x86assembler.cpp \
	../../utils/AsmJit/x86/x86compiler.cpp \
	../../utils/AsmJit/x86/x86compilercontext.cpp \
	../../utils/AsmJit/x86/x86compilerfunc.cpp \
	../../utils/AsmJit/x86/x86compileritem.cpp \
	../../utils/AsmJit/x86/x86cpuinfo.cpp \
	../../utils/AsmJit/x86/x86defs.cpp \
	../../utils/AsmJit/x86/x86func.cpp \
	../../utils/AsmJit/x86/x86operand.cpp \
	../../utils/AsmJit/x86/x86util.cpp \
	../../utils/colorspacehandler/colorspacehandler.cpp \
	../../utils/datetime.cpp \
	../../utils/dlditool.cpp \
	../../utils/emufat.cpp \
	../../utils/fsnitro.cpp \
	../../utils/libfat/cache.cpp \
	../../utils/libfat/directory.cpp \
	../../utils/libfat/disc.cpp \
	../../utils/libfat/fatdir.cpp \
	../../utils/libfat/fatfile.cpp \
	../../utils/libfat/filetime.cpp \
	../../utils/libfat/file_allocation_table.cpp \
	../../utils/libfat/libfat.cpp \
	../../utils/libfat/libfat_public_api.cpp \
	../../utils/libfat/lock.cpp \
	../../utils/libfat/partition.cpp \
	../../utils/tinyxml/tinystr.cpp \
	../../utils/tinyxml/tinyxml.cpp \
	../../utils/tinyxml/tinyxmlerror.cpp \
	../../utils/tinyxml/tinyxmlparser.cpp \
	../../utils/vfat.cpp \
	../../version.cpp \
	../../wifi.cpp \
	../../addons/slot2_expMemory.cpp \
	../../addons/slot2_gbagame.cpp \
	../../addons/slot2_guitarGrip.cpp \
	../../addons/slot2_none.cpp \
	../../addons/slot2_rumblepak.cpp \
	../../gdbstub/gdbstub.cpp \
	../../utils/guid.cpp \
	../../utils/task.cpp \
	../../utils/xstring.cpp \
	../../utils/decrypt/crc.cpp \
	../../utils/decrypt/decrypt.cpp \
	../../utils/decrypt/header.cpp \
	../../metaspu/metaspu.cpp \
	../../metaspu/SndOut.cpp \
	../../metaspu/Timestretcher.cpp \
	../../metaspu/win32/ConfigSoundtouch.cpp \
	../../metaspu/SoundTouch/3dnow_win.cpp \
	../../metaspu/SoundTouch/AAFilter.cpp \
	../../metaspu/SoundTouch/cpu_detect_x86_win.cpp \
	../../metaspu/SoundTouch/FIFOSampleBuffer.cpp \
	../../metaspu/SoundTouch/FIRFilter.cpp \
	../../metaspu/SoundTouch/mmx_optimized.cpp \
	../../metaspu/SoundTouch/RateTransposer.cpp \
	../../metaspu/SoundTouch/SoundTouch.cpp \
	../../metaspu/SoundTouch/sse_optimized.cpp \
	../../metaspu/SoundTouch/TDStretch.cpp \
	../../metaspu/SoundTouch/WavFile.cpp \
	../modules/Disassembler.cpp \
	../modules/osd/agg/aggdraw.cpp \
	../modules/osd/agg/agg_osd.cpp \
	File_Extractor/fex/Binary_Extractor.cpp \
	File_Extractor/fex/blargg_common.cpp \
	File_Extractor/fex/blargg_errors.cpp \
	File_Extractor/fex/Data_Reader.cpp \
	File_Extractor/fex/fex.cpp \
	File_Extractor/fex/File_Extractor.cpp \
	File_Extractor/fex/Gzip_Extractor.cpp \
	File_Extractor/fex/Gzip_Reader.cpp \
	File_Extractor/fex/Rar_Extractor.cpp \
	File_Extractor/fex/Zip7_Extractor.cpp \
	File_Extractor/fex/Zip_Extractor.cpp \
	File_Extractor/fex/Zlib_Inflater.cpp \
	File_Extractor/unrar/archive.cpp \
	File_Extractor/unrar/arcread.cpp \
	File_Extractor/unrar/blake2s.cpp \
	File_Extractor/unrar/blake2sp.cpp \
	File_Extractor/unrar/blake2s_sse.cpp \
	File_Extractor/unrar/coder.cpp \
	File_Extractor/unrar/crc.cpp \
	File_Extractor/unrar/encname.cpp \
	File_Extractor/unrar/extract.cpp \
	File_Extractor/unrar/getbits.cpp \
	File_Extractor/unrar/hash.cpp \
	File_Extractor/unrar/headers.cpp \
	File_Extractor/unrar/model.cpp \
	File_Extractor/unrar/pathfn.cpp \
	File_Extractor/unrar/rarvm.cpp \
	File_Extractor/unrar/rarvmtbl.cpp \
	File_Extractor/unrar/rawread.cpp \
	File_Extractor/unrar/secpassword.cpp \
	File_Extractor/unrar/strfn.cpp \
	File_Extractor/unrar/suballoc.cpp \
	File_Extractor/unrar/timefn.cpp \
	File_Extractor/unrar/unicode.cpp \
	File_Extractor/unrar/unpack.cpp \
	File_Extractor/unrar/unpack15.cpp \
	File_Extractor/unrar/unpack20.cpp \
	File_Extractor/unrar/unpack30.cpp \
	File_Extractor/unrar/unpack50.cpp \
	File_Extractor/unrar/unpack50frag.cpp \
	File_Extractor/unrar/unpackinline.cpp \
	File_Extractor/unrar/unrar.cpp \
	File_Extractor/unrar/unrar_misc.cpp \
	File_Extractor/unrar/unrar_open.cpp \
	$(AGG_SRCS)

# TODO: these might give better performance on supported cpu
#	../../utils/colorspacehandler\colorspacehandler_AVX2.cpp \
#	../../utils/colorspacehandler\colorspacehandler_SSE2.cpp \

CSRCS = \
	../../libretro-common/compat/compat_fnmatch.c \
	../../libretro-common/compat/compat_getopt.c \
	../../libretro-common/compat/compat_posix_string.c \
	../../libretro-common/compat/compat_snprintf.c \
	../../libretro-common/compat/compat_strcasestr.c \
	../../libretro-common/compat/compat_strl.c \
	../../libretro-common/encodings/encoding_utf.c \
	../../libretro-common/features/features_cpu.c \
	../../libretro-common/file/archive_file.c \
	../../libretro-common/file/archive_file_zlib.c \
	../../libretro-common/file/file_path.c \
	../../libretro-common/file/nbio/nbio_stdio.c \
	../../libretro-common/file/retro_dirent.c \
	../../libretro-common/file/retro_stat.c \
	../../libretro-common/formats/bmp/rbmp_encode.c \
	../../libretro-common/formats/png/rpng.c \
	../../libretro-common/formats/png/rpng_encode.c \
	../../libretro-common/hash/rhash.c \
	../../libretro-common/lists/dir_list.c \
	../../libretro-common/lists/file_list.c \
	../../libretro-common/lists/string_list.c \
	../../libretro-common/rthreads/rsemaphore.c \
	../../libretro-common/rthreads/rthreads.c \
	../../libretro-common/streams/file_stream.c \
	../../libretro-common/streams/memory_stream.c \
	File_Extractor/7z_C/7zAlloc.c \
	File_Extractor/7z_C/7zBuf.c \
	File_Extractor/7z_C/7zBuf2.c \
	File_Extractor/7z_C/7zCrc.c \
	File_Extractor/7z_C/7zCrcOpt.c \
	File_Extractor/7z_C/7zDec.c \
	File_Extractor/7z_C/7zDecode.c \
	File_Extractor/7z_C/7zExtract.c \
	File_Extractor/7z_C/7zFile.c \
	File_Extractor/7z_C/7zIn.c \
	File_Extractor/7z_C/7zStream.c \
	File_Extractor/7z_C/Alloc.c \
	File_Extractor/7z_C/Bcj2.c \
	File_Extractor/7z_C/Bra.c \
	File_Extractor/7z_C/Bra86.c \
	File_Extractor/7z_C/BraIA64.c \
	File_Extractor/7z_C/CpuArch.c \
	File_Extractor/7z_C/Delta.c \
	File_Extractor/7z_C/LzFind.c \
	File_Extractor/7z_C/LzFindMt.c \
	File_Extractor/7z_C/Lzma2Dec.c \
	File_Extractor/7z_C/Lzma2Enc.c \
	File_Extractor/7z_C/Lzma86Dec.c \
	File_Extractor/7z_C/Lzma86Enc.c \
	File_Extractor/7z_C/LzmaDec.c \
	File_Extractor/7z_C/LzmaEnc.c \
	File_Extractor/7z_C/LzmaLib.c \
	File_Extractor/7z_C/MtCoder.c \
	File_Extractor/7z_C/Ppmd7.c \
	File_Extractor/7z_C/Ppmd7Dec.c \
	File_Extractor/7z_C/Ppmd7Enc.c \
	File_Extractor/7z_C/win32/Threads.c \

RESRCS = resources.rc

RESOBJS = $(RESRCS:.rc=.o)
OBJS=$(SRCS:.cpp=.o)
COBJS=$(CSRCS:.c=.o)


all: desmume.exe

../../version.cpp: ../../scmrev.h

../../scmrev.h:
	SRCROOT=. sh ../cocoa/git-scmrev.sh

zlib-1.2.8.tar.xz:
	wget http://download.sourceforge.net/project/libpng/zlib/1.2.8/zlib-1.2.8.tar.xz

zlib-1.2.8/.extracted: zlib-1.2.8.tar.xz
	tar xf $<
	touch $@

zlib-1.2.8/libz.a: zlib-1.2.8/.extracted
	( cd zlib-1.2.8 ; CC=$(CC) AR=$(AR) RANLIB=$(RANLIB) ./configure ; make -j libz.a ; cd .. )

lua-5.1.5.tar.gz:
	wget https://www.lua.org/ftp/lua-5.1.5.tar.gz

lua-5.1.5/.extracted: lua-5.1.5.tar.gz
	tar xf $<
	touch $@

lua-5.1.5/src/liblua.a: lua-5.1.5/.extracted
	( cd lua-5.1.5/src ; make -j CC=$(CC) AR="$(AR) rcu" RANLIB=$(RANLIB) MYCFLAGS="-O3" liblua.a ; )

resources.o: resources.rc
	$(WINDRES) $< $@

desmume.exe: $(OBJS) $(COBJS) $(RESOBJS) lua-5.1.5/src/liblua.a zlib-1.2.8/libz.a
	$(CXX) $^ $(LDFLAGS) -o $@ $(LIBS)

clean:
	rm -f $(OBJS) $(COBJS) $(RESOBJS) desmume.exe lua-5.1.5/src/liblua.a zlib-1.2.8/libz.a lua-5.1.5/src/*.o zlib-1.2.8/*.o ../../scmrev.h

lightclean:
	rm -f $(OBJS) desmume.exe

.PHONY: all clean lightclean
