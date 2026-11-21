/*
	emulation.cpp - core bridge for the DeSmuME GTK4 frontend

	This file owns the globals the shared core expects from its host
	(execute/click, the sound and 3D backend lists), the minimal core
	initialisation sequence, and the one-frame pipeline that feeds
	DesmumeDisplay.  Keeping this separate from main.cpp means the GTK layer
	never has to include the emulator's C++ headers.

	The frame path converts GPU->GetDisplayInfo().masterCustomBuffer straight
	into a pooled opaque BGRA8888 buffer (the Opaque converters already set
	alpha=0xFF) and hands the GBytes to the display, which builds zero-copy
	GdkMemoryTextures from it.  No filter memcpy and no per-pixel alpha fixup:
	per-frame pixel work matches GTK3's DidFrameEnd path with fewer copies.
*/

#include "emulation.h"

#include <stdio.h>
#include <string.h>

#include "NDSSystem.h"
#include "GPU.h"
#include "MMU.h"
#include "registers.h"
#include "SPU.h"
#include "render3D.h"
#include "rasterize.h"
#include "saves.h"
#include "cheatSystem.h"
#include "movie.h"
#include "utils/datetime.h"
#include "utils/xstring.h"
#include "path.h"
#include "../../shared/ram_debug_core.h"
#include "filter/videofilter.h"
#include "utils/colorspacehandler/colorspacehandler.h"

/* P7a: WebGPU off-screen GPU filters. wgpu_present.cpp is compiled into
 * libdesmume for every POSIX build; without -Dwgpu=true its methods are stubs
 * and beginOffscreen() simply returns false, so the plain frame path remains. */
#include "../../shared/wgpu_present.h"
#include "../shared/wgpu_upscaler_linux.h"

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
static gboolean sStateExists[NB_STATES] = { FALSE };

static DesmumeFrameSink sFrameSink = NULL;
static gpointer sFrameSinkData = NULL;

/* ------------------------------------------------------- zero-copy frame pool
 * The core's custom buffer is converted straight into a pooled BGRA8888
 * buffer, which is handed to the display as a refcounted GBytes. GDK wraps
 * those bytes in GdkMemoryTexture without copying; GSK keeps its own refs
 * until rendering is done. This removes the two extra full-frame copies the
 * original path paid per frame (VideoFilter::RunFilter's memcpy and the
 * widget's wrap_frame memcpy + per-pixel alpha fixup): the Opaque converters
 * already write alpha=0xFF, so no fixup is needed. */
/* Buffers are bucketed by byte size so the plain (native-size) path and the
 * GPU-filter (upscaled-size) path never allocate on each other's behalf. */
typedef struct
{
	gatomicrefcount ref_count;
	GMutex lock;
	GHashTable *buckets; /* gsize (via GSIZE_TO_POINTER) -> GQueue of guchar* */
} FramePool;

typedef struct
{
	FramePool *pool;
	guchar *data;
	gsize size;
} BufferRef;

static FramePool *sFramePool = NULL;

static void
free_buffer_queue(gpointer data)
{
	GQueue *queue = (GQueue *)data;
	guchar *buffer;
	while ((buffer = (guchar *)g_queue_pop_head(queue)) != NULL)
	{
		g_free(buffer);
	}
	g_queue_free(queue);
}

static FramePool *
frame_pool_new(void)
{
	FramePool *pool = g_new0(FramePool, 1);
	g_atomic_ref_count_init(&pool->ref_count);
	g_mutex_init(&pool->lock);
	pool->buckets = g_hash_table_new_full(g_direct_hash, g_direct_equal,
	                                      NULL, free_buffer_queue);
	return pool;
}

static void
frame_pool_unref(FramePool *pool)
{
	if (g_atomic_ref_count_dec(&pool->ref_count))
	{
		g_hash_table_destroy(pool->buckets);
		g_mutex_clear(&pool->lock);
		g_free(pool);
	}
}

static void
release_buffer(gpointer user_data)
{
	BufferRef *ref = (BufferRef *)user_data;
	FramePool *pool = ref->pool;

	g_mutex_lock(&pool->lock);
	GQueue *queue = (GQueue *)g_hash_table_lookup(
		pool->buckets, GSIZE_TO_POINTER(ref->size));
	if (queue == NULL)
	{
		queue = g_queue_new();
		g_hash_table_insert(pool->buckets, GSIZE_TO_POINTER(ref->size), queue);
	}
	g_queue_push_head(queue, ref->data);
	g_mutex_unlock(&pool->lock);

	frame_pool_unref(pool);
	g_free(ref);
}

/* Acquire a writable frame buffer of `size` bytes. Returns a GBytes owning
 * the buffer; callers write into g_bytes_get_data() cast to non-const before
 * the GBytes is published, then transfer ownership. */
static GBytes *
frame_pool_acquire(gsize size, guchar **out_data)
{
	FramePool *pool = sFramePool;
	guchar *data = NULL;

	g_mutex_lock(&pool->lock);
	GQueue *queue = (GQueue *)g_hash_table_lookup(
		pool->buckets, GSIZE_TO_POINTER(size));
	if (queue != NULL)
	{
		data = (guchar *)g_queue_pop_head(queue);
	}
	g_mutex_unlock(&pool->lock);

	if (data == NULL)
	{
		data = (guchar *)g_malloc(size);
	}

	BufferRef *ref = g_new0(BufferRef, 1);
	ref->pool = pool;
	ref->data = data;
	ref->size = size;
	/* The frame keeps the pool alive past shutdown while GSK still holds it. */
	g_atomic_ref_count_inc(&pool->ref_count);

	*out_data = data;
	return g_bytes_new_with_free_func(data, size, release_buffer, ref);
}

/* P7a GPU filter state.  The present object renders the composited core frame
 * into an off-screen texture and reads the filtered result back into a pooled
 * frame buffer, which present_current_frame() forwards to the display. */
static WGPUPresent *sGpuFilterPresent = NULL;
static WGPUPresentShaderSource *sGpuShaderSource = NULL;
static int sGpuFilterUpscaler = 0;      /* WGPUPresentUpscaler_Off */
static float sGpuFilterScale = 1.0f;
static gboolean sGpuFilterWanted = FALSE;
static gboolean sGpuFilterReady = FALSE;

/* P7a: tear down the off-screen WebGPU filter pipeline.  Called on setting
 * changes, ROM unload and shutdown; safe to call repeatedly. */
static void
release_gpu_filter(void)
{
	delete sGpuFilterPresent;
	sGpuFilterPresent = NULL;
	/* sGpuShaderSource is a process-lifetime static from the GResource loader;
	 * never delete it here. */
	sGpuFilterReady = FALSE;
}

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

/* ------------------------------------------------------- frame pacing (P3d) */

/* GTK3 keeps these as globals in main.cpp and drives them from the config and
 * the KEY_BOOST latch.  The GTK4 front end keeps them in the core bridge so
 * the GTK layer only has to forward actions/keys. */
static gboolean sFpsLimiter = TRUE;       /* config.fpslimiter */
static gboolean sAutoFrameskip = TRUE;    /* config.autoframeskip */
static int sFrameskipMax = 0;             /* config.frameskip, 0..9 */
static gboolean sFastForward = FALSE;     /* the toggled "fast forward" action */
static int sFrameMod3 = 0;                /* 17+17+16 ms => 60 fps over 3 frames */

/* Rolling FPS counter.  Counts every emulated core frame, including the ones
 * skipped for frameskip, matching GTK3's fps_FrameCount. */
static gint64 sFpsSecStart = 0;
static guint sFpsFrameCount = 0;
static guint sFpsValue = 0;

/* The tick source normally fires every ~16.7 ms already, so we only need the
 * monotonic clock to (a) measure a real one-second FPS window and (b) decide
 * how many skipped frames to run when a tick arrives late.  We deliberately
 * do not sleep inside the GTK callback: that would stall drawing and input. */
static gint64 sNextFrameTime = 0;

/* --------------------------------------------------------- internal cheats */

gboolean
desmume_emu_add_cheat(guint32 address, int sizeInBytes, guint32 value,
                      const char *description, char **error_message)
{
	if (error_message != NULL)
	{
		*error_message = NULL;
	}
	if (!sHasROM || (cheats == NULL))
	{
		if (error_message != NULL)
		{
			*error_message = g_strdup("Load a ROM before adding cheats.");
		}
		return FALSE;
	}
	if ((address < RAMDebugCore::kMainMemAddress) ||
	    (address >= (RAMDebugCore::kMainMemAddress + RAMDebugCore::kMainMemSize)))
	{
		if (error_message != NULL)
		{
			*error_message = g_strdup(
				"Internal cheats can only target main RAM (0x02000000 - 0x023FFFFF).");
		}
		return FALSE;
	}

	const int sizeInBytesClamped = (sizeInBytes <= 1) ? 1 : (sizeInBytes >= 4) ? 4 : 2;
	const u8 cheatSize = (u8)(sizeInBytesClamped - 1); /* CHEATS::add uses 0=1 byte */
	char *descriptionCopy = g_strdup((description != NULL) ? description : "");
	const bool added = cheats->add(cheatSize, address, value, descriptionCopy, true);
	g_free(descriptionCopy);
	if (!added)
	{
		if (error_message != NULL)
		{
			*error_message = g_strdup("The cheat could not be added.");
		}
		return FALSE;
	}

	/* Persist the list the same way the Cheat List window does on close. */
	cheats->save();
	return TRUE;
}

void
desmume_emu_set_cheats_disabled(gboolean disabled)
{
	CommonSettings.cheatsDisable = (disabled != FALSE);
}

gboolean
desmume_emu_get_cheats_disabled(void)
{
	return CommonSettings.cheatsDisable ? TRUE : FALSE;
}

/* ------------------------------------------------------- P7a GPU filter */

gboolean
desmume_emu_set_gpu_filter(int upscaler, float scale)
{
	if (!WGPUPresent::isUpscaler(upscaler))
	{
		upscaler = WGPUPresentUpscaler_Off;
	}
	if (!(scale >= 1.0f))
	{
		scale = 1.0f;
	}
	if (scale > 4.0f)
	{
		scale = 4.0f;
	}

	const gboolean was_ready = sGpuFilterReady;
	const int old_upscaler = sGpuFilterUpscaler;

	sGpuFilterWanted = (upscaler != WGPUPresentUpscaler_Off) ? TRUE : FALSE;
	sGpuFilterUpscaler = upscaler;
	sGpuFilterScale = scale;

	if (!sGpuFilterWanted)
	{
		release_gpu_filter();
		return TRUE;
	}

	/* A device/pipeline is only created once and reused; changing the filter
	 * or scale takes effect on the next presented frame. */
	if (sGpuFilterPresent == NULL)
	{
		sGpuFilterPresent = new WGPUPresent();
		if (sGpuShaderSource == NULL)
		{
			sGpuShaderSource = WGPUCreateLinuxUpscalerShaderSource();
		}
		if (sGpuShaderSource != NULL)
		{
			sGpuFilterPresent->setUpscalerShaderSource(sGpuShaderSource);
		}
		// The off-screen target is (re)allocated by presentOffscreen() to match
		// the frame size, so the begin dimensions here are just a seed.
		if (!sGpuFilterPresent->beginOffscreen(256, 384, 0))
		{
			g_printerr("GTK4: WebGPU off-screen filter unavailable; "
			           "falling back to the plain frame path\n");
			release_gpu_filter();
			return FALSE;
		}
	}

	sGpuFilterPresent->setUpscaler(upscaler);
	sGpuFilterReady = TRUE;

	(void)was_ready;
	(void)old_upscaler;
	return TRUE;
}

int
desmume_emu_get_gpu_filter(void)
{
	return sGpuFilterUpscaler;
}

float
desmume_emu_get_gpu_scale(void)
{
	return sGpuFilterScale;
}

gboolean
desmume_emu_gpu_filter_active(void)
{
	return (sGpuFilterReady && sGpuFilterPresent != NULL &&
	        sGpuFilterUpscaler != WGPUPresentUpscaler_Off) ? TRUE : FALSE;
}

gboolean
desmume_emu_self_test_gpu_filter(void)
{
	if (!desmume_emu_gpu_filter_active())
	{
		return FALSE;
	}

	const int src_w = 256;
	const int src_h = 384;
	const int screen_h = src_h / 2;
	const int dst_w = (int)(src_w * sGpuFilterScale + 0.5f);
	const int dst_h = (int)(src_h * sGpuFilterScale + 0.5f);
	const size_t src_size = (size_t)src_w * src_h * 4;
	const size_t dst_size = (size_t)dst_w * dst_h * 4;

	uint32_t *src = (uint32_t *)g_malloc0(src_size);
	uint32_t *dst = (uint32_t *)g_malloc0(dst_size);
	if ((src == NULL) || (dst == NULL))
	{
		g_free(src);
		g_free(dst);
		return FALSE;
	}

	// A cheap gradient so the shader has non-uniform input to sample.
	for (int y = 0; y < src_h; ++y)
	{
		for (int x = 0; x < src_w; ++x)
		{
			src[(size_t)y * src_w + x] =
				(uint32_t)(0xFF000000u | ((x & 0xFF) << 16) | ((y & 0xFF) << 8));
		}
	}

	WGPUPresentRect srcRects[2];
	srcRects[0] = { 0, 0, src_w, screen_h };
	srcRects[1] = { 0, screen_h, src_w, src_h };

	WGPUPresentRect dstRects[2];
	dstRects[0] = { 0, 0, dst_w, dst_h / 2 };
	dstRects[1] = { 0, dst_h / 2, dst_w, dst_h };

	const bool ok = sGpuFilterPresent->presentOffscreen(
		src, src_w, src_h, srcRects, dstRects, NULL, 0, 0,
		dst, dst_w, dst_h, NULL);

	// Sanity: at least one pixel must be non-black after the blit.
	bool has_pixels = false;
	if (ok)
	{
		for (size_t i = 0; i < dst_w * (size_t)dst_h; ++i)
		{
			if ((dst[i] & 0x00FFFFFFu) != 0)
			{
				has_pixels = true;
				break;
			}
		}
	}

	g_free(src);
	g_free(dst);
	return (ok && has_pixels) ? TRUE : FALSE;
}

/* --------------------------------------------------------- movie (P4b) */

static gboolean
copy_movie_error(const char *error, char **error_message)
{
	if (error_message != NULL)
	{
		*error_message = (error != NULL) ? g_strdup(error) : NULL;
	}
	return error == NULL;
}

gboolean
desmume_emu_load_movie(const char *path, gboolean read_only,
                       int pause_frame, char **error_message)
{
	if (!sHasROM || (path == NULL))
	{
		if (error_message != NULL)
		{
			*error_message = g_strdup("Load a ROM before playing a movie.");
		}
		return FALSE;
	}
	/* Pause emulation while the movie takes over the frame loop. */
	desmume_emu_set_running(FALSE);
	const char *error = FCEUI_LoadMovie(path, read_only != FALSE, false, pause_frame);
	return copy_movie_error(error, error_message);
}

gboolean
desmume_emu_save_movie(const char *path, const char *author,
                       DesmumeMovieStartFrom start_from, const char *sram_path,
                       int year, int month, int day,
                       int hour, int minute, int second,
                       const char *description, char **error_message)
{
	if (!sHasROM || (path == NULL))
	{
		if (error_message != NULL)
		{
			*error_message = g_strdup("Load a ROM before recording a movie.");
		}
		return FALSE;
	}

	const START_FROM from = (start_from == DESMUME_MOVIE_START_SRAM)
		? START_SRAM
		: (start_from == DESMUME_MOVIE_START_SAVESTATE) ? START_SAVESTATE : START_BLANK;
	const DateTime rtc(year, month, day, hour, minute, second);
	const std::wstring wideAuthor = mbstowcs(author != NULL ? author : "");
	const std::wstring wideDescription = mbstowcs(description != NULL ? description : "");
	const char *error = FCEUI_SaveMovie(path, wideAuthor, from,
	                                    sram_path != NULL ? sram_path : "",
	                                    rtc, wideDescription);
	return copy_movie_error(error, error_message);
}

void
desmume_emu_stop_movie(void)
{
	FCEUI_StopMovie();
}

gboolean
desmume_emu_movie_active(void)
{
	return (movieMode == MOVIEMODE_PLAY || movieMode == MOVIEMODE_RECORD)
		? TRUE : FALSE;
}

char *
desmume_emu_rom_base_name(void)
{
	if (!sHasROM || (sRomName == NULL))
	{
		return g_strdup("");
	}
	char *base = g_path_get_basename(sRomName);
	char *dot = g_strrstr(base, ".");
	if (dot != NULL)
	{
		*dot = '\0';
	}
	char *result = g_strdup(base);
	g_free(base);
	return result;
}

/* ------------------------------------------------------------- key / touch */

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

	/* NDS_Init() -> MMU_Init() -> Mic_Init() opens an SDL audio stream.  SDL
	 * installs SIGINT/SIGTERM handlers by default and turns them into an
	 * SDL_QUIT event; this frontend has no SDL event loop, so Ctrl+C would be
	 * swallowed and the process would stay alive.  Ask SDL to leave POSIX
	 * signals alone before any SDL subsystem is touched.  Overwrite=TRUE so a
	 * stale environment value cannot reintroduce the hang. */
	g_setenv("SDL_NO_SIGNAL_HANDLERS", "1", TRUE);

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

	if (sFramePool == NULL)
	{
		sFramePool = frame_pool_new();
	}

	sInitialized = TRUE;
	return TRUE;
}

void
desmume_emu_shutdown(void)
{
	execute = false;
	sRunning = FALSE;

	release_gpu_filter();

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

	/* Drop the bridge's pool reference. Any GBytes still held by GSK keep the
	 * pool alive until their release callbacks run. */
	if (sFramePool != NULL)
	{
		frame_pool_unref(sFramePool);
		sFramePool = NULL;
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

	g_free(sRomName);
	sRomName = g_path_get_basename(path);

	/* Restart frame pacing/FPS accounting for the new ROM. */
	sFpsSecStart = 0;
	sFpsFrameCount = 0;
	sFpsValue = 0;
	sNextFrameTime = 0;
	sFrameMod3 = 0;

	sHasROM = TRUE;
	sRunning = TRUE;
	execute = true;
	desmume_emu_scan_states();
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

	for (int slot = 0; slot < NB_STATES; ++slot)
	{
		sStateExists[slot] = FALSE;
	}

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
desmume_emu_reset(void)
{
	if (!sHasROM)
	{
		return;
	}
	NDS_Reset();
}

static void
refresh_state_exists(void)
{
	for (int slot = 0; slot < NB_STATES; ++slot)
	{
		sStateExists[slot] = (savestates[slot].exists != FALSE);
	}
}

void
desmume_emu_scan_states(void)
{
	if (!sHasROM)
	{
		return;
	}
	scan_savestates();
	refresh_state_exists();
}

gboolean
desmume_emu_state_exists(int slot)
{
	if ((slot < 0) || (slot >= NB_STATES))
	{
		return FALSE;
	}
	return sStateExists[slot];
}

gboolean
desmume_emu_save_state(int slot)
{
	if (!sHasROM || (slot < 0) || (slot >= NB_STATES))
	{
		return FALSE;
	}
	savestate_slot(slot);
	refresh_state_exists();
	return TRUE;
}

gboolean
desmume_emu_load_state(int slot)
{
	if (!sHasROM || (slot < 0) || (slot >= NB_STATES))
	{
		return FALSE;
	}
	loadstate_slot(slot);
	return TRUE;
}

gboolean
desmume_emu_screenshot(const char *path, char **error_message)
{
	if (!sHasROM || (path == NULL) || (*path == '\0'))
	{
		if (error_message != NULL)
		{
			*error_message = g_strdup("No ROM is loaded.");
		}
		return FALSE;
	}

	const NDSDisplayInfo &info = GPU->GetDisplayInfo();
	if ((info.masterCustomBuffer == NULL) || (info.customWidth == 0) ||
	    (info.customHeight == 0))
	{
		if (error_message != NULL)
		{
			*error_message = g_strdup("No frame is available yet.");
		}
		return FALSE;
	}

	const gsize width = (gsize)info.customWidth;
	const gsize height = (gsize)info.customHeight * 2;
	const gsize pixel_count = width * height;
	const gsize stride = width * 4;

	guchar *bgra = (guchar *)g_malloc(stride * height);
	if (info.colorFormat == NDSColorFormat_BGR555_Rev)
	{
		ColorspaceConvertBuffer555xTo8888Opaque<true, false, BESwapNone>(
			(const u16 *)info.masterCustomBuffer, (u32 *)bgra, pixel_count);
	}
	else
	{
		ColorspaceConvertBuffer888xTo8888Opaque<true, false>(
			(const u32 *)info.masterCustomBuffer, (u32 *)bgra, pixel_count);
	}

	GBytes *bytes = g_bytes_new_take(bgra, stride * height);
	GdkTexture *texture = gdk_memory_texture_new((int)width, (int)height,
	                                             GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
	                                             bytes, stride);
	g_bytes_unref(bytes);

	const gboolean ok = gdk_texture_save_to_png(texture, path);
	g_object_unref(texture);

	if (!ok && (error_message != NULL))
	{
		*error_message = g_strdup_printf("Could not write '%s'.", path);
	}
	return ok;
}

void
desmume_emu_set_frame_sink(DesmumeFrameSink sink, gpointer user_data)
{
	sFrameSink = sink;
	sFrameSinkData = user_data;
}

/* ------------------------------------------------------------------- frame */

/* One core frame.  NDS_SkipNextFrame() is set before input/exec, matching
 * GTK3's EmuLoop: skipped frames still advance the core but ask the GPU not
 * to produce a display buffer for them. */
static void
run_core_frame(gboolean skip)
{
	if (skip)
	{
		NDS_SkipNextFrame();
	}

	apply_keys();
	NDS_beginProcessingInput();
	NDS_endProcessingInput();

	NDS_exec<false>();
	SPU_Emulate_user();
}

/* Convert the GPU's composite frame to BGRA8888 and hand it to the display.
 *
 * Performance note: this writes the color conversion straight into a pooled
 * opaque BGRA8888 buffer and transfers that GBytes to the sink. The previous
 * implementation ran VideoFilter::RunFilter() (a full-frame memcpy for the
 * None filter this frontend uses) and then the widget ran wrap_frame() (a
 * second full-frame memcpy plus a per-pixel alpha fixup). Both are gone:
 * ColorspaceConvertBuffer*Opaque already writes alpha=0xFF on every pixel,
 * so the pool buffer is a valid premultiplied B8G8R8A8 image for GDK as-is.
 * GTK3's Cairo path writes the conversion into the VideoFilter source buffer
 * and draws from it with zero copies; this path now does the same amount of
 * pixel work with no intermediate copies at all. */
static void
present_current_frame(void)
{
	if (sFrameSink == NULL || sFramePool == NULL)
	{
		return;
	}

	const NDSDisplayInfo &info = GPU->GetDisplayInfo();
	if ((info.masterCustomBuffer == NULL) || (info.customWidth == 0) ||
	    (info.customHeight == 0))
	{
		return;
	}

	const size_t src_width = info.customWidth;
	const size_t src_height = info.customHeight * 2;
	const size_t pixel_count = src_width * src_height;
	const gsize src_bytes = pixel_count * 4;

	/* The Opaque converters fill alpha with 0xFF, so no alpha pass is needed. */
	guchar *src_data = NULL;
	GBytes *frame = frame_pool_acquire(src_bytes, &src_data);
	if (frame == NULL)
	{
		return;
	}

	u32 *dst = (u32 *)src_data;
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

	int out_width = (int)src_width;
	int out_height = (int)src_height;

	/* P7a: run the selected GPU upscaler through the WebGPU off-screen path.
	 * The result is read back into a second pooled buffer and forwarded to the
	 * same sink, so the GSK texture path (and all layout/rotation logic) is
	 * unchanged. Any failure falls back to the unfiltered frame for this tick
	 * rather than showing a stale or black screen. */
	if (sGpuFilterReady && sGpuFilterPresent != NULL)
	{
		const int screen_h = out_height / 2;
		const int dst_w = (int)(out_width * sGpuFilterScale + 0.5f);
		const int dst_h = (int)(out_height * sGpuFilterScale + 0.5f);
		const int dst_screen_h = dst_h / 2;

		if ((screen_h > 0) && (dst_w > 0) && (dst_screen_h > 0))
		{
			const gsize needed = (gsize)dst_w * (gsize)dst_h * 4;
			guchar *gpu_data = NULL;
			GBytes *gpu_frame = frame_pool_acquire(needed, &gpu_data);

			if (gpu_frame != NULL)
			{
				WGPUPresentRect srcRects[2];
				srcRects[0].left = 0; srcRects[0].top = 0;
				srcRects[0].right = out_width; srcRects[0].bottom = screen_h;
				srcRects[1].left = 0; srcRects[1].top = screen_h;
				srcRects[1].right = out_width; srcRects[1].bottom = out_height;

				WGPUPresentRect dstRects[2];
				dstRects[0].left = 0; dstRects[0].top = 0;
				dstRects[0].right = dst_w; dstRects[0].bottom = dst_screen_h;
				dstRects[1].left = 0; dstRects[1].top = dst_screen_h;
				dstRects[1].right = dst_w; dstRects[1].bottom = dst_h;

				const bool ok = sGpuFilterPresent->presentOffscreen(
					(const uint32_t *)src_data, out_width, out_height,
					srcRects, dstRects, NULL, 0, 0,
					(uint32_t *)gpu_data, dst_w, dst_h, NULL);

				if (ok)
				{
					g_bytes_unref(frame);
					frame = gpu_frame;
					out_width = dst_w;
					out_height = dst_h;
				}
				else
				{
					g_bytes_unref(gpu_frame);
					/* Fall through with the unfiltered frame. */
				}
			}
		}
	}

	/* Ownership of `frame` transfers to the sink. */
	sFrameSink(frame, out_width, out_height, sFrameSinkData);
}

/* Number of emulated frames run per tick when the frame-rate limiter is off
 * or the boost/fast-forward key is held.  The GTK tick source fires at 60 Hz,
 * so four frames per tick gives roughly 4x speed; each tick presents the last
 * frame only, so the display rate stays manageable. */
#define DESMUME_FAST_FWD_FRAMES 4

void
desmume_emu_step(void)
{
	if (!sHasROM || !sRunning)
	{
		return;
	}

	const gint64 now = g_get_monotonic_time();
	const gboolean boost = sFastForward || (((sKeys >> 13) & 1) != 0);
	const gboolean throttle = sFpsLimiter && !boost;
	guint emulated = 0;

	if (!throttle)
	{
		/* No pacing: GTK3 runs the idle loop flat out here.  Our tick source
		 * is fixed at 60 Hz, so run several core frames per tick instead and
		 * present only the last one. */
		for (int i = 0; i < DESMUME_FAST_FWD_FRAMES; ++i)
		{
			run_core_frame(FALSE);
			++emulated;
		}
		present_current_frame();

		/* Resync so re-enabling the limiter does not try to catch up. */
		sNextFrameTime = 0;
		sFrameMod3 = 0;
	}
	else
	{
		if (sNextFrameTime == 0)
		{
			sNextFrameTime = now;
			sFrameMod3 = 0;
		}

		/* Fixed frameskip runs a set number of unrendered frames up front. */
		if (!sAutoFrameskip)
		{
			for (int i = 0; i < sFrameskipMax; ++i)
			{
				run_core_frame(TRUE);
				++emulated;
			}
		}

		/* The presented frame, then advance the 16/16/17 ms clock. */
		run_core_frame(FALSE);
		++emulated;
		present_current_frame();

		sFrameMod3 = (sFrameMod3 + 1) % 3;
		sNextFrameTime += (sFrameMod3 == 0) ? 16000 : 17000;

		/* Auto frameskip: catch up on ticks that arrived late.  Cap at the
		 * configured maximum; if that is still not enough, reset the clock
		 * rather than accumulating an ever-growing debt (GTK3 does the same). */
		if (sAutoFrameskip)
		{
			gint64 this_tick = g_get_monotonic_time();
			int skipped = 0;
			while (this_tick > sNextFrameTime && skipped < sFrameskipMax)
			{
				run_core_frame(TRUE);
				++emulated;
				++skipped;
				this_tick = g_get_monotonic_time();
				sFrameMod3 = (sFrameMod3 + 1) % 3;
				sNextFrameTime += (sFrameMod3 == 0) ? 16000 : 17000;
			}
			if (this_tick > sNextFrameTime)
			{
				sNextFrameTime = this_tick;
				sFrameMod3 = 0;
			}
		}
	}

	/* Rolling one-second FPS window, counting skipped frames like GTK3. */
	sFpsFrameCount += emulated;
	if (sFpsSecStart == 0)
	{
		sFpsSecStart = now;
		sFpsFrameCount = 0;
	}
	else if ((now - sFpsSecStart) >= 1000000)
	{
		sFpsValue = sFpsFrameCount;
		sFpsFrameCount = 0;
		sFpsSecStart = now;
	}
}

gboolean
desmume_emu_step_frame_advance(void)
{
	if (!sHasROM)
	{
		return FALSE;
	}

	/* A single frame must run even while paused: temporarily allow execution,
	 * then restore the caller's running state. */
	const gboolean was_running = sRunning;
	sRunning = TRUE;
	execute = true;

	run_core_frame(FALSE);
	present_current_frame();

	sRunning = was_running;
	execute = (sRunning ? true : false);

	/* Resync timing so resuming normal playback does not skip a burst. */
	sNextFrameTime = 0;
	sFrameMod3 = 0;
	return TRUE;
}

/* --------------------------------------------------------- P3d speed state */

void desmume_emu_set_fps_limiter(gboolean enabled) { sFpsLimiter = enabled; }
gboolean desmume_emu_get_fps_limiter(void) { return sFpsLimiter; }
void desmume_emu_set_auto_frameskip(gboolean enabled) { sAutoFrameskip = enabled; }
gboolean desmume_emu_get_auto_frameskip(void) { return sAutoFrameskip; }

void
desmume_emu_set_frameskip(int value)
{
	if (value < 0) value = 0;
	if (value > 9) value = 9;
	sFrameskipMax = value;
}

int desmume_emu_get_frameskip(void) { return sFrameskipMax; }

void
desmume_emu_set_fast_forward(gboolean enabled)
{
	sFastForward = enabled;
}

gboolean desmume_emu_get_fast_forward(void) { return sFastForward; }

guint desmume_emu_get_fps(void) { return sFpsValue; }

/* ------------------------------------------------------------- IO registers */

guint32
desmume_emu_read_ioreg(int cpu, DesmumeIOReg reg)
{
	if (!sHasROM || (cpu < 0) || (cpu > 1))
	{
		return 0;
	}

	switch (reg)
	{
	case DESMUME_IOREG_IME:
		return MMU.reg_IME[cpu];
	case DESMUME_IOREG_IE:
		return MMU.reg_IE[cpu];
	case DESMUME_IOREG_IF:
		return (cpu == 0) ? MMU.gen_IF<0>() : MMU.gen_IF<1>();
	case DESMUME_IOREG_IPCFIFOCNT:
		return ((u16 *)(MMU.MMU_MEM[cpu][0x40]))[0x184 >> 1];
	case DESMUME_IOREG_POWCNT:
		return ((u16 *)(MMU.MMU_MEM[cpu][0x40]))[0x304 >> 1];
	case DESMUME_IOREG_SPICNT:
		return ((u16 *)(MMU.MMU_MEM[cpu][0x40]))[0x1C0 >> 1];
	default:
		return 0;
	}
}
