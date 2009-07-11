include $(top_srcdir)/src/desmume.mk

AM_CPPFLAGS += $(SDL_CFLAGS) $(GTK_CFLAGS) $(GTHREAD_CFLAGS) $(X_CFLAGS) $(LUA_CFLAGS) $(ALSA_CFLAGS)

EXTRA_DIST = build.bat instruction_tabdef.inc thumb_tabdef.inc fs-linux.cpp fs-windows.cpp \
	matrix_sse2-x64.asm matrix_sse2-x86.asm
if HAVE_GDB_STUB
SUBDIRS = . gdbstub $(UI_DIR)
else
SUBDIRS = . $(UI_DIR)
endif
DIST_SUBDIRS = . gdbstub cli cocoa gtk gtk-glade windows
noinst_LIBRARIES = libdesmume.a
INCLUDES = -I@top_srcdir@/src/agg/include
libdesmume_a_SOURCES = \
        armcpu.cpp armcpu.h ARM9.h \
        arm_instructions.cpp arm_instructions.h \
        bios.cpp bios.h bits.h cp15.cpp cp15.h \
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
	addons/compactFlash.cpp addons/gbagame.cpp addons/none.cpp addons/rumblepak.cpp addons/guitarGrip.cpp fs.h \
	agg/src/agg_arc.cpp agg/src/agg_arrowhead.cpp agg/src/agg_bezier_arc.cpp agg/src/agg_bspline.cpp agg/src/agg_curves.cpp \
	agg/src/agg_embedded_raster_fonts.cpp agg/src/agg_gsv_text.cpp agg/src/agg_image_filters.cpp agg/src/agg_line_aa_basics.cpp agg/src/agg_line_profile_aa.cpp \
	agg/src/agg_rounded_rect.cpp agg/src/agg_sqrt_tables.cpp \
	agg/src/agg_trans_affine.cpp agg/src/agg_trans_double_path.cpp agg/src/agg_trans_single_path.cpp agg/src/agg_trans_warp_magnifier.cpp \
	agg/src/agg_vcgen_bspline.cpp agg/src/agg_vcgen_contour.cpp agg/src/agg_vcgen_dash.cpp agg/src/agg_vcgen_markers_term.cpp agg/src/agg_vcgen_smooth_poly1.cpp agg/src/agg_vcgen_stroke.cpp \
	agg/src/agg_vpgen_clip_polygon.cpp agg/src/agg_vpgen_clip_polyline.cpp agg/src/agg_vpgen_segmentator \
	cheatSystem.cpp cheatSystem.h \
	texcache.cpp texcache.h rasterize.cpp rasterize.h \
	version.h
if HAVE_GLIB
libdesmume_a_SOURCES += commandline.h commandline.cpp
endif
if HAVE_ALSA
libdesmume_a_SOURCES += mic_alsa.cpp
else
libdesmume_a_SOURCES += mic.cpp
endif
if HAVE_LUA
libdesmume_a_SOURCES += lua-engine.cpp
endif
if HAVE_GDB_STUB
libdesmume_a_SOURCES += gdbstub.h
endif
libdesmume_a_LIBADD = fs-$(desmume_arch).$(OBJEXT)
