/*
	emulation.cpp - core bridge for the DeSmuME GTK4 frontend

	This file owns the globals the shared core expects from its host
	(execute/click, the sound and 3D backend lists), the minimal core
	initialisation sequence, and the one-frame pipeline that feeds
	DesmumeDisplay.  Keeping this separate from main.cpp means the GTK layer
	never has to include the emulator's C++ headers.

	The frame path is the same one GTK3 uses in Gtk3GPUEventHandler::DidFrameEnd
	plus VideoFilter::RunFilter(): GPU->GetDisplayInfo().masterCustomBuffer is
	converted to opaque BGRA8888 into the filter's source buffer, the filter
	copies it to the destination, and the destination is handed to the display.
	With VideoFilterTypeID_None the filter is a straight memcpy, so this is a
	single conversion per frame; the widget then pools the texture backing.
*/

#include "emulation.h"

#include <stdio.h>
#include <string.h>

#include "NDSSystem.h"
#include "GPU.h"
#include "SPU.h"
#include "render3D.h"
#include "rasterize.h"
#include "saves.h"
#include "filter/videofilter.h"
#include "utils/colorspacehandler/colorspacehandler.h"

/* ------------------------------------------------ host globals used by core */

volatile bool execute = false;
BOOL click = FALSE;

/* Sound is deliberately disabled in P3a: no SDL audio device is opened, so
 * the main loop stays entirely on the GTK thread.  A real sound core will be
 * selected when the audio settings dialog arrives. */
SoundInterface_struct *SNDCoreList[] =
{
	&SNDDummy,
	&SNDDummy,
	NULL
};

GPU3DInterface *core3DList[] =
{
	&gpu3DNull,
	&gpu3DRasterize,
	NULL
};

/* ------------------------------------------------------------------- state */

static gboolean sInitialized = FALSE;
static gboolean sHasROM = FALSE;
static gboolean sRunning = FALSE;
static char *sRomName = NULL;

static VideoFilter *sVideo = NULL;
static DesmumeFrameSink sFrameSink = NULL;
static gpointer sFrameSinkData = NULL;

/* Bit layout mirrors shared/ctrlssdl.h so the defaults are identical to the
 * GTK3 frontend: 0 A, 1 B, 2 Select, 3 Start, 4 Right, 5 Left, 6 Up,
 * 7 Down, 8 R, 9 L, 10 X, 11 Y, 12 Debug, 13 Boost, 14 unused, 15 Lid. */
static guint sKeyBindings[16] =
{
	GDK_KEY_x, GDK_KEY_z, GDK_KEY_Shift_R, GDK_KEY_Return,
	GDK_KEY_Right, GDK_KEY_Left, GDK_KEY_Up, GDK_KEY_Down,
	GDK_KEY_w, GDK_KEY_q, GDK_KEY_s, GDK_KEY_a,
	GDK_KEY_p, GDK_KEY_o, 0, GDK_KEY_BackSpace
};

static guint16 sKeys = 0;

/* ----------------------------------------------------------- key / touch */

static void
apply_keys(void)
{
	const guint16 k = sKeys;
	NDS_setPad((k >> 4) & 1,   /* R */
	           (k >> 5) & 1,   /* L */
	           (k >> 7) & 1,   /* Down */
	           (k >> 6) & 1,   /* Up */
	           (k >> 2) & 1,   /* Select */
	           (k >> 3) & 1,   /* Start */
	           (k >> 1) & 1,   /* B */
	           (k >> 0) & 1,   /* A */
	           (k >> 11) & 1,  /* Y */
	           (k >> 10) & 1,  /* X */
	           (k >> 9) & 1,   /* L shoulder */
	           (k >> 8) & 1,   /* R shoulder */
	           (k >> 12) & 1,  /* Debug */
	           (k >> 15) & 1); /* Lid */
}

void
desmume_emu_set_key(guint keyval, gboolean pressed)
{
	for (guint i = 0; i < G_N_ELEMENTS(sKeyBindings); ++i)
	{
		if ((sKeyBindings[i] != 0) && (keyval == sKeyBindings[i]))
		{
			if (pressed)
			{
				sKeys |= (guint16)(1u << i);
			}
			else
			{
				sKeys &= (guint16)~(1u << i);
			}
			return;
		}
	}
}

void
desmume_emu_touch(double x, double y)
{
	if (x < 0.0) x = 0.0;
	if (x > 255.0) x = 255.0;
	if (y < 0.0) y = 0.0;
	if (y > 191.0) y = 191.0;

	NDS_setTouchPos((u16)x, (u16)y);
}

void
desmume_emu_release_touch(void)
{
	NDS_releaseTouch();
}

/* --------------------------------------------------------------- lifecycle */

gboolean
desmume_emu_init(void)
{
	if (sInitialized)
	{
		return TRUE;
	}

	execute = false;

	if (NDS_Init() != 0)
	{
		g_printerr("GTK4: NDS_Init() failed\n");
		return FALSE;
	}

	/* The null renderer would leave 3D screens black.  The rasteriser is
	 * always compiled into libdesmume on POSIX builds. */
	if (!GPU->Change3DRendererByID(RENDERID_SOFTRASTERIZER))
	{
		g_printerr("GTK4: software 3D renderer unavailable; 3D will be blank\n");
		GPU->Change3DRendererByID(RENDERID_NULL);
	}

	sInitialized = TRUE;
	return TRUE;
}

void
desmume_emu_shutdown(void)
{
	execute = false;
	sRunning = FALSE;

	delete sVideo;
	sVideo = NULL;

	if (sHasROM)
	{
		NDS_FreeROM();
		sHasROM = FALSE;
	}

	g_free(sRomName);
	sRomName = NULL;

	if (sInitialized)
	{
		NDS_DeInit();
		sInitialized = FALSE;
	}
}

gboolean
desmume_emu_load_rom(const char *path, char **error_message)
{
	if ((path == NULL) || (*path == '\0'))
	{
		if (error_message != NULL)
		{
			*error_message = g_strdup("No ROM path was provided.");
		}
		return FALSE;
	}

	if (!desmume_emu_init())
	{
		if (error_message != NULL)
		{
			*error_message = g_strdup("The emulator core could not be initialised.");
		}
		return FALSE;
	}

	if (sHasROM)
	{
		desmume_emu_unload_rom();
	}

	clear_savestates();

	const int result = NDS_LoadROM(path);
	if (result < 0)
	{
		if (error_message != NULL)
		{
			*error_message = g_strdup_printf("Could not load '%s' (core error %d).",
			                                 path, result);
		}
		return FALSE;
	}

	const NDSDisplayInfo &info = GPU->GetDisplayInfo();
	const size_t src_width = info.customWidth;
	const size_t src_height = info.customHeight * 2;

	if (sVideo == NULL)
	{
		sVideo = new VideoFilter(src_width, src_height,
		                         VideoFilterTypeID_None, CommonSettings.num_cores);
	}
	else
	{
		sVideo->SetSourceSize(src_width, src_height);
	}

	g_free(sRomName);
	sRomName = g_path_get_basename(path);

	sHasROM = TRUE;
	sRunning = TRUE;
	execute = true;
	return TRUE;
}

void
desmume_emu_unload_rom(void)
{
	if (!sHasROM)
	{
		return;
	}

	execute = false;
	sRunning = FALSE;
	NDS_FreeROM();
	sHasROM = FALSE;

	g_free(sRomName);
	sRomName = NULL;
}

gboolean
desmume_emu_has_rom(void)
{
	return sHasROM;
}

const char *
desmume_emu_rom_name(void)
{
	return (sRomName != NULL) ? sRomName : "";
}

void
desmume_emu_set_running(gboolean running)
{
	sRunning = running && sHasROM;
	execute = (sRunning ? true : false);
}

gboolean
desmume_emu_get_running(void)
{
	return sRunning;
}

void
desmume_emu_set_frame_sink(DesmumeFrameSink sink, gpointer user_data)
{
	sFrameSink = sink;
	sFrameSinkData = user_data;
}

/* ------------------------------------------------------------------- frame */

void
desmume_emu_step(void)
{
	if (!sHasROM || (sVideo == NULL) || !sRunning)
	{
		return;
	}

	/* Same input boundary GTK3 uses in EmuProcessFrameInput().  Movie and Lua
	 * hooks are intentionally absent in P3a; they arrive with the tool layer. */
	apply_keys();
	NDS_beginProcessingInput();
	NDS_endProcessingInput();

	NDS_exec<false>();
	SPU_Emulate_user();

	const NDSDisplayInfo &info = GPU->GetDisplayInfo();
	if ((info.masterCustomBuffer == NULL) || (info.customWidth == 0) ||
	    (info.customHeight == 0))
	{
		return;
	}

	const size_t src_width = info.customWidth;
	const size_t src_height = info.customHeight * 2;
	if ((sVideo->GetSrcWidth() != src_width) ||
	    (sVideo->GetSrcHeight() != src_height))
	{
		sVideo->SetSourceSize(src_width, src_height);
	}

	u32 *dst = sVideo->GetSrcBufferPtr();
	if (dst == NULL)
	{
		return;
	}

	const size_t pixel_count = src_width * src_height;
	if (info.colorFormat == NDSColorFormat_BGR555_Rev)
	{
		ColorspaceConvertBuffer555xTo8888Opaque<true, false, BESwapNone>(
			(const u16 *)info.masterCustomBuffer, dst, pixel_count);
	}
	else
	{
		ColorspaceConvertBuffer888xTo8888Opaque<true, false>(
			(const u32 *)info.masterCustomBuffer, dst, pixel_count);
	}

	u32 *filtered = sVideo->RunFilter();
	if ((filtered != NULL) && (sFrameSink != NULL))
	{
		sFrameSink((const guchar *)filtered,
		           (int)sVideo->GetDstWidth(),
		           (int)sVideo->GetDstHeight(),
		           sFrameSinkData);
	}
}
