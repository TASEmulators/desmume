 /*
	Copyright (C) 2007 Pascal Giard (evilynux)
	Copyright (C) 2006-2025 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GTK_UI
#define GTK_UI
#endif

#include "version.h"

#include <stdlib.h>
#include <stdio.h>
#include <cstdio>
#include <string.h>
#include <gtk/gtk.h>
#include <gio/gapplication.h>
#include <gdk/gdkkeysyms.h>
#include <SDL3/SDL.h>
#include <X11/Xlib.h>
#include <vector>
#include <set>
#include <map>
#include <string>

#if defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#endif
#if defined(GDK_WINDOWING_WAYLAND)
#include <gdk/gdkwayland.h>
#endif

#ifdef AGG2D_USE_VECTORFONTS
#include <fontconfig/fontconfig.h>
#endif

#include "types.h"
#include "main.h"
#include "firmware.h"
#include "NDSSystem.h"
#include "wifi.h"
#include "driver.h"
#include "SPU.h"
#include "../shared/sndsdl.h"
#include "../shared/ctrlssdl.h"
#include "MMU.h"
#include "Database.h"
#include "mc.h"
#include "render3D.h"
#include "desmume.h"
#include "debug.h"
#include "rasterize.h"
#include "saves.h"
#include "mic.h"
#include "movie.h"
#include "tools/moviePlay.h"
#include "tools/movieRecord.h"
#include "dTool.h"
#include "../shared/desmume_config.h"
#include "../shared/gdksdl.cpp"
#include "../../../WGPURender.h"
#if defined(ENABLE_WGPU)
#include "wgpu.h"
#endif
#include "../shared/wgpu_display_linux.h"
#if defined(DESMUME_WGPU_OSD)
#include "frontend/modules/osd/wgpu/wgpu_osd_draw.h"
#endif
#include "cheatsGTK.h"
#include "frontend/modules/osd/agg/agg_osd.h"

#include "../shared/avout_x264.h"
#include "../shared/avout_flac.h"
#ifdef HAVE_FFMPEG_RECORD
#include "../shared/avout_ffmpeg.h"
#endif

#include "commandline.h"

#include "slot2.h"
#include "slot1.h"
#include "path.h"
#include "pathsettings.h"

#include "utils/xstring.h"

#include "filter/videofilter.h"

#ifdef GDB_STUB
	#include "armcpu.h"
	#include "gdbstub.h"
#endif

#ifdef HAVE_LUA
	#include "tools/luaScriptConsole.h"
	#include "lua-engine.h"
#endif

#if defined(ENABLE_OPENGL_STANDARD) || defined(ENABLE_OPENGL_ES)
	#if defined(ENABLE_OPENGL_ES)
		#include "OGLRender_ES3.h"
	#else
		#include "OGLRender_3_2.h"
	#endif

	#if defined(ENABLE_GLX)
		#include "../shared/glx_3Demu.h"
	#elif defined(ENABLE_EGL)
		#include "../shared/egl_3Demu.h"
	#else
		#include "../shared/sdl_3Demu.h"
	#endif
#endif

#include "config.h"

BOOL Mic_ApplySettings(const char *sampleFile);

#undef GPOINTER_TO_INT
#define GPOINTER_TO_INT(p) ((gint)  (glong) (p))

#undef GPOINTER_TO_UINT
#define GPOINTER_TO_UINT(p) ((guint)  (glong) (p))

#define EMULOOP_PRIO (G_PRIORITY_HIGH_IDLE + 20 + 1)

#define GAP_SIZE 64

static int draw_count;
extern int _scanline_filter_a, _scanline_filter_b, _scanline_filter_c, _scanline_filter_d;
VideoFilter* video;

// WebGPU presentation. The object is a stub when the build does not include
// wgpu-native, in which case every frame falls back to the Cairo path below.
static WGPULinuxDisplay wgpu_display;
#if defined(ENABLE_WGPU)
static bool wgpu_initialization_failed = false;
#endif

#if defined(DESMUME_WGPU_OSD)
// GPU HUD state (wgpu-agg.md 22). The target records agg_osd.cpp / Lua agg.*
// drawing and renders it into a texture that WGPU_PresentFrame samples as a
// second layer. sGpuHudView is what the last rendered frame left behind; it has
// to stay alive until the Expose callback that presents that frame, and must be
// dropped whenever the device/surface is recreated.
static WGPUOSDDrawTarget *sGpuHudTarget = NULL;
static bool sGpuHudFontsRegistered = false;
static bool sGpuHudFrameActive = false;
static void *sGpuHudView = NULL;
static int sGpuHudViewWidth = 0;
static int sGpuHudViewHeight = 0;
#endif

// Drops the cached HUD view. Called whenever the WebGPU surface is torn down:
// the view belongs to a texture that is about to be destroyed.
static void WGPU_HudInvalidateView()
{
#if defined(DESMUME_WGPU_OSD)
	sGpuHudView = NULL;
	sGpuHudViewWidth = 0;
	sGpuHudViewHeight = 0;
#endif
}

static void WGPU_KillDisplay()
{
#if defined(DESMUME_WGPU_OSD)
	// The HUD renderer keeps its own references to the presentation instance,
	// device and queue so it can outlive a teardown. Release them *before* the
	// display drops its own references, or the old instance survives until the
	// HUD is rebuilt on the next display and is destroyed while the new one is
	// presenting. NVIDIA's Vulkan driver clears process-global WSI callbacks
	// when any WSI-initialized instance is destroyed, which then makes the next
	// swap-chain acquire on the surviving instance jump to NULL in
	// libnvidia-glcore.
	if (sGpuHudTarget != NULL)
	{
		sGpuHudTarget->killGPU();
	}
#endif

	wgpu_display.kill();
	WGPU_HudInvalidateView();
}

// True when PrepareHudTarget() installed the GPU recorder for this frame.
static bool WGPU_HudFrameActive()
{
#if defined(DESMUME_WGPU_OSD)
	return sGpuHudFrameActive;
#else
	return false;
#endif
}

#define GPU_SCALE_FACTOR_MIN 1.0f
#define GPU_SCALE_FACTOR_MAX 16.0f

float gpu_scale_factor = 1.0f;

Gtk3GPUEventHandler *gtk3gpuEvent = NULL;


desmume::config::Config config;
bool in_joy_config_mode = false;

// Base name (no directory, no extension) of the loaded ROM. Tools use it for
// default file names; RAM Watch suggests "<ROM>.wch" for a new watch list.
static std::string sCurrentROMBaseName;

#ifdef GDB_STUB
gdbstub_handle_t arm9_gdb_stub = NULL;
gdbstub_handle_t arm7_gdb_stub = NULL;
#endif

enum {
    MAIN_BG_0 = 0,
    MAIN_BG_1,
    MAIN_BG_2,
    MAIN_BG_3,
    MAIN_OBJ,
    SUB_BG_0,
    SUB_BG_1,
    SUB_BG_2,
    SUB_BG_3,
    SUB_OBJ
};

#ifdef HAVE_LIBAGG
enum hud_display_enum {
    HUD_DISPLAY_FPS,
    HUD_DISPLAY_INPUT,
    HUD_DISPLAY_GINPUT,
    HUD_DISPLAY_FCOUNTER,
    HUD_DISPLAY_LCOUNTER,
    HUD_DISPLAY_RTC,
    HUD_DISPLAY_MIC,
    HUD_DISPLAY_EDITOR,
};
#endif

#ifdef AGG2D_USE_VECTORFONTS
#define VECTOR_FONT_BASE_SIZE 16

static FcConfig* fontConfig;
static std::string vectorFontFile;

static std::string FindFontFile(const char* fontName, bool bold);
#endif

gboolean EmuLoop(gpointer data);

static AVOutX264 avout_x264;
static AVOutFlac avout_flac;
#ifdef HAVE_FFMPEG_RECORD
static AVOutFFmpeg avout_ffmpeg;

// True while the FFmpeg recorder is capturing the video filter's output
// (which also contains the CPU HUD/Lua overlay) instead of the raw custom
// buffer. The frame serial distinguishes real emulated frames from manual
// RedrawScreen() presents, which reuse the previous frame.
static bool sRecordFilteredOutput = false;
static uint64_t sFrameSerial = 0;
static uint64_t sLastRecordedFrameSerial = 0;
static std::vector<uint8_t> sRecordFrameBuffer;
#endif
static void RecordAV_x264(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void RecordAV_flac(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void RecordAV_stop(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#ifdef HAVE_FFMPEG_RECORD
static void RecordAV_Video(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#else
// The menu entry is always present, so register a placeholder action in builds
// configured without FFmpeg instead of leaving an unknown action behind.
static void RecordAV_VideoUnavailable(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#endif
static void RedrawScreen();

static void DoQuit(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void RecordMovieDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void PlayMovieDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void StopMovie(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void OpenNdsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void OpenRecent(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void SaveStateDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void LoadStateDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ExportBackupMemoryDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ImportBackupMemoryDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
void Launch(GSimpleAction *action, GVariant *parameter, gpointer user_data);
void Pause(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void FrameAdvance(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void EmuSingleFrame();
static void ResetSaveStateTimes();
static void LoadSaveStateInfo();
static void Printscreen(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Reset(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void SoundSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void MicrophoneSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void WifiSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Slot1SettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Slot2SettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void RomInfoDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void FirmwareSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Edit_Controls(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HotkeyConfigDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void TogglePause(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void MenuSave(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void changesavetype(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void MenuLoad(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void About(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleMenuVisible(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleStatusbarVisible(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleToolbarVisible(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleFullscreen(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleCheatsDisabled(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Modify_SPUMode(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Modify_SPUInterpolation(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#ifdef FAKE_MIC
static void ToggleMicNoise(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#endif
static void ToggleFpsLimiter(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleAutoFrameskip(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Modify_Frameskip(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleSwapScreens(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleGap(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void SetRotation(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void SetWinsize(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Modify_PriInterpolation(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void Modify_Interpolation(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void SetOrientation(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ToggleLayerVisibility(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#ifdef HAVE_LIBAGG
static void ToggleHudDisplay(hud_display_enum hudId, gboolean active);
static void HudFps(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudInput(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudGraphicalInput(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudFrameCounter(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudLagCounter(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudRtc(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudMic(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudEditor(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudResetLayout(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void HudSaveLayout();
static void HudLoadLayout();
#endif
#ifdef DESMUME_GTK_FIRMWARE_BROKEN
static void SelectFirmwareFile(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#endif
static void EmulationSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void GraphicsSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void PathSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data);

#ifdef HAVE_LUA
static void AddLuaScript(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void CloseAllLuaScripts(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ShowAboutLuaScripting(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#endif

/* GAction names may only contain alphanumerics, '-' and '.', so hyphens are
 * used instead of underscores.  Keep these names in sync with menu.ui. */
static const GActionEntry app_entries[] = {
    // File
    { "open",          OpenNdsDialog },
    { "recent",        OpenRecent, "s" },
    { "run",           Launch },
    { "pause",         Pause },
    { "toggle-pause",  TogglePause },
    { "frame-advance", FrameAdvance },
    { "reset",         Reset },
    { "save-state-to",   SaveStateDialog },
    { "load-state-from", LoadStateDialog },
    { "savestate",     MenuSave, "u" },
    { "loadstate",     MenuLoad, "u" },
    { "importbackup",  ImportBackupMemoryDialog },
    { "exportbackup",  ExportBackupMemoryDialog },
    { "recordmovie",   RecordMovieDialog },
    { "playmovie",     PlayMovieDialog },
    { "stopmovie",     StopMovie },
    { "record-x264",   RecordAV_x264 },
    { "record-flac",   RecordAV_flac },
#ifdef HAVE_FFMPEG_RECORD
    { "record-video",  RecordAV_Video },
#else
    { "record-video",  RecordAV_VideoUnavailable },
#endif
    { "record-stop",   RecordAV_stop },
#ifdef DESMUME_GTK_FIRMWARE_BROKEN
    { "loadfirmware",  SelectFirmwareFile },
#endif
    { "printscreen",   Printscreen },
    { "quit",          DoQuit },

    // View
    { "orient",         SetOrientation,         "s",  "\"vertical\"" },
    { "swapscreens",    ToggleSwapScreens,      NULL, "true" },
    { "rotate",         SetRotation,            "s",  "\"0\"" },
    { "winsize",        SetWinsize,             "s",  "\"scale\"" },
    { "fullscreen",     ToggleFullscreen,       NULL, "false" },
    { "gap",            ToggleGap,              NULL, "false" },
    { "pri-interpolation", Modify_PriInterpolation, "s", "\"none\"" },
    { "interpolation",  Modify_Interpolation,   "s",  "\"nearest\"" },
    { "view-menu",      ToggleMenuVisible,      NULL, "true" },
    { "view-toolbar",   ToggleToolbarVisible,   NULL, "true" },
    { "view-statusbar", ToggleStatusbarVisible, NULL, "true" },
#ifdef HAVE_LIBAGG
    { "hud-fps",        HudFps,                 NULL, "false" },
    { "hud-input",      HudInput,               NULL, "false" },
    { "hud-graphicalinput", HudGraphicalInput,  NULL, "false" },
    { "hud-framecounter", HudFrameCounter,      NULL, "false" },
    { "hud-lagcounter", HudLagCounter,          NULL, "false" },
    { "hud-rtc",        HudRtc,                 NULL, "false" },
    { "hud-mic",        HudMic,                 NULL, "false" },
    { "hud-editor",     HudEditor,              NULL, "false" },
    { "hud-reset-layout", HudResetLayout},
#endif

    // Config
    { "enablefpslimiter",  ToggleFpsLimiter,    NULL, "true" },
    { "frameskipA",        ToggleAutoFrameskip, NULL, "true" },
    { "frameskip",         Modify_Frameskip,    "s",  "\"0\"" },
    { "soundsettings",     SoundSettingsDialog },
    { "microphone-settings", MicrophoneSettingsDialog },
    { "wifi-settings",     WifiSettingsDialog },
    { "slot1-settings",    Slot1SettingsDialog },
    { "slot2-settings",    Slot2SettingsDialog },
    { "path-settings",     PathSettingsDialog },
    { "rom-info",          RomInfoDialog },
#ifdef FAKE_MIC
    { "micnoise",          ToggleMicNoise,      NULL, "false" },
#endif
    { "emulationsettings",   EmulationSettingsDialog },
    { "graphics-settings",   GraphicsSettingsDialog },
    { "spu-mode",            Modify_SPUMode,          "s", "\"dual-async\"" },
    { "spu-interpolation",   Modify_SPUInterpolation, "s", "\"linear\"" },
    { "cheatsearch",         CheatSearch },
    { "cheatlist",           CheatList },
    { "cheatsdisable",       ToggleCheatsDisabled, NULL, "false" },
    { "savetype",            changesavetype,          "s", "\"autodetect\"" },
    { "firmwaresettings",    FirmwareSettingsDialog },
    { "editctrls",           Edit_Controls },
    { "hotkeys",             HotkeyConfigDialog },
    // Tools
    // dTool entries are populated dynamically in desmume_gtk_menu_tools().
#ifdef HAVE_LUA
    { "addluascript",        AddLuaScript },
    { "closealluascripts",   CloseAllLuaScripts },
    { "aboutluascripting",   ShowAboutLuaScripting },
#endif

    // Help
    { "about",         About },
};

enum winsize_enum {
	WINSIZE_SCALE = 0,
	WINSIZE_HALF = 1,
	WINSIZE_1 = 2,
	WINSIZE_1HALF = 3,
	WINSIZE_2 = 4,
	WINSIZE_2HALF = 5,
	WINSIZE_3 = 6,
	WINSIZE_3HALF = 7,
	WINSIZE_4 = 8,
	WINSIZE_4HALF = 9,
	WINSIZE_5 = 10,
};

static winsize_enum winsize_current;

/* When adding modes here remember to add the relevent entry to screen_size */
enum orientation_enum {
    ORIENT_VERTICAL = 0,
    ORIENT_HORIZONTAL = 1,
    ORIENT_SINGLE = 2,
    ORIENT_HYBRID_EQUAL = 3,
    ORIENT_HYBRID_VERTICAL = 4,
    ORIENT_N
};

struct screen_size_t {
   gint width;
   gint height;
};

const struct screen_size_t screen_size[ORIENT_N] = {
    {GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT*2},
    {GPU_FRAMEBUFFER_NATIVE_WIDTH*2, GPU_FRAMEBUFFER_NATIVE_HEIGHT},
    {GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT},
    {GPU_FRAMEBUFFER_NATIVE_WIDTH*3, GPU_FRAMEBUFFER_NATIVE_HEIGHT*2},
    {GPU_FRAMEBUFFER_NATIVE_WIDTH*3, GPU_FRAMEBUFFER_NATIVE_HEIGHT*2}
};

enum spumode_enum {
    SPUMODE_DUALASYNC = 0,
    SPUMODE_SYNCN = 1,
    SPUMODE_SYNCZ = 2,
    SPUMODE_SYNCP = 3
};

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
&SNDSDL,
NULL
};

static void ApplySPUMode(spumode_enum mode);
static void ApplySPUInterpolation(SPUInterpolationMode mode);

// Filled by Build3DRendererList(). The first entries are fixed, but WebGPU
// contributes one entry per usable backend, so the tail is dynamic.
#define GPU3D_MAX_COUNT 16

GPU3DInterface *core3DList[GPU3D_MAX_COUNT] = { NULL };
static int wgpuFirstCoreIndex = -1;

// Builds the renderer list: the fixed cores, then one entry per WebGPU backend
// that wgpu-native reported ("WebGPU - Vulkan", ...). Software adapters are
// filtered out by the renderer itself; SoftRasterizer stays in the list.
static void Build3DRendererList()
{
	if (core3DList[0] != NULL)
	{
		return;
	}

	int count = 0;

	core3DList[count++] = &gpu3DNull;
	core3DList[count++] = &gpu3DRasterize;

#if defined(ENABLE_OPENGL_ES)
	core3DList[count++] = &gpu3Dgl_ES_3_0;
#elif defined(ENABLE_OPENGL_STANDARD)
	core3DList[count++] = &gpu3Dgl;
	core3DList[count++] = &gpu3DglOld;
	core3DList[count++] = &gpu3Dgl_3_2;
#endif

#if defined(ENABLE_WGPU)
	wgpuFirstCoreIndex = count;

	WGPURender_InitBackendList();

	const size_t backendCount = WGPURender_GetBackendCount();

	for (size_t i = 0; (i < backendCount) && (count < (GPU3D_MAX_COUNT - 2)); i++)
	{
		core3DList[count++] = WGPURender_GetBackendInterface(i);
	}

	// Nothing reported (no usable adapter): still offer the generic entry so the
	// user can try, and so a config that already selected WebGPU keeps working.
	if (backendCount == 0)
	{
		core3DList[count++] = &gpu3DWGPU;
	}
#endif

	core3DList[count] = NULL;
}

// The WGPUInstanceBackend mask the WebGPU presenter should use. A mask of zero
// asks wgpu-native for every backend it was built with, which on Linux also
// creates a GLES instance. That instance is destroyed at process exit, after
// GTK has already released the EGL display; eglDestroyContext() then fails
// without an EGL error and the khronos-egl wrapper panics while unwinding
// through wgpuInstanceRelease(), which aborts the process. Never pass zero:
// use the backend the user picked, or the first one the renderer enumeration
// found (its preferred backend).
#if defined(ENABLE_WGPU)
static uint32_t WGPU_PresentationBackendMask()
{
	WGPURender_InitBackendList();

	const size_t backendCount = WGPURender_GetBackendCount();
	const uint32_t requested = (uint32_t)config.presentationWGPUBackend;

	if (requested != 0)
	{
		for (size_t i = 0; i < backendCount; i++)
		{
			if (WGPURender_GetBackendMask(i) == requested)
			{
				return requested;
			}
		}
	}

	if (backendCount > 0)
	{
		return WGPURender_GetBackendMask(0);
	}

	// No adapter was reported. Pin Vulkan rather than asking for every backend;
	// if it cannot present, begin() fails and Cairo keeps drawing.
	return (uint32_t)WGPUInstanceBackend_Vulkan;
}
#endif // ENABLE_WGPU

int multisampleSizes[] = {0, 2, 4, 8, 16, 32};

static const u32 gtk_kb_cfg[NB_KEYS] = {
    GDK_KEY_x,         // A
    GDK_KEY_z,         // B
    GDK_KEY_Shift_R,   // select
    GDK_KEY_Return,    // start
    GDK_KEY_Right,     // Right
    GDK_KEY_Left,      // Left
    GDK_KEY_Up,        // Up
    GDK_KEY_Down,      // Down
    GDK_KEY_w,         // R
    GDK_KEY_q,         // L
    GDK_KEY_s,         // X
    GDK_KEY_a,         // Y
    GDK_KEY_p,         // DEBUG
    GDK_KEY_o,         // BOOST
    GDK_KEY_BackSpace, // Lid
};

GKeyFile *keyfile;

struct modify_key_ctx {
    gint mk_key_chosen;
    GtkWidget *label;
    u8 key_id;
};

static u16 keys_latch = 0;

/* Buttons currently held through their turbo key.  A turbo key is an extra
   binding that belongs to a DS button but may be a different key, so it is
   tracked apart from keys_latch and merged in by the input core. */
static u16 turbo_latch = 0;

/* Keypad mask of the button whose turbo key is this keyval, or 0. */
static u16 lookup_turbo_key(u32 keyval)
{
    if (keyval == 0)
        return 0;

    for (int i = 0; i < NB_KEYS; i++) {
        if (turbo_keys[i] != 0 && turbo_keys[i] == keyval)
            return KEYMASK_(i);
    }

    return 0;
}
u32 Keypad_Temp[NB_KEYS];
static u32 Joypad_Temp[NB_KEYS];

class configured_features : public CommandLine
{
public:
  int engine_3d;
  int savetype;

  int firmware_language;

  int timeout;
};

static void
init_configured_features( class configured_features *config	)
{
  if(config->render3d == COMMANDLINE_RENDER3D_GL || config->render3d == COMMANDLINE_RENDER3D_OLDGL || config->render3d == COMMANDLINE_RENDER3D_AUTOGL)
    config->engine_3d = 2;
  else if (config->render3d == COMMANDLINE_RENDER3D_WGPU)
	  // Resolved to the first WebGPU entry once Build3DRendererList() has run.
    config->engine_3d = -2;
  else if (config->render3d == COMMANDLINE_RENDER3D_DEFAULT)
	  // Setting it to -1 so that common_gtk_main() will ignore it and load it from file or reconfigure it.
    config->engine_3d = -1;
  else
	  config->engine_3d = 1;

  config->savetype = 0;

  config->timeout = 0;

  /* use the default language */
  config->firmware_language = -1;

  /* If specified by --lang option the lang will change to choosed one */
  if (config->languageSet)
    config->firmware_language = config->language;
}

static int
fill_configured_features( class configured_features *config,
                          char ** argv)
{
  GOptionEntry options[] = {
    { "3d-render", 0, 0, G_OPTION_ARG_INT, &config->engine_3d, "Select 3D rendering engine. Available engines:\n"
        "\t\t\t\t  0 = 3d disabled\n"
        "\t\t\t\t  1 = internal rasterizer (default)\n"
#if defined(ENABLE_OPENGL_STANDARD) || defined(ENABLE_OPENGL_ES)
        "\t\t\t\t  2 = opengl\n"
#endif
        ,"ENGINE"},
    { "save-type", 0, 0, G_OPTION_ARG_INT, &config->savetype, "Select savetype from the following:\n"
    "\t\t\t\t  0 = Autodetect (default)\n"
    "\t\t\t\t  1 = EEPROM 4kbit\n"
    "\t\t\t\t  2 = EEPROM 64kbit\n"
    "\t\t\t\t  3 = EEPROM 512kbit\n"
    "\t\t\t\t  4 = FRAM 256kbit\n"
    "\t\t\t\t  5 = FLASH 2mbit\n"
    "\t\t\t\t  6 = FLASH 4mbit\n",
    "SAVETYPE"},
    { "fwlang", 0, 0, G_OPTION_ARG_INT, &config->firmware_language, "Set the language in the firmware, LANG as follows:\n"
                                    "\t\t\t\t  0 = Japanese\n"
                                    "\t\t\t\t  1 = English\n"
                                    "\t\t\t\t  2 = French\n"
                                    "\t\t\t\t  3 = German\n"
                                    "\t\t\t\t  4 = Italian\n"
                                    "\t\t\t\t  5 = Spanish\n",
                                    "LANG"},
    { "timeout", 0, 0, G_OPTION_ARG_INT, &config->timeout, "Quit DeSmuME after the specified seconds for testing purpose.", "SECONDS"},
    { NULL }
  };

  //g_option_context_add_main_entries (config->ctx, options, "options");
  //g_option_context_add_group (config->ctx, gtk_get_option_group (TRUE));

  if(!config->validate())
    goto error;

  if (config->savetype < 0 || config->savetype > 6) {
    g_printerr("Accepted savetypes are from 0 to 6.\n");
    return false;
  }

  if (config->firmware_language < -1 || config->firmware_language > 5) {
    g_printerr("Firmware language must be set to a value from 0 to 5.\n");
    goto error;
  }

  // Check if the commandLine argument was actually passed
	if (config->engine_3d != -1) {
		if (config->engine_3d != 0 && config->engine_3d != 1
#if defined(ENABLE_OPENGL_STANDARD) || defined(ENABLE_OPENGL_ES)
				&& config->engine_3d != 2
#endif
						) {
			g_printerr("Currently available ENGINES: 0, 1"
#if defined(ENABLE_OPENGL_STANDARD) || defined(ENABLE_OPENGL_ES)
							", 2"
#endif
					"\n");
			goto error;
		}
	}
#ifdef GDB_STUB
  if (config->arm9_gdb_port != 0 && (config->arm9_gdb_port < 1 || config->arm9_gdb_port > 65535)) {
    g_printerr("ARM9 GDB stub port must be in the range 1 to 65535\n");
    goto error;
  }

  if (config->arm7_gdb_port != 0 && (config->arm7_gdb_port < 1 || config->arm7_gdb_port > 65535)) {
    g_printerr("ARM7 GDB stub port must be in the range 1 to 65535\n");
    goto error;
  }
#endif

  return 1;

error:
  config->errorHelp(argv[0]);

  return 0;
}


/*
 * The thread handling functions needed by the GDB stub code.
 */
#ifdef GDB_STUB
void *
createThread_gdb( void (*thread_function)( void *data),
                  void *thread_data)
{
  GThread *new_thread = g_thread_new(NULL,
                                     (GThreadFunc)thread_function,
                                     thread_data);

  return new_thread;
}

void
joinThread_gdb( void *thread_handle) {
  g_thread_join( (GThread *)thread_handle);
}
#endif

/************************ GTK *******************************/

uint SPUMode = SPUMODE_DUALASYNC;
uint Frameskip = 0;
uint autoFrameskipMax = 0;
bool autoframeskip = true;
cairo_filter_t Interpolation = CAIRO_FILTER_NEAREST;

static GtkApplication *pApp;
static GtkWidget *pWindow;
static GtkWidget *pToolBar;
static GtkWidget *pStatusBar;
static GtkWidget *pDrawingArea;
static GtkWidget *pContentBox;
static GtkMenu *pPopupMenu;

struct nds_screen_t {
    guint gap_size;
    gint rotation_angle;
    orientation_enum orientation;
    cairo_matrix_t touch_matrix;
    cairo_matrix_t touch_matrix_hybrid;
    cairo_matrix_t topscreen_matrix;
    cairo_matrix_t topscreen_matrix_hybrid;
    gboolean swap;
};

struct nds_screen_t nds_screen;

static guint regMainLoop = 0;

// Set when a frame was stepped outside EmuLoop (frame advance).  EmuLoop's
// clock then restarts instead of treating the pause as catch-up.
static bool sResyncTiming = false;

#ifdef HAVE_LUA
static void AddLuaScript(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    lua_script_open_console(GTK_WINDOW(pWindow));
}
static void CloseAllLuaScripts(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    lua_script_close_all();
}
static void ShowAboutLuaScripting(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    lua_script_show_about(GTK_WINDOW(pWindow));
}

// gui.popup() / input.popup().  Runs on the GTK main thread because scripts do;
// the returned strings match the Windows MessageBox mapping exactly.
static const char* GtkLua_ShowPopup(const char* message, const char* type, const char* icon)
{
    GtkMessageType messageType = GTK_MESSAGE_INFO;
    if (icon && g_ascii_strcasecmp(icon, "question") == 0)
        messageType = GTK_MESSAGE_QUESTION;
    else if (icon && g_ascii_strcasecmp(icon, "warning") == 0)
        messageType = GTK_MESSAGE_WARNING;
    else if (icon && g_ascii_strcasecmp(icon, "error") == 0)
        messageType = GTK_MESSAGE_ERROR;

    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(pWindow),
        (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        messageType, GTK_BUTTONS_NONE, "%s", message ? message : "");

    // GTK has no stock Abort/Retry/Ignore response ids.
    enum { RESPONSE_ABORT = 1, RESPONSE_RETRY, RESPONSE_IGNORE };

    if (!type || g_ascii_strcasecmp(type, "ok") == 0)
    {
        gtk_dialog_add_button(GTK_DIALOG(dialog), "_OK", GTK_RESPONSE_OK);
    }
    else if (g_ascii_strcasecmp(type, "yesno") == 0)
    {
        gtk_dialog_add_button(GTK_DIALOG(dialog), "_No", GTK_RESPONSE_NO);
        gtk_dialog_add_button(GTK_DIALOG(dialog), "_Yes", GTK_RESPONSE_YES);
    }
    else if (g_ascii_strcasecmp(type, "yesnocancel") == 0)
    {
        gtk_dialog_add_button(GTK_DIALOG(dialog), "_Cancel", GTK_RESPONSE_CANCEL);
        gtk_dialog_add_button(GTK_DIALOG(dialog), "_No", GTK_RESPONSE_NO);
        gtk_dialog_add_button(GTK_DIALOG(dialog), "_Yes", GTK_RESPONSE_YES);
    }
    else if (g_ascii_strcasecmp(type, "okcancel") == 0)
    {
        gtk_dialog_add_button(GTK_DIALOG(dialog), "_Cancel", GTK_RESPONSE_CANCEL);
        gtk_dialog_add_button(GTK_DIALOG(dialog), "_OK", GTK_RESPONSE_OK);
    }
    else // abortretryignore
    {
        gtk_dialog_add_button(GTK_DIALOG(dialog), "_Ignore", RESPONSE_IGNORE);
        gtk_dialog_add_button(GTK_DIALOG(dialog), "_Retry", RESPONSE_RETRY);
        gtk_dialog_add_button(GTK_DIALOG(dialog), "_Abort", RESPONSE_ABORT);
    }

    const char* answer = "ok";
    switch (gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        case GTK_RESPONSE_YES: answer = "yes"; break;
        case GTK_RESPONSE_NO: answer = "no"; break;
        case GTK_RESPONSE_CANCEL: answer = "cancel"; break;
        case RESPONSE_ABORT: answer = "abort"; break;
        case RESPONSE_RETRY: answer = "retry"; break;
        case RESPONSE_IGNORE: answer = "ignore"; break;
        default: break;
    }
    gtk_widget_destroy(dialog);
    return answer;
}

// emu.openscript().  A new console window is the POSIX equivalent of the
// Windows dialog.  The script is started on the next idle, the way Windows
// posts IDC_BUTTON_LUARUN, so emu.openscript() returns before the new script
// runs (pcall(emu.openscript, ...) in the examples depends on that).
static char sLuaOpenScriptError[1200];

static const char* GtkLua_OpenScript(const char* filename, const char* extraDirToCheck)
{
    if (!filename || filename[0] == '\0')
        return "emu.openscript: no filename given";

    std::string path = filename;
    if (path[0] != '/' && extraDirToCheck && extraDirToCheck[0])
    {
        // GetCurrentScriptDir() only appends a separator when the script path
        // contained one; do not glue a bare filename onto another filename.
        const size_t n = strlen(extraDirToCheck);
        const char last = extraDirToCheck[n - 1];
        if (last == '/' || last == '\\')
        {
            std::string candidate = std::string(extraDirToCheck) + path;
            if (g_file_test(candidate.c_str(), G_FILE_TEST_IS_REGULAR))
                path = candidate;
        }
    }

    if (!g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR))
    {
        snprintf(sLuaOpenScriptError, sizeof(sLuaOpenScriptError),
                 "emu.openscript: file not found: %s", path.c_str());
        return sLuaOpenScriptError;
    }

    lua_script_open_console_with_file(GTK_WINDOW(pWindow), path.c_str());
    return NULL;
}
#endif

static inline void UpdateStatusBar (const char *message)
{
    gint pStatusBar_Ctx;

    pStatusBar_Ctx = gtk_statusbar_get_context_id(GTK_STATUSBAR(pStatusBar), "Global");
    gtk_statusbar_pop(GTK_STATUSBAR(pStatusBar), pStatusBar_Ctx);
    gtk_statusbar_push(GTK_STATUSBAR(pStatusBar), pStatusBar_Ctx, message);
}

static void About(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    static const gchar *authors[] = {
    	"yopyop (original author)",
    	"DeSmuME team",
    	NULL
    };

    gtk_show_about_dialog(GTK_WINDOW(pWindow),
            "program-name", "DeSmuME",
            "version", EMU_DESMUME_VERSION_STRING() + 1, // skip space
            "website", "http://desmume.org",
            "logo-icon-name", "org.desmume.DeSmuME",
            "comments", "Nintendo DS emulator based on work by Yopyop",
            "authors", authors,
            NULL);
}

static void ToggleMenuVisible(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    config.view_menu = value;
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(pWindow), config.view_menu);
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

static void ToggleToolbarVisible(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    config.view_toolbar = value;
    if (config.view_toolbar)
      gtk_widget_show(pToolBar);
    else
      gtk_widget_hide(pToolBar);
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

static void ToggleStatusbarVisible(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    config.view_statusbar = value;
    if (config.view_statusbar)
      gtk_widget_show(pStatusBar);
    else
      gtk_widget_hide(pStatusBar);
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

static void ToggleFullscreen(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GVariant *variant = g_action_get_state(G_ACTION(action));
  gboolean fullscreen = !g_variant_get_boolean(variant);
  config.window_fullscreen = fullscreen;
  printf("ToggleFullscreen -> %d\n", fullscreen);
  g_simple_action_set_state(action, g_variant_new_boolean(fullscreen));
  if (config.window_fullscreen)
  {
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(pWindow), FALSE);
    gtk_widget_hide(pToolBar);
    gtk_widget_hide(pStatusBar);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "view-menu")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "view-toolbar")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "view-statusbar")), FALSE);
    gtk_window_fullscreen(GTK_WINDOW(pWindow));
  }
  else
  {
    if (config.view_menu) {
      gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(pWindow), TRUE);
    }
    if (config.view_toolbar) {
      gtk_widget_show(pToolBar);
    }
    if (config.view_statusbar) {
      gtk_widget_show(pStatusBar);
    }
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "view-menu")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "view-toolbar")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "view-statusbar")), TRUE);
    gtk_window_unfullscreen(GTK_WINDOW(pWindow));
  }
}


static int Open(const char *filename)
{
    int res;
    ResetSaveStateTimes();
    res = NDS_LoadROM( filename );
    if(res > 0) {
        // Remember the ROM name so tools can suggest "<ROM>.wch" and friends.
        gchar *baseName = g_path_get_basename(filename);
        gchar *extension = strrchr(baseName, '.');
        if (extension != NULL && extension != baseName) {
            *extension = '\0';
        }
        sCurrentROMBaseName.assign(baseName);
        g_free(baseName);

        LoadSaveStateInfo();
        g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "cheatlist")), TRUE);
        g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "cheatsearch")), TRUE);
        g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "frame-advance")), TRUE);
        g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "rom-info")), TRUE);
    }
    return res;
}

const char *Desmume_CurrentROMBaseName(void)
{
    return sCurrentROMBaseName.c_str();
}

void Launch(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkWidget *pause;
    desmume_resume();

    if(!regMainLoop) {
        regMainLoop = g_idle_add_full(EMULOOP_PRIO, &EmuLoop, pWindow, NULL);
    }

    UpdateStatusBar("Running ...");

    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "pause")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "run")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "reset")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "printscreen")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "exportbackup")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "importbackup")), TRUE);

    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "savestate")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "loadstate")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "save-state-to")), TRUE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "load-state-from")), TRUE);

    //pause = gtk_bin_get_child(GTK_BIN(gtk_ui_manager_get_widget(ui_manager, "/ToolBar/pause")));
    //gtk_widget_grab_focus(pause);
}


void Pause(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkWidget *run;
    desmume_pause();
    UpdateStatusBar("Paused");

    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "pause")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "run")), TRUE);

    //run = gtk_bin_get_child(GTK_BIN(gtk_ui_manager_get_widget(ui_manager, "/ToolBar/run")));
    //gtk_widget_grab_focus(run);
}

static void FrameAdvance(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    if (gameInfo.fROM == NULL)
        return;

    // Same contract as the Windows port: the first press halts the emulator
    // where it is, further presses step exactly one frame.
    if (desmume_running()) {
        Pause(NULL, NULL, NULL);
        gtk_window_set_title(GTK_WINDOW(pWindow), "DeSmuME - Paused");
        return;
    }

    // Take the idle source out of the loop first, so the frame stepped here is
    // the only one that runs; Launch() puts it back when emulation resumes.
    if (regMainLoop) {
        g_source_remove(regMainLoop);
        regMainLoop = 0;
    }

    // Frame advance never skips: the whole point is to see this frame.
    NDS_OmitFrameSkip();

    // NDS_exec() bails out as soon as `execute` is false, so the stepped frame
    // has to run as if the emulator were running.  This is the same trick the
    // Windows port uses; everything here is on the GTK thread, so nothing races.
    execute = true;
    EmuSingleFrame();
    execute = false;

    // The loop normally fixes the title when it notices the pause; a step can
    // happen before that (or with the source already removed), so set it here.
    gtk_window_set_title(GTK_WINDOW(pWindow), "DeSmuME - Paused");

    sResyncTiming = true;
}

// Seeds a file chooser with a folder from the Path Settings dialog, when one
// is configured.  Leaving the entry empty keeps GTK's own last-folder
// behaviour, which is what these dialogs did before the settings existed.
static void SetChooserFolder(GtkFileChooserNative *chooser, PathInfo::KnownPath category)
{
    const std::string dir = path_settings_directory(category);

    if (!dir.empty() && g_file_test(dir.c_str(), G_FILE_TEST_IS_DIR))
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), dir.c_str());
}

static void LoadStateDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_ds, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    pFilter_ds = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_ds, "*.ds*");
    gtk_file_filter_set_name(pFilter_ds, "DeSmuME binary (.ds*)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_native_new("Load State From ...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Open", "_Cancel");
    SetChooserFolder(pFileSelection, path.STATES);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_ds);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        if(savestate_load(sPath) == false ) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to load :\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        } else {
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "run")), TRUE);
            RedrawScreen();
        }

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
}

// Stop Replay is only meaningful while a movie is being played or recorded,
// matching the Cocoa dialog's -isMovieActive check.
static void update_movie_actions()
{
    GSimpleAction *stop = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "stopmovie"));
    if (stop != NULL)
        g_simple_action_set_enabled(stop,
            (movieMode == MOVIEMODE_PLAY || movieMode == MOVIEMODE_RECORD) ? TRUE : FALSE);
}

static void RecordMovieDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    movie_record_dialog_show(GTK_WINDOW(pWindow));
    update_movie_actions();
}

static void StopMovie(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	FCEUI_StopMovie();
    update_movie_actions();
}

static void PlayMovieDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    movie_play_dialog_show(GTK_WINDOW(pWindow));
    update_movie_actions();
}

static void ImportBackupMemoryDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_raw, *pFilter_ar, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    pFilter_raw = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_raw, "*.sav");
    gtk_file_filter_set_name(pFilter_raw, "Raw/No$GBA Save format (*.sav)");

    pFilter_ar = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_ar, "*.duc");
    gtk_file_filter_add_pattern(pFilter_ar, "*.dss");
    gtk_file_filter_set_name(pFilter_ar, "Action Replay DS Save (*.duc,*.dss)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*.sav");
    gtk_file_filter_add_pattern(pFilter_any, "*.duc");
    gtk_file_filter_add_pattern(pFilter_any, "*.dss");
    gtk_file_filter_set_name(pFilter_any, "All supported types");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_native_new("Import Backup Memory From ...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Open", "_Cancel");
    SetChooserFolder(pFileSelection, path.SRAM_IMPORT_EXPORT);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_raw);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_ar);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        if(MMU_new.backupDevice.importData(sPath) == false ) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to import:\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
    Launch(NULL, NULL, NULL);
}

static void ExportBackupMemoryDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_raw, *pFilter_ar, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    pFilter_raw = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_raw, "*.sav");
    gtk_file_filter_set_name(pFilter_raw, "Raw/No$GBA Save format (*.sav)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*.sav");
    gtk_file_filter_set_name(pFilter_any, "All supported types");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_native_new("Export Backup Memory To ...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Save", "_Cancel");
    SetChooserFolder(pFileSelection, path.SRAM_IMPORT_EXPORT);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_raw);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        if(! g_str_has_suffix(sPath, ".sav"))
        {
            sPath = g_strjoin(NULL, sPath, ".sav", NULL);
        }

        if(MMU_new.backupDevice.exportData(sPath) == false ) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to export:\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
    Launch(NULL, NULL, NULL);
}

static void SaveStateDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_ds, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    pFilter_ds = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_ds, "*.ds*");
    gtk_file_filter_set_name(pFilter_ds, "DeSmuME binary (.ds*)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_native_new("Save State To ...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Save", "_Cancel");
    SetChooserFolder(pFileSelection, path.STATES);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(pFileSelection), "save.ds");
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_ds);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        if(savestate_save(sPath) == false ) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to save :\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        } else {
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "run")), TRUE);
        }

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
}

static void RecordAV_x264(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_mkv, *pFilter_mp4, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    pFilter_mkv = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_mkv, "*.mkv");
    gtk_file_filter_set_name(pFilter_mkv, "Matroska (.mkv)");

    pFilter_mp4 = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_mp4, "*.mp4");
    gtk_file_filter_set_name(pFilter_mp4, "MP4 (.mp4)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_native_new("Save video...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Save", "_Cancel");
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);
    SetChooserFolder(pFileSelection, path.AVI_FILES);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_mkv);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_mp4);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        if(avout_x264.begin(sPath)) {
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "record-x264")), FALSE);
        } else {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to record video to:\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
}

static void RecordAV_flac(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_flac, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    pFilter_flac = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_flac, "*.flac");
    gtk_file_filter_set_name(pFilter_flac, "FLAC file (.flac)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_native_new("Save audio...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Save", "_Cancel");
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (pFileSelection), TRUE);
    SetChooserFolder(pFileSelection, path.AVI_FILES);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_flac);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        if(avout_flac.begin(sPath)) {
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "record-flac")), FALSE);
        } else {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to record audio to:\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
}

#ifndef HAVE_FFMPEG_RECORD
static void RecordAV_VideoUnavailable(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            "This build was configured without FFmpeg recording support.\n"
            "The old recorders are still available under Old recorders.");
    gtk_dialog_run(GTK_DIALOG(pDialog));
    gtk_widget_destroy(pDialog);
}
#endif

static void RecordAV_SetStartActionsEnabled(bool enabled)
{
    const char *names[] = {
        "record-x264",
        "record-flac",
#ifdef HAVE_FFMPEG_RECORD
        "record-video",
#endif
    };

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        GAction *action = g_action_map_lookup_action(G_ACTION_MAP(pApp), names[i]);
        if (action != NULL)
            g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled ? TRUE : FALSE);
    }
}

#ifdef HAVE_FFMPEG_RECORD
struct RecordAV_DialogState
{
    GtkWidget *videoCombo;
    GtkWidget *audioCombo;
    GtkWidget *containerCombo;
    GtkWidget *videoBitrate;
    GtkWidget *audioBitrate;
    GtkWidget *filteredCheck;
    GtkWidget *layoutCombo;
    GtkWidget *swapCheck;
    GtkWidget *gapSpin;
    GtkWidget *rotationCombo;
    GtkWidget *sizeLabel;
    GtkWidget *okButton;

    std::vector<FFmpegEncoderInfo> videoEncoders;
    std::vector<FFmpegEncoderInfo> audioEncoders;

    bool updating;
};

static int RecordAV_FindEncoderIndex(const std::vector<FFmpegEncoderInfo>& encoders, const std::string& name)
{
    for (size_t i = 0; i < encoders.size(); i++) {
        if (encoders[i].name == name)
            return (int)i;
    }
    return -1;
}

static int RecordAV_DefaultVideoRow(const std::vector<FFmpegEncoderInfo>& encoders, const std::string& preferred)
{
    // An empty saved value means the user explicitly chose "None (no video)".
    if (preferred.empty())
        return 0;

    int index = RecordAV_FindEncoderIndex(encoders, preferred);
    if (index < 0)
        index = RecordAV_FindEncoderIndex(encoders, "libx264");

    if (index < 0) {
        for (size_t i = 0; i < encoders.size(); i++) {
            if (!encoders[i].hardware) {
                index = (int)i;
                break;
            }
        }
    }

    // Row 0 is "None (no video)"; encoder rows start at 1.
    return (index >= 0) ? index + 1 : 0;
}

static int RecordAV_DefaultAudioRow(const std::vector<FFmpegEncoderInfo>& encoders, const std::string& preferred)
{
    // An empty saved value means the user explicitly chose "None".
    if (preferred.empty())
        return 0;

    int index = RecordAV_FindEncoderIndex(encoders, preferred);
    if (index < 0)
        index = RecordAV_FindEncoderIndex(encoders, "aac");

    return (index >= 0) ? index + 1 : 0;
}

static std::string RecordAV_ContainerName(const RecordAV_DialogState *state)
{
    return (gtk_combo_box_get_active(GTK_COMBO_BOX(state->containerCombo)) == 1) ? "mp4" : "matroska";
}

static std::string RecordAV_SelectedVideoName(const RecordAV_DialogState *state)
{
    const int row = gtk_combo_box_get_active(GTK_COMBO_BOX(state->videoCombo));
    if (row > 0 && row - 1 < (int)state->videoEncoders.size())
        return state->videoEncoders[row - 1].name;
    return std::string();
}

static bool RecordAV_VideoSelected(const RecordAV_DialogState *state)
{
    return gtk_combo_box_get_active(GTK_COMBO_BOX(state->videoCombo)) > 0;
}

static std::string RecordAV_SelectedAudioName(const RecordAV_DialogState *state)
{
    const int row = gtk_combo_box_get_active(GTK_COMBO_BOX(state->audioCombo));
    if (row > 0 && row - 1 < (int)state->audioEncoders.size())
        return state->audioEncoders[row - 1].name;
    return std::string();
}

static void RecordAV_UpdateCodecControls(const RecordAV_DialogState *state)
{
    const bool hasVideo = RecordAV_VideoSelected(state);
    const bool hasAudio = gtk_combo_box_get_active(GTK_COMBO_BOX(state->audioCombo)) > 0;
    const int layout = gtk_combo_box_get_active(GTK_COMBO_BOX(state->layoutCombo));

    gtk_widget_set_sensitive(state->videoBitrate, hasVideo);
    gtk_widget_set_sensitive(state->audioBitrate, hasAudio);
    gtk_widget_set_sensitive(state->filteredCheck, hasVideo);
    gtk_widget_set_sensitive(state->layoutCombo, hasVideo);
    gtk_widget_set_sensitive(state->swapCheck, hasVideo);
    gtk_widget_set_sensitive(state->rotationCombo, hasVideo);
    gtk_widget_set_sensitive(state->gapSpin, hasVideo && layout != FFmpegScreenLayout_Single);
    if (state->okButton != NULL)
        gtk_widget_set_sensitive(state->okButton, hasVideo || hasAudio);
}

static void RecordAV_SourceSize(bool filtered, int *width, int *height)
{
    if (filtered && video != NULL) {
        const int dstWidth = (int)video->GetDstWidth();
        const int dstHeight = (int)video->GetDstHeight();
        if (dstWidth > 0 && dstHeight > 0) {
            *width = dstWidth;
            *height = dstHeight;
            return;
        }
    }

    int sourceWidth = 0;
    int sourceHeight = 0;
    if (GPU != NULL) {
        const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
        sourceWidth = (int)dispInfo.customWidth;
        sourceHeight = (int)(dispInfo.customHeight * 2);
    }

    if (sourceWidth <= 0)
        sourceWidth = AVOUT_FFMPEG_WIDTH;
    if (sourceHeight <= 0)
        sourceHeight = AVOUT_FFMPEG_HEIGHT;

    *width = sourceWidth;
    *height = sourceHeight;
}

static int RecordAV_RotationFromCombo(GtkComboBox *combo)
{
    static const int rotations[] = { 0, 90, 180, 270 };
    const int index = gtk_combo_box_get_active(combo);
    return (index >= 0 && index < (int)(sizeof(rotations) / sizeof(rotations[0]))) ? rotations[index] : 0;
}

static void RecordAV_UpdateOutputSize(const RecordAV_DialogState *state)
{
    if (state->updating)
        return;

    if (!RecordAV_VideoSelected(state)) {
        const bool hasAudio = gtk_combo_box_get_active(GTK_COMBO_BOX(state->audioCombo)) > 0;
        gtk_label_set_text(GTK_LABEL(state->sizeLabel),
                hasAudio ? "Output: audio only" : "Output: nothing selected");
        return;
    }

    const int layout = gtk_combo_box_get_active(GTK_COMBO_BOX(state->layoutCombo));

    bool filtered = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state->filteredCheck)) ? true : false;
    int sourceWidth = 0;
    int sourceHeight = 0;
    RecordAV_SourceSize(filtered, &sourceWidth, &sourceHeight);

    int outputWidth = 0;
    int outputHeight = 0;
    FFmpegRecorder::composedSize(sourceWidth, sourceHeight,
            layout,
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state->swapCheck)) ? true : false,
            gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(state->gapSpin)),
            RecordAV_RotationFromCombo(GTK_COMBO_BOX(state->rotationCombo)),
            &outputWidth, &outputHeight);

    char text[128];
    snprintf(text, sizeof(text), "Output: %d x %d", outputWidth, outputHeight);
    gtk_label_set_text(GTK_LABEL(state->sizeLabel), text);
}

static void RecordAV_FillEncoderCombos(RecordAV_DialogState *state,
                                       const std::string& preferredVideo,
                                       const std::string& preferredAudio)
{
    state->updating = true;

    const std::string container = RecordAV_ContainerName(state);
    state->videoEncoders = FFmpegRecorder::videoEncoders(container);
    state->audioEncoders = FFmpegRecorder::audioEncoders(container);

    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(state->videoCombo));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->videoCombo), "None (no video)");
    for (size_t i = 0; i < state->videoEncoders.size(); i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->videoCombo),
                FFmpegRecorder::encoderLabel(state->videoEncoders[i]).c_str());
    gtk_combo_box_set_active(GTK_COMBO_BOX(state->videoCombo),
            RecordAV_DefaultVideoRow(state->videoEncoders, preferredVideo));

    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(state->audioCombo));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->audioCombo), "None (no audio)");
    for (size_t i = 0; i < state->audioEncoders.size(); i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->audioCombo),
                FFmpegRecorder::encoderLabel(state->audioEncoders[i]).c_str());
    gtk_combo_box_set_active(GTK_COMBO_BOX(state->audioCombo),
            RecordAV_DefaultAudioRow(state->audioEncoders, preferredAudio));

    state->updating = false;

    RecordAV_UpdateCodecControls(state);
    RecordAV_UpdateOutputSize(state);
}

static void RecordAV_AudioEncoderChanged(GtkComboBox *combo, gpointer user_data)
{
    RecordAV_DialogState *state = (RecordAV_DialogState *)user_data;
    if (!state->updating) {
        RecordAV_UpdateCodecControls(state);
        RecordAV_UpdateOutputSize(state);
    }
}

static void RecordAV_VideoEncoderChanged(GtkComboBox *combo, gpointer user_data)
{
    RecordAV_DialogState *state = (RecordAV_DialogState *)user_data;
    if (!state->updating) {
        RecordAV_UpdateCodecControls(state);
        RecordAV_UpdateOutputSize(state);
    }
}

static void RecordAV_ContainerChanged(GtkComboBox *combo, gpointer user_data)
{
    RecordAV_DialogState *state = (RecordAV_DialogState *)user_data;
    if (state->updating)
        return;

    // Keep the current selections when the new container can hold them.
    RecordAV_FillEncoderCombos(state, RecordAV_SelectedVideoName(state),
                               RecordAV_SelectedAudioName(state));
}

static void RecordAV_OutputOptionsChanged(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    const RecordAV_DialogState *state = (const RecordAV_DialogState *)user_data;
    RecordAV_UpdateCodecControls(state);
    RecordAV_UpdateOutputSize(state);
}

static void RecordAV_Video(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    (void)action;
    (void)parameter;
    (void)user_data;

    if (avout_x264.isRecording() || avout_flac.isRecording() || avout_ffmpeg.isRecording()) {
        GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_INFO,
                GTK_BUTTONS_OK,
                "A recording is already in progress.\n"
                "Stop it before starting a new one.");
        gtk_dialog_run(GTK_DIALOG(pDialog));
        gtk_widget_destroy(pDialog);
        return;
    }

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Record video (FFmpeg)",
            GTK_WINDOW(pWindow),
            GTK_DIALOG_MODAL,
            "_Continue", GTK_RESPONSE_OK,
            "_Cancel", GTK_RESPONSE_CANCEL,
            NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_container_add(GTK_CONTAINER(content), grid);

    RecordAV_DialogState state;
    state.updating = false;
    state.okButton = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    state.containerCombo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state.containerCombo), "Matroska (.mkv)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state.containerCombo), "MP4 (.mp4)");
    const bool useMp4 = (config.record_container.get() == "mp4");
    gtk_combo_box_set_active(GTK_COMBO_BOX(state.containerCombo), useMp4 ? 1 : 0);

    state.videoCombo = gtk_combo_box_text_new();
    state.audioCombo = gtk_combo_box_text_new();

    state.videoBitrate = gtk_spin_button_new_with_range(100, 100000, 100);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(state.videoBitrate), (gdouble)config.record_video_bitrate);

    state.audioBitrate = gtk_spin_button_new_with_range(32, 512, 8);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(state.audioBitrate), (gdouble)config.record_audio_bitrate);

    state.filteredCheck = gtk_check_button_new_with_label(
            "Include video filter, Lua and OSD");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(state.filteredCheck),
                                 config.record_filtered ? TRUE : FALSE);
    gtk_widget_set_tooltip_text(state.filteredCheck,
            "Records the frame after the video filter and HUD/Lua overlays. "
            "The output size follows the filter scale (for example 2x = 512x768).");

    state.layoutCombo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state.layoutCombo), "Vertical (top / bottom)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state.layoutCombo), "Horizontal (left / right)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state.layoutCombo), "Single screen");
    int layout = config.record_layout;
    if (layout < FFmpegScreenLayout_Vertical || layout > FFmpegScreenLayout_Single)
        layout = FFmpegScreenLayout_Vertical;
    gtk_combo_box_set_active(GTK_COMBO_BOX(state.layoutCombo), layout);
    gtk_widget_set_tooltip_text(state.layoutCombo,
            "How the two DS screens are arranged in the recorded video. "
            "The window layout is not changed.");

    state.swapCheck = gtk_check_button_new_with_label("Swap the two screens");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(state.swapCheck),
                                 config.record_swap ? TRUE : FALSE);
    gtk_widget_set_tooltip_text(state.swapCheck,
            "Exchanges the two screens in the vertical and horizontal layouts. "
            "With a single screen, shows the touch screen instead of the main screen.");

    int savedGap = config.record_gap;
    if (savedGap < 0)
        savedGap = 0;
    if (savedGap > 256)
        savedGap = 256;
    savedGap &= ~1;
    state.gapSpin = gtk_spin_button_new_with_range(0, 256, 2);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(state.gapSpin), (gdouble)savedGap);

    state.rotationCombo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state.rotationCombo), "0 degrees");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state.rotationCombo), "90 degrees");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state.rotationCombo), "180 degrees");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state.rotationCombo), "270 degrees");
    int rotationIndex = 0;
    switch (config.record_rotation) {
        case 90:  rotationIndex = 1; break;
        case 180: rotationIndex = 2; break;
        case 270: rotationIndex = 3; break;
        default:  rotationIndex = 0; break;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(state.rotationCombo), rotationIndex);

    state.sizeLabel = gtk_label_new("Output: 0 x 0");
    gtk_label_set_xalign(GTK_LABEL(state.sizeLabel), 0.0f);

    g_signal_connect(state.containerCombo, "changed", G_CALLBACK(RecordAV_ContainerChanged), &state);
    g_signal_connect(state.videoCombo, "changed", G_CALLBACK(RecordAV_VideoEncoderChanged), &state);
    g_signal_connect(state.audioCombo, "changed", G_CALLBACK(RecordAV_AudioEncoderChanged), &state);
    g_signal_connect(state.filteredCheck, "toggled", G_CALLBACK(RecordAV_OutputOptionsChanged), &state);
    g_signal_connect(state.layoutCombo, "changed", G_CALLBACK(RecordAV_OutputOptionsChanged), &state);
    g_signal_connect(state.swapCheck, "toggled", G_CALLBACK(RecordAV_OutputOptionsChanged), &state);
    g_signal_connect(state.gapSpin, "value-changed", G_CALLBACK(RecordAV_OutputOptionsChanged), &state);
    g_signal_connect(state.rotationCombo, "changed", G_CALLBACK(RecordAV_OutputOptionsChanged), &state);

    RecordAV_FillEncoderCombos(&state, config.record_video_encoder, config.record_audio_encoder);

    if (state.videoEncoders.empty() && state.audioEncoders.empty()) {
        GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "This FFmpeg build provides no encoders usable with the selected container.");
        gtk_dialog_run(GTK_DIALOG(pDialog));
        gtk_widget_destroy(pDialog);
        gtk_widget_destroy(dialog);
        return;
    }

    GtkWidget *hint = gtk_label_new("Captures the shown frame at 59.83 fps.\n"
                                    "Choose \"None (no video)\" for an audio-only file; at least one of video or audio is required.\n"
                                    "Only encoders this FFmpeg build can use with the selected container are listed.\n"
                                    "Hardware encoders may still fail if the driver or device is unavailable.");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(hint), TRUE);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Video encoder:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), state.videoCombo, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Audio encoder:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), state.audioCombo, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Container:"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), state.containerCombo, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Video bitrate (kbps):"), 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), state.videoBitrate, 1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Audio bitrate (kbps):"), 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), state.audioBitrate, 1, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Screen layout:"), 0, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), state.layoutCombo, 1, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), state.swapCheck, 1, 6, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Screen gap (px):"), 0, 7, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), state.gapSpin, 1, 7, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Rotation:"), 0, 8, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), state.rotationCombo, 1, 8, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), state.filteredCheck, 0, 9, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), state.sizeLabel, 0, 10, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), hint, 0, 11, 2, 1);

    gtk_widget_show_all(dialog);
    const int response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        return;
    }

    const std::string videoEncoder = RecordAV_SelectedVideoName(&state);
    const std::string audioEncoder = RecordAV_SelectedAudioName(&state);
    const std::string container = RecordAV_ContainerName(&state);
    const int selectedLayout = gtk_combo_box_get_active(GTK_COMBO_BOX(state.layoutCombo));
    const bool swapScreens = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state.swapCheck)) ? true : false;
    const int screenGap = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(state.gapSpin));
    const int rotation = RecordAV_RotationFromCombo(GTK_COMBO_BOX(state.rotationCombo));
    const int videoKbps = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(state.videoBitrate));
    const int audioKbps = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(state.audioBitrate));
    const bool filteredOutput = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state.filteredCheck)) ? true : false;

    int sourceWidth = 0;
    int sourceHeight = 0;
    RecordAV_SourceSize(filteredOutput, &sourceWidth, &sourceHeight);

    int outputWidth = 0;
    int outputHeight = 0;
    FFmpegRecorder::composedSize(sourceWidth, sourceHeight, selectedLayout, swapScreens,
                                 screenGap, rotation, &outputWidth, &outputHeight);

    gtk_widget_destroy(dialog);

    if (videoEncoder.empty() && audioEncoder.empty()) {
        GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "Select at least a video encoder or an audio encoder.");
        gtk_dialog_run(GTK_DIALOG(pDialog));
        gtk_widget_destroy(pDialog);
        return;
    }

    const std::string extension = (container == "mp4") ? ".mp4" : ".mkv";

    GtkFileFilter *pFilter_video = gtk_file_filter_new();
    std::string pattern = "*" + extension;
    gtk_file_filter_add_pattern(pFilter_video, pattern.c_str());
    gtk_file_filter_set_name(pFilter_video, (container == "mp4") ? "MP4 (.mp4)" : "Matroska (.mkv)");

    GtkFileFilter *pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    GtkFileChooserNative *pFileSelection = gtk_file_chooser_native_new("Save video...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Save", "_Cancel");
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(pFileSelection), TRUE);
    SetChooserFolder(pFileSelection, path.AVI_FILES);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_video);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection)) == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = (file != NULL) ? g_file_get_path(file) : NULL;

        if (sPath != NULL) {
            std::string path = sPath;

            if (path.size() < extension.size() ||
                path.compare(path.size() - extension.size(), extension.size(), extension) != 0) {
                path += extension;
            }

            FFmpegRecordConfig cfg;
            cfg.videoEncoder = videoEncoder;
            cfg.audioEncoder = audioEncoder;
            cfg.container = container;
            cfg.videoBitrateKbps = videoKbps;
            cfg.audioBitrateKbps = audioKbps;
            cfg.width = outputWidth;
            cfg.height = outputHeight;
            cfg.layout = selectedLayout;
            cfg.swapScreens = swapScreens;
            cfg.screenGap = screenGap;
            cfg.rotation = rotation;

            config.record_video_encoder = videoEncoder;
            config.record_audio_encoder = audioEncoder;
            config.record_container = container;
            config.record_video_bitrate = videoKbps;
            config.record_audio_bitrate = audioKbps;
            config.record_filtered = filteredOutput;
            config.record_layout = selectedLayout;
            config.record_swap = swapScreens;
            config.record_gap = screenGap;
            config.record_rotation = rotation;

            if (avout_ffmpeg.begin(path.c_str(), cfg)) {
                sRecordFilteredOutput = filteredOutput;
                // Do not turn the frame that was already pending when the
                // dialog opened into the first video frame.
                sLastRecordedFrameSerial = sFrameSerial;
                sRecordFrameBuffer.clear();
                RecordAV_SetStartActionsEnabled(false);
                std::string status = "Recording to " + path;
                UpdateStatusBar(status.c_str());
            } else {
                std::string error = avout_ffmpeg.lastError();
                GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                        GTK_DIALOG_MODAL,
                        GTK_MESSAGE_ERROR,
                        GTK_BUTTONS_OK,
                        "Unable to record video to:\n%s\n\n%s", path.c_str(), error.c_str());
                gtk_dialog_run(GTK_DIALOG(pDialog));
                gtk_widget_destroy(pDialog);
            }
        }

        g_free(sPath);
        if (file != NULL)
            g_object_unref(file);
    }
    g_object_unref(pFileSelection);
}

static gboolean RecordAV_Watchdog(gpointer data)
{
    (void)data;

    if (avout_ffmpeg.hasFailed()) {
        std::string error = avout_ffmpeg.lastError();
        avout_ffmpeg.end();
        sRecordFilteredOutput = false;
        sRecordFrameBuffer.clear();
        // The error has been consumed; without this the watchdog would report
        // it again on every tick and never restore the menu.
        avout_ffmpeg.clearFailure();
        RecordAV_SetStartActionsEnabled(true);
        std::string status = error.empty() ? "Recording failed" : ("Recording failed: " + error);
        UpdateStatusBar(status.c_str());
    } else if (!avout_ffmpeg.isRecording() &&
               !avout_x264.isRecording() &&
               !avout_flac.isRecording()) {
        // A failed begin() or an explicit stop must not leave the menu stuck.
        RecordAV_SetStartActionsEnabled(true);
    }

    return TRUE;
}
#endif

static void RecordAV_stop(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    std::string status;
#ifdef HAVE_FFMPEG_RECORD
    if (avout_ffmpeg.isRecording())
        status = "Recording stopped";
#endif
	avout_x264.end();
	avout_flac.end();
#ifdef HAVE_FFMPEG_RECORD
    avout_ffmpeg.end();
    sRecordFilteredOutput = false;
    sRecordFrameBuffer.clear();
    // A runtime failure or a trailer error (e.g. a codec that the MP4 muxer
    // accepts at header time but cannot finalize) is only visible here.
    if (avout_ffmpeg.hasFailed()) {
        const std::string error = avout_ffmpeg.lastError();
        avout_ffmpeg.clearFailure();
        status = "Recording failed";
        GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "The recording could not be finalized:\n%s\n\n"
                "The file may be incomplete or unplayable.",
                error.empty() ? "unknown error" : error.c_str());
        gtk_dialog_run(GTK_DIALOG(pDialog));
        gtk_widget_destroy(pDialog);
    }
#endif
    RecordAV_SetStartActionsEnabled(true);
    if (!status.empty())
        UpdateStatusBar(status.c_str());
}

static void OpenNdsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_nds, *pFilter_dsgba, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    pFilter_nds = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_nds, "*.[nN][dD][sS]");
#ifdef HAVE_LIBZ
    gtk_file_filter_add_pattern(pFilter_nds, "*.[nN][dD][sS].gz");
#endif
    gtk_file_filter_add_pattern(pFilter_nds, "*.[nN][dD][sS].zip");
    gtk_file_filter_set_name(pFilter_nds, "Nds binary (.nds)");

    pFilter_dsgba = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_dsgba, "*.ds.gba");
    gtk_file_filter_set_name(pFilter_dsgba, "Nds binary with loader (.ds.gba)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    /* Creating the selection window */
    pFileSelection = gtk_file_chooser_native_new("Open...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Open", "_Cancel");

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_nds);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_dsgba);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    const std::string romDirectory = path_settings_directory(path.ROMS);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(pFileSelection),
            romDirectory.empty() ? g_get_home_dir() : romDirectory.c_str());

    /* Showing the window */
    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        if(Open((const char*)sPath) < 0) {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Unable to load :\n%s", sPath);
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        } else {
            // Mirrors the Windows frontend: when "use last visited ROM path"
            // is on, the folder of the ROM just opened becomes the Roms
            // category, so the next Open dialog starts there.
            if (config.path_save_last_visit) {
                gchar *dir = g_path_get_dirname(sPath);
                config.path_roms.set(dir);
                path.setpath(path.ROMS, dir);
                config.save();
                g_free(dir);
            }

            GtkRecentData recentData;
            gchar *uri;
            memset(&recentData, 0, sizeof(GtkRecentData));
            recentData.mime_type = g_strdup("application/x-nintendo-ds-rom");
            recentData.app_name = (gchar *) g_get_application_name ();
            recentData.app_exec = g_strdup ("desmume %f");

            GtkRecentManager *manager;
            manager = gtk_recent_manager_get_default ();
            uri = g_filename_to_uri (sPath, NULL, NULL);
            gtk_recent_manager_add_full (manager, uri, &recentData);

            g_free(uri);
            g_free(recentData.app_exec);
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "run")), TRUE);
            Launch(NULL, NULL, NULL);
        }

        g_free(sPath);
    }
    g_object_unref(pFileSelection);
}

static void OpenRecent(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkRecentManager *recent_manager = gtk_recent_manager_get_default();
    gchar *romname;
    int ret;

    if (desmume_running())
        Pause(NULL, NULL, NULL);

    const char *uri = g_variant_get_string(parameter, NULL);
    romname = g_filename_from_uri(uri, NULL, NULL);
    ret = Open(romname);
    if (ret > 0) {
        g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "run")), TRUE);
        Launch(NULL, NULL, NULL);
    } else {
        gtk_recent_manager_remove_item(recent_manager, uri, NULL);
        GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "Unable to load :\n%s", uri);
        gtk_dialog_run(GTK_DIALOG(pDialog));
        gtk_widget_destroy(pDialog);
    }

    g_free(romname);
}

static void Reset(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    bool shouldBeRunning = desmume_running();
    Pause(NULL, NULL, NULL);
    NDS_Reset();
    RedrawScreen();
    if (shouldBeRunning) {
        Launch(NULL, NULL, NULL);
    }
}


/////////////////////////////// DRAWING SCREEN //////////////////////////////////

struct DrawParams {
	bool valid;
	
	//Drawing area size
	gint daW;
	gint daH;
	
	//Image size
	gint imgW;
	gint imgH;
	
	//Buffer size
	gint dstW;
	gint dstH;
	gint dstScale;
	
	gint gap;
	gint gapOffset;
	
	gfloat ratio;
	
	gdouble hybridBigScreenScale;
};

static DrawParams draw_params = {false, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1.0, 0.0};

static void UpdateDrawingAreaAspect()
{
    gint H, W;

    if (nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180) {
        W = screen_size[nds_screen.orientation].width;
        H = screen_size[nds_screen.orientation].height;
    } else {
        W = screen_size[nds_screen.orientation].height;
        H = screen_size[nds_screen.orientation].width;
    }

    if (nds_screen.orientation != ORIENT_SINGLE) {
        if ((nds_screen.orientation == ORIENT_VERTICAL) || (nds_screen.orientation == ORIENT_HYBRID_EQUAL) ||
			(nds_screen.orientation == ORIENT_HYBRID_VERTICAL)) {
            if ((nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180)) {
                if((nds_screen.orientation == ORIENT_HYBRID_EQUAL) || (nds_screen.orientation == ORIENT_HYBRID_VERTICAL))
					W = (W * 2.0/3.0) * (double)(H + nds_screen.gap_size) / (double)H + W / 3.0;
				H += nds_screen.gap_size;
            } else {
                if((nds_screen.orientation == ORIENT_HYBRID_EQUAL) || (nds_screen.orientation == ORIENT_HYBRID_VERTICAL))
					H = (H * 2.0/3.0) * (double)(W + nds_screen.gap_size) / (double)W + H / 3.0;
				W += nds_screen.gap_size;
            }
        }
    }

	if (winsize_current == WINSIZE_SCALE) {
		gtk_window_set_resizable(GTK_WINDOW(pWindow), TRUE);
		gtk_widget_set_size_request(GTK_WIDGET(pDrawingArea), W, H);
	} else {
		gtk_window_unmaximize(GTK_WINDOW(pWindow));
		gtk_window_set_resizable(GTK_WINDOW(pWindow), FALSE);
		gtk_widget_set_size_request(GTK_WIDGET(pDrawingArea), W * winsize_current / 2, H * winsize_current / 2);
	}
	
	draw_params.valid = false;
}

static void ToggleGap(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    config.view_gap = value;
    nds_screen.gap_size = config.view_gap ? GAP_SIZE : 0;
    UpdateDrawingAreaAspect();
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

static void SetRotation(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const char *string = g_variant_get_string(parameter, NULL);
    nds_screen.rotation_angle = g_ascii_strtoll(string, NULL, 10);
    config.view_rot = nds_screen.rotation_angle;
    UpdateDrawingAreaAspect();
    g_simple_action_set_state(action, parameter);
}

static void SetWinsize(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const char *string = g_variant_get_string(parameter, NULL);
    winsize_enum winsize;
    if (strcmp(string, "scale") == 0)
        winsize = WINSIZE_SCALE;
    else if (strcmp(string, "0.5") == 0)
        winsize = WINSIZE_HALF;
    else if (strcmp(string, "1") == 0)
        winsize = WINSIZE_1;
    else if (strcmp(string, "1.5") == 0)
        winsize = WINSIZE_1HALF;
    else if (strcmp(string, "2") == 0)
        winsize = WINSIZE_2;
    else if (strcmp(string, "2.5") == 0)
        winsize = WINSIZE_2HALF;
    else if (strcmp(string, "3") == 0)
        winsize = WINSIZE_3;
    else if (strcmp(string, "3.5") == 0)
        winsize = WINSIZE_3HALF;
    else if (strcmp(string, "4") == 0)
        winsize = WINSIZE_4;
    else if (strcmp(string, "4.5") == 0)
        winsize = WINSIZE_4HALF;
    else if (strcmp(string, "5") == 0)
        winsize = WINSIZE_5;
    winsize_current = winsize;
    config.window_scale = winsize_current;
    if (config.window_fullscreen) {
        g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "fullscreen")), FALSE);
    }
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(pApp), "fullscreen")), winsize_current == WINSIZE_SCALE);
    UpdateDrawingAreaAspect();
    g_simple_action_set_state(action, parameter);
}

static void SetOrientation(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const char *string = g_variant_get_string(parameter, NULL);
	orientation_enum orient=ORIENT_VERTICAL;
	if(strcmp(string, "vertical") == 0)
		orient = ORIENT_VERTICAL;
	else if(strcmp(string, "horizontal") == 0)
		orient = ORIENT_HORIZONTAL;
	else if(strcmp(string, "single") == 0)
		orient = ORIENT_SINGLE;
	else if(strcmp(string, "hybrid_equal") == 0)
		orient = ORIENT_HYBRID_EQUAL;
	else if(strcmp(string, "hybrid_vertical") == 0)
		orient = ORIENT_HYBRID_VERTICAL;
    nds_screen.orientation = orient;
#ifdef HAVE_LIBAGG
    osd->singleScreen = nds_screen.orientation == ORIENT_SINGLE;
	bool hybrid = (nds_screen.orientation == ORIENT_HYBRID_EQUAL) || (nds_screen.orientation == ORIENT_HYBRID_VERTICAL);
    osd->swapScreens = nds_screen.swap && !hybrid;
#endif
    config.view_orient = nds_screen.orientation;
    UpdateDrawingAreaAspect();
    g_simple_action_set_state(action, parameter);
}

static void ToggleSwapScreens(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    nds_screen.swap = value;
#ifdef HAVE_LIBAGG
	bool hybrid = (nds_screen.orientation == ORIENT_HYBRID_EQUAL) || (nds_screen.orientation == ORIENT_HYBRID_VERTICAL);
    osd->swapScreens = nds_screen.swap && !hybrid;
#endif
    config.view_swap = nds_screen.swap;
    RedrawScreen();
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

static int ConfigureDrawingArea(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
#if defined(ENABLE_WGPU)
	if (config.presentationWGPU && (event != NULL))
	{
		GdkWindow *window = gtk_widget_get_window(widget);
		const int scale = (window != NULL) ? MAX(1, gdk_window_get_scale_factor(window)) : 1;

		wgpu_display.setClientSize(event->width * scale, event->height * scale, scale);

		// A new size or a re-realized window is a good moment to retry a device
		// that was not available earlier.
		if (!wgpu_display.active)
		{
			wgpu_initialization_failed = false;
		}
	}
#endif

    return TRUE;
}

static inline void drawScreen(cairo_t* cr, u32* buf, gint w, gint h) {
	cairo_surface_t* surf = cairo_image_surface_create_for_data((u8*)buf, CAIRO_FORMAT_RGB24, w, h, w * sizeof(buf[0]));
	cairo_set_source_surface(cr, surf, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr), Interpolation);
	cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
	cairo_rectangle(cr, 0, 0, w, h);
	cairo_fill(cr);
	cairo_surface_destroy(surf);
}

static inline void drawTopScreen(cairo_t* cr, u32* buf, gint w, gint h, gint gap, gint rotation_angle, bool swap, orientation_enum orientation) {
	if (orientation == ORIENT_SINGLE && swap) {
		return;
	}
	cairo_save(cr);
	switch (orientation) {
	case ORIENT_VERTICAL:
		if (swap) {
			cairo_translate(cr, 0, h + gap);
		}
		break;
	case ORIENT_HORIZONTAL:
		if (swap) {
			cairo_translate(cr, w, 0);
		}
		break;
	case ORIENT_HYBRID_EQUAL:
	case ORIENT_HYBRID_VERTICAL:
		if (!swap && (draw_params.hybridBigScreenScale > 0.0)) {
			cairo_save(cr);
			cairo_scale(cr, draw_params.hybridBigScreenScale, draw_params.hybridBigScreenScale);
			cairo_get_matrix(cr, &nds_screen.topscreen_matrix_hybrid);
			drawScreen(cr, buf, w, h);
			cairo_restore(cr);
		}
		cairo_translate(cr, w * draw_params.hybridBigScreenScale, 0);
		break;
	}
	// Used for HUD editor mode
	cairo_get_matrix(cr, &nds_screen.topscreen_matrix);
	drawScreen(cr, buf, w, h);
	cairo_restore(cr);
}

static inline void drawBottomScreen(cairo_t* cr, u32* buf, gint w, gint h, gint gap, gint rotation_angle, bool swap, orientation_enum orientation) {
	if (orientation == ORIENT_SINGLE && !swap) {
		return;
	}
	cairo_save(cr);
	switch (orientation) {
	case ORIENT_VERTICAL:
		if (!swap) {
			cairo_translate(cr, 0, h + gap);
		}
		break;
	case ORIENT_HORIZONTAL:
		if (!swap) {
			cairo_translate(cr, w, 0);
		}
		break;
	case ORIENT_HYBRID_EQUAL:
	case ORIENT_HYBRID_VERTICAL:
		if (swap && (draw_params.hybridBigScreenScale > 0.0)) {
			cairo_save(cr);
			if(orientation == ORIENT_HYBRID_VERTICAL)
				cairo_translate(cr, 0, h * (2.0 - draw_params.hybridBigScreenScale) + gap);
			cairo_scale(cr, draw_params.hybridBigScreenScale, draw_params.hybridBigScreenScale);
			cairo_get_matrix(cr, &nds_screen.touch_matrix_hybrid);
			drawScreen(cr, buf, w, h);
			cairo_restore(cr);
		}
		cairo_translate(cr, w * draw_params.hybridBigScreenScale, h + gap);
		break;
	}
	// Store the matrix for converting touchscreen coordinates
	cairo_get_matrix(cr, &nds_screen.touch_matrix);
	drawScreen(cr, buf, w, h);
	cairo_restore(cr);
}

static void calc_hybrid_vertical_params(gint daW, gint daH, gint gap, gint dstScale, gint & imgW, gint & imgH)
{
	gdouble hyb_hratio1, hyb_hratio2, hyb_vratio1, hyb_vratio2, hyb_ratio1, hyb_ratio2;
	hyb_hratio1 = (gdouble)daW / (gdouble)(GPU_FRAMEBUFFER_NATIVE_WIDTH * dstScale / 2);
	hyb_vratio1 = (gdouble)daH / (gdouble)(GPU_FRAMEBUFFER_NATIVE_HEIGHT * dstScale + gap);
	hyb_ratio1 = MIN(hyb_hratio1, hyb_vratio1);
	gint w_rem = daW - GPU_FRAMEBUFFER_NATIVE_WIDTH * dstScale / 2.0 * hyb_ratio1;
	hyb_hratio2 = (gdouble)w_rem / (gdouble)(GPU_FRAMEBUFFER_NATIVE_WIDTH * dstScale / 2);
	hyb_vratio2 = (gdouble)daH / (gdouble)(GPU_FRAMEBUFFER_NATIVE_HEIGHT * dstScale / 2);
	hyb_ratio2 = MIN(hyb_hratio2, hyb_vratio2);
	imgW = GPU_FRAMEBUFFER_NATIVE_WIDTH * dstScale / 2 * (1.0 + hyb_ratio2 / hyb_ratio1);
	imgH = GPU_FRAMEBUFFER_NATIVE_HEIGHT * dstScale + gap;
	draw_params.hybridBigScreenScale = hyb_ratio2 / hyb_ratio1;
}

#if defined(ENABLE_WGPU)

// ---------------------------------------------------------------------------
// WebGPU presentation
//
// The composited frame is the same one the Cairo path draws: video's
// destination buffer, main screen on top, sub screen below. The destination
// rectangles are computed in the unrotated local image space and transformed
// with the same matrix the Cairo path uses, so embedding, integer scaling and
// rotation stay in sync with the window code above. WGPUPresent applies the
// rotation to the source UVs, exactly like the OpenGL and Windows paths.
// ---------------------------------------------------------------------------

struct WGPULinuxLocalRect
{
	bool visible;
	int x;
	int y;
	int w;
	int h;
};

static WGPUPresentRect WGPU_TransformRect(const cairo_matrix_t *matrix, const WGPULinuxLocalRect &rect)
{
	WGPUPresentRect out;

	if (!rect.visible || (rect.w <= 0) || (rect.h <= 0))
	{
		out.left = 0;
		out.top = 0;
		out.right = 0;
		out.bottom = 0;
		return out;
	}

	double x0 = rect.x;
	double y0 = rect.y;
	double x1 = rect.x + rect.w;
	double y1 = rect.y + rect.h;

	cairo_matrix_transform_point(matrix, &x0, &y0);
	cairo_matrix_transform_point(matrix, &x1, &y1);

	out.left   = (int)lround(MIN(x0, x1));
	out.top    = (int)lround(MIN(y0, y1));
	out.right  = (int)lround(MAX(x0, x1));
	out.bottom = (int)lround(MAX(y0, y1));

	return out;
}

// GTK3 reports GdkWindow dimensions and hands out Cairo coordinates in
// application (logical) pixels, while a Vulkan/X11 swap chain and the X11
// client-size query use device pixels. Scale the final destination rects so
// the two agree on HiDPI displays.
static WGPUPresentRect WGPU_ScaleRect(const WGPUPresentRect &rect, int scale)
{
	WGPUPresentRect out;

	out.left   = rect.left * scale;
	out.top    = rect.top * scale;
	out.right  = rect.right * scale;
	out.bottom = rect.bottom * scale;

	return out;
}

static void WGPU_GetLocalRects(int dstW, int dstH, int gap,
							   WGPULinuxLocalRect *mainRect,
							   WGPULinuxLocalRect *subRect,
							   WGPULinuxLocalRect *gapRect)
{
	const int screenW = dstW;
	const int screenH = dstH / 2;

	mainRect->visible = true;
	mainRect->x = 0;
	mainRect->y = 0;
	mainRect->w = screenW;
	mainRect->h = screenH;

	subRect->visible = true;
	subRect->x = 0;
	subRect->y = 0;
	subRect->w = screenW;
	subRect->h = screenH;

	gapRect->visible = false;
	gapRect->x = 0;
	gapRect->y = 0;
	gapRect->w = 0;
	gapRect->h = 0;

	switch (nds_screen.orientation)
	{
		case ORIENT_VERTICAL:
			if (nds_screen.swap)
			{
				mainRect->y = screenH + gap;
			}
			else
			{
				subRect->y = screenH + gap;
			}

			if (gap > 0)
			{
				gapRect->visible = true;
				gapRect->x = 0;
				gapRect->y = screenH;
				gapRect->w = screenW;
				gapRect->h = gap;
			}
			break;

		case ORIENT_HORIZONTAL:
			if (nds_screen.swap)
			{
				subRect->x = 0;
				mainRect->x = screenW;
			}
			else
			{
				subRect->x = screenW;
			}
			break;

		case ORIENT_SINGLE:
			// Only the selected screen is shown, either at the origin.
			if (nds_screen.swap)
			{
				mainRect->visible = false;
			}
			else
			{
				subRect->visible = false;
			}
			break;

		default:
			// Hybrid layouts need a third, differently scaled quad, which the
			// portable presenter does not expose yet. Leave them to Cairo.
			mainRect->visible = false;
			subRect->visible = false;
			break;
	}
}

static void WGPU_SetScreenMatrix(cairo_matrix_t *matrix, const cairo_matrix_t *base, int x, int y)
{
	*matrix = *base;
	cairo_matrix_translate(matrix, x, y);
	cairo_matrix_scale(matrix, (double)draw_params.dstScale / 2.0, (double)draw_params.dstScale / 2.0);
	cairo_matrix_invert(matrix);
}

// Position of the drawing area inside the toplevel's client area, in GTK's
// logical pixels. GDK's Wayland backend hands out the toplevel wl_surface for
// child windows, so the WebGPU subsurface has to be offset past the menu and
// toolbar; on X11 the drawing area has a window of its own and this is unused.
static void WGPU_GetDrawingAreaPosition(GtkWidget *widget, int *x, int *y)
{
	*x = 0;
	*y = 0;

	GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
	if ( (toplevel != NULL) && gtk_widget_get_realized(toplevel) )
	{
		gtk_widget_translate_coordinates(widget, toplevel, 0, 0, x, y);
	}
}

static bool WGPU_BeginNativeWindow(GtkWidget *widget, GdkWindow *window)
{
#if defined(GDK_WINDOWING_X11)
	if (GDK_IS_X11_WINDOW(window))
	{
		GdkDisplay *gdkDisplay = gdk_window_get_display(window);
		return wgpu_display.beginX11(GDK_DISPLAY_XDISPLAY(gdkDisplay),
									 (unsigned long)gdk_x11_window_get_xid(window),
									 WGPU_PresentationBackendMask());
	}
#endif

#if defined(GDK_WINDOWING_WAYLAND)
	// Present through a child wl_subsurface so that wgpu and GDK own separate
	// wl_surfaces; committing to GDK's surface directly would race GTK's own
	// Cairo frames and make the compositor report a protocol error.
	if (GDK_IS_WAYLAND_WINDOW(window))
	{
		GdkDisplay *gdkDisplay = gdk_window_get_display(window);
		int x = 0;
		int y = 0;
		WGPU_GetDrawingAreaPosition(widget, &x, &y);
		return wgpu_display.beginWayland(gdk_wayland_display_get_wl_display(gdkDisplay),
										 gdk_wayland_window_get_wl_surface(window),
										 x, y, WGPU_PresentationBackendMask());
	}
#endif

	return false;
}

#if defined(DESMUME_WGPU_OSD)
// Is there anything that should go into the HUD layer this frame?  The target
// is only installed when something will draw, so a frame with the HUD and Lua
// both idle keeps the old cheap path.
static bool GtkHudNeedsDrawing()
{
	bool wanted = (
		CommonSettings.hud.ShowInputDisplay ||
		CommonSettings.hud.ShowGraphicalInputDisplay ||
		CommonSettings.hud.FpsDisplay ||
		CommonSettings.hud.FrameCounterDisplay ||
		CommonSettings.hud.ShowLagFrameCounter ||
		CommonSettings.hud.ShowMicrophone ||
		CommonSettings.hud.ShowRTC ||
		HudEditorMode ||
		HudNeedsDrawStateSlots()
	);

#ifdef HAVE_LUA
	wanted = wanted || AnyLuaActive();
#endif

	return wanted;
}

// The GPU recorder only knows the same layouts the WebGPU present path can
// draw; a Cairo frame must keep the CPU-composited HUD, so stay off those.
static bool WGPU_HudWanted()
{
	if ( !config.presentationWGPU || !config.presentationWGPUHud )
	{
		return false;
	}

	if ( (nds_screen.orientation == ORIENT_HYBRID_EQUAL) ||
		 (nds_screen.orientation == ORIENT_HYBRID_VERTICAL) )
	{
		return false;
	}

	const int rotation = nds_screen.rotation_angle;
	return (rotation == 0) || (rotation == 90) || (rotation == 180) || (rotation == 270);
}

// Creates the target (and the device, if the first expose has not run yet),
// resizes it to the frame, clears it and installs it as aggDraw.hud.  Returns
// false when the caller must fall back to the AGG target in the source buffer.
static bool WGPU_HudPrepare(int width, int height)
{
	if ( (width <= 0) || (height <= 0) || !WGPU_HudWanted() || !GtkHudNeedsDrawing() )
	{
		return false;
	}

	if ( (pDrawingArea == NULL) || !gtk_widget_get_realized(pDrawingArea) )
	{
		return false;
	}

	GdkWindow *window = gtk_widget_get_window(pDrawingArea);
	if (window == NULL)
	{
		return false;
	}

	// The OSD is recorded before the first Expose, so the device has to be
	// brought up here rather than lazily in WGPU_PresentFrame().
	if (!wgpu_display.active)
	{
		if (wgpu_initialization_failed)
		{
			return false;
		}

		if (!WGPU_BeginNativeWindow(pDrawingArea, window))
		{
			wgpu_initialization_failed = true;
			return false;
		}
	}

	if (sGpuHudTarget == NULL)
	{
		sGpuHudTarget = new WGPUOSDDrawTarget(width, height);
	}
	else
	{
		// Resize() releases the renderer when the size actually changed; the
		// beginGPU() below then rebuilds it, including the glyph atlas.
		sGpuHudTarget->setDrawTargetDims(NULL, width, height, width * 4);
	}

	if (!sGpuHudFontsRegistered)
	{
		std::vector<WGPUOSDFont> fonts;
		fonts.reserve((size_t)font_Nums);

		for (int i = 0; i < font_Nums; i++)
		{
			if (fonts_list[i].font == NULL)
			{
				continue;
			}

			WGPUOSDFont font;
			font.name = fonts_list[i].name;
			font.data = fonts_list[i].font;
			font.size = 0;
			fonts.push_back(font);
		}

		sGpuHudTarget->setFonts(fonts.empty() ? NULL : &fonts[0], (int)fonts.size());
		sGpuHudFontsRegistered = true;
	}

	if (!sGpuHudTarget->beginGPU(wgpu_display.instance(), wgpu_display.device(), wgpu_display.queue()))
	{
		return false;
	}

	// Every frame starts from an empty HUD texture; the source buffer used to
	// provide that reset by being overwritten with the game frame.
	sGpuHudTarget->clear();

	WGPU_HudInvalidateView();
	Agg_setHudTarget(sGpuHudTarget);
	sGpuHudFrameActive = true;

	return true;
}

// Renders the recorded HUD into its texture and restores the default target.
// The returned view is sampled by the WebGPU present pass of this frame.
static bool WGPU_HudRenderFrame()
{
	if (!sGpuHudFrameActive)
	{
		return false;
	}

	sGpuHudFrameActive = false;
	WGPU_HudInvalidateView();

	const bool ok = (sGpuHudTarget != NULL) && sGpuHudTarget->render();

	if (ok)
	{
		sGpuHudView = sGpuHudTarget->textureView();
		sGpuHudViewWidth = sGpuHudTarget->targetWidth();
		sGpuHudViewHeight = sGpuHudTarget->targetHeight();
	}

	Agg_setHudTarget(NULL);

	return (sGpuHudView != NULL);
}

// Puts the last rendered HUD back into the CPU screenshot, which reads the
// source buffer and therefore no longer contains it.  The texture holds
// premultiplied BGRA; the screenshot is straight RGBA.
static void WGPU_HudCompositeScreenshot(u8 *dst, int width, int height)
{
	if ( (dst == NULL) || !config.presentationWGPUHud || (sGpuHudView == NULL) ||
		 (sGpuHudTarget == NULL) )
	{
		return;
	}

	const int hudWidth = sGpuHudTarget->targetWidth();
	const int hudHeight = sGpuHudTarget->targetHeight();

	if ( (hudWidth != width) || (hudHeight != height) )
	{
		return;
	}

	std::vector<uint8_t> hud((size_t)hudWidth * (size_t)hudHeight * 4);

	if (!sGpuHudTarget->readPixels(hud.data(), hudWidth * 4))
	{
		return;
	}

	for (int y = 0; y < height; y++)
	{
		const uint8_t *src = hud.data() + (size_t)y * (size_t)hudWidth * 4;
		uint8_t *out = dst + (size_t)y * (size_t)width * 4;

		for (int x = 0; x < width; x++, src += 4, out += 4)
		{
			const unsigned a = src[3];

			if (a == 0)
			{
				continue;
			}

			const unsigned inv = 255u - a;
			const unsigned r = src[2] + (out[0] * inv + 127u) / 255u;
			const unsigned g = src[1] + (out[1] * inv + 127u) / 255u;
			const unsigned b = src[0] + (out[2] * inv + 127u) / 255u;

			out[0] = (uint8_t)((r > 255) ? 255 : r);
			out[1] = (uint8_t)((g > 255) ? 255 : g);
			out[2] = (uint8_t)((b > 255) ? 255 : b);
			out[3] = 255;
		}
	}
}

// Blends the last rendered HUD layer into a frame captured from the video
// filter destination. The HUD texture is premultiplied BGRA at the source
// resolution; a scaled filter (2x/3x/...) makes the destination larger, so
// sample the HUD bilinearly just like the presenter's GPU sampler does.
static void WGPU_HudCompositeFrame(u8 *dst, int dstWidth, int dstHeight)
{
	if ( (dst == NULL) || (dstWidth <= 0) || (dstHeight <= 0) ||
		 !config.presentationWGPUHud || (sGpuHudView == NULL) ||
		 (sGpuHudTarget == NULL) )
	{
		return;
	}

	const int hudWidth = sGpuHudTarget->targetWidth();
	const int hudHeight = sGpuHudTarget->targetHeight();

	if ( (hudWidth <= 0) || (hudHeight <= 0) )
	{
		return;
	}

	std::vector<uint8_t> hud((size_t)hudWidth * (size_t)hudHeight * 4);

	if (!sGpuHudTarget->readPixels(hud.data(), hudWidth * 4))
	{
		return;
	}

	const int64_t maxX16 = (int64_t)(hudWidth - 1) << 16;
	const int64_t maxY16 = (int64_t)(hudHeight - 1) << 16;

	for (int y = 0; y < dstHeight; y++)
	{
		int64_t fy16 = ((int64_t)(2 * y + 1) * (int64_t)hudHeight * 32768) / (int64_t)dstHeight - 32768;
		if (fy16 < 0)
			fy16 = 0;
		else if (fy16 > maxY16)
			fy16 = maxY16;

		const int y0 = (int)(fy16 >> 16);
		const int y1 = (y0 + 1 < hudHeight) ? (y0 + 1) : y0;
		const unsigned wy = (unsigned)((fy16 >> 8) & 0xFF);
		const unsigned wy0 = 256u - wy;

		uint8_t *out = dst + (size_t)y * (size_t)dstWidth * 4;

		for (int x = 0; x < dstWidth; x++, out += 4)
		{
			int64_t fx16 = ((int64_t)(2 * x + 1) * (int64_t)hudWidth * 32768) / (int64_t)dstWidth - 32768;
			if (fx16 < 0)
				fx16 = 0;
			else if (fx16 > maxX16)
				fx16 = maxX16;

			const int x0 = (int)(fx16 >> 16);
			const int x1 = (x0 + 1 < hudWidth) ? (x0 + 1) : x0;
			const unsigned wx = (unsigned)((fx16 >> 8) & 0xFF);
			const unsigned wx0 = 256u - wx;

			const uint8_t *p00 = hud.data() + (((size_t)y0 * (size_t)hudWidth + (size_t)x0) * 4);
			const uint8_t *p01 = hud.data() + (((size_t)y0 * (size_t)hudWidth + (size_t)x1) * 4);
			const uint8_t *p10 = hud.data() + (((size_t)y1 * (size_t)hudWidth + (size_t)x0) * 4);
			const uint8_t *p11 = hud.data() + (((size_t)y1 * (size_t)hudWidth + (size_t)x1) * 4);

			const unsigned w00 = wx0 * wy0;
			const unsigned w01 = wx  * wy0;
			const unsigned w10 = wx0 * wy;
			const unsigned w11 = wx  * wy;

			const unsigned b = (p00[0] * w00 + p01[0] * w01 + p10[0] * w10 + p11[0] * w11) >> 16;
			const unsigned g = (p00[1] * w00 + p01[1] * w01 + p10[1] * w10 + p11[1] * w11) >> 16;
			const unsigned r = (p00[2] * w00 + p01[2] * w01 + p10[2] * w10 + p11[2] * w11) >> 16;
			const unsigned a = (p00[3] * w00 + p01[3] * w01 + p10[3] * w10 + p11[3] * w11) >> 16;

			if (a == 0)
			{
				continue;
			}

			const unsigned inv = 255u - a;
			const unsigned db = b + (out[0] * inv + 127u) / 255u;
			const unsigned dg = g + (out[1] * inv + 127u) / 255u;
			const unsigned dr = r + (out[2] * inv + 127u) / 255u;

			out[0] = (uint8_t)((db > 255) ? 255 : db);
			out[1] = (uint8_t)((dg > 255) ? 255 : dg);
			out[2] = (uint8_t)((dr > 255) ? 255 : dr);
			out[3] = 255;
		}
	}
}
#else
static bool WGPU_HudPrepare(int, int) { return false; }
static bool WGPU_HudRenderFrame() { return false; }
static void WGPU_HudCompositeScreenshot(u8 *, int, int) {}
static void WGPU_HudCompositeFrame(u8 *, int, int) {}
#endif

// Returns true when WebGPU presented the frame.  False means Cairo should draw
// it instead; a failed device or surface is torn down so the next expose can
// retry (or keep using Cairo when the device is unavailable).
static bool WGPU_PresentFrame(GtkWidget *widget, GdkWindow *window, gint daW, gint daH)
{
	if ( (nds_screen.orientation == ORIENT_HYBRID_EQUAL) ||
		 (nds_screen.orientation == ORIENT_HYBRID_VERTICAL) )
	{
		return false;
	}

	if ( (nds_screen.rotation_angle != 0) && (nds_screen.rotation_angle != 90) &&
		 (nds_screen.rotation_angle != 180) && (nds_screen.rotation_angle != 270) )
	{
		return false;
	}

	if (!wgpu_display.active)
	{
		if (wgpu_initialization_failed)
		{
			return false;
		}

		if (!WGPU_BeginNativeWindow(widget, window))
		{
			wgpu_initialization_failed = true;
			return false;
		}
	}

	u32 *fbuf = video->GetDstBufferPtr();
	const int dstW = video->GetDstWidth();
	const int dstH = video->GetDstHeight();

	if ( (fbuf == NULL) || (dstW <= 0) || (dstH < 2) )
	{
		return false;
	}

	WGPULinuxLocalRect localMain;
	WGPULinuxLocalRect localSub;
	WGPULinuxLocalRect localGap;
	WGPU_GetLocalRects(dstW, dstH, draw_params.gap, &localMain, &localSub, &localGap);

	// Same transform as ExposeDrawingArea's Cairo setup. For 90/270 imgW and
	// imgH hold the swapped bounding box, while the local image keeps the
	// unrotated dimensions.
	double localW = draw_params.imgW;
	double localH = draw_params.imgH;

	if ( (nds_screen.rotation_angle == 90) || (nds_screen.rotation_angle == 270) )
	{
		localW = draw_params.imgH;
		localH = draw_params.imgW;
	}

	cairo_matrix_t base;
	cairo_matrix_init_identity(&base);
	cairo_matrix_translate(&base, daW / 2.0, daH / 2.0);
	cairo_matrix_scale(&base, draw_params.ratio, draw_params.ratio);
	cairo_matrix_rotate(&base, M_PI / 180.0 * nds_screen.rotation_angle);
	cairo_matrix_translate(&base, -localW / 2.0, -localH / 2.0);

	// Keep the touch/stylus mapping identical to the Cairo path, including the
	// swapped and rotated layouts.
	if (localMain.visible)
	{
		WGPU_SetScreenMatrix(&nds_screen.topscreen_matrix, &base, localMain.x, localMain.y);
	}
	if (localSub.visible)
	{
		WGPU_SetScreenMatrix(&nds_screen.touch_matrix, &base, localSub.x, localSub.y);
	}

	WGPUPresentRect dstRects[2];
	const int scale = MAX(1, gdk_window_get_scale_factor(window));

	// WGPUPresent maps srcRects to destinations according to the rotation:
	//   0/270 : dst[0] <- main, dst[1] <- sub
	//   90/180: dst[0] <- sub,  dst[1] <- main
	const int rotation = nds_screen.rotation_angle;
	const bool swapDestinationOrder = (rotation == 90) || (rotation == 180);

	for (int i = 0; i < 2; i++)
	{
		const bool isMain = swapDestinationOrder ? (i == 1) : (i == 0);
		dstRects[i] = WGPU_ScaleRect(WGPU_TransformRect(&base, isMain ? localMain : localSub), scale);
	}

	WGPUPresentRect srcRects[2];
	srcRects[0].left   = 0;
	srcRects[0].top    = 0;
	srcRects[0].right  = dstW;
	srcRects[0].bottom = dstH / 2;

	srcRects[1].left   = 0;
	srcRects[1].top    = dstH / 2;
	srcRects[1].right  = dstW;
	srcRects[1].bottom = dstH;

	WGPUPresentRect gapRect = WGPU_ScaleRect(WGPU_TransformRect(&base, localGap), scale);
	const WGPUPresentRect *gapPtr = (gapRect.right > gapRect.left && gapRect.bottom > gapRect.top) ? &gapRect : NULL;

	// 0x00BBGGRR. Cairo uses 30% gray for the gap; keep the same colour.
	const u32 gapColor = 0x004D4D4D;

	// The HUD layer is only valid for the frame that rendered it; PresentFrame()
	// clears it when the HUD render fails.
	const WGPUPresentOverlay *overlayPtr = NULL;

#if defined(DESMUME_WGPU_OSD)
	WGPUPresentOverlay overlay;

	if ( (sGpuHudView != NULL) && (sGpuHudViewWidth > 0) && (sGpuHudViewHeight > 0) )
	{
		memset(&overlay, 0, sizeof(overlay));
		overlay.textureView = sGpuHudView;
		overlay.width = sGpuHudViewWidth;
		overlay.height = sGpuHudViewHeight;
		overlay.premultiplied = true;
		overlayPtr = &overlay;
	}
#endif

	wgpu_display.setClientSize(daW * scale, daH * scale, scale);

	int daX = 0;
	int daY = 0;
	WGPU_GetDrawingAreaPosition(widget, &daX, &daY);
	wgpu_display.setClientPosition(daX, daY);

	if (!wgpu_display.present(fbuf, dstW, dstH, srcRects, dstRects, gapPtr, rotation, gapColor, overlayPtr))
	{
		WGPU_KillDisplay();
		return false;
	}

	return true;
}

#else // !ENABLE_WGPU
static bool WGPU_HudPrepare(int, int) { return false; }
static bool WGPU_HudRenderFrame() { return false; }
static void WGPU_HudCompositeScreenshot(u8 *, int, int) {}
static void WGPU_HudCompositeFrame(u8 *, int, int) {}
#endif // ENABLE_WGPU

/* Drawing callback */
static gboolean ExposeDrawingArea (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	GdkWindow* window = gtk_widget_get_window(widget);
	gint daW, daH;
#if GTK_CHECK_VERSION(2,24,0)
	daW = gdk_window_get_width(window);
	daH = gdk_window_get_height(window);
#else
	gdk_drawable_get_size(window, &daW, &daH);
#endif
	u32* fbuf = video->GetDstBufferPtr();
	gint dstW = video->GetDstWidth();
	gint dstH = video->GetDstHeight();
	
	if(draw_params.valid)
		if(draw_params.daW != daW || draw_params.daH != daH || draw_params.dstW != dstW || draw_params.dstH != dstH)
			draw_params.valid = false;
	
	if(!draw_params.valid) {
		draw_params.daW = daW;
		draw_params.daH = daH;
		draw_params.dstW = dstW;
		draw_params.dstH = dstH;
		
		draw_params.dstScale = dstW * 2 / GPU_FRAMEBUFFER_NATIVE_WIDTH; // Actual scale * 2 to handle 1.5x filters
		
		draw_params.gap = ((nds_screen.orientation == ORIENT_VERTICAL) || (nds_screen.orientation == ORIENT_HYBRID_EQUAL)
				|| (nds_screen.orientation == ORIENT_HYBRID_VERTICAL)) ? nds_screen.gap_size * draw_params.dstScale / 2 : 0;
		if(nds_screen.orientation == ORIENT_HYBRID_VERTICAL) {
			if (nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180)
				calc_hybrid_vertical_params(daW, daH, draw_params.gap, draw_params.dstScale, draw_params.imgW, draw_params.imgH);
			else
				calc_hybrid_vertical_params(daH, daW, draw_params.gap, draw_params.dstScale, draw_params.imgH, draw_params.imgW);
		}
		else {
			if (nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180) {
				draw_params.imgW = screen_size[nds_screen.orientation].width * draw_params.dstScale / 2;
				draw_params.imgH = screen_size[nds_screen.orientation].height * draw_params.dstScale / 2 + draw_params.gap;
				if(nds_screen.orientation == ORIENT_HYBRID_EQUAL)
					draw_params.imgW = (draw_params.imgW * 2.0/3.0) * (double)draw_params.imgH /
						(double)(draw_params.imgH - draw_params.gap) + draw_params.imgW / 3.0;
			} else {
				draw_params.imgH = screen_size[nds_screen.orientation].width * draw_params.dstScale / 2;
				draw_params.imgW = screen_size[nds_screen.orientation].height * draw_params.dstScale / 2 + draw_params.gap;
				if(nds_screen.orientation == ORIENT_HYBRID_EQUAL)
					draw_params.imgH = (draw_params.imgH * 2.0/3.0) * (double)draw_params.imgW /
						(double)(draw_params.imgW - draw_params.gap) + draw_params.imgH / 3.0;
			}
			if(nds_screen.orientation == ORIENT_HYBRID_EQUAL)
				draw_params.hybridBigScreenScale = (double)(dstH + draw_params.gap) / (double)(dstH / 2);
		}
		
		// Calculate scale to fit display area to window
		gfloat hratio = (gfloat)daW / (gfloat)draw_params.imgW;
		gfloat vratio = (gfloat)daH / (gfloat)draw_params.imgH;
		draw_params.ratio = MIN(hratio, vratio);
		
		draw_params.gapOffset = 0;
		switch(nds_screen.orientation) {
			case ORIENT_HYBRID_EQUAL:
				draw_params.gapOffset = dstW * ((gdouble)(dstH + draw_params.gap) / (dstH / 2.0));
				break;
			case ORIENT_HYBRID_VERTICAL:
				draw_params.gapOffset = dstW * draw_params.hybridBigScreenScale;
				break;
		}
		
		draw_params.valid = true;
	}

#if defined(ENABLE_WGPU)
	// Present the composited frame directly through WebGPU. If the device or
	// surface is unavailable this returns false and the Cairo path below draws
	// the frame instead, so a missing adapter never leaves a blank window.
	if (config.presentationWGPU && WGPU_PresentFrame(widget, window, daW, daH))
	{
		draw_count++;
		return TRUE;
	}
#endif

	GdkDrawingContext *context = gdk_window_begin_draw_frame(window, gdk_window_get_clip_region(window));
	cairo_t* cr = gdk_drawing_context_get_cairo_context(context);

	// Scale to window size at center of area
	cairo_translate(cr, daW / 2, daH / 2);
	cairo_scale(cr, draw_params.ratio, draw_params.ratio);
	// Rotate area
	cairo_rotate(cr, M_PI / 180 * nds_screen.rotation_angle);
	// Translate area to top-left corner
	if (nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180) {
		cairo_translate(cr, -draw_params.imgW / 2, -draw_params.imgH / 2);
	} else {
		cairo_translate(cr, -draw_params.imgH / 2, -draw_params.imgW / 2);
	}
	// Draw both screens
	drawTopScreen(cr, fbuf, dstW, dstH / 2, draw_params.gap, nds_screen.rotation_angle, nds_screen.swap, nds_screen.orientation);
	drawBottomScreen(cr, fbuf + dstW * dstH / 2, dstW, dstH / 2, draw_params.gap, nds_screen.rotation_angle, nds_screen.swap, nds_screen.orientation);
	// Draw gap
	
	cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
	cairo_rectangle(cr, draw_params.gapOffset, dstH / 2, dstW, draw_params.gap);
	cairo_fill(cr);
	// Complete the touch transformation matrix
	cairo_matrix_scale(&nds_screen.topscreen_matrix, (double)draw_params.dstScale / 2, (double)draw_params.dstScale / 2);
	cairo_matrix_invert(&nds_screen.topscreen_matrix);
	cairo_matrix_scale(&nds_screen.touch_matrix, (double)draw_params.dstScale / 2, (double)draw_params.dstScale / 2);
	cairo_matrix_invert(&nds_screen.touch_matrix);
	if((nds_screen.orientation == ORIENT_HYBRID_EQUAL) || (nds_screen.orientation == ORIENT_HYBRID_VERTICAL)) {
		cairo_matrix_scale(&nds_screen.topscreen_matrix_hybrid, (double)draw_params.dstScale / 2, (double)draw_params.dstScale / 2);
		cairo_matrix_invert(&nds_screen.topscreen_matrix_hybrid);
		cairo_matrix_scale(&nds_screen.touch_matrix_hybrid, (double)draw_params.dstScale / 2, (double)draw_params.dstScale / 2);
		cairo_matrix_invert(&nds_screen.touch_matrix_hybrid);
	}

	gdk_window_end_draw_frame(window, context);
	draw_count++;

	return TRUE;
}

// Set when a frame has been converted into the video source buffer and is
// waiting for the Lua GUI callback and the built-in HUD before presentation.
static bool sFramePresentPending = false;

// Attach the AGG HUD/Lua draw target to the frame in the video source buffer
// and keep the OSD scale in sync with the current framebuffer size.
//
// This runs as soon as a frame lands in the source buffer, before the Lua GUI
// callback, so that agg.*/gui.* drawing ends up in the buffer that
// PresentFrame() filters and displays.  The built-in HUD is deliberately not
// drawn here: the Lua callback runs first and PresentFrame() overlays the HUD
// afterwards, matching the Windows frontend's compositing order.
static void PrepareHudTarget()
{
#ifdef HAVE_LIBAGG
	// RedrawScreen() can run during UI setup before the first Agg_init(); the
	// target only exists once the OSD has been initialised.
	if (aggDraw.hud == NULL)
		return;

	const int w = (int)video->GetSrcWidth();
	const int h = (int)video->GetSrcHeight();
	const double oldGpuScaleFactor = osd->scale;
	const double newGpuScaleFactor = ( ((double)w / (double)GPU_FRAMEBUFFER_NATIVE_WIDTH) + ((double)h / (double)(GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2.0)) ) / 2.0;

	if ( ((newGpuScaleFactor + 0.001) < oldGpuScaleFactor) || ((newGpuScaleFactor - 0.001) > oldGpuScaleFactor) )
	{
#ifdef AGG2D_USE_VECTORFONTS
		if (vectorFontFile.size() > 0)
		{
			aggDraw.hud->setVectorFont(vectorFontFile, VECTOR_FONT_BASE_SIZE * newGpuScaleFactor, true);
			osd->useVectorFonts = (newGpuScaleFactor >= 1.1f);
		}
		else
		{
			osd->useVectorFonts = false;
		}
#endif // AGG2D_USE_VECTORFONTS

		Agg_setCustomSize(w, h);
		osd->scale = newGpuScaleFactor;
		Hud.rescale(oldGpuScaleFactor, newGpuScaleFactor);
		HudSaveLayout();
	}

	// Always re-attach, even when every built-in HUD element is disabled:
	// Lua scripts draw through aggDraw.hud unconditionally, and the source
	// buffer is the frame that is about to be presented.
	//
	// The GPU HUD target records the same drawing into a texture that the
	// WebGPU present pass samples instead of compositing here.  When it is not
	// available (disabled, unsupported layout, no device) the AGG target keeps
	// the CPU behaviour unchanged.
	WGPU_HudInvalidateView();

	if (WGPU_HudPrepare(w, h))
		return;

	Agg_setHudTarget(NULL);
	aggDraw.hud->setDrawTargetDims( (u8 *)video->GetSrcBufferPtr(), w, h, w * sizeof(u32) );
#endif // HAVE_LIBAGG
}

// Overlay the built-in HUD, run the video filter (source -> destination) and
// ask GTK to repaint.  Called after the Lua GUI callback for a freshly
// converted frame, or directly by RedrawScreen() for non-frame redraws.
static void PresentFrame()
{
	if (!sFramePresentPending)
		return;
	sFramePresentPending = false;

	gtk3gpuEvent->VideoFetchLock();

#ifdef HAVE_FFMPEG_RECORD
	bool capturedFrame = false;
	int capturedWidth = 0;
	int capturedHeight = 0;
#endif
	
#ifdef HAVE_LIBAGG
	const bool needsHUDDisplay = (
		CommonSettings.hud.ShowInputDisplay ||
		CommonSettings.hud.ShowGraphicalInputDisplay ||
		CommonSettings.hud.FpsDisplay ||
		CommonSettings.hud.FrameCounterDisplay ||
		CommonSettings.hud.ShowLagFrameCounter ||
		CommonSettings.hud.ShowMicrophone ||
		CommonSettings.hud.ShowRTC ||
		HudEditorMode
	);

	// The GPU recorder only carries the bitmap glyph atlas, so vector-font text
	// would be dropped.  Draw the same bitmap text the Windows frontend uses
	// while the GPU target is active and restore the configured mode afterwards.
	const bool gpuHudFrame = WGPU_HudFrameActive();
	const bool savedUseVectorFonts = osd->useVectorFonts;

	if (gpuHudFrame)
		osd->useVectorFonts = false;

	if (needsHUDDisplay)
	{
		osd->update();
		DrawHUD();
		osd->clear();
	}

	osd->useVectorFonts = savedUseVectorFonts;

	// Turn the recorded agg_osd/Lua drawing into the texture overlaid by the
	// WebGPU present pass.  If that fails, fall back to compositing the
	// built-in HUD into the source buffer so the frame is not left without it.
	const bool hudRendered = WGPU_HudRenderFrame();

	if (gpuHudFrame && !hudRendered)
	{
		Agg_setHudTarget(NULL);
		aggDraw.hud->setDrawTargetDims( (u8 *)video->GetSrcBufferPtr(), (int)video->GetSrcWidth(), (int)video->GetSrcHeight(), (int)video->GetSrcWidth() * sizeof(u32) );

		if (needsHUDDisplay)
		{
			osd->update();
			DrawHUD();
			osd->clear();
		}
	}
#endif // HAVE_LIBAGG
	
	video->RunFilter();

#ifdef HAVE_FFMPEG_RECORD
	// Capture at the latest point that is still a single CPU frame: after the
	// video filter, HUD and Lua have been composited, but before the presenter
	// applies the window layout/rotation and (on WGPU) the HUD texture overlay.
	// The frame serial keeps manual RedrawScreen() presents from becoming
	// duplicate video frames.
	if (sRecordFilteredOutput && avout_ffmpeg.isRecording() && avout_ffmpeg.hasVideo() &&
	    sFrameSerial != sLastRecordedFrameSerial)
	{
		sLastRecordedFrameSerial = sFrameSerial;

		const int width = (int)video->GetDstWidth();
		const int height = (int)video->GetDstHeight();
		const u32 *pixels = video->GetDstBufferPtr();

		if ( (pixels != NULL) && (width > 0) && (height > 0) )
		{
			const size_t frameBytes = (size_t)width * (size_t)height * 4;
			sRecordFrameBuffer.resize(frameBytes);
			memcpy(sRecordFrameBuffer.data(), pixels, frameBytes);
			capturedWidth = width;
			capturedHeight = height;
			capturedFrame = true;
		}
	}
#endif

	gtk3gpuEvent->VideoFetchUnlock();

#ifdef HAVE_FFMPEG_RECORD
	// The WGPU HUD readback can wait on the GPU, so do it after releasing the
	// video fetch lock.
	if (capturedFrame)
	{
		WGPU_HudCompositeFrame(sRecordFrameBuffer.data(), capturedWidth, capturedHeight);
		avout_ffmpeg.updateVideo(sRecordFrameBuffer.data(), capturedWidth, capturedHeight,
		                         FFmpegCaptureFormat_BGRA8888);
	}
#endif
	
	gtk_widget_queue_draw(pDrawingArea);
}

static void RedrawScreen()
{
	PrepareHudTarget();
	// RedrawScreen() is also used outside a frame (menu actions, resize,
	// pause), where no DidFrameEnd() ran.  Force a present so those paths
	// still refresh the window.
	sFramePresentPending = true;
	PresentFrame();
}

/////////////////////////////// KEYS AND STYLUS UPDATE ///////////////////////////////////////

static bool move_started_in_big_screen = false;
static bool hud_move_started_in_big_screen = false;

#ifdef HAVE_LIBAGG
static gboolean rotoscaled_hudedit(gint x, gint y, gboolean start)
{
	const size_t videoWidth  = video->GetSrcWidth();
	const size_t videoHeight = video->GetSrcHeight();
	const int scaleFactor = (int)( ( (((float)videoWidth / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH) + ((float)videoHeight / ((float)GPU_FRAMEBUFFER_NATIVE_HEIGHT*2.0f))) / 2.0f ) + 0.5f );
	double devX, devY;
	gint X, Y, topX = -1, topY = -1, botX = -1, botY = -1, hybTopX = -1, hybTopY = -1, hybBotX = -1, hybBotY = -1;
	static gint startScreen = 0;
	bool hybrid = ((nds_screen.orientation == ORIENT_HYBRID_EQUAL) || (nds_screen.orientation == ORIENT_HYBRID_VERTICAL)) &&
				  (draw_params.hybridBigScreenScale > 0.0);

	if (nds_screen.orientation != ORIENT_SINGLE || !nds_screen.swap) {
		devX = x;
		devY = y;
		cairo_matrix_transform_point(&nds_screen.topscreen_matrix, &devX, &devY);
		topX = devX * scaleFactor;
		topY = devY * scaleFactor;
		
		if(hybrid && !nds_screen.swap) {
			devX = x;
			devY = y;
			cairo_matrix_transform_point(&nds_screen.topscreen_matrix_hybrid, &devX, &devY);
			hybTopX = devX * scaleFactor;
			hybTopY = devY * scaleFactor;
		}
	}

	if (nds_screen.orientation != ORIENT_SINGLE || nds_screen.swap) {
		devX = x;
		devY = y;
		cairo_matrix_transform_point(&nds_screen.touch_matrix, &devX, &devY);
		botX = devX * scaleFactor;
		botY = devY * scaleFactor;
		
		if(hybrid && nds_screen.swap) {
			devX = x;
			devY = y;
			cairo_matrix_transform_point(&nds_screen.touch_matrix_hybrid, &devX, &devY);
			hybBotX = devX * scaleFactor;
			hybBotY = devY * scaleFactor;
		}
	}

	if (topX >= 0 && topY >= 0 && topX < videoWidth && topY < videoHeight) {
		X = topX;
		Y = topY + (osd->swapScreens ? videoHeight : 0);
		startScreen = 0;
		if(start)
			hud_move_started_in_big_screen = false;
	} else if (hybTopX >= 0 && hybTopY >= 0 && hybTopX < videoWidth && hybTopY < videoHeight) {
		X = hybTopX;
		Y = hybTopY;
		startScreen = 0;
		if(start)
			hud_move_started_in_big_screen = true;
	} else if (botX >= 0 && botY >= 0 && botX < videoWidth && botY < videoHeight) {
		X = botX;
		Y = botY + (osd->swapScreens ? 0 : videoHeight);
		startScreen = 1;
		if(start)
			hud_move_started_in_big_screen = false;
	} else if (hybBotX >= 0 && hybBotY >= 0 && hybBotX < videoWidth && hybBotY < videoHeight) {
		X = hybBotX;
		Y = hybBotY + videoHeight;
		startScreen = 1;
		if(start)
			hud_move_started_in_big_screen = true;
	} else if (!start) {
		if (startScreen == 0) {
			if(!hud_move_started_in_big_screen || !hybrid) {
				X = CLAMP(topX, 0, videoWidth-1);
				Y = CLAMP(topY, 0, videoHeight-1) + (osd->swapScreens ? videoHeight : 0);
			} else {
				X = CLAMP(hybTopX, 0, videoWidth-1);
				Y = CLAMP(hybTopY, 0, videoHeight-1);
			}
		} else {
			if(!hud_move_started_in_big_screen || !hybrid) {
				X = CLAMP(botX, 0, videoWidth-1);
				Y = CLAMP(botY, 0, videoHeight-1) + (osd->swapScreens ? 0 : videoHeight);
			} else {
				X = CLAMP(hybBotX, 0, videoWidth-1);
				Y = CLAMP(hybBotY, 0, videoHeight-1) + videoHeight;
			}
		}
	} else {
		LOG("TopX=%d, TopY=%d, BotX=%d, BotY=%d\n", topX, topY, botX, botY);
		return FALSE;
	}

	LOG("TopX=%d, TopY=%d, BotX=%d, BotY=%d, X=%d, Y=%d\n", topX, topY, botX, botY, X, Y);
	EditHud(X, Y, &Hud);
	RedrawScreen();
	return TRUE;
}
#endif

static gboolean rotoscaled_touchpos(gint x, gint y, gboolean start)
{
    double devX, devY;
    u16 EmuX, EmuY;
    gint X, Y;

    if (nds_screen.orientation == ORIENT_SINGLE && !nds_screen.swap) {
        return FALSE;
    }

    devX = x;
    devY = y;
    cairo_matrix_transform_point(&nds_screen.touch_matrix, &devX, &devY);
    X = devX;
    Y = devY;

    LOG("X=%d, Y=%d\n", X, Y);

    if ((!start && !move_started_in_big_screen) || (X >= 0 && Y >= 0 && X < 256 && Y < 192)) {
        if(start)
			move_started_in_big_screen = false;
        EmuX = CLAMP(X, 0, 255);
        EmuY = CLAMP(Y, 0, 191);
        NDS_setTouchPos(EmuX, EmuY);
        return TRUE;
    }
	
	if((((nds_screen.orientation == ORIENT_HYBRID_EQUAL) ||
		 (nds_screen.orientation == ORIENT_HYBRID_VERTICAL)) && (draw_params.hybridBigScreenScale > 0.0)) && (nds_screen.swap)) {
		devX = x;
		devY = y;
		cairo_matrix_transform_point(&nds_screen.touch_matrix_hybrid, &devX, &devY);
		X = devX;
		Y = devY;
		
		if ((!start && move_started_in_big_screen) || (X >= 0 && Y >= 0 && X < 256 && Y < 192)) {
			if(start)
				move_started_in_big_screen = true;
			EmuX = CLAMP(X, 0, 255);
			EmuY = CLAMP(Y, 0, 191);
			NDS_setTouchPos(EmuX, EmuY);
			return TRUE;
		}
	}
	
    return FALSE;
}

static gboolean Stylus_Move(GtkWidget *w, GdkEventMotion *e, gpointer data)
{
    GdkModifierType state;
    gint x,y;

    if(click) {
        if(e->is_hint)
            gdk_window_get_device_position(gtk_widget_get_window(w), e->device, &x, &y, &state);
        else {
            x= (gint)e->x;
            y= (gint)e->y;
            state=(GdkModifierType)e->state;
        }

        if(state & GDK_BUTTON1_MASK) {
#ifdef HAVE_LIBAGG
            if (HudEditorMode) {
                rotoscaled_hudedit(x, y, FALSE);
            } else {
#else
            {
#endif
                rotoscaled_touchpos(x, y, FALSE);
            }
        }
    }

    return TRUE;
}

static gboolean Stylus_Press(GtkWidget * w, GdkEventButton * e,
                             gpointer data)
{
    GdkModifierType state;
    gint x, y;

    if (e->button == 3) {
        gtk_menu_popup_at_pointer(pPopupMenu, (GdkEvent *) e);
    }

    if (e->button == 1) {
        gdk_window_get_device_position(gtk_widget_get_window(w), e->device, &x, &y, &state);

        if(state & GDK_BUTTON1_MASK) {
#ifdef HAVE_LIBAGG
            if (HudEditorMode) {
                click = rotoscaled_hudedit(x, y, TRUE);
            } else
#endif
            if (desmume_running()) {
                click = rotoscaled_touchpos(x, y, TRUE);
            }
        }
    }

    return TRUE;
}
static gboolean Stylus_Release(GtkWidget *w, GdkEventButton *e, gpointer data)
{
#ifdef HAVE_LIBAGG
    HudClickRelease(&Hud);
#endif
    if(click) NDS_releaseTouch();
    click = FALSE;
    return TRUE;
}

static void loadgame(int num){
   if (desmume_running())
   {
       Pause(NULL, NULL, NULL);
       loadstate_slot(num);
       Launch(NULL, NULL, NULL);
   }
   else
       loadstate_slot(num);
   RedrawScreen();
}

static void savegame(int num){
   if (desmume_running())
   {
       Pause(NULL, NULL, NULL);
       savestate_slot(num);
       Launch(NULL, NULL, NULL);
   }
   else
       savestate_slot(num);
   LoadSaveStateInfo();
   RedrawScreen();
}

static void MenuLoad(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    loadgame(g_variant_get_uint32(parameter));
}

static void MenuSave(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    savegame(g_variant_get_uint32(parameter));
}

#ifdef HAVE_LUA
// ---------------------------------------------------------------------------
// input.get() / controller.get() (M2)
//
// GTK delivers key events to the toplevel while it has focus, so we track them
// here instead of polling the X server: this mirrors the Windows
// GetKeyboardState() foreground path and works on both X11 and Wayland.
// Mouse position and buttons are queried live from the pointer device, and the
// cairo matrices the display path already maintains convert window coordinates
// to NDS screen pixels (including rotation, swap and hybrid layouts).
// ---------------------------------------------------------------------------
static std::set<guint> sLuaPressedKeys;

// Writes the input.get() name for a GDK keyval into buf.  The vocabulary is
// the same as the Windows s_keyToName[] table: uppercase letters, "leftclick",
// "space", "numpad0", ...
static bool GtkLua_KeyName(guint keyval, char* buf, size_t size)
{
    if (keyval >= GDK_KEY_a && keyval <= GDK_KEY_z)
    {
        snprintf(buf, size, "%c", (int)('A' + (keyval - GDK_KEY_a)));
        return true;
    }
    if (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9)
    {
        snprintf(buf, size, "%c", (int)('0' + (keyval - GDK_KEY_0)));
        return true;
    }
    // Ctrl+letter can arrive as a control character instead of the keyval.
    if (keyval >= 0x01 && keyval <= 0x1A)
    {
        snprintf(buf, size, "%c", (int)('A' + (keyval - 0x01)));
        return true;
    }
    if (keyval >= GDK_KEY_KP_0 && keyval <= GDK_KEY_KP_9)
    {
        snprintf(buf, size, "numpad%c", (int)('0' + (keyval - GDK_KEY_KP_0)));
        return true;
    }
    if (keyval >= GDK_KEY_F1 && keyval <= GDK_KEY_F24)
    {
        snprintf(buf, size, "F%d", (int)(keyval - GDK_KEY_F1) + 1);
        return true;
    }

    const char* name = NULL;
    switch (keyval)
    {
        case GDK_KEY_BackSpace: name = "backspace"; break;
        case GDK_KEY_Tab: case GDK_KEY_ISO_Left_Tab: name = "tab"; break;
        case GDK_KEY_Return: case GDK_KEY_KP_Enter: name = "enter"; break;
        case GDK_KEY_Shift_L: case GDK_KEY_Shift_R: name = "shift"; break;
        case GDK_KEY_Control_L: case GDK_KEY_Control_R: name = "control"; break;
        case GDK_KEY_Alt_L: case GDK_KEY_Alt_R: name = "alt"; break;
        case GDK_KEY_Pause: case GDK_KEY_Break: name = "pause"; break;
        case GDK_KEY_Caps_Lock: name = "capslock"; break;
        case GDK_KEY_Escape: name = "escape"; break;
        case GDK_KEY_space: name = "space"; break;
        case GDK_KEY_Page_Up: case GDK_KEY_KP_Page_Up: name = "pageup"; break;
        case GDK_KEY_Page_Down: case GDK_KEY_KP_Page_Down: name = "pagedown"; break;
        case GDK_KEY_End: case GDK_KEY_KP_End: name = "end"; break;
        case GDK_KEY_Home: case GDK_KEY_KP_Home: name = "home"; break;
        case GDK_KEY_Left: case GDK_KEY_KP_Left: name = "left"; break;
        case GDK_KEY_Up: case GDK_KEY_KP_Up: name = "up"; break;
        case GDK_KEY_Right: case GDK_KEY_KP_Right: name = "right"; break;
        case GDK_KEY_Down: case GDK_KEY_KP_Down: name = "down"; break;
        case GDK_KEY_Insert: case GDK_KEY_KP_Insert: name = "insert"; break;
        case GDK_KEY_Delete: case GDK_KEY_KP_Delete: name = "delete"; break;
        case GDK_KEY_KP_Multiply: name = "numpad*"; break;
        case GDK_KEY_KP_Add: name = "numpad+"; break;
        case GDK_KEY_KP_Subtract: name = "numpad-"; break;
        case GDK_KEY_KP_Decimal: name = "numpad."; break;
        case GDK_KEY_KP_Divide: name = "numpad/"; break;
        case GDK_KEY_Num_Lock: name = "numlock"; break;
        case GDK_KEY_Scroll_Lock: name = "scrolllock"; break;
        case GDK_KEY_semicolon: name = "semicolon"; break;
        case GDK_KEY_plus: case GDK_KEY_equal: name = "plus"; break;
        case GDK_KEY_comma: name = "comma"; break;
        case GDK_KEY_minus: name = "minus"; break;
        case GDK_KEY_period: name = "period"; break;
        case GDK_KEY_slash: name = "slash"; break;
        case GDK_KEY_grave: case GDK_KEY_asciitilde: name = "tilde"; break;
        case GDK_KEY_bracketleft: case GDK_KEY_braceleft: name = "leftbracket"; break;
        case GDK_KEY_backslash: case GDK_KEY_bar: name = "backslash"; break;
        case GDK_KEY_bracketright: case GDK_KEY_braceright: name = "rightbracket"; break;
        case GDK_KEY_apostrophe: case GDK_KEY_quotedbl: name = "quote"; break;
        default: break;
    }

    if (name == NULL)
        return false;
    snprintf(buf, size, "%s", name);
    return true;
}

// Convert a drawing-area position to touch-screen-relative NDS pixels.
// Returns false when the point is outside both screens.
static bool GtkLua_PointerToScreen(gint wx, gint wy, int* outX, int* outY)
{
    if (!draw_params.valid)
        return false;

    const bool hybrid = ((nds_screen.orientation == ORIENT_HYBRID_EQUAL) ||
                         (nds_screen.orientation == ORIENT_HYBRID_VERTICAL)) &&
                        (draw_params.hybridBigScreenScale > 0.0);
    double devX, devY;

    // Main (upper) screen.  Windows reports it as negative y, relative to the
    // touch screen.
    if (nds_screen.orientation != ORIENT_SINGLE || !nds_screen.swap)
    {
        devX = wx; devY = wy;
        cairo_matrix_transform_point(&nds_screen.topscreen_matrix, &devX, &devY);
        if (devX >= 0 && devX < 256 && devY >= 0 && devY < 192)
        {
            *outX = (int)devX;
            *outY = (int)devY - 192;
            return true;
        }
    }
    if (hybrid && !nds_screen.swap)
    {
        devX = wx; devY = wy;
        cairo_matrix_transform_point(&nds_screen.topscreen_matrix_hybrid, &devX, &devY);
        if (devX >= 0 && devX < 256 && devY >= 0 && devY < 192)
        {
            *outX = (int)devX;
            *outY = (int)devY - 192;
            return true;
        }
    }

    // Touch (lower) screen.
    if (nds_screen.orientation != ORIENT_SINGLE || nds_screen.swap)
    {
        devX = wx; devY = wy;
        cairo_matrix_transform_point(&nds_screen.touch_matrix, &devX, &devY);
        if (devX >= 0 && devX < 256 && devY >= 0 && devY < 192)
        {
            *outX = (int)devX;
            *outY = (int)devY;
            return true;
        }
    }
    if (hybrid && nds_screen.swap)
    {
        devX = wx; devY = wy;
        cairo_matrix_transform_point(&nds_screen.touch_matrix_hybrid, &devX, &devY);
        if (devX >= 0 && devX < 256 && devY >= 0 && devY < 192)
        {
            *outX = (int)devX;
            *outY = (int)devY;
            return true;
        }
    }

    return false;
}

static bool GtkLua_GetInputStatus(LuaInputStatus* out)
{
    if (out == NULL)
        return false;
    memset(out, 0, sizeof(*out));

    GdkWindow* window = (pDrawingArea != NULL) ? gtk_widget_get_window(pDrawingArea) : NULL;
    GdkDevice* pointer = NULL;
    GdkDisplay* display = gdk_display_get_default();
    if (display != NULL)
    {
        GdkSeat* seat = gdk_display_get_default_seat(display);
        if (seat != NULL)
            pointer = gdk_seat_get_pointer(seat);
    }

    gint wx = 0, wy = 0;
    GdkModifierType mask = (GdkModifierType)0;
    if (window != NULL && pointer != NULL)
        gdk_window_get_device_position(window, pointer, &wx, &wy, &mask);

    // Mouse buttons share the keyboard namespace on Windows.
    if (mask & GDK_BUTTON1_MASK)
        snprintf(out->keyNames[out->keyCount++], LUA_INPUT_KEY_NAME_SIZE, "leftclick");
    if (mask & GDK_BUTTON3_MASK)
        snprintf(out->keyNames[out->keyCount++], LUA_INPUT_KEY_NAME_SIZE, "rightclick");
    if (mask & GDK_BUTTON2_MASK)
        snprintf(out->keyNames[out->keyCount++], LUA_INPUT_KEY_NAME_SIZE, "middleclick");

    for (std::set<guint>::const_iterator it = sLuaPressedKeys.begin();
         it != sLuaPressedKeys.end() && out->keyCount < LUA_INPUT_MAX_KEYS; ++it)
    {
        char name[LUA_INPUT_KEY_NAME_SIZE];
        if (!GtkLua_KeyName(*it, name, sizeof(name)))
            continue;

        bool duplicate = false;
        for (int i = 0; i < out->keyCount; i++)
        {
            if (strcmp(out->keyNames[i], name) == 0)
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            snprintf(out->keyNames[out->keyCount++], LUA_INPUT_KEY_NAME_SIZE, "%s", name);
    }

    if (window != NULL && pointer != NULL && GtkLua_PointerToScreen(wx, wy, &out->mouseX, &out->mouseY))
        out->mouseValid = true;

    return true;
}

static bool GtkLua_GetControllerStatus(unsigned int joystickID, bool returnDiagonals, LuaControllerStatus* out)
{
    if (out == NULL)
        return false;
    memset(out, 0, sizeof(*out));

    struct joystick_raw_state raw;
    if (!get_joystick_raw_state(joystickID, &raw))
        return false;

    // SDL hat bits -> the four Windows dpad directions.  Like the Windows
    // JOY_RETURNPOV code, a diagonal reports nothing unless the script asked
    // for diagonals.
    if (raw.hasHat && raw.hat >= 0)
    {
        const int count = ((raw.hat & SDL_HAT_UP) ? 1 : 0) +
                          ((raw.hat & SDL_HAT_RIGHT) ? 1 : 0) +
                          ((raw.hat & SDL_HAT_DOWN) ? 1 : 0) +
                          ((raw.hat & SDL_HAT_LEFT) ? 1 : 0);
        if (count == 1 || returnDiagonals)
        {
            out->dpad[0] = (raw.hat & SDL_HAT_UP) != 0;
            out->dpad[1] = (raw.hat & SDL_HAT_RIGHT) != 0;
            out->dpad[2] = (raw.hat & SDL_HAT_DOWN) != 0;
            out->dpad[3] = (raw.hat & SDL_HAT_LEFT) != 0;
        }
    }

    out->buttonCount = raw.buttonCount;
    for (int i = 0; i < raw.buttonCount; i++)
        out->buttons[i] = raw.buttons[i];

    out->axisCount = raw.axisCount;
    for (int i = 0; i < raw.axisCount; i++)
        out->axes[i] = raw.axes[i];

    return true;
}

static gboolean LuaWindowFocusOut(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
    // GTK does not deliver key releases to an unfocused window; drop the cache
    // so a script does not see a key as held forever.
    sLuaPressedKeys.clear();
    return FALSE;
}
#endif // HAVE_LUA

#ifdef HAVE_LUA
// ---------------------------------------------------------------------------
// emu.addmenu() / emu.setmenuiteminfo() / emu.registermenustart().
//
// GTK3 has no imperative menu API, so the script's menu tree is kept here and
// rendered into GMenu models.  Each leaf item gets its own stateful GAction:
// a shared action could not carry per-item enabled/checked state.  GMenu has no
// separator item, so a separator starts a new section; GtkMenuTracker draws the
// section boundary as a separator.
//
// Everything runs on the GTK main thread, which is also the emulation/Lua
// thread, so no locking is needed.  Clicks are still queued and drained at a
// frame boundary: the action callback can fire from LuaPumpEvents() while a
// script is executing, and re-entering that lua_State is not safe.
// ---------------------------------------------------------------------------
enum
{
    GTK_LUA_MENU_ITEM_FIRST = 40000,
    GTK_LUA_MENU_ITEM_COUNT = 512
};

struct GtkLuaMenu;

struct GtkLuaMenuEntry
{
    int id;
    std::string title;
    bool separator;
    bool checkable; // became a toggle item once checked=true was requested
    bool checked;
    GtkLuaMenu* submenu;
};

struct GtkLuaAction
{
    GSimpleAction* action;
    bool stateful;
};

struct GtkLuaMenu
{
    int id;
    std::string title;
    bool topLevel;
    bool appended;
    GMenu* model;
    std::vector<GtkLuaMenuEntry> entries;
};

static GtkLuaMenu sLuaMenuRoot;
static std::map<int, GtkLuaMenu*> sLuaMenus;
static std::map<int, GtkLuaMenu*> sLuaEntryParent;
static std::map<int, GtkLuaAction> sLuaActions;
static bool sLuaMenuItemUsed[GTK_LUA_MENU_ITEM_COUNT];
static GMenu* sLuaAppMenubar = NULL;
static bool sLuaMenuInitPending = false;
static bool sLuaMenuTimerStarted = false;
static std::vector<int> sLuaPendingMenuClicks;

static void GtkLua_DrainMenuCalls();
static void GtkLua_RebuildAll();
static void GtkLua_MenuItemActivated(GSimpleAction* action, GVariant* parameter, gpointer user_data);

// GMenu labels use '_' for mnemonics; the Windows/macOS Lua API treats the
// title as literal text, so escape it.
static std::string GtkLua_EscapeLabel(const std::string& text)
{
    std::string out;
    out.reserve(text.size() + 8);
    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] == '_')
            out += "__";
        else
            out += text[i];
    }
    return out;
}

static GtkLuaMenuEntry* GtkLua_FindEntry(GtkLuaMenu* menu, int id)
{
    if (menu == NULL)
        return NULL;
    for (size_t i = 0; i < menu->entries.size(); i++)
    {
        if (menu->entries[i].id == id)
            return &menu->entries[i];
    }
    return NULL;
}

static GSimpleAction* GtkLua_EnsureAction(int id, bool checkable, bool checked)
{
    std::map<int, GtkLuaAction>::iterator it = sLuaActions.find(id);
    if (it != sLuaActions.end() && it->second.stateful == checkable)
    {
        if (checkable)
            g_simple_action_set_state(it->second.action, g_variant_new_boolean(checked ? TRUE : FALSE));
        return it->second.action;
    }
    if (pApp == NULL)
        return NULL;

    char name[32];
    snprintf(name, sizeof(name), "lua%d", id);

    if (it != sLuaActions.end())
    {
        // The item changed between a plain item and a toggle item; GtkMenuTracker
        // picks the widget type (GtkMenuItem vs GtkCheckMenuItem) from the action,
        // so replace it instead of keeping a checkbox on every Lua item.
        g_action_map_remove_action(G_ACTION_MAP(pApp), name);
        sLuaActions.erase(it);
    }

    GSimpleAction* action = checkable
        ? g_simple_action_new_stateful(name, NULL, g_variant_new_boolean(checked ? TRUE : FALSE))
        : g_simple_action_new(name, NULL);
    g_signal_connect(action, "activate", G_CALLBACK(GtkLua_MenuItemActivated),
                     GINT_TO_POINTER(id));
    // The action map takes its own reference; we only need the borrowed
    // pointer for set_enabled/set_state until the item is freed.
    g_action_map_add_action(G_ACTION_MAP(pApp), G_ACTION(action));
    g_object_unref(action);

    GtkLuaAction entry;
    entry.action = action;
    entry.stateful = checkable;
    sLuaActions[id] = entry;
    return action;
}

static void GtkLua_MenuItemActivated(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
    sLuaPendingMenuClicks.push_back(GPOINTER_TO_INT(user_data));
    if (!AnyLuaRunning())
        GtkLua_DrainMenuCalls();
}

static void GtkLua_ClearModel(GMenu* model)
{
    while (g_menu_model_get_n_items(G_MENU_MODEL(model)) > 0)
        g_menu_remove(model, 0);
}

static void GtkLua_RebuildMenu(GtkLuaMenu* menu)
{
    if (menu == NULL || menu->model == NULL)
        return;

    GtkLua_ClearModel(menu->model);

    GMenu* section = g_menu_new();
    bool sectionHasItems = false;

    for (size_t i = 0; i < menu->entries.size(); i++)
    {
        GtkLuaMenuEntry& entry = menu->entries[i];

        if (entry.separator)
        {
            // A section boundary is how GtkMenuTracker represents a separator.
            if (sectionHasItems)
            {
                g_menu_append_section(menu->model, NULL, G_MENU_MODEL(section));
                g_object_unref(section);
                section = g_menu_new();
                sectionHasItems = false;
            }
            continue;
        }

        std::string label = GtkLua_EscapeLabel(entry.title);
        GMenuItem* item = g_menu_item_new(label.c_str(), NULL);
        if (entry.submenu != NULL)
        {
            if (entry.submenu->model == NULL)
                entry.submenu->model = g_menu_new();
            g_menu_item_set_submenu(item, G_MENU_MODEL(entry.submenu->model));
        }
        else if (GtkLua_EnsureAction(entry.id, entry.checkable, entry.checked) != NULL)
        {
            char actionName[32];
            snprintf(actionName, sizeof(actionName), "app.lua%d", entry.id);
            g_menu_item_set_action_and_target_value(item, actionName, NULL);
        }

        g_menu_append_item(section, item);
        g_object_unref(item);
        sectionHasItems = true;
    }

    if (sectionHasItems)
        g_menu_append_section(menu->model, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
}

static void GtkLua_RemoveTopLevelFromBar(GtkLuaMenu* menu)
{
    if (sLuaAppMenubar == NULL || menu == NULL || menu->model == NULL)
        return;

    const int count = (int)g_menu_model_get_n_items(G_MENU_MODEL(sLuaAppMenubar));
    for (int i = count - 1; i >= 0; i--)
    {
        GMenuModel* link = g_menu_model_get_item_link(
            G_MENU_MODEL(sLuaAppMenubar), i, G_MENU_LINK_SUBMENU);
        const bool match = (link == G_MENU_MODEL(menu->model));
        if (link != NULL)
            g_object_unref(link);
        if (match)
        {
            g_menu_remove(sLuaAppMenubar, i);
            break;
        }
    }
    menu->appended = false;
}

static void GtkLua_DestroyMenu(GtkLuaMenu* menu)
{
    if (menu == NULL)
        return;

    for (size_t i = 0; i < menu->entries.size(); i++)
        GtkLua_DestroyMenu(menu->entries[i].submenu);
    menu->entries.clear();

    if (menu->topLevel)
        GtkLua_RemoveTopLevelFromBar(menu);
    if (menu->model != NULL)
    {
        g_object_unref(menu->model);
        menu->model = NULL;
    }
    if (menu->id != 0)
        sLuaMenus.erase(menu->id);
    delete menu;
}

static void GtkLua_RebuildAll()
{
    // Rebuild every model first.  A parent references its child's GMenu object,
    // which stays the same across rebuilds, so order does not matter.
    for (std::map<int, GtkLuaMenu*>::iterator it = sLuaMenus.begin();
         it != sLuaMenus.end(); ++it)
    {
        GtkLua_RebuildMenu(it->second);
    }

    if (sLuaAppMenubar == NULL)
        return;

    // Add top-level menus that are not in the bar yet, in tree order.  Menus
    // already in the bar keep their position while their model updates.
    for (size_t i = 0; i < sLuaMenuRoot.entries.size(); i++)
    {
        GtkLuaMenu* menu = sLuaMenuRoot.entries[i].submenu;
        if (menu == NULL || menu->appended)
            continue;
        std::string label = GtkLua_EscapeLabel(menu->title);
        g_menu_append_submenu(sLuaAppMenubar, label.c_str(), G_MENU_MODEL(menu->model));
        menu->appended = true;
    }
}

static int GtkLua_AllocItemID()
{
    for (int i = 0; i < GTK_LUA_MENU_ITEM_COUNT; i++)
    {
        if (!sLuaMenuItemUsed[i])
        {
            sLuaMenuItemUsed[i] = true;
            return GTK_LUA_MENU_ITEM_FIRST + i;
        }
    }
    return -1;
}

static void GtkLua_FreeItemID(int itemID)
{
    const int index = itemID - GTK_LUA_MENU_ITEM_FIRST;
    if (index >= 0 && index < GTK_LUA_MENU_ITEM_COUNT)
        sLuaMenuItemUsed[index] = false;

    std::map<int, GtkLuaAction>::iterator it = sLuaActions.find(itemID);
    if (it != sLuaActions.end())
    {
        if (pApp != NULL)
        {
            char name[32];
            snprintf(name, sizeof(name), "lua%d", itemID);
            g_action_map_remove_action(G_ACTION_MAP(pApp), name);
        }
        sLuaActions.erase(it);
    }
    sLuaEntryParent.erase(itemID);
}

static bool GtkLua_IsOwnedItemID(int itemID)
{
    const int index = itemID - GTK_LUA_MENU_ITEM_FIRST;
    return index >= 0 && index < GTK_LUA_MENU_ITEM_COUNT && sLuaMenuItemUsed[index];
}

static LuaMenuHandle GtkLua_RootMenu()
{
    return &sLuaMenuRoot;
}

static LuaMenuHandle GtkLua_FindSubmenu(LuaMenuHandle handle, const char* title, int* outItemID)
{
    if (handle == NULL || title == NULL)
        return NULL;

    GtkLuaMenu* menu = static_cast<GtkLuaMenu*>(handle);
    for (size_t i = 0; i < menu->entries.size(); i++)
    {
        GtkLuaMenuEntry& entry = menu->entries[i];
        if (entry.submenu != NULL && entry.title == title)
        {
            if (outItemID != NULL)
                *outItemID = entry.id;
            return entry.submenu;
        }
    }
    return NULL;
}

static LuaMenuHandle GtkLua_AddSubmenu(LuaMenuHandle parentHandle, const char* title, int itemID)
{
    if (parentHandle == NULL || title == NULL)
        return NULL;

    GtkLuaMenu* parent = static_cast<GtkLuaMenu*>(parentHandle);
    GtkLuaMenu* menu = new GtkLuaMenu();
    menu->id = itemID;
    menu->title = title;
    menu->topLevel = (parent == &sLuaMenuRoot);
    menu->appended = false;
    menu->model = g_menu_new();

    GtkLuaMenuEntry entry;
    entry.id = itemID;
    entry.title = title;
    entry.separator = false;
    entry.checkable = false;
    entry.checked = false;
    entry.submenu = menu;
    parent->entries.push_back(entry);

    sLuaMenus[itemID] = menu;
    sLuaEntryParent[itemID] = parent;
    GtkLua_RebuildAll();
    return menu;
}

static bool GtkLua_AddItem(LuaMenuHandle handle, int itemID, const char* title)
{
    if (handle == NULL || title == NULL)
        return false;

    GtkLuaMenu* menu = static_cast<GtkLuaMenu*>(handle);
    GtkLuaMenuEntry entry;
    entry.id = itemID;
    entry.title = title;
    entry.separator = false;
    entry.checkable = false;
    entry.checked = false;
    entry.submenu = NULL;
    menu->entries.push_back(entry);
    sLuaEntryParent[itemID] = menu;
    GtkLua_RebuildAll();
    return true;
}

static bool GtkLua_AddSeparator(LuaMenuHandle handle, int itemID)
{
    if (handle == NULL)
        return false;

    GtkLuaMenu* menu = static_cast<GtkLuaMenu*>(handle);
    GtkLuaMenuEntry entry;
    entry.id = itemID;
    entry.separator = true;
    entry.checkable = false;
    entry.checked = false;
    entry.submenu = NULL;
    menu->entries.push_back(entry);
    sLuaEntryParent[itemID] = menu;
    GtkLua_RebuildAll();
    return true;
}

static void GtkLua_RemoveItem(LuaMenuHandle handle, int itemID)
{
    if (handle == NULL)
        return;

    GtkLuaMenu* menu = static_cast<GtkLuaMenu*>(handle);
    for (size_t i = 0; i < menu->entries.size(); i++)
    {
        if (menu->entries[i].id != itemID)
            continue;

        GtkLuaMenu* child = menu->entries[i].submenu;
        menu->entries.erase(menu->entries.begin() + i);
        sLuaEntryParent.erase(itemID);
        if (child != NULL)
            GtkLua_DestroyMenu(child);
        GtkLua_RebuildAll();
        return;
    }
}

static bool GtkLua_IsMenuEmpty(LuaMenuHandle handle)
{
    GtkLuaMenu* menu = static_cast<GtkLuaMenu*>(handle);
    return menu == NULL || menu->entries.empty();
}

static void GtkLua_SetEnabled(LuaMenuHandle handle, int itemID, bool enabled)
{
    (void)handle;
    std::map<int, GtkLuaAction>::iterator it = sLuaActions.find(itemID);
    if (it != sLuaActions.end())
        g_simple_action_set_enabled(it->second.action, enabled ? TRUE : FALSE);
}

static void GtkLua_SetChecked(LuaMenuHandle handle, int itemID, bool checked)
{
    (void)handle;

    GtkLuaMenuEntry* entry = NULL;
    std::map<int, GtkLuaMenu*>::iterator pit = sLuaEntryParent.find(itemID);
    if (pit != sLuaEntryParent.end())
        entry = GtkLua_FindEntry(pit->second, itemID);

    if (entry == NULL)
        return;

    entry->checked = checked;

    // The first explicit "checked" field turns the item into a toggle item,
    // even when the value is false: the script has declared that this entry
    // participates in check state, and doing it on the first refresh (rather
    // than the first true value) keeps later refreshes from rebuilding the
    // model while the menu is open.
    if (!entry->checkable)
    {
        entry->checkable = true;
        GtkLua_RebuildAll();
        return;
    }

    std::map<int, GtkLuaAction>::iterator it = sLuaActions.find(itemID);
    if (it != sLuaActions.end())
        g_simple_action_set_state(it->second.action, g_variant_new_boolean(checked ? TRUE : FALSE));
}

static void GtkLua_SetTitle(LuaMenuHandle handle, int itemID, const char* title)
{
    if (title == NULL)
        return;

    GtkLuaMenu* menu = static_cast<GtkLuaMenu*>(handle);
    GtkLuaMenuEntry* entry = GtkLua_FindEntry(menu, itemID);
    if (entry == NULL)
    {
        // The handle can be stale if another script owns the same submenu;
        // fall back to the parent recorded when the item was created.
        std::map<int, GtkLuaMenu*>::iterator it = sLuaEntryParent.find(itemID);
        if (it != sLuaEntryParent.end())
            entry = GtkLua_FindEntry(it->second, itemID);
    }
    if (entry != NULL)
    {
        if (entry->title == title)
            return;
        entry->title = title;
        GtkLua_RebuildAll();
    }
}

static void GtkLua_QueueMenuInit()
{
    sLuaMenuInitPending = true;
}

static void GtkLua_DrainMenuCalls()
{
    // Snapshot first: a handler can queue more clicks or stop scripts.
    std::vector<int> pending;
    pending.swap(sLuaPendingMenuClicks);

    if (sLuaMenuInitPending)
    {
        sLuaMenuInitPending = false;
        // Do not rebuild the models here: setmenuiteminfo() updates the
        // per-item actions in place, and rebuilding while the menu is open
        // would drop the hover highlight and close an open submenu.  The tree
        // hooks (add/remove/title) rebuild themselves when they change the
        // model.
        CallRegisteredLuaFunctions(LUACALL_ONINITMENU);
    }

    for (size_t i = 0; i < pending.size(); i++)
        CallRegisteredLuaMenuHandlers((PlatformMenuItem)pending[i]);
}

// GMenuModel has no "about to show" signal, so refresh the menu state on a
// short timer (the closest GTK equivalent of WM_INITMENUPOPUP) and before the
// next frame when a script is running.  Only marks the request when Lua is
// executing; the frame loop then runs it at a safe point.
static gboolean GtkLua_MenuRefreshTimer(gpointer data)
{
    (void)data;
    if (sLuaMenuRoot.entries.empty() && sLuaPendingMenuClicks.empty())
        return G_SOURCE_CONTINUE;

    sLuaMenuInitPending = true;
    if (!AnyLuaRunning())
        GtkLua_DrainMenuCalls();
    return G_SOURCE_CONTINUE;
}

static void GtkLua_SetAppMenubar(GMenu* menubar)
{
    if (sLuaAppMenubar != NULL)
        g_object_unref(sLuaAppMenubar);
    sLuaAppMenubar = menubar;
    if (sLuaAppMenubar != NULL)
        g_object_ref(sLuaAppMenubar);

    if (!sLuaMenuTimerStarted)
    {
        sLuaMenuTimerStarted = true;
        g_timeout_add(250, GtkLua_MenuRefreshTimer, NULL);
    }

    GtkLua_RebuildAll();
}

static const LuaMenuHooks sGtkLuaMenuHooks = {
    GtkLua_AllocItemID,
    GtkLua_FreeItemID,
    GtkLua_IsOwnedItemID,
    GtkLua_RootMenu,
    GtkLua_FindSubmenu,
    GtkLua_AddSubmenu,
    GtkLua_AddItem,
    GtkLua_AddSeparator,
    GtkLua_RemoveItem,
    GtkLua_IsMenuEmpty,
    GtkLua_SetEnabled,
    GtkLua_SetChecked,
    GtkLua_SetTitle,
    GtkLua_QueueMenuInit
};
#endif // HAVE_LUA

static gint Key_Press(GtkWidget *w, GdkEventKey *e, gpointer data)
{
#ifdef HAVE_LUA
  // Record every key, not just the ones bound to a DS button: input.get()
  // reports raw keyboard state.
  sLuaPressedKeys.insert(e->keyval);
#endif
  guint mask;
  mask = gtk_accelerator_get_default_mod_mask ();
  if( (e->state & mask) == 0){
    u16 Key = lookup_key(e->keyval);
    u16 TurboKey = lookup_turbo_key(e->keyval);

    if(TurboKey){
      ADD_KEY( turbo_latch, TurboKey );
      ctrls_set_turbo_held( turbo_latch );
    }

    if(Key || TurboKey){
      if(Key)
        ADD_KEY( keys_latch, Key );
      return 1;
    }
  }

#ifdef PROFILE_MEMORY_ACCESS
  if ( e->keyval == GDK_Tab) {
    print_memory_profiling();
    return 1;
  }
#endif
  return 0;
}

static gint Key_Release(GtkWidget *w, GdkEventKey *e, gpointer data)
{
#ifdef HAVE_LUA
  sLuaPressedKeys.erase(e->keyval);
#endif
  u16 TurboKey = lookup_turbo_key(e->keyval);
  if(TurboKey){
    RM_KEY( turbo_latch, TurboKey );
    ctrls_set_turbo_held( turbo_latch );
  }

  u16 Key = lookup_key(e->keyval);
  RM_KEY( keys_latch, Key );
  return 1;

}

/////////////////////////////// SOUND SETTINGS //////////////////////////////////////

// SPU volume is a percentage; older configs stored the SDL core's 0..128 range
// in the same key.
static int ClampSoundVolume(int volume)
{
	if (volume < 0)
		return 0;
	if (volume > 100)
		return 100;
	return volume;
}

static void SoundVolumeChanged(GtkRange *range, gpointer data)
{
	const int volume = (int)(gtk_range_get_value(range) + 0.5);
	gchar *text = g_strdup_printf("%d", volume);

	gtk_label_set_text(GTK_LABEL(data), text);
	// Audible while the dialog is open, like the Windows sound dialog.
	SPU_SetVolume(volume);

	g_free(text);
}

// The synchronizer method frame is only meaningful in synchronous mode.  Both
// radios carry the frame and are connected with the synchronous one as user
// data, so the frame follows the group state whichever radio was clicked.
static void SoundSyncModeToggled(GtkToggleButton *button, gpointer data)
{
	GtkWidget *frame = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "method-frame"));

	if (frame != NULL)
		gtk_widget_set_sensitive(frame, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data)));
}

static void SoundSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkBuilder *builder = gtk_builder_new_from_resource("/org/desmume/DeSmuME/sound_settings.ui");
	GtkDialog *dialog = GTK_DIALOG(gtk_builder_get_object(builder, "dialog"));
	GtkToggleButton *wDual = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "dual_radio"));
	GtkToggleButton *wSync = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sync_radio"));
	GtkWidget *wMethodFrame = GTK_WIDGET(gtk_builder_get_object(builder, "sync_method_frame"));
	GtkToggleButton *wMethodN = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "method_n"));
	GtkToggleButton *wMethodZ = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "method_z"));
	GtkToggleButton *wMethodP = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "method_p"));
	GtkComboBoxText *wCoreCombo = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "sound_core_combo"));
	GtkComboBox *wInterpolation = GTK_COMBO_BOX(gtk_builder_get_object(builder, "interpolation_combo"));
	GtkWidget *wVolume = GTK_WIDGET(gtk_builder_get_object(builder, "volume_scale"));
	GtkWidget *wVolumeValue = GTK_WIDGET(gtk_builder_get_object(builder, "volume_value"));
	g_object_unref(builder);

	const int previousVolume = ClampSoundVolume(config.audio_volume);

	// Sound core list: "None" plus the cores this build actually links, which
	// in this frontend is the SDL one. Same shape as the Windows combo, which
	// also starts at index 1 so the dummy core is not listed twice.
	gtk_combo_box_text_remove_all(wCoreCombo);
	gtk_combo_box_text_append(wCoreCombo, "0", "None");
	for (int i = 1; SNDCoreList[i] != NULL; i++) {
		char id[16];
		g_snprintf(id, sizeof(id), "%d", SNDCoreList[i]->id);
		gtk_combo_box_text_append(wCoreCombo, id, SNDCoreList[i]->Name);
	}

	char coreId[16];
	g_snprintf(coreId, sizeof(coreId), "%d", config.audio_enabled ? config.audio_core.get() : 0);
	if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(wCoreCombo), coreId))
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(wCoreCombo), "0");

	// Synchronization mode and method. The method only applies in synchronous
	// mode, so the frame is greyed out in dual mode, as on Windows.
	gtk_toggle_button_set_active(wSync, SPUMode != SPUMODE_DUALASYNC);
	gtk_toggle_button_set_active(wDual, SPUMode == SPUMODE_DUALASYNC);
	gtk_toggle_button_set_active(wMethodN, config.audio_sync == SPUMODE_SYNCN);
	gtk_toggle_button_set_active(wMethodZ, config.audio_sync == SPUMODE_SYNCZ);
#ifdef HAVE_LIBSOUNDTOUCH
	gtk_toggle_button_set_active(wMethodP, config.audio_sync == SPUMODE_SYNCP);
#endif

	g_object_set_data(G_OBJECT(wDual), "method-frame", wMethodFrame);
	g_object_set_data(G_OBJECT(wSync), "method-frame", wMethodFrame);
	g_signal_connect(G_OBJECT(wDual), "toggled", G_CALLBACK(SoundSyncModeToggled), wSync);
	g_signal_connect(G_OBJECT(wSync), "toggled", G_CALLBACK(SoundSyncModeToggled), wSync);
	gtk_widget_set_sensitive(wMethodFrame, SPUMode != SPUMODE_DUALASYNC);

	gtk_combo_box_set_active(wInterpolation, (int)CommonSettings.spuInterpolationMode);

	// Volume: a percentage, applied while the dialog is open so it can be
	// dialled in by ear; Cancel puts the old value back.
	gtk_range_set_range(GTK_RANGE(wVolume), 0, 100);
	gtk_range_set_increments(GTK_RANGE(wVolume), 1, 10);
	g_signal_connect(G_OBJECT(wVolume), "value-changed", G_CALLBACK(SoundVolumeChanged), wVolumeValue);
	gtk_range_set_value(GTK_RANGE(wVolume), previousVolume);

	gtk_widget_show_all(gtk_dialog_get_content_area(dialog));

#ifndef HAVE_LIBSOUNDTOUCH
	// The time-stretch method needs libsoundtouch, which this build has not got.
	gtk_widget_set_visible(GTK_WIDGET(wMethodP), FALSE);
#endif

	switch (gtk_dialog_run(dialog)) {
	case GTK_RESPONSE_OK:
	{
		const gchar *selectedCoreId = gtk_combo_box_get_active_id(GTK_COMBO_BOX(wCoreCombo));
		const int core = (selectedCoreId != NULL) ? (int)g_ascii_strtoll(selectedCoreId, NULL, 10) : SNDCORE_DUMMY;

		config.audio_core = core;
		config.audio_enabled = (core != SNDCORE_DUMMY);

		if (core != SPU_currentCoreNum)
			SPU_ChangeSoundCore(core, 735 * 4);

		spumode_enum mode = SPUMODE_DUALASYNC;
		if (gtk_toggle_button_get_active(wSync)) {
			if (gtk_toggle_button_get_active(wMethodZ))
				mode = SPUMODE_SYNCZ;
#ifdef HAVE_LIBSOUNDTOUCH
			else if (gtk_toggle_button_get_active(wMethodP))
				mode = SPUMODE_SYNCP;
#endif
			else
				mode = SPUMODE_SYNCN;
		}
		ApplySPUMode(mode);

		const int interpolation = gtk_combo_box_get_active(wInterpolation);
		if (interpolation >= SPUInterpolation_None && interpolation <= SPUInterpolation_CatmullRom)
			ApplySPUInterpolation((SPUInterpolationMode)interpolation);

		const int volume = (int)(gtk_range_get_value(GTK_RANGE(wVolume)) + 0.5);
		config.audio_volume = volume;
		SPU_SetVolume(volume);
		break;
	}
	case GTK_RESPONSE_CANCEL:
	case GTK_RESPONSE_NONE:
		SPU_SetVolume(previousVolume);
		break;
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void MicrophoneSampleModeToggled(GtkToggleButton *button, gpointer data)
{
	gtk_widget_set_sensitive(GTK_WIDGET(data), gtk_toggle_button_get_active(button));
}

static void MicrophoneSampleBrowse(GtkWidget *button, gpointer data)
{
	GtkWidget *entry = GTK_WIDGET(data);
	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_add_pattern(filter, "*.wav");
	gtk_file_filter_set_name(filter, "8-bit mono PCM WAV files");

	GtkFileChooserNative *chooser = gtk_file_chooser_native_new(
		"Select microphone sample...", GTK_WINDOW(pWindow),
		GTK_FILE_CHOOSER_ACTION_OPEN, "_Open", "_Cancel");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

	const char *current = gtk_entry_get_text(GTK_ENTRY(entry));
	if (current != NULL && g_path_is_absolute(current))
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser), current);

	if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
		GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(chooser));
		if (file != NULL) {
			gchar *path = g_file_get_path(file);
			if (path != NULL) {
				gtk_entry_set_text(GTK_ENTRY(entry), path);
				g_free(path);
			}
			g_object_unref(file);
		}
	}

	g_object_unref(filter);
	g_object_unref(chooser);
}

static void MicrophoneSettingsReportError(GtkWindow *parent)
{
	GtkWidget *message = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
	                                            GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
	                                            "Unable to initialize the selected microphone source.");
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(message),
	                                         "Check the terminal for details.");
	gtk_dialog_run(GTK_DIALOG(message));
	gtk_widget_destroy(message);
}

static void MicrophoneSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons("Microphone Settings",
	                        GTK_WINDOW(pWindow), GTK_DIALOG_MODAL,
	                        "_OK", GTK_RESPONSE_OK,
	                        "_Cancel", GTK_RESPONSE_CANCEL,
	                        NULL);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

	GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_set_border_width(GTK_CONTAINER(content), 12);

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_box_pack_start(GTK_BOX(content), box, TRUE, TRUE, 0);

	GtkWidget *frame = gtk_frame_new("Microphone");
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 0);

	GtkWidget *modes = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width(GTK_CONTAINER(modes), 10);
	gtk_container_add(GTK_CONTAINER(frame), modes);

	GtkWidget *physical = gtk_radio_button_new_with_label(NULL,
		"Use connected physical microphone (not suitable for TAS)");
	GtkWidget *noise = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(physical),
		"Use internal noise sample");
	GtkWidget *random = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(physical),
		"Use rand() white noise (not suitable for TAS)");
	GtkWidget *sample = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(physical),
		"Use microphone samples");
	gtk_box_pack_start(GTK_BOX(modes), physical, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(modes), noise, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(modes), random, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(modes), sample, FALSE, FALSE, 0);

	GtkWidget *sampleBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_start(sampleBox, 24);
	gtk_box_pack_start(GTK_BOX(modes), sampleBox, FALSE, FALSE, 0);
	GtkWidget *sampleLabel = gtk_label_new_with_mnemonic("_Sample:");
	GtkWidget *sampleEntry = gtk_entry_new();
	gtk_widget_set_hexpand(sampleEntry, TRUE);
	gtk_entry_set_text(GTK_ENTRY(sampleEntry), config.mic_sample_file.get().c_str());
	gtk_label_set_mnemonic_widget(GTK_LABEL(sampleLabel), sampleEntry);
	gtk_box_pack_start(GTK_BOX(sampleBox), sampleLabel, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sampleBox), sampleEntry, TRUE, TRUE, 0);
	GtkWidget *browse = gtk_button_new_with_mnemonic("_Browse…");
	gtk_box_pack_start(GTK_BOX(sampleBox), browse, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(browse), "clicked", G_CALLBACK(MicrophoneSampleBrowse), sampleEntry);
	g_signal_connect(G_OBJECT(sample), "toggled", G_CALLBACK(MicrophoneSampleModeToggled), sampleBox);

	GtkWidget *hint = gtk_label_new("The noise and sample modes require the microphone hotkey.");
	gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
	gtk_box_pack_start(GTK_BOX(box), hint, FALSE, FALSE, 0);

	const int mode = CLAMP((int)CommonSettings.micMode, (int)MicMode_InternalNoise, (int)MicMode_Physical);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(physical), mode == MicMode_Physical);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(noise), mode == MicMode_InternalNoise);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(random), mode == MicMode_Random);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sample), mode == MicMode_Sample);
	gtk_widget_set_sensitive(sampleBox, mode == MicMode_Sample);

	gtk_widget_show_all(content);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		MicMode selectedMode = MicMode_InternalNoise;
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(physical)))
			selectedMode = MicMode_Physical;
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(random)))
			selectedMode = MicMode_Random;
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sample)))
			selectedMode = MicMode_Sample;

		CommonSettings.micMode = selectedMode;
		config.mic_mode = selectedMode;
		config.mic_sample_file = gtk_entry_get_text(GTK_ENTRY(sampleEntry));
		if (!Mic_ApplySettings(config.mic_sample_file.get().c_str()))
			MicrophoneSettingsReportError(GTK_WINDOW(dialog));
	}

	gtk_widget_destroy(dialog);
}

static void WifiCompatibilityToggled(GtkToggleButton *button, gpointer data)
{
	gtk_widget_set_sensitive(GTK_WIDGET(data), gtk_toggle_button_get_active(button));
}

static void WifiSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons("Wi-Fi Settings",
	                        GTK_WINDOW(pWindow), GTK_DIALOG_MODAL,
	                        "_OK", GTK_RESPONSE_OK,
	                        "_Cancel", GTK_RESPONSE_CANCEL,
	                        NULL);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid, TRUE, TRUE, 0);

	GtkWidget *enabled = gtk_check_button_new_with_mnemonic("_Enable Wi-Fi emulation");
	gtk_grid_attach(GTK_GRID(grid), enabled, 0, 0, 2, 1);
	GtkWidget *compatibility = gtk_check_button_new_with_mnemonic("_Compatibility mode");
	gtk_grid_attach(GTK_GRID(grid), compatibility, 0, 1, 2, 1);

	GtkWidget *adapterLabel = gtk_label_new_with_mnemonic("_Bridge adapter:");
	gtk_label_set_xalign(GTK_LABEL(adapterLabel), 1.0f);
	GtkWidget *adapter = gtk_combo_box_text_new();
	gtk_widget_set_hexpand(adapter, TRUE);
	gtk_grid_attach(GTK_GRID(grid), adapterLabel, 0, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), adapter, 1, 2, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(adapterLabel), adapter);

	const WifiEmulationLevel emulationLevel = wifiHandler->GetSelectedEmulationLevel();
	const bool wifiEnabled = emulationLevel != WifiEmulationLevel_Off;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enabled), wifiEnabled);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compatibility),
	                             wifiEnabled ? emulationLevel == WifiEmulationLevel_Compatibility : config.wifi_compatibility_mode);
	gtk_widget_set_sensitive(GTK_WIDGET(compatibility), wifiEnabled);
	g_signal_connect(G_OBJECT(enabled), "toggled", G_CALLBACK(WifiCompatibilityToggled), compatibility);

#ifdef EXPERIMENTAL_WIFI_COMM
	gtk_widget_set_sensitive(enabled, TRUE);
	gtk_widget_set_sensitive(compatibility, wifiEnabled);
#else
	gtk_widget_set_sensitive(enabled, FALSE);
	gtk_widget_set_sensitive(compatibility, FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enabled), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compatibility), FALSE);
#endif

	std::vector<std::string> devices;
	int deviceCount = -1;
	const bool pcapSupported = wifiHandler->IsPCapSupported();
	if (pcapSupported)
		deviceCount = wifiHandler->GetBridgeDeviceList(&devices);

	int selectedDevice = config.wifi_bridge_adapter;
	bool adapterAvailable = false;
	if (!pcapSupported)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(adapter), "Error: Could not load libpcap.");
	}
	else if (deviceCount < 0)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(adapter), "Error: Searching for a device failed.");
	}
	else if (deviceCount == 0)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(adapter), "No devices were found.");
	}
	else
	{
		for (const std::string &device : devices)
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(adapter), device.c_str());

		if (selectedDevice < 0 || selectedDevice >= deviceCount)
			selectedDevice = 0;
		gtk_combo_box_set_active(GTK_COMBO_BOX(adapter), selectedDevice);
		adapterAvailable = true;
	}
	gtk_widget_set_sensitive(adapter, adapterAvailable);

	gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		bool shouldReset = false;
		if (gameInfo.fROM != NULL)
		{
			GtkWidget *prompt = gtk_message_dialog_new(GTK_WINDOW(dialog), GTK_DIALOG_MODAL,
			                                           GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
			                                           "The current ROM needs to be reset to apply changes.\nReset now?");
			shouldReset = gtk_dialog_run(GTK_DIALOG(prompt)) == GTK_RESPONSE_YES;
			gtk_widget_destroy(prompt);
		}

#ifdef EXPERIMENTAL_WIFI_COMM
		const bool enableWifi = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enabled));
		const bool compatibilityMode = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(compatibility));
		config.wifi_enabled = enableWifi;
		config.wifi_compatibility_mode = compatibilityMode;
		wifiHandler->SetEmulationLevel(enableWifi
		                               ? (compatibilityMode ? WifiEmulationLevel_Compatibility : WifiEmulationLevel_Normal)
		                               : WifiEmulationLevel_Off);
#else
		config.wifi_enabled = false;
		config.wifi_compatibility_mode = false;
		wifiHandler->SetEmulationLevel(WifiEmulationLevel_Off);
#endif

		const int activeDevice = gtk_combo_box_get_active(GTK_COMBO_BOX(adapter));
		if (adapterAvailable && activeDevice >= 0)
		{
			CommonSettings.WifiBridgeDeviceID = activeDevice;
			config.wifi_bridge_adapter = activeDevice;
			wifiHandler->SetBridgeDeviceIndex(activeDevice);
		}

		if (shouldReset)
			Reset(NULL, NULL, NULL);
	}

	gtk_widget_destroy(dialog);
}

static void SelectAddonPath(GtkButton *button, gpointer user_data)
{
	GtkWidget *entry = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "path-entry"));
	const GtkFileChooserAction chooserAction =
		(GtkFileChooserAction)GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "chooser-action"));
	const char *title = (const char *)g_object_get_data(G_OBJECT(button), "chooser-title");
	GtkWidget *chooser = gtk_file_chooser_dialog_new(title, GTK_WINDOW(pWindow), chooserAction,
	                                                "_Cancel", GTK_RESPONSE_CANCEL,
	                                                "_Select", GTK_RESPONSE_ACCEPT, NULL);
	const char *currentPath = gtk_entry_get_text(GTK_ENTRY(entry));
	if (currentPath[0] != '\0')
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser), currentPath);

	if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT)
	{
		char *selectedPath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
		if (selectedPath != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(entry), selectedPath);
			g_free(selectedPath);
		}
	}
	gtk_widget_destroy(chooser);
}

static GtkWidget *CreateAddonPathRow(GtkGrid *grid, int row, const char *labelText,
                                     const std::string &initialPath, GtkFileChooserAction action,
                                     const char *chooserTitle)
{
	GtkWidget *label = gtk_label_new_with_mnemonic(labelText);
	gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget *entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), initialPath.c_str());
	gtk_widget_set_hexpand(entry, TRUE);
	GtkWidget *browse = gtk_button_new_with_label("Browse…");
	g_object_set_data(G_OBJECT(browse), "path-entry", entry);
	g_object_set_data(G_OBJECT(browse), "chooser-action", GINT_TO_POINTER(action));
	g_object_set_data(G_OBJECT(browse), "chooser-title", (gpointer)chooserTitle);
	g_signal_connect(browse, "clicked", G_CALLBACK(SelectAddonPath), NULL);
	gtk_box_pack_start(GTK_BOX(box), entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), browse, FALSE, FALSE, 0);
	g_object_set_data(G_OBJECT(box), "path-entry", entry);
	gtk_grid_attach(grid, label, 0, row, 1, 1);
	gtk_grid_attach(grid, box, 1, row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	return box;
}

struct Slot1DialogControls
{
	GtkWidget *description;
	GtkWidget *r4Controls;
	GtkWidget *debugControls;
	GtkWidget *typeCombo;
	GtkWidget *useRomPath;
};

static void Slot1RomPathToggled(GtkToggleButton *button, gpointer user_data)
{
	Slot1DialogControls *controls = (Slot1DialogControls *)user_data;
	const bool isR4 = gtk_combo_box_get_active(GTK_COMBO_BOX(controls->typeCombo)) == NDS_SLOT1_R4;
	gtk_widget_set_sensitive(controls->r4Controls,
	                         isR4 && !gtk_toggle_button_get_active(button));
}

static void Slot1TypeChanged(GtkComboBox *combo, gpointer user_data)
{
	Slot1DialogControls *controls = (Slot1DialogControls *)user_data;
	const int type = gtk_combo_box_get_active(combo);
	if (type < 0 || type >= NDS_SLOT1_COUNT)
		return;

	gtk_label_set_text(GTK_LABEL(controls->description), slot1_List[type]->info()->descr());
	gtk_widget_set_sensitive(controls->r4Controls,
	                         type == NDS_SLOT1_R4 &&
	                         !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->useRomPath)));
	gtk_widget_set_sensitive(controls->useRomPath, type == NDS_SLOT1_R4);
	gtk_widget_set_sensitive(controls->debugControls, type == NDS_SLOT1_RETAIL_DEBUG);
}

static void Slot1SettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	slot1_Init();
	GtkWidget *dialog = gtk_dialog_new_with_buttons("Slot 1 Settings", GTK_WINDOW(pWindow),
	                                                GTK_DIALOG_MODAL, "_Cancel", GTK_RESPONSE_CANCEL,
	                                                "_OK", GTK_RESPONSE_OK, NULL);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid, TRUE, TRUE, 0);

	GtkWidget *typeLabel = gtk_label_new_with_mnemonic("_Device:");
	gtk_label_set_xalign(GTK_LABEL(typeLabel), 1.0f);
	GtkWidget *typeCombo = gtk_combo_box_text_new();
	for (int i = 0; i < NDS_SLOT1_COUNT; i++)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(typeCombo), slot1_List[i]->info()->name());
	gtk_grid_attach(GTK_GRID(grid), typeLabel, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), typeCombo, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(typeLabel), typeCombo);

	GtkWidget *description = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(description), 0.0f);
	gtk_label_set_line_wrap(GTK_LABEL(description), TRUE);
	gtk_grid_attach(GTK_GRID(grid), description, 0, 1, 2, 1);

	GtkWidget *r4Controls = CreateAddonPathRow(GTK_GRID(grid), 2, "_FAT directory:",
	                                          config.slot1_fat_path.get(),
	                                          GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
	                                          "Select the R4 FAT directory");
	GtkWidget *useRomPath = gtk_check_button_new_with_mnemonic("Use the loaded _ROM directory");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(useRomPath), config.slot1_fat_path_type);
	gtk_grid_attach(GTK_GRID(grid), useRomPath, 1, 3, 1, 1);
	std::string initialDebugPath = config.slot1_debug_path.get();
	if (initialDebugPath.empty())
		initialDebugPath = path.getpath(path.SLOT1D);
	GtkWidget *debugControls = CreateAddonPathRow(GTK_GRID(grid), 4, "_Debug filesystem:",
	                                              initialDebugPath,
	                                              GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
	                                              "Select the Slot 1 debug filesystem directory");

	int currentType = (int)slot1_GetCurrentType();
	if (currentType < 0 || currentType >= NDS_SLOT1_COUNT)
		currentType = NDS_SLOT1_RETAIL_AUTO;
	gtk_combo_box_set_active(GTK_COMBO_BOX(typeCombo), currentType);
	Slot1DialogControls controls = { description, r4Controls, debugControls, typeCombo, useRomPath };
	g_signal_connect(typeCombo, "changed", G_CALLBACK(Slot1TypeChanged), &controls);
	g_signal_connect(useRomPath, "toggled", G_CALLBACK(Slot1RomPathToggled), &controls);
	Slot1TypeChanged(GTK_COMBO_BOX(typeCombo), &controls);
	gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));

	while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		const int type = gtk_combo_box_get_active(GTK_COMBO_BOX(typeCombo));
		const std::string fatPath = gtk_entry_get_text(GTK_ENTRY(
			g_object_get_data(G_OBJECT(r4Controls), "path-entry")));
		const bool useRomDirectory = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(useRomPath));
		GtkWidget *debugEntry = GTK_WIDGET(g_object_get_data(G_OBJECT(debugControls), "path-entry"));
		const std::string debugPath = gtk_entry_get_text(GTK_ENTRY(debugEntry));

		if (type < 0 || type >= NDS_SLOT1_COUNT ||
		    (type == NDS_SLOT1_R4 && !useRomDirectory && fatPath.empty()) ||
		    (type == NDS_SLOT1_R4 && !useRomDirectory &&
		     !g_file_test(fatPath.c_str(), G_FILE_TEST_IS_DIR)) ||
		    (type == NDS_SLOT1_RETAIL_DEBUG &&
		     (debugPath.empty() || !g_file_test(debugPath.c_str(), G_FILE_TEST_IS_DIR))))
		{
			GtkWidget *error = gtk_message_dialog_new(GTK_WINDOW(dialog), GTK_DIALOG_MODAL,
			                                          GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
			                                          "Choose a directory for the selected Slot 1 device.");
			gtk_dialog_run(GTK_DIALOG(error));
			gtk_widget_destroy(error);
			continue;
		}

		config.slot1_fat_path = fatPath;
		config.slot1_fat_path_type = useRomDirectory;
		config.slot1_debug_path = debugPath;
		slot1_SetFatDir(fatPath, useRomDirectory);
		if (!debugPath.empty())
			path.setpath(path.SLOT1D, debugPath);
		if (!slot1_Change((NDS_SLOT1_TYPE)type))
			slot1_Reset();
		config.slot1_id = slot1_List[type]->info()->id();
		break;
	}
	gtk_widget_destroy(dialog);
}

struct Slot2DialogControls
{
	GtkWidget *description;
	GtkWidget *gbaControls;
	GtkWidget *cflashMode;
	GtkWidget *cflashPathControls;
	GtkWidget *typeCombo;
};

static void Slot2TypeChanged(GtkComboBox *combo, gpointer user_data)
{
	Slot2DialogControls *controls = (Slot2DialogControls *)user_data;
	const int type = gtk_combo_box_get_active(combo);
	if (type < 0 || type >= NDS_SLOT2_COUNT)
		return;

	gtk_label_set_text(GTK_LABEL(controls->description), slot2_List[type]->info()->descr());
	gtk_widget_set_sensitive(controls->gbaControls, type == NDS_SLOT2_GBACART);
	gtk_widget_set_sensitive(controls->cflashMode, type == NDS_SLOT2_CFLASH);
	gtk_widget_set_sensitive(controls->cflashPathControls,
	                         type == NDS_SLOT2_CFLASH &&
	                         gtk_combo_box_get_active(GTK_COMBO_BOX(controls->cflashMode)) !=
	                             ADDON_CFLASH_MODE_RomPath);
}

static void Slot2CFlashModeChanged(GtkComboBox *combo, gpointer user_data)
{
	Slot2DialogControls *controls = (Slot2DialogControls *)user_data;
	const int type = gtk_combo_box_get_active(GTK_COMBO_BOX(controls->typeCombo));
	GList *children = gtk_container_get_children(GTK_CONTAINER(controls->cflashPathControls));
	if (children != NULL && children->next != NULL)
	{
		GtkWidget *browse = GTK_WIDGET(children->next->data);
		if (gtk_combo_box_get_active(combo) == ADDON_CFLASH_MODE_File)
		{
			g_object_set_data(G_OBJECT(browse), "chooser-action",
			                  GINT_TO_POINTER(GTK_FILE_CHOOSER_ACTION_OPEN));
			g_object_set_data(G_OBJECT(browse), "chooser-title", (gpointer)"Select a CompactFlash image file");
		}
		else
		{
			g_object_set_data(G_OBJECT(browse), "chooser-action",
			                  GINT_TO_POINTER(GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER));
			g_object_set_data(G_OBJECT(browse), "chooser-title", (gpointer)"Select a CompactFlash directory");
		}
	}
	g_list_free(children);
	gtk_widget_set_sensitive(controls->cflashPathControls,
	                         type == NDS_SLOT2_CFLASH &&
	                         gtk_combo_box_get_active(combo) != ADDON_CFLASH_MODE_RomPath);
}

static void Slot2SettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	slot2_Init();
	GtkWidget *dialog = gtk_dialog_new_with_buttons("Slot 2 Settings", GTK_WINDOW(pWindow),
	                                                GTK_DIALOG_MODAL, "_Cancel", GTK_RESPONSE_CANCEL,
	                                                "_OK", GTK_RESPONSE_OK, NULL);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid, TRUE, TRUE, 0);

	GtkWidget *typeLabel = gtk_label_new_with_mnemonic("_Device:");
	gtk_label_set_xalign(GTK_LABEL(typeLabel), 1.0f);
	GtkWidget *typeCombo = gtk_combo_box_text_new();
	for (int i = 0; i < NDS_SLOT2_COUNT; i++)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(typeCombo), slot2_List[i]->info()->name());
	gtk_grid_attach(GTK_GRID(grid), typeLabel, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), typeCombo, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(typeLabel), typeCombo);

	GtkWidget *description = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(description), 0.0f);
	gtk_label_set_line_wrap(GTK_LABEL(description), TRUE);
	gtk_grid_attach(GTK_GRID(grid), description, 0, 1, 2, 1);

	GtkWidget *gbaControls = CreateAddonPathRow(GTK_GRID(grid), 2, "_GBA cartridge ROM:",
	                                            config.slot2_gba_rom.get(),
	                                            GTK_FILE_CHOOSER_ACTION_OPEN,
	                                            "Select a GBA cartridge ROM");
	GtkWidget *cflashModeLabel = gtk_label_new_with_mnemonic("_CompactFlash mode:");
	gtk_label_set_xalign(GTK_LABEL(cflashModeLabel), 1.0f);
	GtkWidget *cflashMode = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cflashMode), "Directory");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cflashMode), "Image file");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cflashMode), "Loaded ROM directory");
	int savedCFlashMode = CLAMP((int)config.slot2_cflash_mode, ADDON_CFLASH_MODE_Path,
	                            ADDON_CFLASH_MODE_RomPath);
	gtk_combo_box_set_active(GTK_COMBO_BOX(cflashMode), savedCFlashMode);
	gtk_grid_attach(GTK_GRID(grid), cflashModeLabel, 0, 3, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), cflashMode, 1, 3, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(cflashModeLabel), cflashMode);
	GtkWidget *cflashPathControls = CreateAddonPathRow(GTK_GRID(grid), 4, "_CompactFlash path:",
	                                                   config.slot2_cflash_path.get(),
	                                                   GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
	                                                   "Select a CompactFlash directory or image");

	int currentType = (int)slot2_GetCurrentType();
	if (currentType < 0 || currentType >= NDS_SLOT2_COUNT)
		currentType = NDS_SLOT2_AUTO;
	gtk_combo_box_set_active(GTK_COMBO_BOX(typeCombo), currentType);
	Slot2DialogControls controls = { description, gbaControls, cflashMode, cflashPathControls,
	                                 typeCombo };
	g_signal_connect(typeCombo, "changed", G_CALLBACK(Slot2TypeChanged), &controls);
	g_signal_connect(cflashMode, "changed", G_CALLBACK(Slot2CFlashModeChanged), &controls);
	Slot2TypeChanged(GTK_COMBO_BOX(typeCombo), &controls);
	Slot2CFlashModeChanged(GTK_COMBO_BOX(cflashMode), &controls);
	gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	Slot2TypeChanged(GTK_COMBO_BOX(typeCombo), &controls);
	Slot2CFlashModeChanged(GTK_COMBO_BOX(cflashMode), &controls);

	while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		const int type = gtk_combo_box_get_active(GTK_COMBO_BOX(typeCombo));
		GtkWidget *gbaEntry = GTK_WIDGET(g_object_get_data(G_OBJECT(gbaControls), "path-entry"));
		const std::string gbaRom = gtk_entry_get_text(GTK_ENTRY(gbaEntry));
		GtkWidget *cflashEntry = GTK_WIDGET(g_object_get_data(G_OBJECT(cflashPathControls), "path-entry"));
		const std::string cflashPath = gtk_entry_get_text(GTK_ENTRY(cflashEntry));
		const int cflashModeValue = gtk_combo_box_get_active(GTK_COMBO_BOX(cflashMode));

		if (type < 0 || type >= NDS_SLOT2_COUNT ||
		    (type == NDS_SLOT2_GBACART && gbaRom.empty()) ||
		    (type == NDS_SLOT2_CFLASH && cflashModeValue != ADDON_CFLASH_MODE_RomPath && cflashPath.empty()))
		{
			GtkWidget *error = gtk_message_dialog_new(GTK_WINDOW(dialog), GTK_DIALOG_MODAL,
			                                          GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
			                                          "Choose a path for the selected Slot 2 device.");
			gtk_dialog_run(GTK_DIALOG(error));
			gtk_widget_destroy(error);
			continue;
		}
		if (type == NDS_SLOT2_GBACART)
		{
			FILE *test = fopen(gbaRom.c_str(), "rb");
			if (test == NULL)
			{
				GtkWidget *error = gtk_message_dialog_new(GTK_WINDOW(dialog), GTK_DIALOG_MODAL,
				                                          GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				                                          "The selected GBA ROM cannot be opened.");
				gtk_dialog_run(GTK_DIALOG(error));
				gtk_widget_destroy(error);
				continue;
			}
			fclose(test);
		}
		if (type == NDS_SLOT2_CFLASH && cflashModeValue == ADDON_CFLASH_MODE_Path &&
		    !g_file_test(cflashPath.c_str(), G_FILE_TEST_IS_DIR))
		{
			GtkWidget *error = gtk_message_dialog_new(GTK_WINDOW(dialog), GTK_DIALOG_MODAL,
			                                          GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
			                                          "The selected CompactFlash directory does not exist.");
			gtk_dialog_run(GTK_DIALOG(error));
			gtk_widget_destroy(error);
			continue;
		}
		if (type == NDS_SLOT2_CFLASH && cflashModeValue == ADDON_CFLASH_MODE_File)
		{
			FILE *image = fopen(cflashPath.c_str(), "rb+");
			if (image == NULL)
			{
				GtkWidget *error = gtk_message_dialog_new(GTK_WINDOW(dialog), GTK_DIALOG_MODAL,
				                                          GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				                                          "The CompactFlash image cannot be opened for writing.");
				gtk_dialog_run(GTK_DIALOG(error));
				gtk_widget_destroy(error);
				continue;
			}
			fclose(image);
		}

		config.slot2_gba_rom = gbaRom;
		config.slot2_cflash_mode = cflashModeValue;
		config.slot2_cflash_path = cflashPath;
		if (type == NDS_SLOT2_GBACART)
		{
			GBACartridge_RomPath = gbaRom;
			const size_t extension = gbaRom.find_last_of('.');
			const size_t separator = gbaRom.find_last_of("/\\");
			GBACartridge_SRAMPath = extension == std::string::npos ||
			                        (separator != std::string::npos && extension < separator)
			                        ? gbaRom + ".sav" : gbaRom.substr(0, extension) + ".sav";
		}
		if (type == NDS_SLOT2_CFLASH)
		{
			CFlash_Mode = (ADDON_CFLASH_MODE)cflashModeValue;
			CFlash_Path = cflashPath;
		}

		const NDS_SLOT2_TYPE selectedType = (NDS_SLOT2_TYPE)type;
		const bool changed = slot2_Change(selectedType);
		if (!changed)
			slot2_Reset();
		config.slot2_id = slot2_List[type]->info()->id();
		break;
	}
	gtk_widget_destroy(dialog);
}

static void AddRomInfoRow(GtkGrid *grid, int row, const char *title, const std::string &value)
{
	GtkWidget *titleLabel = gtk_label_new(title);
	gtk_label_set_xalign(GTK_LABEL(titleLabel), 1.0f);
	GtkWidget *valueLabel = gtk_label_new(value.c_str());
	gtk_label_set_xalign(GTK_LABEL(valueLabel), 0.0f);
	gtk_label_set_selectable(GTK_LABEL(valueLabel), TRUE);
	gtk_grid_attach(grid, titleLabel, 0, row, 1, 1);
	gtk_grid_attach(grid, valueLabel, 1, row, 1, 1);
}

static void RomInfoDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	if (gameInfo.fROM == NULL)
		return;

	GtkWidget *dialog = gtk_dialog_new_with_buttons("ROM Info", GTK_WINDOW(pWindow),
	                                                GTK_DIALOG_MODAL, "_Close", GTK_RESPONSE_CLOSE, NULL);
	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid, TRUE, TRUE, 0);

	AddRomInfoRow(GTK_GRID(grid), 0, "Internal title:", gameInfo.ROMname);
	AddRomInfoRow(GTK_GRID(grid), 1, "Game code:", gameInfo.ROMserial);
	char value[128];
	const u16 makerCode = T1ReadWord((u8 *)&gameInfo.header, 0x10);
	std::snprintf(value, sizeof(value), "%c%c (%s)",
	              ((u8 *)&gameInfo.header)[0x10], ((u8 *)&gameInfo.header)[0x11],
	              Database::MakerNameForMakerCode(makerCode, true));
	AddRomInfoRow(GTK_GRID(grid), 2, "Maker:", value);
	if (gameInfo.header.cardSize < 48)
		std::snprintf(value, sizeof(value), "%llu KiB",
		              (unsigned long long)(0x80ull << gameInfo.header.cardSize));
	else
		std::snprintf(value, sizeof(value), "Unknown");
	AddRomInfoRow(GTK_GRID(grid), 3, "Cartridge capacity:", value);
	std::snprintf(value, sizeof(value), "%u bytes", gameInfo.romsize);
	AddRomInfoRow(GTK_GRID(grid), 4, "Loaded ROM size:", value);
	std::snprintf(value, sizeof(value), "0x%08X", gameInfo.crc);
	AddRomInfoRow(GTK_GRID(grid), 5, "CRC32:", value);
	const int saveType = CommonSettings.manualBackupType;
	AddRomInfoRow(GTK_GRID(grid), 6, "Save type:",
	              saveType >= 0 && saveType <= 6
	                  ? save_types[saveType].descr : "Unknown");
#ifdef HAVE_JIT
	AddRomInfoRow(GTK_GRID(grid), 7, "CPU engine:", CommonSettings.use_jit ? "JIT recompiler" : "Interpreter");
#else
	AddRomInfoRow(GTK_GRID(grid), 7, "CPU engine:", "Interpreter");
#endif
	const int renderer = config.core3D;
	if (renderer >= 0 && renderer < GPU3D_MAX_COUNT && core3DList[renderer] != NULL)
		AddRomInfoRow(GTK_GRID(grid), 8, "3D engine:", core3DList[renderer]->name);

	gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

/////////////////////////////// FIRMWARE SETTINGS //////////////////////////////////////

// The firmware stores text as UTF-16 code units. GLib's gunichar2 is exactly
// that width, so use its converters instead of going through wchar_t, which is
// 32 bits here and pairs adjacent units into bogus code points.
static char *FirmwareTextToUTF8(const u16 *text, size_t length)
{
	return g_utf16_to_utf8((const gunichar2 *)text, (glong)length, NULL, NULL, NULL);
}

static void SetFirmwareTextFromUTF8(const char *utf8, u16 *text, u8 *textLength, size_t maxUnits)
{
	glong unitCount = 0;
	gunichar2 *units = g_utf8_to_utf16(utf8 != NULL ? utf8 : "", -1, NULL, &unitCount, NULL);
	size_t length = (units != NULL && unitCount > 0) ? (size_t)unitCount : 0;

	if (length > maxUnits)
	{
		length = maxUnits;
		// Never keep half of a surrogate pair.
		if (units[length - 1] >= 0xD800 && units[length - 1] <= 0xDBFF)
			length--;
	}

	for (size_t i = 0; i < length; i++)
		text[i] = units[i];
	text[length] = 0;
	*textLength = (u8)length;

	g_free(units);
}

static std::string NormalizeMACAddress(const char *text)
{
	std::string digits;
	for (const char *p = text; p != NULL && *p != '\0' && digits.size() < 12; p++)
	{
		if (g_ascii_isxdigit(*p))
			digits += *p;
	}
	return digits;
}

// Apply the persisted [Firmware] settings to the core configuration. This runs
// after config.load() and before the command-line --lang handling, so --lang
// still wins.
static void ApplyFirmwareSettingsConfig()
{
	FirmwareConfig &fw = CommonSettings.fwConfig;

	SetFirmwareTextFromUTF8(config.fwconfig_nickname.get().c_str(), fw.nickname, &fw.nicknameLength, MAX_FW_NICKNAME_LENGTH);
	SetFirmwareTextFromUTF8(config.fwconfig_message.get().c_str(), fw.message, &fw.messageLength, MAX_FW_MESSAGE_LENGTH);
	fw.favoriteColor = (u8)CLAMP(config.fwconfig_favorite_color, 0, 15);
	fw.birthdayMonth = (u8)CLAMP(config.fwconfig_birth_month, 1, 12);
	fw.birthdayDay = (u8)CLAMP(config.fwconfig_birth_day, 1, 31);
	fw.language = (u8)CLAMP(config.firmware_language, 0, 5);
	NDS_SetFirmwareMACAddressFromStr(fw, config.fwconfig_mac_address.get().c_str());
}

static void ApplyWifiSettingsConfig()
{
	CommonSettings.WifiBridgeDeviceID = config.wifi_bridge_adapter;
	wifiHandler->SetBridgeDeviceIndex(CommonSettings.WifiBridgeDeviceID);

#ifdef EXPERIMENTAL_WIFI_COMM
	if (config.wifi_enabled)
		wifiHandler->SetEmulationLevel(config.wifi_compatibility_mode
		                               ? WifiEmulationLevel_Compatibility
		                               : WifiEmulationLevel_Normal);
	else
		wifiHandler->SetEmulationLevel(WifiEmulationLevel_Off);
#else
	wifiHandler->SetEmulationLevel(WifiEmulationLevel_Off);
#endif
}

static void ApplyMicrophoneSettingsConfig()
{
	const int mode = CLAMP((int)config.mic_mode, (int)MicMode_InternalNoise, (int)MicMode_Physical);
	CommonSettings.micMode = (MicMode)mode;
	if (!Mic_ApplySettings(config.mic_sample_file.get().c_str()))
		fprintf(stderr, "Unable to apply the configured microphone source.\n");
}

static GtkWidget *FirmwareCombo(const char *const *items, int count, int active)
{
	GtkWidget *combo = gtk_combo_box_text_new();
	for (int i = 0; i < count; i++)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), items[i]);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
	return combo;
}

static void AttachFirmwareRow(GtkGrid *grid, int row, const char *labelText, GtkWidget *widget)
{
	GtkWidget *label = gtk_label_new_with_mnemonic(labelText);
	gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
	gtk_widget_set_hexpand(widget, TRUE);
	gtk_grid_attach(grid, label, 0, row, 1, 1);
	gtk_grid_attach(grid, widget, 1, row, 1, 1);
}

static void FirmwareSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	static const char *const languageNames[] = {"Japanese", "English", "French", "German", "Italian", "Spanish"};
	static const char *const colorNames[] = {
		"Gray", "Brown", "Red", "Pink", "Orange", "Yellow", "Lime Green", "Green",
		"Dark Green", "Sea Green", "Turquoise", "Blue", "Dark Blue", "Dark Purple",
		"Violet", "Magenta"
	};
	static const char *const monthNames[] = {
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"
	};

	FirmwareConfig &fw = CommonSettings.fwConfig;
	char macBuffer[13];
	NDS_GetFirmwareMACAddressAsStr(fw, macBuffer);

	GtkWidget *dialog = gtk_dialog_new_with_buttons("Firmware Settings",
	                        GTK_WINDOW(pWindow), GTK_DIALOG_MODAL,
	                        "_OK", GTK_RESPONSE_OK,
	                        "_Cancel", GTK_RESPONSE_CANCEL,
	                        NULL);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid, TRUE, TRUE, 0);

	char *nickname = FirmwareTextToUTF8(fw.nickname, fw.nicknameLength);
	char *message = FirmwareTextToUTF8(fw.message, fw.messageLength);

	GtkWidget *nicknameEntry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(nicknameEntry), MAX_FW_NICKNAME_LENGTH);
	gtk_entry_set_text(GTK_ENTRY(nicknameEntry), nickname != NULL ? nickname : "");
	AttachFirmwareRow(GTK_GRID(grid), 0, "_Nickname:", nicknameEntry);

	GtkWidget *messageEntry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(messageEntry), MAX_FW_MESSAGE_LENGTH);
	gtk_entry_set_text(GTK_ENTRY(messageEntry), message != NULL ? message : "");
	AttachFirmwareRow(GTK_GRID(grid), 1, "_Message:", messageEntry);

	GtkWidget *colorCombo = FirmwareCombo(colorNames, (int)G_N_ELEMENTS(colorNames), CLAMP((int)fw.favoriteColor, 0, 15));
	AttachFirmwareRow(GTK_GRID(grid), 2, "Favourite _color:", colorCombo);

	GtkWidget *monthCombo = FirmwareCombo(monthNames, (int)G_N_ELEMENTS(monthNames), CLAMP((int)fw.birthdayMonth - 1, 0, 11));
	AttachFirmwareRow(GTK_GRID(grid), 3, "Birth _month:", monthCombo);

	GtkWidget *dayCombo = gtk_combo_box_text_new();
	for (int day = 1; day <= 31; day++)
	{
		char text[4];
		snprintf(text, sizeof(text), "%d", day);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dayCombo), text);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(dayCombo), CLAMP((int)fw.birthdayDay - 1, 0, 30));
	AttachFirmwareRow(GTK_GRID(grid), 4, "Birth_day:", dayCombo);

	GtkWidget *languageCombo = FirmwareCombo(languageNames, (int)G_N_ELEMENTS(languageNames), CLAMP((int)fw.language, 0, 5));
	AttachFirmwareRow(GTK_GRID(grid), 5, "_Language:", languageCombo);

	GtkWidget *macEntry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(macEntry), 12);
	gtk_entry_set_text(GTK_ENTRY(macEntry), macBuffer);
	gtk_entry_set_placeholder_text(GTK_ENTRY(macEntry), "12 hex digits");
	AttachFirmwareRow(GTK_GRID(grid), 6, "MA_C address:", macEntry);

	gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		const char *nicknameText = gtk_entry_get_text(GTK_ENTRY(nicknameEntry));
		if (nicknameText[0] == '\0')
			nicknameText = "DeSmuME";
		SetFirmwareTextFromUTF8(nicknameText, fw.nickname, &fw.nicknameLength, MAX_FW_NICKNAME_LENGTH);
		config.fwconfig_nickname = nicknameText;

		const char *messageText = gtk_entry_get_text(GTK_ENTRY(messageEntry));
		SetFirmwareTextFromUTF8(messageText, fw.message, &fw.messageLength, MAX_FW_MESSAGE_LENGTH);
		config.fwconfig_message = messageText;

		fw.favoriteColor = (u8)gtk_combo_box_get_active(GTK_COMBO_BOX(colorCombo));
		config.fwconfig_favorite_color = fw.favoriteColor;

		fw.birthdayMonth = (u8)(gtk_combo_box_get_active(GTK_COMBO_BOX(monthCombo)) + 1);
		config.fwconfig_birth_month = fw.birthdayMonth;

		fw.birthdayDay = (u8)(gtk_combo_box_get_active(GTK_COMBO_BOX(dayCombo)) + 1);
		config.fwconfig_birth_day = fw.birthdayDay;

		fw.language = (u8)gtk_combo_box_get_active(GTK_COMBO_BOX(languageCombo));
		config.firmware_language = fw.language;

		std::string mac = NormalizeMACAddress(gtk_entry_get_text(GTK_ENTRY(macEntry)));
		NDS_SetFirmwareMACAddressFromStr(fw, mac.c_str());
		NDS_GetFirmwareMACAddressAsStr(fw, macBuffer);
		config.fwconfig_mac_address = macBuffer;
	}

	g_free(nickname);
	g_free(message);
	gtk_widget_destroy(dialog);
}

/////////////////////////////// CONTROLS EDIT //////////////////////////////////////

/* Pause/Resume has no single menu entry (Run and Pause are separate actions
   whose enabled state flips), so the hotkey table needs an action of its own. */
static void TogglePause(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    if (gameInfo.fROM == NULL)
        return;

    GAction *run = g_action_map_lookup_action(G_ACTION_MAP(pApp), "run");
    if (run != NULL && g_action_get_enabled(run))
        Launch(NULL, NULL, NULL);
    else
        Pause(NULL, NULL, NULL);
}

/* Menu shortcuts that can be rebound.  The strings are detailed action names,
   so the layout entries can carry their target.  These deliberately live here
   instead of as "accel" attributes in menu.ui: an accel attribute is what
   gtk_application_set_menubar() installs *and* what the menu displays, so a
   rebound key would neither take effect nor show up.  Setting the accelerator
   on the action instead updates both. */
struct hotkey_def {
    const char *slug;
    const char *label;
    const char *action;
    const char *def_accel;  /* NULL when the action starts unbound */
};

static const hotkey_def hotkey_defs[] = {
    { "open",             "Open ROM",              "app.open",                   "<Primary>o" },
    { "pause",            "Pause / Resume",        "app.toggle-pause",           "Pause" },
    { "reset",            "Reset",                 "app.reset",                  NULL },
    { "frame-advance",    "Frame Advance",         "app.frame-advance",          "<Primary>n" },
    { "screenshot",       "Take a Screenshot",     "app.printscreen",            "<Primary>s" },
    { "fullscreen",       "Toggle Fullscreen",     "app.fullscreen",             "F11" },
    { "swap-screens",     "Swap Screens",          "app.swapscreens",            "space" },
    { "view-menu",        "Toggle Menu Bar",       "app.view-menu",              NULL },

    { "fps-limiter",      "Toggle Speed Limiter",  "app.enablefpslimiter",       NULL },
    { "audio",            "Toggle Audio",          "app.enableaudio",            NULL },
    { "view-statusbar",   "Toggle Status Bar",     "app.view-statusbar",         NULL },
    { "view-toolbar",     "Toggle Toolbar",        "app.view-toolbar",           NULL },
    { "layout-vertical",  "Layout: Vertical",      "app.orient('vertical')",     "<Primary>1" },
    { "layout-horizontal","Layout: Horizontal",    "app.orient('horizontal')",   "<Primary>2" },
    { "layout-single",    "Layout: Single Screen", "app.orient('single')",       "<Primary>0" },
    { "layout-hybrid",    "Layout: Hybrid",        "app.orient('hybrid_equal')", "<Primary>3" },
};

#define HOTKEY_COUNT ((int)(sizeof(hotkey_defs) / sizeof(hotkey_defs[0])))

/* Accelerator strings as last read from (or written to) the config file.  An
   empty string means "no shortcut"; NULL only until HotkeysLoad() has run. */
static gchar *sHotkeyAccel[HOTKEY_COUNT];

static void ParseAccelString(const char *accel, guint *keyval, GdkModifierType *mods)
{
    *keyval = 0;
    *mods = (GdkModifierType)0;

    if (accel == NULL || accel[0] == '\0')
        return;

    gtk_accelerator_parse(accel, keyval, mods);
    if (*keyval == 0)
        *mods = (GdkModifierType)0;
}

static void HotkeysLoad(void)
{
    for (int i = 0; i < HOTKEY_COUNT; i++) {
        gchar *value = NULL;

        if (keyfile != NULL)
            value = g_key_file_get_string(keyfile, "HOTKEYS", hotkey_defs[i].slug, NULL);
        if (value == NULL)
            value = g_strdup(hotkey_defs[i].def_accel != NULL ? hotkey_defs[i].def_accel : "");

        g_free(sHotkeyAccel[i]);
        sHotkeyAccel[i] = value;
    }
}

static void HotkeysSave(void)
{
    if (keyfile == NULL)
        return;

    for (int i = 0; i < HOTKEY_COUNT; i++) {
        g_key_file_set_string(keyfile, "HOTKEYS", hotkey_defs[i].slug,
                              sHotkeyAccel[i] != NULL ? sHotkeyAccel[i] : "");
    }

    desmume_config_flush(keyfile);
}

/* Install the configured shortcuts.  Must run after gtk_application_set_menubar(),
   which installs the accelerators it finds in the model. */
static void HotkeysApply(void)
{
    if (pApp == NULL)
        return;

    static const gchar *none[] = { NULL };

    for (int i = 0; i < HOTKEY_COUNT; i++) {
        const gchar *accel = sHotkeyAccel[i];

        if (accel != NULL && accel[0] != '\0') {
            const gchar *list[] = { accel, NULL };
            gtk_application_set_accels_for_action(pApp, hotkey_defs[i].action, list);
        } else {
            gtk_application_set_accels_for_action(pApp, hotkey_defs[i].action, none);
        }
    }
}

/* Which key tables a dialog owns (live) and which ones it only has to check
   its keys against (taken from the saved configuration). */
enum key_group {
    KEYGROUP_CONTROL = 1 << 0,
    KEYGROUP_TURBO   = 1 << 1,
    KEYGROUP_HOTKEY  = 1 << 2,
};

enum key_slot_kind {
    KEY_SLOT_CONTROL,
    KEY_SLOT_TURBO,
    KEY_SLOT_HOTKEY,
};

struct key_slot {
    GtkWidget *button;
    GtkWidget *label;
    guint keyval;
    GdkModifierType mods;
    key_slot_kind kind;
    int index;      /* key_names index, or hotkey_defs index for hotkeys */
};

struct key_dialog {
    GtkWidget *dialog;
    GtkWidget *hint;            /* transient message line, may be NULL */
    std::vector<key_slot> slots;
    int capturing;              /* index into slots, -1 when idle */
    guint live_groups;
    guint ext_groups;
};

struct key_binding {
    guint keyval;
    GdkModifierType mods;
};

/* Grid order for the control dialog, Windows-style: D-pad and face buttons on
   the left, Start/Select/shoulders plus the frontend extras on the right. */
static const int control_grid[2][8] = {
    {  6, 5,  7,  4, 1, 0, 11, 10 },   /* Up, Left, Down, Right, B, A, Y, X */
    {  3, 2,  9,  8, 12, 13, 14, -1 }, /* Start, Select, L, R, Debug, Boost, Lid */
};

/* Turbo keys are offered for the buttons a player would auto-fire. */
static const int turbo_grid[2][6] = {
    { 0, 1, 10, 11, 9, 8 },   /* A, B, X, Y, L, R */
    { 6, 7,  5,  4, 3, 2 },   /* Up, Down, Left, Right, Start, Select */
};

static void KeyDialogSetHint(key_dialog *state, const char *text)
{
    if (state->hint != NULL)
        gtk_label_set_text(GTK_LABEL(state->hint), text);
}

/* Readable name of a bare key symbol, e.g. "Backspace" or "Up".  Returns a
   pointer to a static buffer for the fallback path. */
static const char *KeySymbolLabel(guint keyval)
{
    static char buffer[64];
    static const struct { const char *symbol; const char *text; } renames[] = {
        { "BackSpace", "Backspace" }, { "Return", "Enter" }, { "Escape", "Esc" },
        { "space", "Space" }, { "Print", "Print Screen" },
        { "Scroll_Lock", "Scroll Lock" }, { "Caps_Lock", "Caps Lock" },
        { "Num_Lock", "Num Lock" }, { "Page_Up", "Page Up" }, { "Page_Down", "Page Down" },
    };

    const char *name = gdk_keyval_name(keyval);
    if (name == NULL) {
        g_snprintf(buffer, sizeof(buffer), "0x%04X", keyval);
        return buffer;
    }

    for (size_t i = 0; i < sizeof(renames) / sizeof(renames[0]); i++) {
        if (strcmp(name, renames[i].symbol) == 0)
            return renames[i].text;
    }

    size_t n = 0;
    for (const char *p = name; *p != '\0' && n < sizeof(buffer) - 1; p++)
        buffer[n++] = (*p == '_') ? ' ' : *p;
    buffer[n] = '\0';
    return buffer;
}

static bool KeyIsModifierKey(guint keyval)
{
    switch (keyval) {
    case GDK_KEY_Shift_L: case GDK_KEY_Shift_R:
    case GDK_KEY_Control_L: case GDK_KEY_Control_R:
    case GDK_KEY_Alt_L: case GDK_KEY_Alt_R:
    case GDK_KEY_Meta_L: case GDK_KEY_Meta_R:
    case GDK_KEY_Super_L: case GDK_KEY_Super_R:
    case GDK_KEY_Hyper_L: case GDK_KEY_Hyper_R:
    case GDK_KEY_Caps_Lock: case GDK_KEY_Num_Lock: case GDK_KEY_Shift_Lock:
    case GDK_KEY_ISO_Level3_Shift: case GDK_KEY_ISO_Level5_Shift:
        return true;
    default:
        return false;
    }
}

static void EnsureKeyDialogStyle(void)
{
    static bool done = false;
    if (done)
        return;
    done = true;

    GtkCssProvider *provider = gtk_css_provider_new();
    /* Conflicts are drawn in red on a faint red fill so they stay readable on
       both light and dark themes. */
    gtk_css_provider_load_from_data(provider,
        "button.keyfield.conflict, button.keyfield.conflict label { color: #e01b24; }\n"
        "button.keyfield.conflict { background-image: none; background-color: alpha(#e01b24, 0.16);"
        " border-color: alpha(#e01b24, 0.70); }\n"
        "button.keyfield.capturing { background-image: none; background-color: alpha(#3584e4, 0.20); }\n"
        "label.keyhint { font-size: 92%; }\n",
        -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void KeySlotRefresh(key_dialog *state, int index)
{
    key_slot *slot = &state->slots[index];
    gchar *text;

    if (state->capturing == index)
        text = g_strdup("Press a key...");
    else if (slot->keyval == 0)
        text = g_strdup("Disabled");
    else if (slot->kind == KEY_SLOT_HOTKEY)
        text = gtk_accelerator_get_label(slot->keyval, slot->mods);
    else
        text = g_strdup(KeySymbolLabel(slot->keyval));

    gtk_label_set_text(GTK_LABEL(slot->label), text);
    g_free(text);
}

/* Bindings of the groups this dialog does not own, so an assignment that
   clashes with the other dialog is flagged as well. */
static void KeyDialogCollectExternal(const key_dialog *state, std::vector<key_binding> &out)
{
    out.clear();

    if (state->ext_groups & KEYGROUP_CONTROL) {
        for (int i = 0; i < NB_KEYS; i++) {
            if (keyboard_cfg[i] == 0)
                continue;
            key_binding binding = { keyboard_cfg[i], (GdkModifierType)0 };
            out.push_back(binding);
        }
    }

    if (state->ext_groups & KEYGROUP_TURBO) {
        for (int i = 0; i < NB_KEYS; i++) {
            if (turbo_keys[i] == 0)
                continue;
            key_binding binding = { turbo_keys[i], (GdkModifierType)0 };
            out.push_back(binding);
        }
    }

    if (state->ext_groups & KEYGROUP_HOTKEY) {
        for (int i = 0; i < HOTKEY_COUNT; i++) {
            guint keyval;
            GdkModifierType mods;
            ParseAccelString(sHotkeyAccel[i], &keyval, &mods);
            if (keyval == 0)
                continue;
            key_binding binding = { keyval, mods };
            out.push_back(binding);
        }
    }
}

/* Two entries clash when they use the same key with the same modifiers.  A
   modified hotkey (Ctrl+X) does not clash with a bare game button (X): the
   accelerator consumes the key before the emulator sees it. */
static void KeyDialogUpdateConflicts(key_dialog *state)
{
    std::vector<key_binding> external;
    KeyDialogCollectExternal(state, external);

    for (size_t i = 0; i < state->slots.size(); i++) {
        const key_slot &slot = state->slots[i];
        bool conflict = false;

        if (slot.keyval != 0) {
            for (size_t j = 0; j < external.size() && !conflict; j++)
                conflict = (external[j].keyval == slot.keyval && external[j].mods == slot.mods);

            for (size_t j = 0; j < state->slots.size() && !conflict; j++) {
                if (j == i)
                    continue;
                const key_slot &other = state->slots[j];
                conflict = (other.keyval == slot.keyval && other.mods == slot.mods);
            }
        }

        GtkStyleContext *context = gtk_widget_get_style_context(slot.button);
        if (conflict)
            gtk_style_context_add_class(context, "conflict");
        else
            gtk_style_context_remove_class(context, "conflict");
    }
}

static void KeyDialogEndCapture(key_dialog *state)
{
    const int index = state->capturing;
    state->capturing = -1;

    if (index >= 0) {
        gtk_style_context_remove_class(gtk_widget_get_style_context(state->slots[index].button), "capturing");
        KeySlotRefresh(state, index);
        gtk_window_set_focus(GTK_WINDOW(state->dialog), state->slots[index].button);
    }

    KeyDialogUpdateConflicts(state);
}

static void KeyDialogStartCapture(key_dialog *state, int index)
{
    if (state->capturing == index)
        return;

    state->capturing = index;
    for (size_t i = 0; i < state->slots.size(); i++)
        KeySlotRefresh(state, (int)i);
    gtk_style_context_add_class(gtk_widget_get_style_context(state->slots[index].button), "capturing");

    /* Move the focus off the buttons: otherwise Space or Return would activate
       the focused button instead of being captured. */
    gtk_window_set_focus(GTK_WINDOW(state->dialog), NULL);
    KeyDialogSetHint(state, "Press the key to use, or Escape to disable it.");
}

static void KeySlotClicked(GtkWidget *widget, gpointer data)
{
    key_dialog *state = (key_dialog *)data;
    const gint index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "key-slot-index"));

    KeyDialogStartCapture(state, index);
}

static gboolean KeyDialogKeyPress(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    key_dialog *state = (key_dialog *)data;

    if (state->capturing < 0)
        return FALSE;

    key_slot *slot = &state->slots[state->capturing];
    const GdkModifierType mods = (GdkModifierType)(event->state & gtk_accelerator_get_default_mod_mask());

    if (event->keyval == GDK_KEY_Escape && mods == 0) {
        slot->keyval = 0;
        slot->mods = (GdkModifierType)0;
        KeyDialogEndCapture(state);
        return TRUE;
    }

    if (event->keyval == 0 || KeyIsModifierKey(event->keyval))
        return TRUE;

    if (slot->kind != KEY_SLOT_HOTKEY && mods != 0) {
        KeyDialogSetHint(state, "Game and turbo buttons are single keys: press it without Shift/Ctrl/Alt.");
        return TRUE;
    }

    slot->keyval = event->keyval;
    slot->mods = (slot->kind == KEY_SLOT_HOTKEY) ? mods : (GdkModifierType)0;
    KeyDialogEndCapture(state);
    return TRUE;
}

static void KeyDialogAttachSlot(key_dialog *state, GtkGrid *grid, int row, int col,
                                const char *labelText, key_slot_kind kind, int index,
                                guint keyval, GdkModifierType mods)
{
    GtkWidget *label = gtk_label_new(labelText);
    gtk_label_set_xalign(GTK_LABEL(label), 1.0);
    gtk_grid_attach(grid, label, col, row, 1, 1);

    GtkWidget *button = gtk_button_new();
    GtkWidget *text = gtk_label_new(NULL);
    gtk_container_add(GTK_CONTAINER(button), text);
    gtk_widget_set_size_request(button, 150, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(button), "keyfield");
    gtk_grid_attach(grid, button, col + 1, row, 1, 1);

    key_slot slot;
    slot.button = button;
    slot.label = text;
    slot.keyval = keyval;
    slot.mods = mods;
    slot.kind = kind;
    slot.index = index;
    state->slots.push_back(slot);

    g_object_set_data(G_OBJECT(button), "key-slot-index", GINT_TO_POINTER((gint)(state->slots.size() - 1)));
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(KeySlotClicked), state);

    KeySlotRefresh(state, (int)(state->slots.size() - 1));
}

static void KeyDialogInit(key_dialog *state, const char *title, guint live_groups, guint ext_groups)
{
    EnsureKeyDialogStyle();

    state->dialog = gtk_dialog_new_with_buttons(title, GTK_WINDOW(pWindow), GTK_DIALOG_MODAL,
                                                "_Cancel", GTK_RESPONSE_CANCEL,
                                                "_OK", GTK_RESPONSE_OK,
                                                NULL);
    state->hint = NULL;
    state->capturing = -1;
    state->live_groups = live_groups;
    state->ext_groups = ext_groups;

    gtk_widget_add_events(state->dialog, GDK_KEY_PRESS_MASK);
    g_signal_connect(G_OBJECT(state->dialog), "key-press-event", G_CALLBACK(KeyDialogKeyPress), state);
}

static GtkWidget *KeyDialogContent(key_dialog *state)
{
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(state->dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    gtk_box_pack_start(GTK_BOX(content), box, TRUE, TRUE, 0);

    return box;
}

static GtkWidget *KeyDialogGridFrame(const char *title, GtkWidget *parent, GtkWidget **outGrid)
{
    GtkWidget *frame = gtk_frame_new(title);
    gtk_box_pack_start(GTK_BOX(parent), frame, FALSE, FALSE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_container_add(GTK_CONTAINER(frame), grid);

    *outGrid = grid;
    return frame;
}

static GtkWidget *KeyDialogSidePanel(key_dialog *state, GtkWidget *parent, const char *legend)
{
    GtkWidget *side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_valign(side, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(parent), side, FALSE, FALSE, 0);

    GtkWidget *text = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(text), legend);
    gtk_label_set_xalign(GTK_LABEL(text), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(text), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(text), 24);
    gtk_box_pack_start(GTK_BOX(side), text, FALSE, FALSE, 0);

    state->hint = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(state->hint), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(state->hint), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(state->hint), 24);
    gtk_style_context_add_class(gtk_widget_get_style_context(state->hint), "keyhint");
    gtk_box_pack_start(GTK_BOX(side), state->hint, FALSE, FALSE, 0);

    return side;
}

static void SaveKeyboardConfigIni(void)
{
    /* The CLI frontend has no config menu, so it reads SDL keycodes from
       SDLKEYS; write that view of the same table as well as the GDK one. */
    u32 gdk_cfg[NB_KEYS];
    u32 sdl_cfg[NB_KEYS];

    memcpy(gdk_cfg, keyboard_cfg, sizeof(gdk_cfg));
    for (int i = 0; i < NB_KEYS; i++) {
        int sk = gdk_to_sdl_keycode(keyboard_cfg[i]);
        /* if we don't know the keycode, chances are that SDL knows it anyways */
        if (sk == -1)
            sk = (int)keyboard_cfg[i];
        sdl_cfg[i] = (u32)sk;
    }

    memcpy(keyboard_cfg, sdl_cfg, sizeof(keyboard_cfg));
    desmume_config_update_keys(keyfile, "SDLKEYS");
    memcpy(keyboard_cfg, gdk_cfg, sizeof(keyboard_cfg));
    desmume_config_update_keys(keyfile, "KEYS");
}

static GtkWidget *JoystickControlsPage(void);

static void Edit_Controls(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    key_dialog state;

    memcpy(&Keypad_Temp, &keyboard_cfg, sizeof(keyboard_cfg));
    memcpy(Joypad_Temp, joypad_cfg, sizeof(Joypad_Temp));

    KeyDialogInit(&state, "Control Config", KEYGROUP_CONTROL, KEYGROUP_TURBO | KEYGROUP_HOTKEY);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(state.dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(content), notebook, TRUE, TRUE, 0);

    GtkWidget *keyboardPage = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), keyboardPage, gtk_label_new("Keyboard"));

    GtkWidget *grid = NULL;
    KeyDialogGridFrame("Buttons", keyboardPage, &grid);
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 2; col++) {
            const int key = control_grid[col][row];
            if (key < 0)
                continue;

            gchar *name = g_ascii_strup(key_names[key], -1);
            KeyDialogAttachSlot(&state, GTK_GRID(grid), row, col * 2, name,
                                KEY_SLOT_CONTROL, key, Keypad_Temp[key], (GdkModifierType)0);
            g_free(name);
        }
    }

    GtkWidget *side = KeyDialogSidePanel(&state, keyboardPage,
        "<b>Red</b> means the key is used by another action.\n"
        "A button can be disabled using <b>Escape</b>.");

    GtkWidget *allow = gtk_check_button_new_with_label("Allow Left+Right / Up+Down");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(allow), ctrls_get_allow_up_down());
    gtk_box_pack_start(GTK_BOX(side), allow, FALSE, FALSE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), JoystickControlsPage(), gtk_label_new("Joystick"));

    KeyDialogUpdateConflicts(&state);
    gtk_widget_show_all(content);

    switch (gtk_dialog_run(GTK_DIALOG(state.dialog))) {
    case GTK_RESPONSE_OK:
        for (size_t i = 0; i < state.slots.size(); i++) {
            const key_slot &slot = state.slots[i];
            if (slot.kind == KEY_SLOT_CONTROL)
                Keypad_Temp[slot.index] = slot.keyval;
        }

        ctrls_set_allow_up_down(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(allow)));
        config.allow_up_down = ctrls_get_allow_up_down();

        memcpy(keyboard_cfg, Keypad_Temp, sizeof(keyboard_cfg));
        SaveKeyboardConfigIni();

        memcpy(joypad_cfg, Joypad_Temp, sizeof(joypad_cfg));
        desmume_config_update_joykeys(keyfile);
        break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_NONE:
        break;
    }

    gtk_widget_destroy(state.dialog);
}

static void HotkeyConfigDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    key_dialog state;

    KeyDialogInit(&state, "Hotkey Configuration", KEYGROUP_TURBO | KEYGROUP_HOTKEY, KEYGROUP_CONTROL);

    GtkWidget *box = KeyDialogContent(&state);

    GtkWidget *column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_pack_start(GTK_BOX(box), column, FALSE, FALSE, 0);

    GtkWidget *hotkeyGrid = NULL;
    KeyDialogGridFrame("Hotkeys", column, &hotkeyGrid);
    for (int i = 0; i < HOTKEY_COUNT; i++) {
        guint keyval;
        GdkModifierType mods;
        ParseAccelString(sHotkeyAccel[i], &keyval, &mods);

        KeyDialogAttachSlot(&state, GTK_GRID(hotkeyGrid), i % 8, (i < 8) ? 0 : 2,
                            hotkey_defs[i].label, KEY_SLOT_HOTKEY, i, keyval, mods);
    }

    GtkWidget *turboGrid = NULL;
    KeyDialogGridFrame("Turbo Keys", column, &turboGrid);
    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 2; col++) {
            const int key = turbo_grid[col][row];

            gchar *name = g_ascii_strup(key_names[key], -1);
            KeyDialogAttachSlot(&state, GTK_GRID(turboGrid), row, col * 2, name,
                                KEY_SLOT_TURBO, key, turbo_keys[key], (GdkModifierType)0);
            g_free(name);
        }
    }

    KeyDialogSidePanel(&state, box,
        "<b>Red</b> means the key is used by another action.\n"
        "A shortcut can be disabled using <b>Escape</b>.\n"
        "Holding a turbo key auto-fires that button, and it does not have to be "
        "the button's own key.");

    KeyDialogUpdateConflicts(&state);
    gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(state.dialog)));

    switch (gtk_dialog_run(GTK_DIALOG(state.dialog))) {
    case GTK_RESPONSE_OK:
        for (size_t i = 0; i < state.slots.size(); i++) {
            const key_slot &slot = state.slots[i];

            if (slot.kind == KEY_SLOT_TURBO) {
                turbo_keys[slot.index] = slot.keyval;
            } else if (slot.kind == KEY_SLOT_HOTKEY) {
                gchar *accel = (slot.keyval != 0)
                    ? gtk_accelerator_name(slot.keyval, slot.mods)
                    : g_strdup("");
                g_free(sHotkeyAccel[slot.index]);
                sHotkeyAccel[slot.index] = accel;
            }
        }

        desmume_config_update_turbokeys(keyfile);
        HotkeysSave();
        HotkeysApply();
        break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_NONE:
        break;
    }

    gtk_widget_destroy(state.dialog);
}

static gboolean JoyKeyAcceptTimerFunc(gpointer data)
{
    if(!in_joy_config_mode)
        return FALSE;
    struct modify_key_ctx *ctx = (struct modify_key_ctx*)data;
    bool modified = FALSE;
    u16 key=get_joy_key(ctx->key_id, &modified);
    if(modified) {
        
        ctx->mk_key_chosen = key;
        gchar* YouPressed = g_strdup_printf("You pressed : %d\nClick OK to keep this key.", ctx->mk_key_chosen);
        gtk_label_set_text(GTK_LABEL(ctx->label), YouPressed);
        g_free(YouPressed);
        return FALSE;
    }
    return in_joy_config_mode;
}

static void JoyKeyAcceptTimerStart(GtkWidget *w, GdkEventFocus *e, struct modify_key_ctx *ctx)
{
    g_timeout_add(200, JoyKeyAcceptTimerFunc, ctx);
}

/* Text for a joystick binding field: 0xFFFF (and 0) mean "not bound". */
static gchar *JoyKeyLabel(int key)
{
    if (Joypad_Temp[key] == 0xFFFF || Joypad_Temp[key] == 0)
        return g_strdup("Disabled");

    return g_strdup_printf("%d", Joypad_Temp[key]);
}

static void Modify_JoyKey(GtkWidget* widget, gpointer data)
{
    struct modify_key_ctx ctx;
    GtkWidget *mkDialog;
    gchar *Key_Label;
    gchar *Title;
    gint Key;

    Key = GPOINTER_TO_INT(data);
    /* Joypad keys start at 1 */
    ctx.key_id = Key+1;
    ctx.mk_key_chosen = Joypad_Temp[Key];
    Title = g_strdup_printf("Press \"%s\" button ...\n", key_names[Key]);
    mkDialog = gtk_dialog_new_with_buttons(Title,
        GTK_WINDOW(gtk_widget_get_toplevel(widget)),
        GTK_DIALOG_MODAL,
        "_OK",GTK_RESPONSE_OK,
        "_Cancel",GTK_RESPONSE_CANCEL,
        NULL);

    ctx.label = gtk_label_new(Title);
    g_free(Title);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(mkDialog))), ctx.label, TRUE, FALSE, 0);
    gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(mkDialog)));

    in_joy_config_mode = true;
    g_signal_connect(G_OBJECT(mkDialog), "focus_in_event", G_CALLBACK(JoyKeyAcceptTimerStart), &ctx);
    JoyKeyAcceptTimerFunc(&ctx);
	
    switch(gtk_dialog_run(GTK_DIALOG(mkDialog))) {
    case GTK_RESPONSE_OK:
        Joypad_Temp[Key] = ctx.mk_key_chosen;
        Key_Label = JoyKeyLabel(Key);
        gtk_button_set_label(GTK_BUTTON(widget), Key_Label);
        g_free(Key_Label);
        break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_NONE:
        ctx.mk_key_chosen = 0;
        break;
    }
    in_joy_config_mode = false;

    gtk_widget_destroy(mkDialog);

}

/////////////////////////////// EMULATION SETTINGS //////////////////////////////////////

// Signature of the settings that are only read while the NDS is reset (BIOS
// and firmware images, timing and game hacks).  Comparing it before and after
// the dialog tells whether a reset is needed for the changes to take effect.
static std::string EmulationResetSignature()
{
	std::string signature;

	signature += CommonSettings.advanced_timing ? "1" : "0";
	signature += CommonSettings.gamehacks.en ? "1" : "0";
	signature += CommonSettings.DebugConsole ? "1" : "0";
	signature += CommonSettings.EnsataEmulation ? "1" : "0";
	signature += '|';
	signature += CommonSettings.UseExtBIOS ? "1" : "0";
	signature += CommonSettings.ARM9BIOS;
	signature += '|';
	signature += CommonSettings.ARM7BIOS;
	signature += '|';
	signature += CommonSettings.SWIFromBIOS ? "1" : "0";
	signature += CommonSettings.PatchSWI3 ? "1" : "0";
	signature += '|';
	signature += CommonSettings.UseExtFirmware ? "1" : "0";
	signature += CommonSettings.ExtFirmwarePath;
	signature += '|';
	signature += CommonSettings.BootFromFirmware ? "1" : "0";
	signature += CommonSettings.UseExtFirmwareSettings ? "1" : "0";

	return signature;
}

// Push the persisted emulation settings into the core.  Runs right after
// config.load(), before the first reset.
static void ApplyEmulationSettingsConfig()
{
	CommonSettings.advanced_timing = config.emu_advanced_timing;
	CommonSettings.gamehacks.en = config.emu_game_hacks;
	CommonSettings.DebugConsole = config.emu_debug_console;
	CommonSettings.EnsataEmulation = config.emu_ensata;

	CommonSettings.UseExtBIOS = config.bios_use_external;
	g_snprintf(CommonSettings.ARM9BIOS, MAX_PATH, "%s", config.bios_arm9_path.get().c_str());
	g_snprintf(CommonSettings.ARM7BIOS, MAX_PATH, "%s", config.bios_arm7_path.get().c_str());
	CommonSettings.SWIFromBIOS = config.bios_swi_from_bios;
	CommonSettings.PatchSWI3 = config.bios_patch_swi3;

	CommonSettings.UseExtFirmware = config.fw_use_external;
	g_snprintf(CommonSettings.ExtFirmwarePath, MAX_PATH, "%s", config.fw_image_path.get().c_str());
	CommonSettings.BootFromFirmware = config.fw_boot_from;
	CommonSettings.UseExtFirmwareSettings = config.fw_use_ext_settings;
}

struct emu_settings_widgets {
	GtkWidget *use_ext_bios;
	GtkWidget *bios9, *bios9_browse;
	GtkWidget *bios7, *bios7_browse;
	GtkWidget *swi_from_bios, *patch_swi3;
	GtkWidget *use_ext_firmware;
	GtkWidget *firmware, *firmware_browse;
	GtkWidget *boot_from_firmware, *use_ext_firmware_settings;
};

// Mirrors the Windows dialog: the image paths follow their "use external"
// switch, and booting from firmware needs both an external BIOS and an
// external firmware image.
static void EmulationSettingsSync(emu_settings_widgets *w)
{
	const gboolean useBios = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->use_ext_bios));
	const gboolean useFirmware = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->use_ext_firmware));

	gtk_widget_set_sensitive(w->bios9, useBios);
	gtk_widget_set_sensitive(w->bios9_browse, useBios);
	gtk_widget_set_sensitive(w->bios7, useBios);
	gtk_widget_set_sensitive(w->bios7_browse, useBios);
	gtk_widget_set_sensitive(w->swi_from_bios, useBios);
	gtk_widget_set_sensitive(w->patch_swi3, useBios);

	gtk_widget_set_sensitive(w->firmware, useFirmware);
	gtk_widget_set_sensitive(w->firmware_browse, useFirmware);
	gtk_widget_set_sensitive(w->use_ext_firmware_settings, useFirmware);

	gtk_widget_set_sensitive(w->boot_from_firmware, useBios && useFirmware);
}

static void EmulationSettingsToggled(GtkToggleButton *button, gpointer data)
{
	EmulationSettingsSync((emu_settings_widgets *)data);
}

// The block size only matters while the recompiler is on.
static void EmulationSettingsDynarecToggled(GtkToggleButton *button, gpointer data)
{
	gtk_widget_set_sensitive(GTK_WIDGET(data), gtk_toggle_button_get_active(button));
}

// Pick a BIOS or firmware image and put its path in the matching entry.
static void EmulationSettingsBrowse(GtkWidget *button, gpointer data)
{
	GtkWidget *entry = GTK_WIDGET(data);

	GtkFileFilter *filter_image = gtk_file_filter_new();
	gtk_file_filter_add_pattern(filter_image, "*.bin");
	gtk_file_filter_add_pattern(filter_image, "*.rom");
	gtk_file_filter_add_pattern(filter_image, "*.fw");
	gtk_file_filter_set_name(filter_image, "BIOS and firmware images");

	GtkFileFilter *filter_any = gtk_file_filter_new();
	gtk_file_filter_add_pattern(filter_any, "*");
	gtk_file_filter_set_name(filter_any, "All files");

	GtkFileChooserNative *chooser = gtk_file_chooser_native_new(
		"Select image...", GTK_WINDOW(pWindow),
		GTK_FILE_CHOOSER_ACTION_OPEN, "_Open", "_Cancel");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter_image);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter_any);

	const char *current = gtk_entry_get_text(GTK_ENTRY(entry));
	if (current != NULL && g_path_is_absolute(current))
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser), current);

	if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
		GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(chooser));
		gchar *path = g_file_get_path(file);

		if (path != NULL) {
			gtk_entry_set_text(GTK_ENTRY(entry), path);
			g_free(path);
		}

		g_object_unref(file);
	}

	g_object_unref(chooser);
}

// The settings above only take effect on the next reset, so offer to do it now
// rather than leaving the user wondering why nothing changed.
static void EmulationSettingsOfferReset()
{
	GtkWidget *message = gtk_message_dialog_new(GTK_WINDOW(pWindow), GTK_DIALOG_MODAL,
	                                            GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
	                                            "These settings are applied when the emulation is reset.");
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(message),
	                                         "Reset the emulation now? Unsaved progress will be lost.");
	gtk_dialog_add_buttons(GTK_DIALOG(message),
	                       "_Cancel", GTK_RESPONSE_CANCEL,
	                       "_Reset", GTK_RESPONSE_ACCEPT,
	                       NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(message), GTK_RESPONSE_CANCEL);

	if (gtk_dialog_run(GTK_DIALOG(message)) == GTK_RESPONSE_ACCEPT)
		Reset(NULL, NULL, NULL);

	gtk_widget_destroy(message);
}

static void EmulationSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkBuilder *builder = gtk_builder_new_from_resource("/org/desmume/DeSmuME/emulation_settings.ui");
	GtkDialog *dialog = GTK_DIALOG(gtk_builder_get_object(builder, "dialog"));
	emu_settings_widgets w;

	GtkWidget *wAdvancedTiming = GTK_WIDGET(gtk_builder_get_object(builder, "advanced_timing"));
	GtkWidget *wGameHacks = GTK_WIDGET(gtk_builder_get_object(builder, "game_hacks"));
	GtkWidget *wDebugConsole = GTK_WIDGET(gtk_builder_get_object(builder, "debug_console"));
	GtkWidget *wEnsata = GTK_WIDGET(gtk_builder_get_object(builder, "ensata"));
	GtkWidget *wCpuFrame = GTK_WIDGET(gtk_builder_get_object(builder, "cpu_frame"));
	GtkWidget *wDynarec = GTK_WIDGET(gtk_builder_get_object(builder, "use_dynarec"));
	GtkWidget *wBlockSize = GTK_WIDGET(gtk_builder_get_object(builder, "block_size_spin"));

	w.use_ext_bios = GTK_WIDGET(gtk_builder_get_object(builder, "use_ext_bios"));
	w.bios9 = GTK_WIDGET(gtk_builder_get_object(builder, "bios9_entry"));
	w.bios9_browse = GTK_WIDGET(gtk_builder_get_object(builder, "bios9_browse"));
	w.bios7 = GTK_WIDGET(gtk_builder_get_object(builder, "bios7_entry"));
	w.bios7_browse = GTK_WIDGET(gtk_builder_get_object(builder, "bios7_browse"));
	w.swi_from_bios = GTK_WIDGET(gtk_builder_get_object(builder, "swi_from_bios"));
	w.patch_swi3 = GTK_WIDGET(gtk_builder_get_object(builder, "patch_swi3"));
	w.use_ext_firmware = GTK_WIDGET(gtk_builder_get_object(builder, "use_ext_firmware"));
	w.firmware = GTK_WIDGET(gtk_builder_get_object(builder, "firmware_entry"));
	w.firmware_browse = GTK_WIDGET(gtk_builder_get_object(builder, "firmware_browse"));
	w.boot_from_firmware = GTK_WIDGET(gtk_builder_get_object(builder, "boot_from_firmware"));
	w.use_ext_firmware_settings = GTK_WIDGET(gtk_builder_get_object(builder, "use_ext_firmware_settings"));
	g_object_unref(builder);

	const std::string previousSignature = EmulationResetSignature();

	// General
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wAdvancedTiming), CommonSettings.advanced_timing);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wGameHacks), CommonSettings.gamehacks.en);

	// Debug
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wDebugConsole), CommonSettings.DebugConsole);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wEnsata), CommonSettings.EnsataEmulation);

	// CPU emulation mode
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wDynarec), config.use_jit);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(wBlockSize), config.jit_max_block_size.get());
	gtk_widget_set_sensitive(wBlockSize, config.use_jit);
	g_signal_connect(G_OBJECT(wDynarec), "toggled", G_CALLBACK(EmulationSettingsDynarecToggled), wBlockSize);

	// BIOS
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w.use_ext_bios), CommonSettings.UseExtBIOS);
	gtk_entry_set_text(GTK_ENTRY(w.bios9), CommonSettings.ARM9BIOS);
	gtk_entry_set_text(GTK_ENTRY(w.bios7), CommonSettings.ARM7BIOS);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w.swi_from_bios), CommonSettings.SWIFromBIOS);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w.patch_swi3), CommonSettings.PatchSWI3);

	// Firmware
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w.use_ext_firmware), CommonSettings.UseExtFirmware);
	gtk_entry_set_text(GTK_ENTRY(w.firmware), CommonSettings.ExtFirmwarePath);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w.boot_from_firmware), CommonSettings.BootFromFirmware);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w.use_ext_firmware_settings), CommonSettings.UseExtFirmwareSettings);

	g_signal_connect(G_OBJECT(w.use_ext_bios), "toggled", G_CALLBACK(EmulationSettingsToggled), &w);
	g_signal_connect(G_OBJECT(w.use_ext_firmware), "toggled", G_CALLBACK(EmulationSettingsToggled), &w);
	g_signal_connect(G_OBJECT(w.bios9_browse), "clicked", G_CALLBACK(EmulationSettingsBrowse), w.bios9);
	g_signal_connect(G_OBJECT(w.bios7_browse), "clicked", G_CALLBACK(EmulationSettingsBrowse), w.bios7);
	g_signal_connect(G_OBJECT(w.firmware_browse), "clicked", G_CALLBACK(EmulationSettingsBrowse), w.firmware);

	EmulationSettingsSync(&w);

	gtk_widget_show_all(gtk_dialog_get_content_area(dialog));

#ifndef HAVE_JIT
	// No dynamic recompiler in this build, so the whole section is moot.
	gtk_widget_set_visible(wCpuFrame, FALSE);
#endif

	switch (gtk_dialog_run(dialog)) {
	case GTK_RESPONSE_OK:
		CommonSettings.advanced_timing = config.emu_advanced_timing = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wAdvancedTiming));
		CommonSettings.gamehacks.en = config.emu_game_hacks = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wGameHacks));
		CommonSettings.DebugConsole = config.emu_debug_console = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wDebugConsole));
		CommonSettings.EnsataEmulation = config.emu_ensata = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wEnsata));

		CommonSettings.UseExtBIOS = config.bios_use_external = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w.use_ext_bios));
		g_snprintf(CommonSettings.ARM9BIOS, MAX_PATH, "%s", gtk_entry_get_text(GTK_ENTRY(w.bios9)));
		config.bios_arm9_path = CommonSettings.ARM9BIOS;
		g_snprintf(CommonSettings.ARM7BIOS, MAX_PATH, "%s", gtk_entry_get_text(GTK_ENTRY(w.bios7)));
		config.bios_arm7_path = CommonSettings.ARM7BIOS;
		CommonSettings.SWIFromBIOS = config.bios_swi_from_bios = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w.swi_from_bios));
		CommonSettings.PatchSWI3 = config.bios_patch_swi3 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w.patch_swi3));

		CommonSettings.UseExtFirmware = config.fw_use_external = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w.use_ext_firmware));
		g_snprintf(CommonSettings.ExtFirmwarePath, MAX_PATH, "%s", gtk_entry_get_text(GTK_ENTRY(w.firmware)));
		config.fw_image_path = CommonSettings.ExtFirmwarePath;
		CommonSettings.BootFromFirmware = config.fw_boot_from = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w.boot_from_firmware));
		CommonSettings.UseExtFirmwareSettings = config.fw_use_ext_settings = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w.use_ext_firmware_settings));

#ifdef HAVE_JIT
		config.use_jit = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wDynarec));
		config.jit_max_block_size = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(wBlockSize));
		CommonSettings.jit_max_block_size = config.jit_max_block_size;
		arm_jit_sync();
		arm_jit_reset(CommonSettings.use_jit = config.use_jit);
#endif

		if (gameInfo.fROM != NULL && EmulationResetSignature() != previousSignature)
			EmulationSettingsOfferReset();
		break;
	case GTK_RESPONSE_CANCEL:
	case GTK_RESPONSE_NONE:
		break;
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void PathSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	path_settings_dialog_show(GTK_WINDOW(pWindow));
}

static GtkWidget *JoystickControlsPage(void)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    GtkWidget *grid = NULL;
    KeyDialogGridFrame("Buttons", page, &grid);

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 2; col++) {
            const int key = control_grid[col][row];
            if (key < 0)
                continue;

            gchar *name = g_ascii_strup(key_names[key], -1);
            GtkWidget *label = gtk_label_new(name);
            g_free(name);
            gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
            gtk_grid_attach(GTK_GRID(grid), label, col * 2, row, 1, 1);

            gchar *keyLabel = JoyKeyLabel(key);
            GtkWidget *keyButton = gtk_button_new_with_label(keyLabel);
            g_free(keyLabel);
            gtk_widget_set_size_request(keyButton, 150, -1);
            gtk_grid_attach(GTK_GRID(grid), keyButton, col * 2 + 1, row, 1, 1);
            g_signal_connect(G_OBJECT(keyButton), "clicked", G_CALLBACK(Modify_JoyKey), GINT_TO_POINTER(key));
        }
    }

    GtkWidget *side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_valign(side, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(page), side, FALSE, FALSE, 0);

    GtkWidget *hint = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(hint),
        "Click a button, then press the pad button or move the axis to bind it.");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(hint), 24);
    gtk_box_pack_start(GTK_BOX(side), hint, FALSE, FALSE, 0);

    return page;
}


// The GPU filters are WGSL chains run by the WebGPU presenter, so the choice is
// only meaningful while that presentation is selected. The value is kept when
// software presentation is chosen, exactly as the Windows frontend does.
#if defined(ENABLE_WGPU)
static void PresentationComboChanged(GtkComboBox *combo, gpointer user_data)
{
	const char *id = gtk_combo_box_get_active_id(combo);
	gtk_widget_set_sensitive(GTK_WIDGET(user_data), (id != NULL) && (strcmp(id, "software") != 0));
}
#endif

// The 3D output color depth is the GPU event handler's framebuffer format.
static NDSColorFormat ColorFormatFromDepth(int depth)
{
	switch (depth)
	{
		case 15:
			return NDSColorFormat_BGR555_Rev;

		case 24:
			return NDSColorFormat_BGR888_Rev;

		default:
			return NDSColorFormat_BGR666_Rev;
	}
}

// The Windows port's "Default" button next to the engine combo selects its
// hard-coded default core, which is the software rasterizer.
static void CoreDefaultClicked(GtkWidget *widget, gpointer data)
{
	gtk_combo_box_set_active(GTK_COMBO_BOX(data), RENDERID_SOFTRASTERIZER);
}

static int ResolveVideoFilterThreadCount(const int configuredThreadCount)
{
	int threadCount = (configuredThreadCount > 0) ? configuredThreadCount : NDS_GetCPUCoreCount();

	if (threadCount < 1)
	{
		threadCount = 1;
	}
	else if (threadCount > 64)
	{
		threadCount = 64;
	}

	return threadCount;
}

static void GraphicsSettingsDialog(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
	GtkDialog *dialog;
	GtkComboBox *coreCombo, *wScale, *wColorDepth;
	GtkToggleButton *wPosterize, *wSmoothing, *wHCInterpolate;
	GtkToggleButton *wTxtHack, *wLineHack, *wEdgeMark, *wFog, *wTextures;
	GtkToggleButton *wShadowPolygons, *wZeroAlpha, *wNDSDepth, *wDepthLEqual;
	GtkSpinButton *wGPUScale, *wFilterThreads;
	GtkLabel *wMultisampleLabel;
	GtkWidget *wCoreDefault, *wAccelFrame, *wAccuracyFrame, *wPresentationFrame;
#if defined(ENABLE_OPENGL_STANDARD) || defined(ENABLE_OPENGL_ES) || defined(ENABLE_WGPU)
	GtkComboBox *wMultisample;
#endif
#if defined(ENABLE_WGPU)
	GtkComboBox *wPresentation, *wGPUFilter;
#endif

	Build3DRendererList();

	GtkBuilder *builder = gtk_builder_new_from_resource("/org/desmume/DeSmuME/graphics.ui");
	dialog = GTK_DIALOG(gtk_builder_get_object(builder, "dialog"));
	coreCombo = GTK_COMBO_BOX(gtk_builder_get_object(builder, "core_combo"));
	wCoreDefault = GTK_WIDGET(gtk_builder_get_object(builder, "core_default_button"));
	wScale = GTK_COMBO_BOX(gtk_builder_get_object(builder, "scale"));
	wColorDepth = GTK_COMBO_BOX(gtk_builder_get_object(builder, "color_depth_combo"));
	wGPUScale = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "gpuscale"));
	gtk_spin_button_set_range(wGPUScale, GPU_SCALE_FACTOR_MIN, GPU_SCALE_FACTOR_MAX);
	gtk_spin_button_set_increments(wGPUScale, 0.5, 1.0);
	wFilterThreads = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "filter_threads"));
	gtk_spin_button_set_range(wFilterThreads, 0, 64);
	gtk_spin_button_set_increments(wFilterThreads, 1, 1);
	gtk_spin_button_set_value(wFilterThreads, (config.view_filterThreads > 0) ? config.view_filterThreads.get() : ResolveVideoFilterThreadCount(0));
	wMultisampleLabel = GTK_LABEL(gtk_builder_get_object(builder, "multisample_label"));
	wAccelFrame = GTK_WIDGET(gtk_builder_get_object(builder, "accel_frame"));
	wAccuracyFrame = GTK_WIDGET(gtk_builder_get_object(builder, "accuracy_frame"));
	wPresentationFrame = GTK_WIDGET(gtk_builder_get_object(builder, "presentation_frame"));
	// Only name the backends that are actually compiled into this build.
#if (defined(ENABLE_OPENGL_STANDARD) || defined(ENABLE_OPENGL_ES)) && defined(ENABLE_WGPU)
	gtk_label_set_text(wMultisampleLabel, "Multisample Antialiasing (OpenGL/WebGPU):");
#elif defined(ENABLE_OPENGL_STANDARD) || defined(ENABLE_OPENGL_ES)
	gtk_label_set_text(wMultisampleLabel, "Multisample Antialiasing (OpenGL):");
#elif defined(ENABLE_WGPU)
	gtk_label_set_text(wMultisampleLabel, "Multisample Antialiasing (WebGPU):");
#else
	gtk_label_set_text(wMultisampleLabel, "Multisample Antialiasing:");
#endif
#if defined(ENABLE_OPENGL_STANDARD) || defined(ENABLE_OPENGL_ES) || defined(ENABLE_WGPU)
	wMultisample = GTK_COMBO_BOX(gtk_builder_get_object(builder, "multisample"));
#endif
	wPosterize = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "posterize"));
	wSmoothing = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "smoothing"));
	wHCInterpolate = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "hc_interpolate"));
	wTxtHack = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "txt_hack"));
	wLineHack = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "line_hack"));
	wEdgeMark = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "edgemark"));
	wFog = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "fog"));
	wTextures = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "textures"));
	wShadowPolygons = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "shadow_polygons"));
	wZeroAlpha = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "zero_alpha_blending"));
	wNDSDepth = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "nds_depth_calc"));
	wDepthLEqual = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "depth_lequal_facing"));
#if defined(ENABLE_WGPU)
	wPresentation = GTK_COMBO_BOX(gtk_builder_get_object(builder, "presentation_combo"));
	wGPUFilter = GTK_COMBO_BOX(gtk_builder_get_object(builder, "gpu_filter_combo"));
#endif
	g_object_unref(builder);

	// Populate the combo from the runtime renderer list so that the WebGPU
	// entries ("WebGPU - Vulkan", ...) appear next to the built-in cores.
	GtkComboBoxText *coreComboText = GTK_COMBO_BOX_TEXT(coreCombo);
	gtk_combo_box_text_remove_all(coreComboText);
	for (int i = 0; (i < GPU3D_MAX_COUNT) && (core3DList[i] != NULL); i++)
	{
		gtk_combo_box_text_append_text(coreComboText, core3DList[i]->name);
	}

	g_signal_connect(G_OBJECT(wCoreDefault), "clicked", G_CALLBACK(CoreDefaultClicked), coreCombo);

	gtk_combo_box_set_active(coreCombo, cur3DCore);

#if defined(ENABLE_WGPU)
	// Screen presentation. Like the 3D renderer list, offer one entry per
	// usable WebGPU backend ("WebGPU - Vulkan", ...) next to the software
	// path. The id of a WebGPU item is its WGPUInstanceBackend mask, which is
	// what WGPULinuxDisplay pins the present instance to.
	GtkComboBoxText *presentationComboText = GTK_COMBO_BOX_TEXT(wPresentation);
	gtk_combo_box_text_remove_all(presentationComboText);

	const size_t presentationBackendCount = WGPURender_GetBackendCount();

	if (presentationBackendCount > 0)
	{
		for (size_t i = 0; i < presentationBackendCount; i++)
		{
			char id[32];
			char label[64];
			g_snprintf(id, sizeof(id), "%u", (unsigned)WGPURender_GetBackendMask(i));
			g_snprintf(label, sizeof(label), "WebGPU - %s", WGPURender_GetBackendLabel(i));
			gtk_combo_box_text_append(presentationComboText, id, label);
		}
	}
	else
	{
		char id[32];
		g_snprintf(id, sizeof(id), "%u", (unsigned)WGPU_PresentationBackendMask());
		gtk_combo_box_text_append(presentationComboText, id, "WebGPU");
	}

	gtk_combo_box_text_append(presentationComboText, "software", "Software (Cairo)");

	if (config.presentationWGPU)
	{
		char activePresentationId[32];
		g_snprintf(activePresentationId, sizeof(activePresentationId), "%u", (unsigned)WGPU_PresentationBackendMask());

		// The configured backend may have disappeared (another machine, a
		// driver change); then the first WebGPU entry is the fallback.
		if (!gtk_combo_box_set_active_id(wPresentation, activePresentationId))
		{
			gtk_combo_box_set_active(wPresentation, 0);
		}
	}
	else
	{
		gtk_combo_box_set_active_id(wPresentation, "software");
	}

	// Populate the GPU filters from the chain table, so a filter added there
	// shows up here without a second list to keep in step. The id of each item
	// is its WGPUPresentUpscaler value.
	GtkComboBoxText *filterComboText = GTK_COMBO_BOX_TEXT(wGPUFilter);
	gtk_combo_box_text_remove_all(filterComboText);
	gtk_combo_box_text_append(filterComboText, "0", "Off");

	for (int value = WGPUPresentUpscaler_Off + 1; value < WGPUPresentUpscalerCount; value++)
	{
		const char *name = WGPUPresent::upscalerName(value);

		if (name == NULL)
		{
			continue;
		}

		char id[16];
		g_snprintf(id, sizeof(id), "%d", value);
		gtk_combo_box_text_append(filterComboText, id, name);
	}

	int activeFilter = WGPUPresent::isUpscaler(config.gpuFilter) ? config.gpuFilter : WGPUPresentUpscaler_Off;
	char activeFilterId[16];
	g_snprintf(activeFilterId, sizeof(activeFilterId), "%d", activeFilter);
	gtk_combo_box_set_active_id(wGPUFilter, activeFilterId);

	gtk_widget_set_sensitive(GTK_WIDGET(wGPUFilter), config.presentationWGPU);
	g_signal_connect(wPresentation, "changed", G_CALLBACK(PresentationComboChanged), wGPUFilter);
#endif // ENABLE_WGPU

	// The shift it work for scale up to 4. For scaling more than 4, a mapping function is required
	gtk_combo_box_set_active(wScale, CommonSettings.GFX3D_Renderer_TextureScalingFactor >> 1);

	// GPU scaling factor
	gtk_spin_button_set_value(wGPUScale, gpu_scale_factor);

	// GPU color depth
	{
		char depthId[8];
		g_snprintf(depthId, sizeof(depthId), "%d", config.gfx3dColorDepth.get());
		if (!gtk_combo_box_set_active_id(wColorDepth, depthId))
		{
			gtk_combo_box_set_active_id(wColorDepth, "18");
		}
	}

	// 3D Texture Deposterization
	gtk_toggle_button_set_active(wPosterize, CommonSettings.GFX3D_Renderer_TextureDeposterize);

	// 3D Texture Smoothing
	gtk_toggle_button_set_active(wSmoothing, CommonSettings.GFX3D_Renderer_TextureSmoothing);

#if defined(ENABLE_OPENGL_STANDARD) || defined(ENABLE_OPENGL_ES) || defined(ENABLE_WGPU)
	// Multisample antialiasing. This applies to the OpenGL and WebGPU
	// renderers, so it must not be tied to the OpenGL build options.
	int currentMultisample = CommonSettings.GFX3D_Renderer_MultisampleSize;
	int currentActive = 0;
	// find smallest option that is larger than current value, i.e. round up to power of 2
	while ((currentActive < 5) && (multisampleSizes[currentActive] < currentMultisample)) { currentActive++; }
	gtk_combo_box_set_active(wMultisample, currentActive);
#endif

	// SoftRasterizer options
	gtk_toggle_button_set_active(wHCInterpolate, CommonSettings.GFX3D_HighResolutionInterpolateColor);
	gtk_toggle_button_set_active(wTxtHack, CommonSettings.GFX3D_TXTHack);
	gtk_toggle_button_set_active(wLineHack, CommonSettings.GFX3D_LineHack);

	// General options (read by every 3D renderer, per frame)
	gtk_toggle_button_set_active(wEdgeMark, CommonSettings.GFX3D_EdgeMark);
	gtk_toggle_button_set_active(wFog, CommonSettings.GFX3D_Fog);
	gtk_toggle_button_set_active(wTextures, CommonSettings.GFX3D_Texture);

	// Rendering accuracy options (OpenGL bakes these into its shaders)
	gtk_toggle_button_set_active(wShadowPolygons, CommonSettings.OpenGL_Emulation_ShadowPolygon);
	gtk_toggle_button_set_active(wZeroAlpha, CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending);
	gtk_toggle_button_set_active(wNDSDepth, CommonSettings.OpenGL_Emulation_NDSDepthCalculation);
	gtk_toggle_button_set_active(wDepthLEqual, CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing);

	gtk_widget_show_all(gtk_dialog_get_content_area(dialog));

	// Sections that only exist for some builds go away after show_all(), which
	// would otherwise make them visible again.
#if !defined(ENABLE_WGPU)
	gtk_widget_set_visible(wPresentationFrame, FALSE);
#endif
#if !defined(ENABLE_OPENGL_STANDARD) && !defined(ENABLE_OPENGL_ES) && !defined(ENABLE_WGPU)
	gtk_widget_set_visible(wAccelFrame, FALSE);
	gtk_widget_set_visible(wAccuracyFrame, FALSE);
#endif

    switch (gtk_dialog_run(dialog)) {
    case GTK_RESPONSE_OK:
    // Start: OK Response Block
    {
    	int sel3DCore = gtk_combo_box_get_active(coreCombo);

		if ( (sel3DCore < 0) || (sel3DCore >= GPU3D_MAX_COUNT) || (core3DList[sel3DCore] == NULL) )
		{
			sel3DCore = RENDERID_SOFTRASTERIZER;
		}

		size_t scale = 1;

		switch (gtk_combo_box_get_active(wScale)){
		case 1:
			scale = 2;
			break;
		case 2:
			scale = 4;
			break;
		default:
			break;
		}

		// Read every widget before touching CommonSettings: a renderer that is
		// rebuilt for a new 3D core reads these values while it initializes,
		// so they must already be in place by then.
		const bool bPosterize = gtk_toggle_button_get_active(wPosterize);
		const bool bSmoothing = gtk_toggle_button_get_active(wSmoothing);
		const bool bHCInterpolate = gtk_toggle_button_get_active(wHCInterpolate);
		const bool bTxtHack = gtk_toggle_button_get_active(wTxtHack);
		const bool bLineHack = gtk_toggle_button_get_active(wLineHack);
		const bool bEdgeMark = gtk_toggle_button_get_active(wEdgeMark);
		const bool bFog = gtk_toggle_button_get_active(wFog);
		const bool bTextures = gtk_toggle_button_get_active(wTextures);
		const bool bShadowPolygons = gtk_toggle_button_get_active(wShadowPolygons);
		const bool bZeroAlpha = gtk_toggle_button_get_active(wZeroAlpha);
		const bool bNDSDepth = gtk_toggle_button_get_active(wNDSDepth);
		const bool bDepthLEqual = gtk_toggle_button_get_active(wDepthLEqual);

		const char *colorDepthId = gtk_combo_box_get_active_id(wColorDepth);
		const int colorDepth = (colorDepthId != NULL) ? (int)g_ascii_strtoll(colorDepthId, NULL, 10) : 18;

		// The OpenGL renderer bakes the accuracy options into its shaders when
		// it initializes, so it has to be rebuilt when they change. The WebGPU
		// and software renderers read them every frame.
		const bool accuracyChanged =
			(CommonSettings.OpenGL_Emulation_ShadowPolygon != bShadowPolygons) ||
			(CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending != bZeroAlpha) ||
			(CommonSettings.OpenGL_Emulation_NDSDepthCalculation != bNDSDepth) ||
			(CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing != bDepthLEqual);

		CommonSettings.GFX3D_Renderer_TextureDeposterize = config.textureDeposterize = bPosterize;
		CommonSettings.GFX3D_Renderer_TextureSmoothing = config.textureSmoothing = bSmoothing;
		CommonSettings.GFX3D_Renderer_TextureScalingFactor = config.textureUpscale = scale;
		CommonSettings.GFX3D_HighResolutionInterpolateColor = config.highColorInterpolation = bHCInterpolate;
		CommonSettings.GFX3D_TXTHack = config.gfx3dTxtHack = bTxtHack;
		CommonSettings.GFX3D_LineHack = config.gfx3dLineHack = bLineHack;
		CommonSettings.GFX3D_EdgeMark = config.gfx3dEdgeMark = bEdgeMark;
		CommonSettings.GFX3D_Fog = config.gfx3dFog = bFog;
		CommonSettings.GFX3D_Texture = config.gfx3dTexture = bTextures;
		CommonSettings.OpenGL_Emulation_ShadowPolygon = config.gfx3dShadowPolygon = bShadowPolygons;
		CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending = config.gfx3dZeroAlphaBlending = bZeroAlpha;
		CommonSettings.OpenGL_Emulation_NDSDepthCalculation = config.gfx3dNdsDepthCalculation = bNDSDepth;
		CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing = config.gfx3dDepthLEqualFacing = bDepthLEqual;
		config.gfx3dColorDepth = colorDepth;
		gtk3gpuEvent->SetColorFormat(ColorFormatFromDepth(colorDepth));

#if defined(ENABLE_OPENGL_STANDARD) || defined(ENABLE_OPENGL_ES) || defined(ENABLE_WGPU)
		int selectedMultisample = gtk_combo_box_get_active(wMultisample);
		if (selectedMultisample < 0)
		{
			selectedMultisample = 0;
		}
		config.multisamplingSize = multisampleSizes[selectedMultisample];
		config.multisampling = selectedMultisample != 0;
		CommonSettings.GFX3D_Renderer_MultisampleSize = multisampleSizes[selectedMultisample];
#endif

		gpu_scale_factor = gtk_spin_button_get_value(wGPUScale);
		if(gpu_scale_factor < GPU_SCALE_FACTOR_MIN)
			gpu_scale_factor = GPU_SCALE_FACTOR_MIN;
		if(gpu_scale_factor > GPU_SCALE_FACTOR_MAX)
			gpu_scale_factor = GPU_SCALE_FACTOR_MAX;
		gtk_spin_button_set_value(wGPUScale, gpu_scale_factor);
		config.gpuScaleFactor = gpu_scale_factor;

		// Video filter threads. 0 keeps the automatic (CPU core count) value.
		const int filterThreads = gtk_spin_button_get_value_as_int(wFilterThreads);
		config.view_filterThreads = filterThreads;
		if (video != NULL)
		{
			video->SetThreadCount((size_t)ResolveVideoFilterThreadCount(filterThreads));
		}

		gtk3gpuEvent->SetFramebufferDimensions( (size_t)(((float)GPU_FRAMEBUFFER_NATIVE_WIDTH * gpu_scale_factor) + 0.5), (size_t)(((float)GPU_FRAMEBUFFER_NATIVE_HEIGHT * gpu_scale_factor) + 0.5) );

    	// Change only if needed
		if (sel3DCore != cur3DCore)
		{
			// OpenGL entries need their context callbacks selected first. The
			// other cores (Null, SoftRasterizer, WebGPU) need none.
#if defined(ENABLE_OPENGL_ES) && !defined(ENABLE_GLX)
			if (sel3DCore == 2)
			{
	#if defined(ENABLE_EGL)
				oglrender_init = &egl_initOpenGL_ES_3_0;
	#else
				oglrender_init = &sdl_initOpenGL_ES_3_0;
	#endif
			}
#elif defined(ENABLE_OPENGL_STANDARD)
			switch (sel3DCore)
			{
				case 2:
	#if defined(ENABLE_GLX)
					oglrender_init = &glx_initOpenGL_StandardAuto;
	#elif defined(ENABLE_EGL)
					oglrender_init = &egl_initOpenGL_StandardAuto;
	#else
					oglrender_init = &sdl_initOpenGL_StandardAuto;
	#endif
					break;

				case 3:
	#if defined(ENABLE_GLX)
					oglrender_init = &glx_initOpenGL_LegacyAuto;
	#elif defined(ENABLE_EGL)
					oglrender_init = &egl_initOpenGL_LegacyAuto;
	#else
					oglrender_init = &sdl_initOpenGL_LegacyAuto;
	#endif
					break;

				case 4:
	#if defined(ENABLE_GLX)
					oglrender_init = &glx_initOpenGL_3_2_CoreProfile;
	#elif defined(ENABLE_EGL)
					oglrender_init = &egl_initOpenGL_3_2_CoreProfile;
	#else
					oglrender_init = &sdl_initOpenGL_3_2_CoreProfile;
	#endif
					break;
			}
#endif
			
			if (GPU->Change3DRendererByID(sel3DCore))
			{
				config.core3D = sel3DCore;
			}
			else
			{
				GPU->Change3DRendererByID(RENDERID_SOFTRASTERIZER);
				g_printerr("3D renderer initialization failed!\nFalling back to 3D core: %s\n", core3DList[RENDERID_SOFTRASTERIZER]->name);
				config.core3D = RENDERID_SOFTRASTERIZER;
			}
		}
		else if (accuracyChanged)
		{
			// Same core, but the shaders were built with the old settings.
			GPU->Change3DRendererByID(cur3DCore);
		}

#if defined(ENABLE_WGPU)
		// GPU filter. It only runs when a frame goes through the WebGPU
		// presenter; the value is kept while software presentation is in use.
		const char *filterId = gtk_combo_box_get_active_id(wGPUFilter);
		int selectedFilter = (filterId != NULL) ? (int)g_ascii_strtoll(filterId, NULL, 10) : WGPUPresentUpscaler_Off;

		if (!WGPUPresent::isUpscaler(selectedFilter))
		{
			selectedFilter = WGPUPresentUpscaler_Off;
		}

		config.gpuFilter = selectedFilter;
		wgpu_display.setUpscaler(selectedFilter);

		// Screen presentation path. The selected WebGPU backend is the id of
		// the combo item, or "software" for Cairo. Switching either tears the
		// WebGPU subsurface down so that Cairo can draw into GDK's surface
		// again; the next expose recreates it with the new backend.
		const char *presentationId = gtk_combo_box_get_active_id(wPresentation);
		const bool useWGPUPresentation = (presentationId != NULL) && (strcmp(presentationId, "software") != 0);

		uint32_t selectedPresentationBackend = (uint32_t)config.presentationWGPUBackend;
		if (useWGPUPresentation)
		{
			selectedPresentationBackend = (presentationId != NULL)
				? (uint32_t)g_ascii_strtoull(presentationId, NULL, 10)
				: WGPU_PresentationBackendMask();
		}

		const bool presentationChanged = (useWGPUPresentation != config.presentationWGPU) ||
			( useWGPUPresentation &&
			  (selectedPresentationBackend != (uint32_t)config.presentationWGPUBackend) );

		config.presentationWGPU = useWGPUPresentation;
		if (useWGPUPresentation)
		{
			config.presentationWGPUBackend = (int)selectedPresentationBackend;
		}

		if (presentationChanged)
		{
			WGPU_KillDisplay();

			if (useWGPUPresentation)
			{
				wgpu_initialization_failed = false;
			}

			if (pDrawingArea != NULL)
			{
				gtk_widget_queue_draw(pDrawingArea);
			}
		}
#endif // ENABLE_WGPU
    }
    // End: OK Response Block
        break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_NONE:
        break;
    }

	gtk_widget_destroy(GTK_WIDGET(dialog));

}
static void ToggleLayerVisibility(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    guint Layer = GPOINTER_TO_UINT(user_data);

    // FIXME: make it work after resume
    if (!desmume_running())
        return;

    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean active = !g_variant_get_boolean(variant);
    g_simple_action_set_state(action, g_variant_new_boolean(active));

    switch (Layer) {
    case MAIN_BG_0:
    case MAIN_BG_1:
    case MAIN_BG_2:
    case MAIN_BG_3:
    case MAIN_OBJ:
        GPU->GetEngineMain()->SetLayerEnableState(Layer, (active == TRUE) ? true : false);
        break;
    case SUB_BG_0:
    case SUB_BG_1:
    case SUB_BG_2:
    case SUB_BG_3:
    case SUB_OBJ:
        GPU->GetEngineSub()->SetLayerEnableState(Layer-SUB_BG_0, (active == TRUE) ? true : false);
        break;
    default:
        break;
    }
}

static void Printscreen(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	const gint videoWidth  = (gint)video->GetSrcWidth();
	const gint videoHeight = (gint)video->GetSrcHeight();
	u8 *screenshotBuffer = (u8 *)malloc_alignedPage(videoWidth * videoHeight * sizeof(u32));
	
    GdkPixbuf *screenshot;
    GError *error = NULL;
    static unsigned int seq = 0;
    gint H, W;

    if (nds_screen.rotation_angle == 0 || nds_screen.rotation_angle == 180) {
        W = videoWidth;
        H = videoHeight;
    } else {
        W = videoHeight;
        H = videoWidth;
    }

	gtk3gpuEvent->VideoFetchLock();
	ColorspaceConvertBuffer888xTo8888Opaque<true, false>(video->GetSrcBufferPtr(), (u32 *)screenshotBuffer, videoWidth * videoHeight);
	gtk3gpuEvent->VideoFetchUnlock();

	// In GPU HUD mode the OSD lives in a texture, not in the source buffer.
	WGPU_HudCompositeScreenshot(screenshotBuffer, videoWidth, videoHeight);
	
	screenshot = gdk_pixbuf_new_from_data(screenshotBuffer,
                          GDK_COLORSPACE_RGB,
                          TRUE,
                          8,
                          W,
                          H,
                          W * sizeof(u32),
                          NULL,
                          NULL);

    // Folder, extension and file name all come from Config > Path Settings.
    // An empty folder keeps the old behaviour: the XDG Pictures folder, then
    // Desktop, then home.
    const std::string dir = path_settings_screenshot_directory();
    const std::string extension = path_settings_screenshot_extension();
    const std::string base = path_settings_screenshot_name(sCurrentROMBaseName, seq++);

    std::string filename = dir + "/" + base + extension;
    for (int attempt = 1;
         (attempt < 1000) && g_file_test(filename.c_str(), G_FILE_TEST_EXISTS);
         attempt++)
    {
        filename = dir + "/" + base + "-" + std::to_string(attempt) + extension;
    }

    gdk_pixbuf_save(screenshot, filename.c_str(),
                    path_settings_screenshot_image_format().c_str(), &error, NULL);
    if (error) {
        g_error_free (error);
        g_printerr("Failed to save %s", filename.c_str());
    }

    g_object_unref(screenshot);
    free_aligned(screenshotBuffer);
}

#ifdef DESMUME_GTK_FIRMWARE_BROKEN
static void SelectFirmwareFile(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkFileFilter *pFilter_nds, *pFilter_bin, *pFilter_any;
    GtkFileChooserNative *pFileSelection;

    BOOL oldState = desmume_running();
    Pause(NULL, NULL, NULL);

    pFilter_nds = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_nds, "*.nds");
    gtk_file_filter_set_name(pFilter_nds, "Nds binary (.nds)");

    pFilter_bin = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_bin, "*.bin");
    gtk_file_filter_set_name(pFilter_bin, "Binary file (.bin)");

    pFilter_any = gtk_file_filter_new();
    gtk_file_filter_add_pattern(pFilter_any, "*");
    gtk_file_filter_set_name(pFilter_any, "All files");

    pFileSelection = gtk_file_chooser_native_new("Load firmware...",
            GTK_WINDOW(pWindow),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Open", "_Cancel");

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_nds);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_bin);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pFileSelection), pFilter_any);

    int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(pFileSelection));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(pFileSelection));
        gchar *sPath = g_file_get_path(file);

        CommonSettings.UseExtFirmware = true;
        strncpy(CommonSettings.ExtFirmwarePath, (const char*)sPath, g_utf8_strlen(sPath, -1));
        g_free(sPath);
    }
    g_object_unref(pFileSelection);

    if(oldState) Launch(NULL, NULL, NULL);
}
#endif

static void Modify_PriInterpolation(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    std::string string = g_variant_get_string(parameter, NULL);
    VideoFilterTypeID filter;
    if (string == "none")
        filter = VideoFilterTypeID_None;
    else if (string == "lq2x")
        filter = VideoFilterTypeID_LQ2X;
    else if (string == "lq2xs")
        filter = VideoFilterTypeID_LQ2XS;
    else if (string == "hq2x")
        filter = VideoFilterTypeID_HQ2X;
    else if (string == "hq2xs")
        filter = VideoFilterTypeID_HQ2XS;
    else if (string == "hq3x")
        filter = VideoFilterTypeID_HQ3X;
    else if (string == "hq3xs")
        filter = VideoFilterTypeID_HQ3XS;
    else if (string == "hq4x")
        filter = VideoFilterTypeID_HQ4X;
    else if (string == "hq4xs")
        filter = VideoFilterTypeID_HQ4XS;
    else if (string == "2xsai")
        filter = VideoFilterTypeID_2xSaI;
    else if (string == "super2xsai")
        filter = VideoFilterTypeID_Super2xSaI;
    else if (string == "supereagle")
        filter = VideoFilterTypeID_SuperEagle;
    else if (string == "scanline")
        filter = VideoFilterTypeID_Scanline;
    else if (string == "bilinear")
        filter = VideoFilterTypeID_Bilinear;
    else if (string == "nearest2x")
        filter = VideoFilterTypeID_Nearest2X;
    else if (string == "nearest_1point5x")
        filter = VideoFilterTypeID_Nearest1_5X;
    else if (string == "nearestplus_1point5x")
        filter = VideoFilterTypeID_NearestPlus1_5X;
    else if (string == "epx")
        filter = VideoFilterTypeID_EPX;
    else if (string == "epxplus")
        filter = VideoFilterTypeID_EPXPlus;
    else if (string == "epx_1point5x")
        filter = VideoFilterTypeID_EPX1_5X;
    else if (string == "epxplus_1point5x")
        filter = VideoFilterTypeID_EPXPlus1_5X;
    else if (string == "2xbrz")
        filter = VideoFilterTypeID_2xBRZ;
    else if (string == "3xbrz")
        filter = VideoFilterTypeID_3xBRZ;
    else if (string == "4xbrz")
        filter = VideoFilterTypeID_4xBRZ;
    else if (string == "5xbrz")
        filter = VideoFilterTypeID_5xBRZ;
    video->ChangeFilterByID(filter);
    config.view_filter = filter;
    RedrawScreen();
    g_simple_action_set_state(action, parameter);
}

static void Modify_Interpolation(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    std::string string = g_variant_get_string(parameter, NULL);
    cairo_filter_t filter;
    if (string == "fast")
        filter = CAIRO_FILTER_FAST;
    else if (string == "good")
        filter = CAIRO_FILTER_GOOD;
    else if (string == "best")
        filter = CAIRO_FILTER_BEST;
    else if (string == "nearest")
        filter = CAIRO_FILTER_NEAREST;
    else if (string == "bilinear")
        filter = CAIRO_FILTER_BILINEAR;
    else if (string == "gaussian")
        filter = CAIRO_FILTER_GAUSSIAN;
    Interpolation = filter;
    config.view_cairoFilter = Interpolation;
    RedrawScreen();
    g_simple_action_set_state(action, parameter);
}

// Shared by the (now menu-less) spu-mode action and the sound settings dialog.
static void ApplySPUMode(spumode_enum mode)
{
    switch (mode) {
    case SPUMODE_SYNCN:
    case SPUMODE_SYNCZ:
#ifdef HAVE_LIBSOUNDTOUCH
    case SPUMODE_SYNCP:
#endif
        SPUMode = mode;
        SPU_SetSynchMode(1, mode-1);
        break;

    case SPUMODE_DUALASYNC:
    default:
        SPUMode = SPUMODE_DUALASYNC;
        SPU_SetSynchMode(0, 0);
        break;
    }
    config.audio_sync = SPUMode;
}

static void ApplySPUInterpolation(SPUInterpolationMode mode)
{
    CommonSettings.spuInterpolationMode = mode;
    config.audio_interpolation = CommonSettings.spuInterpolationMode;
}

static void Modify_SPUMode(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const char *string = g_variant_get_string(parameter, NULL);
    spumode_enum mode = SPUMODE_DUALASYNC;

    if (strcmp(string, "sync-n") == 0)
        mode = SPUMODE_SYNCN;
    else if (strcmp(string, "sync-z") == 0)
        mode = SPUMODE_SYNCZ;
#ifdef HAVE_LIBSOUNDTOUCH
    else if (strcmp(string, "sync-p") == 0)
        mode = SPUMODE_SYNCP;
#endif

    ApplySPUMode(mode);
    g_simple_action_set_state(action, parameter);
}

static void Modify_SPUInterpolation(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const char *string = g_variant_get_string(parameter, NULL);
    SPUInterpolationMode mode = (SPUInterpolationMode)CommonSettings.spuInterpolationMode;

    if (strcmp(string, "none") == 0)
        mode = SPUInterpolation_None;
    else if (strcmp(string, "linear") == 0)
        mode = SPUInterpolation_Linear;
    else if (strcmp(string, "cosine") == 0)
        mode = SPUInterpolation_Cosine;
    else if (strcmp(string, "catmullrom") == 0)
        mode = SPUInterpolation_CatmullRom;

    ApplySPUInterpolation(mode);
    g_simple_action_set_state(action, parameter);
}

static void Modify_Frameskip(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const char *string = g_variant_get_string(parameter, NULL);
    autoFrameskipMax = g_ascii_strtoll(string, NULL, 10);
    config.frameskip = autoFrameskipMax;
    if (!autoframeskip) {
        Frameskip = autoFrameskipMax;
    }
    char buf[255];
    snprintf(buf, sizeof buf, "Frameskip value is set to %s", string);
    driver->AddLine(buf);
    g_simple_action_set_state(action, parameter);
}

/////////////////////////////// TOOLS MANAGEMENT ///////////////////////////////

extern const dTool_t *dTools_list[];
extern const int dTools_list_size;

BOOL *dTools_running;

static void Start_dTool(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    (void)action;
    (void)parameter;
    int tool = GPOINTER_TO_INT(user_data);

    if(dTools_running == NULL || dTools_running[tool])
        return;

    dTools_list[tool]->open(tool);
    dTools_running[tool] = TRUE;
}

void dTool_CloseCallback(int tool)
{
    if (dTools_running == NULL)
        return;

    dTools_running[tool] = FALSE;
}


static inline void _updateDTools()
{
    if (dTools_running == NULL)
        return;

    for(int i = 0; i < dTools_list_size; i++) {
        if(dTools_running[i]) { dTools_list[i]->update(); }
    }
}

/////////////////////////////// MAIN EMULATOR LOOP ///////////////////////////////

// False while a core frame is executing.  Declared outside HAVE_LUA because
// EMU_IsAtFrameBoundary() is always part of GtkDriver.
static bool sLuaInFrame = false;

// Process one frame's input the same way Windows' StepRunLoop_Core() does:
// start processing, inject movie playback, let the Lua before-emulation hook
// adjust that same input, commit it once, then record the final input.
// Keeping playback inside begin/end is required because
// FCEUMOV_HandlePlayback() writes through NDS_getProcessingUserInput().
static void EmuProcessFrameInput()
{
    NDS_beginProcessingInput();
    {
        FCEUMOV_HandlePlayback();
#ifdef HAVE_LUA
        CallRegisteredLuaFunctions(LUACALL_BEFOREEMULATION);
#endif
    }
    NDS_endProcessingInput();
    FCEUMOV_HandleRecording();
}

static void EmuRunCoreFrame()
{
    EmuProcessFrameInput();
    desmume_cycle();    /* Emule ! */
}

#ifdef HAVE_LUA
// One emulated frame for the Lua-driven path (emu.frameadvance() and friends).
// Mirrors the body of EmuLoop() without its FPS/frameskip bookkeeping: while a
// script drives emulation, the script owns the frame cadence.
static void EmuStepFrame()
{
    if(!in_joy_config_mode)
        process_joystick_events(&keys_latch);
    update_keypad(keys_latch);
    if(touchpad.down) {
        NDS_setTouchPos(touchpad.x * 256, touchpad.y * 192);
        touchpad.down = 2;
    }
    if(touchpad.click) {
        NDS_releaseTouch();
        touchpad.click = 0;
    }

    EmuProcessFrameInput();

    sLuaInFrame = true;
    desmume_cycle();
    sLuaInFrame = false;

    CallRegisteredLuaFunctions(LUACALL_AFTEREMULATION);
    // Menu clicks and emu.registermenustart() refresh requests are drained on
    // the Lua/emulation thread at a frame boundary, as on macOS.
    GtkLua_DrainMenuCalls();
    CallRegisteredLuaFunctions(LUACALL_AFTEREMULATIONGUI);

    // Composite the Lua overlay and the built-in HUD into the frame now that
    // the script has had its chance to draw.
    PresentFrame();
}

// Pumps pending GTK events while a script blocks the main loop.  Throttled so
// a fast-forward loop does not repaint thousands of times per second, but
// often enough that Stop and window close stay responsive.
static gint64 sLuaLastEventPump = 0;

static void LuaPumpEvents(bool force)
{
    const gint64 now = g_get_monotonic_time();
    if (!force && now - sLuaLastEventPump < 16000)
        return;
    sLuaLastEventPump = now;

    // Bounded so a source that keeps re-queueing itself cannot livelock the
    // frame.  EmuLoop is removed for the duration, so it is not one of them.
    for (int i = 0; i < 256 && gtk_events_pending(); i++)
        gtk_main_iteration_do(FALSE);
}
#endif

class GtkDriver : public BaseDriver
{
public:
    virtual void EMU_DebugIdleUpdate()
    {
        usleep(1000);
        _updateDTools();
        while (gtk_events_pending())
            gtk_main_iteration();
    }

    // emu.frameadvance() asks for a repaint before its first frame so GUI
    // drawing from the script body is visible.
    virtual void USR_RefreshScreen()
    {
        RedrawScreen();
    }

    virtual void EMU_PauseEmulation(bool pause)
    {
        if(pause)
            desmume_pause();
        else
            desmume_resume();
    }

	// HUD uses this to show pause state
	virtual bool EMU_IsEmulationPaused() { return !desmume_running(); }

	virtual bool EMU_HasEmulationStarted() { return gameInfo.fROM != NULL; }

	// False while a core frame is executing, so emu.frameadvance() called from
	// a before/after-emulation callback falls back to emu.wait() instead of
	// recursively stepping the core.
	virtual bool EMU_IsAtFrameBoundary() { return !sLuaInFrame; }

#ifdef HAVE_LUA
	virtual eStepMainLoopResult EMU_StepMainLoop(bool allowSleep, bool allowPause, int frameSkip, bool disableUser, bool disableCore);
#endif

	virtual bool AVI_IsRecording()
	{
		bool recording = avout_x264.isRecording() || avout_flac.isRecording();
#ifdef HAVE_FFMPEG_RECORD
		recording = recording || avout_ffmpeg.isRecording();
#endif
		return recording;
	}

	virtual void AVI_SoundUpdate(void* soundData, int soundLen) {
		avout_flac.updateAudio(soundData, soundLen);
#ifdef HAVE_FFMPEG_RECORD
		avout_ffmpeg.updateAudio(soundData, soundLen);
#endif
	}
};

static bool sQuitRequested = false;

// Shared by File > Quit and by the window manager closing the main window.
// Script consoles are independent toplevels (not GtkApplication windows), so
// they have to be closed explicitly or they keep the process alive.
static void QuitApplication()
{
    emu_halt(EMUHALT_REASON_USER_REQUESTED_HALT, NDSErrorTag_None);
    if (regMainLoop) {
        g_source_remove(regMainLoop);
        regMainLoop = 0;
    }
#ifdef HAVE_LUA
    lua_script_close_all();
#endif
    if (pApp != NULL)
        g_application_quit(G_APPLICATION(pApp));
}

static void DoQuit(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    if (sQuitRequested)
        return;
    sQuitRequested = true;

    QuitApplication();
    if (pWindow != NULL)
        gtk_window_close(GTK_WINDOW(pWindow));
}

static void OnMainWindowDestroy(GtkWidget *widget, gpointer user_data)
{
    if (sQuitRequested)
        return;
    sQuitRequested = true;
    QuitApplication();
}


// Runs exactly one frame through the same pipeline EmuLoop uses: merge the
// joystick into the key latch, hand the result to the core, run the Lua frame
// callbacks, then present.  Split out of EmuLoop so frame advance can step a
// single frame while the emulator is halted without duplicating the input path.
static void EmuSingleFrame()
{
    /* Merge the joystick keys with the keyboard ones */
    if(!in_joy_config_mode)
        process_joystick_events(&keys_latch);
    /* Update! */
    update_keypad(keys_latch);
    /* Update mouse position and click (source - game controller touchpad)*/
    if(touchpad.down) {
        NDS_setTouchPos(touchpad.x * 256, touchpad.y * 192);
        touchpad.down = 2;
    }
    if(touchpad.click)
    {
        NDS_releaseTouch();
        touchpad.click = 0;
    }

    EmuRunCoreFrame();

#ifdef HAVE_LUA
    CallRegisteredLuaFunctions(LUACALL_AFTEREMULATION);
    // See EmuStepFrame(): menu handlers run at a frame boundary, never from
    // the GTK action callback.
    GtkLua_DrainMenuCalls();
    CallRegisteredLuaFunctions(LUACALL_AFTEREMULATIONGUI);
#endif

    // The Lua GUI callback (if any) has drawn into the source buffer; overlay
    // the built-in HUD, filter, and present the finished frame.
    PresentFrame();

    _updateDTools();
}

gboolean EmuLoop(gpointer data)
{
    /* SDL3's SDL_GetTicks() returns Uint64 milliseconds. */
    static Uint64 fps_SecStart, next_fps_SecStart;
    static Uint32 fps_FrameCount, skipped_frames;
    static Uint64 next_frame_time;
    static int frame_mod3;
    unsigned int i;
    gchar Title[50];

    if (!desmume_running()) {
      // Set the next frame time to 0 so that it will recount
      next_frame_time = 0;
      frame_mod3 = 0;
      gtk_window_set_title(GTK_WINDOW(pWindow), "DeSmuME - Paused");
      fps_SecStart = 0;
      if (regMainLoop) {
          g_source_remove(regMainLoop);
          regMainLoop = 0;
      }
      RedrawScreen();
      return FALSE;
    }

    if (sResyncTiming) {
        // A frame was advanced while paused; start the clock over so the
        // first frame after resuming is not treated as falling behind.
        sResyncTiming = false;
        next_frame_time = 0;
        frame_mod3 = 0;
        fps_SecStart = 0;
    }

    fps_FrameCount += Frameskip + 1;
    next_fps_SecStart = SDL_GetTicks();

    if (fps_SecStart == 0) {
    	fps_SecStart = next_fps_SecStart;
        fps_FrameCount = 0;
        gtk_window_set_title(GTK_WINDOW(pWindow), "DeSmuME - Running");
    }

    bool oneSecond = false;
    if ((next_fps_SecStart - fps_SecStart) >= 1000) {
        oneSecond = true;
        fps_SecStart = next_fps_SecStart;

        float emu_ratio = fps_FrameCount / 60.0;
        LOG("auto: %d fps: %u skipped: %u emu_ratio: %f Frameskip: %u\n", autoframeskip, fps_FrameCount, skipped_frames, emu_ratio, Frameskip);

        snprintf(Title, sizeof(Title), "DeSmuME - %dfps, %d skipped, draw: %dfps", fps_FrameCount, skipped_frames, draw_count);
        gtk_window_set_title(GTK_WINDOW(pWindow), Title);

#ifdef HAVE_LIBAGG
        Hud.fps = fps_FrameCount;
#endif

        fps_FrameCount = 0;
        skipped_frames = 0;
        draw_count = 0;
    }

	// HUD display things (copied from Windows main.cpp)
#ifdef HAVE_LIBAGG
	Hud.fps3d = GPU->GetFPSRender3D();
	
	if (nds.idleFrameCounter==0 || oneSecond)
	{
		u32 loadAvgARM9;
		u32 loadAvgARM7;
		NDS_GetCPULoadAverage(loadAvgARM9, loadAvgARM7);
		
		Hud.cpuload[ARMCPU_ARM9] = (int)loadAvgARM9;
		Hud.cpuload[ARMCPU_ARM7] = (int)loadAvgARM7;

	}
	Hud.cpuloopIterationCount = nds.cpuloopIterationCount;
#endif

    /* Input, core, Lua callbacks and presentation for this frame */
    EmuSingleFrame();


    if (!config.fpslimiter || keys_latch & KEYMASK_(KEY_BOOST - 1)) {
        if (autoframeskip) {
            Frameskip = 0;
        } else {
            for (i = 0; i < Frameskip; i++) {
                NDS_SkipNextFrame();
#ifdef HAVE_LIBAGG
                Hud.fps3d = GPU->GetFPSRender3D();
#endif
                EmuRunCoreFrame();
                skipped_frames++;
            }
        }
        next_frame_time = SDL_GetTicks() + 16;
        frame_mod3 = 0;
    } else {
        if (!autoframeskip) {
            for (i = 0; i < Frameskip; i++) {
                NDS_SkipNextFrame();
#ifdef HAVE_LIBAGG
                Hud.fps3d = GPU->GetFPSRender3D();
#endif
                EmuRunCoreFrame();
                skipped_frames++;
                // Update next frame time
                frame_mod3 = (frame_mod3 + 1) % 3;
                next_frame_time += (frame_mod3 == 0) ? 16 : 17;
            }
        }
        Uint64 this_tick = SDL_GetTicks();
        if (this_tick < next_frame_time) {
            Uint64 timeleft = next_frame_time - this_tick;
            usleep((timeleft - 1) * 1000);
            while (SDL_GetTicks() < next_frame_time);
            this_tick = SDL_GetTicks();
        }
        if (autoframeskip) {
            // Determine the auto frameskip value, maximum 4
            for (Frameskip = 0; this_tick > next_frame_time && Frameskip < autoFrameskipMax; Frameskip++, this_tick = SDL_GetTicks()) {
                // Aggressively skip frames to avoid delay
                NDS_SkipNextFrame();
#ifdef HAVE_LIBAGG
                Hud.fps3d = GPU->GetFPSRender3D();
#endif
                EmuRunCoreFrame();
                skipped_frames++;
                // Update next frame time
                frame_mod3 = (frame_mod3 + 1) % 3;
                next_frame_time += (frame_mod3 == 0) ? 16 : 17;
            }
            if (Frameskip > autoFrameskipMax) {
                Frameskip = autoFrameskipMax;
            }
            //while (SDL_GetTicks() < next_frame_time);
            //this_tick = SDL_GetTicks();
        }
        if (this_tick > next_frame_time && this_tick - next_frame_time >= 17) {
            // If we fall too much behind, don't try to catch up.
            next_frame_time = this_tick;
        }
        // We want to achieve a total 17 + 17 + 16 = 50 ms for every 3 frames.
        frame_mod3 = (frame_mod3 + 1) % 3;
        next_frame_time += (frame_mod3 == 0) ? 16 : 17;
    }

    return TRUE;
}

#ifdef HAVE_LUA
BaseDriver::eStepMainLoopResult GtkDriver::EMU_StepMainLoop(bool allowSleep, bool allowPause, int frameSkip, bool disableUser, bool disableCore)
{
    // The script runs synchronously on the GTK main thread.  Take the idle
    // source that normally drives emulation out of the loop; otherwise pumping
    // events below would run a second frame on top of this one.  It is put back
    // before returning, so normal emulation resumes as soon as the script ends.
    const bool hadMainLoop = (regMainLoop != 0);
    if (hadMainLoop)
    {
        g_source_remove(regMainLoop);
        regMainLoop = 0;
    }

    // Paused: keep the UI alive so Run/Stop still work, and let StepEmulation()
    // call us again.  This is the same contract as Windows' StepRunLoop_Paused.
    if (!desmume_running() && allowPause)
    {
        LuaPumpEvents(true);
        g_usleep(10000);
        if (hadMainLoop && regMainLoop == 0 && desmume_running())
            regMainLoop = g_idle_add_full(EMULOOP_PRIO, &EmuLoop, pWindow, NULL);
        return ESTEP_CALL_AGAIN;
    }

    if (!disableCore && gameInfo.fROM != NULL)
    {
        // frameSkip only suppresses rendering; the core still executes exactly
        // one frame per call, matching Windows' StepRunLoop_Throttle().
        static int sLuaSkipCounter = 0;
        const int skipRate = (frameSkip < 0) ? (int)Frameskip : frameSkip;
        if (skipRate > 0)
        {
            if (sLuaSkipCounter > 0)
            {
                NDS_SkipNextFrame();
                sLuaSkipCounter--;
            }
            else
            {
                sLuaSkipCounter = skipRate;
            }
        }
        else
        {
            sLuaSkipCounter = 0;
        }

        EmuStepFrame();
    }

    if (!disableUser)
        _updateDTools();

    // Pace only when the script asked for normal speed.  Pump events while
    // waiting so Stop and repainting keep working even during the sleep.
    static gint64 sLuaNextFrame = 0;
    const bool throttle = allowSleep && config.fpslimiter && !(keys_latch & KEYMASK_(KEY_BOOST - 1));
    if (throttle && !disableCore)
    {
        const gint64 now = g_get_monotonic_time();
        if (sLuaNextFrame == 0 || sLuaNextFrame < now - 100000)
            sLuaNextFrame = now;
        sLuaNextFrame += 16667;

        while (g_get_monotonic_time() < sLuaNextFrame)
        {
            LuaPumpEvents(false);
            g_usleep(1000);
        }
    }
    else
    {
        sLuaNextFrame = 0;
        LuaPumpEvents(false);
    }

    if (hadMainLoop)
        regMainLoop = g_idle_add_full(EMULOOP_PRIO, &EmuLoop, pWindow, NULL);

    return ESTEP_DONE;
}
#endif

GMenuModel *savestates_menu;
GMenuModel *loadstates_menu;

// The following functions are adapted from the Windows port:
//     UpdateSaveStateMenu, ResetSaveStateTimes, LoadSaveStateInfo
static void UpdateSaveStateMenu(int pos, char* txt)
{
    GMenuItem *item;
    gint position = ((pos == 0) ? 10 : pos) - 1;

    item = g_menu_item_new_from_model(savestates_menu, position);
    g_menu_item_set_label(item, txt);
    g_menu_remove(G_MENU(savestates_menu), position);
    g_menu_insert_item(G_MENU(savestates_menu), position, item);

    item = g_menu_item_new_from_model(loadstates_menu, position);
    g_menu_item_set_label(item, txt);
    g_menu_remove(G_MENU(loadstates_menu), position);
    g_menu_insert_item(G_MENU(loadstates_menu), position, item);
}

static void ResetSaveStateTimes()
{
	char ntxt[64];
	for(int i = 0; i < NB_STATES;i++)
	{
		snprintf(ntxt, sizeof(ntxt), "_%d", i);
		UpdateSaveStateMenu(i, ntxt);
	}
}

static void LoadSaveStateInfo()
{
	scan_savestates();
	char ntxt[128];
	for(int i = 0; i < NB_STATES; i++)
	{
		if(savestates[i].exists)
		{
			snprintf(ntxt, sizeof(ntxt), "_%d    %s", i, savestates[i].date);
			UpdateSaveStateMenu(i, ntxt);
		}
	}
}

static void changesavetype(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const char *string = g_variant_get_string(parameter, NULL);
    int savetype;
    if (strcmp(string, "autodetect") == 0)
        savetype = 0;
    else if (strcmp(string, "eeprom-4k") == 0)
        savetype = 1;
    else if (strcmp(string, "eeprom-64k") == 0)
        savetype = 2;
    else if (strcmp(string, "eeprom-512k") == 0)
        savetype = 3;
    else if (strcmp(string, "fram-256k") == 0)
        savetype = 4;
    else if (strcmp(string, "flash-2m") == 0)
        savetype = 5;
    else if (strcmp(string, "flash-4m") == 0)
        savetype = 6;
    backup_setManualBackupType(savetype);
    config.savetype=savetype;
    g_simple_action_set_state(action, parameter);
}

static void desmume_gtk_menu_tool_layers(GtkApplication *app)
{
    const char *Layers_Menu[10][3] = {
        {"0", "layermainbg0","_0 Main BG 0"},
        {"1", "layermainbg1","_1 Main BG 1"},
        {"2", "layermainbg2","_2 Main BG 2"},
        {"3", "layermainbg3","_3 Main BG 3"},
        {"4", "layermainobj","_4 Main OBJ"},
        {"5", "layersubbg0", "_5 SUB BG 0"},
        {"6", "layersubbg1", "_6 SUB BG 1"},
        {"7", "layersubbg2", "_7 SUB BG 2"},
        {"8", "layersubbg3", "_8 SUB BG 3"},
        {"9", "layersubobj", "_9 SUB OBJ"}
    };

    std::vector<GActionEntry> entries;
    for (int i = 0; i < 10; i++) {
        GActionEntry entry = {Layers_Menu[i][1], ToggleLayerVisibility, NULL, "true", NULL};
        entries.push_back(entry);
    }
    g_action_map_add_action_entries(G_ACTION_MAP(app), entries.data(), entries.size(), NULL);
}

#ifdef HAVE_LIBAGG

static void ToggleHudDisplay(hud_display_enum hudId, gboolean active)
{
    switch (hudId) {
    case HUD_DISPLAY_FPS:
        CommonSettings.hud.FpsDisplay = active;
        config.hud_fps = active;
        break;
    case HUD_DISPLAY_INPUT:
        CommonSettings.hud.ShowInputDisplay = active;
        config.hud_input = active;
        break;
    case HUD_DISPLAY_GINPUT:
        CommonSettings.hud.ShowGraphicalInputDisplay = active;
        config.hud_graphicalInput = active;
        break;
    case HUD_DISPLAY_FCOUNTER:
        CommonSettings.hud.FrameCounterDisplay = active;
        config.hud_frameCounter = active;
        break;
    case HUD_DISPLAY_LCOUNTER:
        CommonSettings.hud.ShowLagFrameCounter = active;
        config.hud_lagCounter = active;
        break;
    case HUD_DISPLAY_RTC:
        CommonSettings.hud.ShowRTC = active;
        config.hud_rtc = active;
        break;
    case HUD_DISPLAY_MIC:
        CommonSettings.hud.ShowMicrophone = active;
        config.hud_mic = active;
        break;
    case HUD_DISPLAY_EDITOR:
        HudEditorMode = active;
        if(!active)
			HudSaveLayout();
        break;
    default:
        g_printerr("Unknown HUD toggle %u!", hudId);
        break;
    }
    RedrawScreen();
}

#define HudMacro(func_name, enum_value) \
static void func_name(GSimpleAction *action, GVariant *parameter, gpointer user_data) \
{ \
    GVariant *variant = g_action_get_state(G_ACTION(action)); \
    gboolean value = !g_variant_get_boolean(variant); \
    g_simple_action_set_state(action, g_variant_new_boolean(value)); \
    ToggleHudDisplay(enum_value, value); \
}

HudMacro(HudFps, HUD_DISPLAY_FPS)
HudMacro(HudInput, HUD_DISPLAY_INPUT)
HudMacro(HudGraphicalInput, HUD_DISPLAY_GINPUT)
HudMacro(HudFrameCounter, HUD_DISPLAY_FCOUNTER)
HudMacro(HudLagCounter, HUD_DISPLAY_LCOUNTER)
HudMacro(HudRtc, HUD_DISPLAY_RTC)
HudMacro(HudMic, HUD_DISPLAY_MIC)
HudMacro(HudEditor, HUD_DISPLAY_EDITOR)

static void HudResetLayout(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	Hud.reset();
	HudSaveLayout();
}

static void HudSaveCoordsToVector(HudCoordinates* pCoords, int* pDest)
{
	pDest[0]=pCoords->x;
	pDest[1]=pCoords->y;
	pDest[2]=pCoords->xsize;
	pDest[3]=pCoords->ysize;
}

static void HudLoadCoordsFromVector(HudCoordinates* pCoords, int* pSrc)
{
	pCoords->x=pSrc[0];
	pCoords->y=pSrc[1];
	pCoords->xsize=pSrc[2];
	pCoords->ysize=pSrc[3];
}

static void HudSaveLayout()
{
	std::vector<int> vec(8*4); //8 HudCoordinates
	HudSaveCoordsToVector(&Hud.SavestateSlots, vec.data());
	HudSaveCoordsToVector(&Hud.FpsDisplay, vec.data()+4);
	HudSaveCoordsToVector(&Hud.FrameCounter, vec.data()+8);
	HudSaveCoordsToVector(&Hud.InputDisplay, vec.data()+12);
	HudSaveCoordsToVector(&Hud.GraphicalInputDisplay, vec.data()+16);
	HudSaveCoordsToVector(&Hud.LagFrameCounter, vec.data()+20);
	HudSaveCoordsToVector(&Hud.Microphone, vec.data()+24);
	HudSaveCoordsToVector(&Hud.RTCDisplay, vec.data()+28);
	config.hud_layout=vec;
}

static void HudLoadLayout()
{
	std::vector<int> vec=config.hud_layout;
	if(vec.size()==8*4)
	{
		HudLoadCoordsFromVector(&Hud.SavestateSlots, vec.data());
		HudLoadCoordsFromVector(&Hud.FpsDisplay, vec.data()+4);
		HudLoadCoordsFromVector(&Hud.FrameCounter, vec.data()+8);
		HudLoadCoordsFromVector(&Hud.InputDisplay, vec.data()+12);
		HudLoadCoordsFromVector(&Hud.GraphicalInputDisplay, vec.data()+16);
		HudLoadCoordsFromVector(&Hud.LagFrameCounter, vec.data()+20);
		HudLoadCoordsFromVector(&Hud.Microphone, vec.data()+24);
		HudLoadCoordsFromVector(&Hud.RTCDisplay, vec.data()+28);
	}
	else
		Hud.reset();
}

static void desmume_gtk_menu_view_hud(GtkApplication *app)
{
    const struct {
        const gchar* name;
        bool active;
        bool& setting;
    } hud_menu[] = {
        { "hud-fps", config.hud_fps, CommonSettings.hud.FpsDisplay },
        { "hud-input", config.hud_input, CommonSettings.hud.ShowInputDisplay },
        { "hud-graphicalinput", config.hud_graphicalInput, CommonSettings.hud.ShowGraphicalInputDisplay },
        { "hud-framecounter", config.hud_frameCounter, CommonSettings.hud.FrameCounterDisplay },
        { "hud-lagcounter", config.hud_lagCounter, CommonSettings.hud.ShowLagFrameCounter },
        { "hud-rtc", config.hud_rtc, CommonSettings.hud.ShowRTC },
        { "hud-mic", config.hud_mic, CommonSettings.hud.ShowMicrophone },
    };
    guint i;

    for(i = 0; i < sizeof(hud_menu) / sizeof(hud_menu[0]); i++){
        GSimpleAction *action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), hud_menu[i].name));
        g_simple_action_set_state(action, g_variant_new_boolean(hud_menu[i].active ? TRUE : FALSE));
        hud_menu[i].setting = hud_menu[i].active;
    }
}
#endif

/* Cheats are applied by the core, so this only stops it from doing so. The
 * list is left untouched, which is what makes it a useful debugging switch: a
 * cheat that breaks a game can be turned off without losing the entry. */
static void ToggleCheatsDisabled(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    g_variant_unref(variant);

    config.cheats_disabled = value;
    CommonSettings.cheatsDisable = value;
    UpdateStatusBar(value ? "Cheats disabled" : "Cheats enabled");
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

#ifdef FAKE_MIC
static void ToggleMicNoise(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean doNoise = !g_variant_get_boolean(variant);

    Mic_DoNoise(doNoise);
    if (doNoise)
       driver->AddLine("Fake mic enabled");
    else
       driver->AddLine("Fake mic disabled");
    RedrawScreen();
    g_simple_action_set_state(action, g_variant_new_boolean(doNoise));
}
#endif

static void ToggleFpsLimiter(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    config.fpslimiter = value;

    if (config.fpslimiter)
       driver->AddLine("Fps limiter enabled");
    else
       driver->AddLine("Fps limiter disabled");
    RedrawScreen();
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

static void ToggleAutoFrameskip(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *variant = g_action_get_state(G_ACTION(action));
    gboolean value = !g_variant_get_boolean(variant);
    config.autoframeskip = value;

    if (config.autoframeskip) {
        autoframeskip = true;
        Frameskip = 0;
        driver->AddLine("Auto frameskip enabled");
    } else {
        autoframeskip = false;
        Frameskip = autoFrameskipMax;
        driver->AddLine("Auto frameskip disabled");
    }
    RedrawScreen();
    g_simple_action_set_state(action, g_variant_new_boolean(value));
}

static void desmume_gtk_menu_tools(GtkApplication *app)
{
    // Each tool gets its own parameterless action. The old code registered
    // parameterized "u" actions but menu.ui did not supply a target value,
    // which made every Tools menu item permanently insensitive.
    for (int i = 0; i < dTools_list_size; i++) {
        GSimpleAction *action = g_simple_action_new(dTools_list[i]->shortname, NULL);
        g_signal_connect(action, "activate", G_CALLBACK(Start_dTool), GINT_TO_POINTER(i));
        g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action));
        g_object_unref(action);
    }
}

static gboolean timeout_exit_cb(gpointer data)
{
    DoQuit(NULL, NULL, NULL);
    INFO("Quit after %d seconds timeout\n", GPOINTER_TO_INT(data));

    return FALSE;
}

static gboolean OutOfLoopJoyDeviceCheckTimerFunc(gpointer data)
{
    if(!regMainLoop && !in_joy_config_mode)
        process_joystick_device_events();
    return !regMainLoop;
}

static void
common_gtk_main(GApplication *app, gpointer user_data)
{
    configured_features *my_config = static_cast<configured_features*>(user_data);

    config.load();
    ApplyFirmwareSettingsConfig();
    ApplyMicrophoneSettingsConfig();
    ApplyWifiSettingsConfig();
    ApplyEmulationSettingsConfig();
    // Push the Config > Path Settings folders into the path model before any
    // ROM is opened, so battery, state slots and cheat files land where they
    // belong.
    path_settings_apply_config();

#if !defined(ENABLE_WGPU)
	// A config written by a WebGPU build may still request the WebGPU
	// presenter or one of its GPU filters. Neither exists in this build.
	config.presentationWGPU = false;
	config.gpuFilter = WGPUPresentUpscaler_Off;
#endif

	// The GPU filter belongs to the WebGPU presentation path. Nothing is
	// compiled here: the chain of the selected filter is built on the first
	// frame presented with it selected. A value left over from another build
	// falls back to the plain blit.
	if (!WGPUPresent::isUpscaler(config.gpuFilter))
	{
		config.gpuFilter = WGPUPresentUpscaler_Off;
	}
	wgpu_display.setUpscaler(config.gpuFilter);

	// Must exist before any renderer index is validated or shown in the UI.
	Build3DRendererList();

    driver = new GtkDriver();

#ifdef HAVE_LUA
    // Give the engine the UI hooks that need a real frontend.  Without these,
    // gui.popup() returns a fixed answer and emu.openscript() does nothing.
    SetLuaPopupHandler(GtkLua_ShowPopup);
    SetLuaOpenScriptHandler(GtkLua_OpenScript);
    SetLuaInputStatusHandler(GtkLua_GetInputStatus);
    SetLuaGetControllerStatusHandler(GtkLua_GetControllerStatus);
    SetLuaMenuHooks(&sGtkLuaMenuHooks);
#endif

    SDL_TimerID limiter_timer = 0;

    /* use any language set on the command line */
    if ( my_config->firmware_language != -1) {
		CommonSettings.fwConfig.language = my_config->firmware_language;
    }

    //------------------addons----------
    slot1_Init();
    slot2_Init();

    slot1_SetFatDir(config.slot1_fat_path.get(), config.slot1_fat_path_type);
    if (!config.slot1_debug_path.get().empty())
        path.setpath(path.SLOT1D, config.slot1_debug_path.get());
    int slot1Id = config.slot1_id;
    if (slot1Id < 0 || slot1Id > 0xFF)
        slot1Id = 0xFE;
    NDS_SLOT1_TYPE slot1_device_type = NDS_SLOT1_RETAIL_AUTO;
    slot1_getTypeByID((u8)slot1Id, slot1_device_type);
    slot1_Change(slot1_device_type);

    const int configuredCFlashMode = CLAMP((int)config.slot2_cflash_mode,
                                           ADDON_CFLASH_MODE_Path, ADDON_CFLASH_MODE_RomPath);
    CFlash_Mode = (ADDON_CFLASH_MODE)configuredCFlashMode;
    CFlash_Path = config.slot2_cflash_path.get();
    GBACartridge_RomPath = config.slot2_gba_rom.get();
    if (!GBACartridge_RomPath.empty())
    {
        const size_t extension = GBACartridge_RomPath.find_last_of('.');
        const size_t separator = GBACartridge_RomPath.find_last_of("/\\");
        GBACartridge_SRAMPath = extension == std::string::npos ||
                                (separator != std::string::npos && extension < separator)
                                ? GBACartridge_RomPath + ".sav"
                                : GBACartridge_RomPath.substr(0, extension) + ".sav";
    }

    my_config->process_addonCommands();

    int slot2Id = config.slot2_id;
    if (slot2Id < 0 || slot2Id > 0xFF)
        slot2Id = 0xFE;
    NDS_SLOT2_TYPE slot2_device_type = NDS_SLOT2_AUTO;
    slot2_getTypeByID((u8)slot2Id, slot2_device_type);

    if (my_config->is_cflash_configured)
        slot2_device_type = NDS_SLOT2_CFLASH;

    if(my_config->gbaslot_rom != "") {

        //set the GBA rom and sav paths
        GBACartridge_RomPath = my_config->gbaslot_rom.c_str();
        const size_t extension = GBACartridge_RomPath.find_last_of('.');
        const size_t separator = GBACartridge_RomPath.find_last_of("/\\");
        GBACartridge_SRAMPath = extension == std::string::npos ||
                                (separator != std::string::npos && extension < separator)
                                ? GBACartridge_RomPath + ".sav"
                                : GBACartridge_RomPath.substr(0, extension) + ".sav";
        slot2_device_type = NDS_SLOT2_GBACART;
    }
    else if (slot2_device_type == NDS_SLOT2_GBACART)
    {
        FILE *test = fopen(GBACartridge_RomPath.c_str(), "rb");
        if (test != NULL)
            fclose(test);
        else
            slot2_device_type = NDS_SLOT2_NONE;
    }

	switch (slot2_device_type)
	{
		case NDS_SLOT2_NONE:
			break;
		case NDS_SLOT2_AUTO:
			break;
		case NDS_SLOT2_CFLASH:
			break;
		case NDS_SLOT2_RUMBLEPAK:
			break;
		case NDS_SLOT2_GBACART:
			break;
		case NDS_SLOT2_GUITARGRIP:
			break;
		case NDS_SLOT2_EXPMEMORY:
			break;
		case NDS_SLOT2_EASYPIANO:
			break;
		case NDS_SLOT2_PADDLE:
			break;
		case NDS_SLOT2_PASSME:
			break;
		default:
			slot2_device_type = NDS_SLOT2_NONE;
			break;
	}
	
    slot2_Change(slot2_device_type);

    /* FIXME: SDL_INIT_VIDEO is needed for joystick support to work!?
     * Perhaps it needs a "window" to catch events...? */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK|SDL_INIT_GAMEPAD)) {
        g_printerr("Error trying to initialize SDL: %s\n",
                    SDL_GetError());
        // TODO: return a non-zero exit status.
        g_application_quit(app);
    }
	
    desmume_init( my_config->disable_sound || !config.audio_enabled);

	// desmume_init() always brings up the SDL core, so honour a different one
	// from the sound settings dialog here.
	if (!my_config->disable_sound && config.audio_enabled &&
	    config.audio_core.get() != SNDCORE_SDL)
	{
		SPU_ChangeSoundCore(config.audio_core.get(), 735 * 4);
	}
	
    gpu_scale_factor = config.gpuScaleFactor;
    if(gpu_scale_factor < GPU_SCALE_FACTOR_MIN)
        gpu_scale_factor = GPU_SCALE_FACTOR_MIN;
    if(gpu_scale_factor > GPU_SCALE_FACTOR_MAX)
        gpu_scale_factor = GPU_SCALE_FACTOR_MAX;
    config.gpuScaleFactor = gpu_scale_factor;
	
    gtk3gpuEvent = new Gtk3GPUEventHandler;
    GPU->SetEventHandler(gtk3gpuEvent);
    gtk3gpuEvent->SetFramebufferDimensions( (size_t)(((float)GPU_FRAMEBUFFER_NATIVE_WIDTH * gpu_scale_factor) + 0.5), (size_t)(((float)GPU_FRAMEBUFFER_NATIVE_HEIGHT * gpu_scale_factor) + 0.5) );
	
    const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
    video = new VideoFilter(dispInfo.customWidth, dispInfo.customHeight * 2, VideoFilterTypeID_None, ResolveVideoFilterThreadCount(config.view_filterThreads));

    /* Init the hud / osd stuff */
#ifdef HAVE_LIBAGG
	/* SDL3 returns a const pointer and takes a display ID rather than
	   filling in a caller-provided struct. */
	const SDL_DisplayMode *cur_mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
	if(cur_mode != NULL)
		aggDraw.screenBytesPerPixel = SDL_BYTESPERPIXEL(cur_mode->format);
	
    Desmume_InitOnce();
    Hud.reset();
    HudLoadLayout();
    osd = new OSDCLASS(-1);
	
#ifdef AGG2D_USE_VECTORFONTS
	if (vectorFontFile.size() > 0)
	{
		aggDraw.hud->setVectorFont(vectorFontFile, VECTOR_FONT_BASE_SIZE, true);
		osd->useVectorFonts = false;
	}
	else
	{
		osd->useVectorFonts = false;
	}
#endif // AGG2D_USE_VECTORFONTS
	
	Agg_setCustomSize(video->GetSrcWidth(), video->GetSrcHeight());
	osd->scale = 1.0;
	aggDraw.hud->setDrawTargetDims( (u8 *)video->GetSrcBufferPtr(), video->GetSrcWidth(), video->GetSrcHeight(), video->GetSrcWidth() * sizeof(u32) );
#endif // HAVE_LIBAGG

    /*
     * Activate the GDB stubs
     * This has to come after the NDS_Init (called in desmume_init)
     * where the cpus are set up.
     */
#ifdef GDB_STUB
    gdbstub_mutex_init();

    if ( my_config->arm9_gdb_port > 0) {
        arm9_gdb_stub = createStub_gdb( my_config->arm9_gdb_port,
                                         &NDS_ARM9,
                                         &arm9_direct_memory_iface);
		
        if ( arm9_gdb_stub == NULL) {
            g_printerr("Failed to create ARM9 gdbstub on port %d\n",
                       my_config->arm9_gdb_port);
            exit( -1);
        }
        else {
            activateStub_gdb( arm9_gdb_stub);
        }
    }
    if ( my_config->arm7_gdb_port > 0) {
        arm7_gdb_stub = createStub_gdb( my_config->arm7_gdb_port,
                                         &NDS_ARM7,
                                         &arm7_base_memory_iface);
		
        if ( arm7_gdb_stub == NULL) {
            g_printerr("Failed to create ARM7 gdbstub on port %d\n",
                       my_config->arm7_gdb_port);
            exit( -1);
        }
        else {
            activateStub_gdb( arm7_gdb_stub);
        }
    }
#endif

    /* Initialize joysticks */
    if(!init_joy())
        // TODO: return a non-zero exit status.
        g_application_quit(app);

    dTools_running = (BOOL*)malloc(sizeof(BOOL) * dTools_list_size);
    if (dTools_running != NULL)
      memset(dTools_running, FALSE, sizeof(BOOL) * dTools_list_size);

    keyfile = desmume_config_read_file(gtk_kb_cfg);
    // Menu shortcuts live in the same config file, but outside the KEYS group.
    HotkeysLoad();
    ctrls_set_allow_up_down(config.allow_up_down);

    memset(&nds_screen, 0, sizeof(nds_screen));
    nds_screen.orientation = ORIENT_VERTICAL;

    /* Fetch the main elements from the window */
    GtkBuilder *builder = gtk_builder_new_from_resource("/org/desmume/DeSmuME/main.ui");
    pWindow = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    pToolBar = GTK_WIDGET(gtk_builder_get_object(builder, "toolbar"));
    pDrawingArea = GTK_WIDGET(gtk_builder_get_object(builder, "drawing-area"));
    pStatusBar = GTK_WIDGET(gtk_builder_get_object(builder, "status-bar"));
    pContentBox = GTK_WIDGET(gtk_builder_get_object(builder, "content-box"));
    g_object_unref(builder);

    /* Set colors for content box background and status bar text */
    GdkRGBA color_black = { 0.0, 0.0, 0.0, 1.0 };
    gtk_widget_override_background_color(pContentBox, GTK_STATE_FLAG_NORMAL, &color_black);

    GdkRGBA color_soft_gray = { 0.8, 0.8, 0.8, 1.0 };
    gtk_widget_override_color(pStatusBar, GTK_STATE_FLAG_NORMAL, &color_soft_gray);

    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(pWindow));

    g_signal_connect(G_OBJECT(pWindow), "destroy", G_CALLBACK(OnMainWindowDestroy), NULL);
    g_signal_connect(G_OBJECT(pWindow), "key_press_event", G_CALLBACK(Key_Press), NULL);
    g_signal_connect(G_OBJECT(pWindow), "key_release_event", G_CALLBACK(Key_Release), NULL);
#ifdef HAVE_LUA
    g_signal_connect(G_OBJECT(pWindow), "focus-out-event", G_CALLBACK(LuaWindowFocusOut), NULL);
#endif

    /* Update audio toggle status */
    if (my_config->disable_sound || !config.audio_enabled) {
        g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "enableaudio")), g_variant_new_boolean(FALSE));
    }
    desmume_gtk_menu_tool_layers(GTK_APPLICATION(app));
#ifdef HAVE_LIBAGG
    desmume_gtk_menu_view_hud(GTK_APPLICATION(app));
#endif
    desmume_gtk_menu_tools(GTK_APPLICATION(app));
    my_config->savetype=config.savetype;
    std::string string;
    switch (my_config->savetype) {
        case 0:
            string = "autodetect";
            break;
        case 1:
            string = "eeprom-4k";
            break;
        case 2:
            string = "eeprom-64k";
            break;
        case 3:
            string = "eeprom-512k";
            break;
        case 4:
            string = "fram-256k";
            break;
        case 5:
            string = "flash-2m";
            break;
        case 6:
            string = "flash-4m";
            break;
    }
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "savetype")), g_variant_new_string(string.c_str()));

    if (config.view_cairoFilter < CAIRO_FILTER_FAST || config.view_cairoFilter > CAIRO_FILTER_BILINEAR) {
    	config.view_cairoFilter = CAIRO_FILTER_NEAREST;
    }
    Interpolation = (cairo_filter_t)config.view_cairoFilter.get();
    switch (Interpolation) {
        case CAIRO_FILTER_FAST:
            string = "fast";
            break;
        case CAIRO_FILTER_NEAREST:
            string = "nearest";
            break;
        case CAIRO_FILTER_GOOD:
            string = "good";
            break;
        case CAIRO_FILTER_BILINEAR:
            string = "bilinear";
            break;
        case CAIRO_FILTER_BEST:
            string = "best";
            break;
    }
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "interpolation")), g_variant_new_string(string.c_str()));

    if (config.view_filter < VideoFilterTypeID_None || config.view_filter >= VideoFilterTypeIDCount) {
        config.view_filter = VideoFilterTypeID_None;
    }
    video->ChangeFilterByID((VideoFilterTypeID)config.view_filter.get());
    g_printerr("Using %d threads for video filter.\n", (int)video->GetThreadCount());
    switch (config.view_filter) {
		case VideoFilterTypeID_None:
			string = "none";
			break;
		case VideoFilterTypeID_LQ2X:
			string = "lq2x";
			break;
		case VideoFilterTypeID_LQ2XS:
			string = "lq2xs";
			break;
		case VideoFilterTypeID_HQ2X:
			string = "hq2x";
			break;
		case VideoFilterTypeID_HQ2XS:
			string = "hq2xs";
			break;
		case VideoFilterTypeID_HQ3X:
			string = "hq3x";
			break;
		case VideoFilterTypeID_HQ3XS:
			string = "hq3xs";
			break;
		case VideoFilterTypeID_HQ4X:
			string = "hq4x";
			break;
		case VideoFilterTypeID_HQ4XS:
			string = "hq4xs";
			break;
		case VideoFilterTypeID_2xSaI:
			string = "2xsai";
			break;
		case VideoFilterTypeID_Super2xSaI:
			string = "super2xsai";
			break;
		case VideoFilterTypeID_SuperEagle:
			string = "supereagle";
			break;
		case VideoFilterTypeID_Scanline:
			string = "scanline";
			break;
		case VideoFilterTypeID_Bilinear:
			string = "bilinear";
			break;
		case VideoFilterTypeID_Nearest2X:
			string = "nearest2x";
			break;
		case VideoFilterTypeID_Nearest1_5X:
			string = "nearest_1point5x";
			break;
		case VideoFilterTypeID_NearestPlus1_5X:
			string = "nearestplus_1point5x";
			break;
		case VideoFilterTypeID_EPX:
			string = "epx";
			break;
		case VideoFilterTypeID_EPXPlus:
			string = "epxplus";
			break;
		case VideoFilterTypeID_EPX1_5X:
			string = "epx_1point5x";
			break;
		case VideoFilterTypeID_EPXPlus1_5X:
			string = "epxplus_1point5x";
			break;
		case VideoFilterTypeID_2xBRZ:
			string = "2xbrz";
			break;
		case VideoFilterTypeID_3xBRZ:
			string = "3xbrz";
			break;
		case VideoFilterTypeID_4xBRZ:
			string = "4xbrz";
			break;
		case VideoFilterTypeID_5xBRZ:
			string = "5xbrz";
			break;
    }
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "pri-interpolation")), g_variant_new_string(string.c_str()));

    switch (config.audio_sync) {
        case SPUMODE_SYNCN:
        case SPUMODE_SYNCZ:
#ifdef HAVE_LIBSOUNDTOUCH
        case SPUMODE_SYNCP:
#endif
            SPUMode = config.audio_sync;
            SPU_SetSynchMode(1, config.audio_sync-1);
            break;

        case SPUMODE_DUALASYNC:
        default:
            config.audio_sync = SPUMODE_DUALASYNC;
            SPUMode = SPUMODE_DUALASYNC;
            SPU_SetSynchMode(0, 0);
            break;
    }
    switch (config.audio_sync) {
    case SPUMODE_SYNCN:
        string = "sync-n";
        break;
    case SPUMODE_SYNCZ:
        string = "sync-z";
        break;
#ifdef HAVE_LIBSOUNDTOUCH
    case SPUMODE_SYNCP:
        string = "sync-p";
        break;
#endif
    case SPUMODE_DUALASYNC:
    default:
        string = "dual-async";
        break;
    }
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "spu-mode")), g_variant_new_string(string.c_str()));

    CommonSettings.spuInterpolationMode = (SPUInterpolationMode)(config.audio_interpolation.get());
    switch (CommonSettings.spuInterpolationMode) {
        case SPUInterpolation_None:
            string = "none";
            break;
        case SPUInterpolation_Linear:
            string = "linear";
            break;
        case SPUInterpolation_Cosine:
            string = "cosine";
            break;
        case SPUInterpolation_CatmullRom:
            string = "catmullrom";
            break;
    }
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "spu-interpolation")), g_variant_new_string(string.c_str()));

    string = std::to_string(config.frameskip);
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "frameskip")), g_variant_new_string(string.c_str()));
    autoFrameskipMax = config.frameskip;
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "frameskipA")), g_variant_new_boolean(config.autoframeskip));
    if (config.autoframeskip) {
        autoframeskip = true;
        Frameskip = 0;
    } else {
        autoframeskip = false;
        Frameskip = autoFrameskipMax;
    }

    switch (config.view_rot) {
        case 0:
        case 90:
        case 180:
        case 270:
            break;
        default:
            config.view_rot = 0;
            break;
    }
    nds_screen.rotation_angle = config.view_rot;
    string = std::to_string(nds_screen.rotation_angle);
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "rotate")), g_variant_new_string(string.c_str()));


    if (config.window_scale < WINSIZE_SCALE || config.window_scale > WINSIZE_5) {
    	config.window_scale = WINSIZE_SCALE;
    }
    winsize_current = (winsize_enum)config.window_scale.get();
    switch (winsize_current) {
        case WINSIZE_SCALE:
            string = "scale";
            break;
        case WINSIZE_HALF:
            string = "0.5";
            break;
        case WINSIZE_1:
            string = "1";
            break;
        case WINSIZE_1HALF:
            string = "1.5";
            break;
        case WINSIZE_2:
            string = "2";
            break;
        case WINSIZE_2HALF:
            string = "2.5";
            break;
        case WINSIZE_3:
            string = "3";
            break;
        case WINSIZE_3HALF:
            string = "3.5";
            break;
        case WINSIZE_4:
            string = "4";
            break;
        case WINSIZE_4HALF:
            string = "4.5";
            break;
        case WINSIZE_5:
            string = "5";
            break;
    }
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "winsize")), g_variant_new_string(string.c_str()));

    if (config.view_orient < ORIENT_VERTICAL || config.view_orient > ORIENT_HYBRID_VERTICAL) {
        config.view_orient = ORIENT_VERTICAL;
    }
    nds_screen.orientation = (orientation_enum)config.view_orient.get();
    switch (nds_screen.orientation) {
        case ORIENT_VERTICAL:
            string = "vertical";
            break;
        case ORIENT_HORIZONTAL:
            string = "horizontal";
            break;
        case ORIENT_SINGLE:
            string = "single";
            break;
        case ORIENT_HYBRID_EQUAL:
            string = "hybrid_equal";
            break;
        case ORIENT_HYBRID_VERTICAL:
            string = "hybrid_vertical";
            break;
    }
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "orient")), g_variant_new_string(string.c_str()));

    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "pause")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "run")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "frame-advance")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "reset")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "printscreen")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "cheatlist")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "cheatsearch")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "importbackup")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "exportbackup")), FALSE);

    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "savestate")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "loadstate")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "save-state-to")), FALSE);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "load-state-from")), FALSE);

    nds_screen.gap_size = config.view_gap ? GAP_SIZE : 0;

    nds_screen.swap = config.view_swap;
    #ifdef HAVE_LIBAGG
        bool hybrid = (nds_screen.orientation == ORIENT_HYBRID_EQUAL) || (nds_screen.orientation == ORIENT_HYBRID_VERTICAL);
        osd->swapScreens = nds_screen.swap && !hybrid;
    #endif
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "swapscreens")), g_variant_new_boolean(config.view_swap));

    builder = gtk_builder_new_from_resource("/org/desmume/DeSmuME/menu.ui");
    GMenuModel *menubar = G_MENU_MODEL(gtk_builder_get_object(builder, "menubar"));
    GMenuModel *open_recent_menu = G_MENU_MODEL(gtk_builder_get_object(builder, "open_recent"));
    savestates_menu = G_MENU_MODEL(gtk_builder_get_object(builder, "savestates"));
    loadstates_menu = G_MENU_MODEL(gtk_builder_get_object(builder, "loadstates"));
    GMenu *config_menu = G_MENU(gtk_builder_get_object(builder, "config"));

#ifdef FAKE_MIC
    GMenuItem *mic_noise = g_menu_item_new("Fake mic _noise", "app.micnoise");
    g_menu_insert_item(config_menu, 3, mic_noise);
#endif

    GMenuItem *emulation_settings = g_menu_item_new("Em_ulation Settings", "app.emulationsettings");
    g_menu_insert_item(config_menu, 1, emulation_settings);

    // The synchronizer methods (including the libsoundtouch one) are chosen in
    // the sound settings dialog, so there is no submenu to extend here.

    GMenu *view_menu = G_MENU(gtk_builder_get_object(builder, "view"));
    GMenuModel *hud = G_MENU_MODEL(gtk_builder_get_object(builder,
#ifdef HAVE_LIBAGG
        "hud_supported"
#else
        "hud_unsupported"
#endif
    ));
    g_menu_append_submenu(view_menu, "_HUD", hud);

    gtk_application_set_menubar(GTK_APPLICATION(app), menubar);

    // After set_menubar(): that call installs the accelerators found in the
    // model, and the user's own shortcuts have to win over them.
    pApp = GTK_APPLICATION(app);
    HotkeysApply();

    GMenu *popup_menu = G_MENU(gtk_builder_get_object(builder, "popup"));
    pPopupMenu = (GtkMenu *) gtk_menu_new_from_model(G_MENU_MODEL(popup_menu));
    gtk_menu_attach_to_widget(pPopupMenu, pWindow, NULL);
    g_object_unref(builder);

    pApp = GTK_APPLICATION(app);

    // No movie is active on startup; Stop Replay stays greyed out until a
    // Play/Record dialog actually starts one.
    update_movie_actions();

#ifdef HAVE_FFMPEG_RECORD
    // Recover menu state if the encoder worker fails after begin() succeeded.
    g_timeout_add_seconds(1, RecordAV_Watchdog, NULL);
#endif

    // The app menubar is alive now (the application holds it after
    // gtk_application_set_menubar).  Top-level Lua menus are appended here.
    GtkLua_SetAppMenubar(G_MENU(menubar));

    GtkRecentManager *manager = gtk_recent_manager_get_default();
    GList *items = gtk_recent_manager_get_items(manager);
    g_list_foreach(items, [](gpointer data, gpointer user_data) {
        // TODO: Why is there no GTK_RECENT_INFO()?!
        GtkRecentInfo *info = static_cast<GtkRecentInfo*>(data);
        const char *mime = gtk_recent_info_get_mime_type(info);
        if (strcmp(mime, "application/x-nintendo-ds-rom") != 0 &&
            strcmp(mime, "application/vnd.nintendo.nitro.rom") != 0) {
            gtk_recent_info_unref(info);
            return;
        }

        const char *uri = gtk_recent_info_get_uri(info);
        const char *name = gtk_recent_info_get_display_name(info);
        // TODO: Is that enough?  Maybe allocate instead?
        char action[1024];
        sprintf(action, "app.recent(\"%s\")", uri);
        g_menu_append(G_MENU(user_data), name, action);

        gtk_recent_info_unref(info);
    }, open_recent_menu);
    g_list_free(items);

    /* This toggle action must not be set active before the pDrawingArea initialization to avoid a GTK warning */
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "gap")), g_variant_new_boolean(config.view_gap));

    gtk_widget_set_events(pDrawingArea,
                          GDK_EXPOSURE_MASK | GDK_LEAVE_NOTIFY_MASK |
                          GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK );

    g_signal_connect(G_OBJECT(pDrawingArea), "button_press_event",
                     G_CALLBACK(Stylus_Press), NULL);
    g_signal_connect(G_OBJECT(pDrawingArea), "button_release_event",
                     G_CALLBACK(Stylus_Release), NULL);
    g_signal_connect(G_OBJECT(pDrawingArea), "motion_notify_event",
                     G_CALLBACK(Stylus_Move), NULL);
    g_signal_connect(G_OBJECT(pDrawingArea), "draw",
                     G_CALLBACK(ExposeDrawingArea), NULL ) ;
    g_signal_connect(G_OBJECT(pDrawingArea), "configure_event",
                     G_CALLBACK(ConfigureDrawingArea), NULL ) ;

    /* Status bar */
    UpdateStatusBar(EMU_DESMUME_NAME_AND_VERSION());

    gtk_widget_show_all(pWindow);

	if (winsize_current == WINSIZE_SCALE) {
		if (config.window_fullscreen) {
            gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(pWindow), FALSE);
			gtk_widget_hide(pToolBar);
			gtk_widget_hide(pStatusBar);
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "view-menu")), FALSE);
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "view-toolbar")), FALSE);
            g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "view-statusbar")), FALSE);
			gtk_window_fullscreen(GTK_WINDOW(pWindow));
		}
	} else {
		config.window_fullscreen = false;
        g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "fullscreen")), FALSE);
	}
	if (!config.view_menu) {
        gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(pWindow), FALSE);
        g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "view-menu")), g_variant_new_boolean(FALSE));
	}
	if (!config.view_toolbar) {
		gtk_widget_hide(pToolBar);
        g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "view-toolbar")), g_variant_new_boolean(FALSE));
	}
	if (!config.view_statusbar) {
		gtk_widget_hide(pStatusBar);
        g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "view-statusbar")), g_variant_new_boolean(FALSE));
	}

    /* Applied by the core on the next frame; the cheat list itself is loaded
     * per ROM, so only the on/off switch is restored here. */
    CommonSettings.cheatsDisable = config.cheats_disabled;
    g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "cheatsdisable")), g_variant_new_boolean(config.cheats_disabled));

    UpdateDrawingAreaAspect();

    if (my_config->disable_limiter || !config.fpslimiter) {
        config.fpslimiter = false;
        g_simple_action_set_state(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "enablefpslimiter")), g_variant_new_boolean(FALSE));
    }

#if defined(ENABLE_OPENGL_STANDARD) || defined(ENABLE_OPENGL_ES)
	#if defined(OGLRENDER_3_2_H)
    OGLLoadEntryPoints_3_2_Func = OGLLoadEntryPoints_3_2;
    OGLCreateRenderer_3_2_Func = OGLCreateRenderer_3_2;
	#endif
	#if defined(OGLRENDER_ES3_H)
	OGLLoadEntryPoints_ES_3_0_Func = &OGLLoadEntryPoints_ES_3_0;
	OGLCreateRenderer_ES_3_0_Func = &OGLCreateRenderer_ES_3_0;
	#endif

	#if defined(ENABLE_GLX)
	oglrender_init = &glx_initOpenGL_StandardAuto;
	oglrender_deinit = &glx_deinitOpenGL;
	oglrender_beginOpenGL = &glx_beginOpenGL;
	oglrender_endOpenGL = &glx_endOpenGL;
	oglrender_framebufferDidResizeCallback = &glx_framebufferDidResizeCallback;
	#elif defined(ENABLE_EGL)
	oglrender_init = &egl_initOpenGL_StandardAuto;
	oglrender_deinit = &egl_deinitOpenGL;
	oglrender_beginOpenGL = &egl_beginOpenGL;
	oglrender_endOpenGL = &egl_endOpenGL;
	oglrender_framebufferDidResizeCallback = &egl_framebufferDidResizeCallback;
	#else
	oglrender_init = &sdl_initOpenGL_StandardAuto;
	oglrender_deinit = &sdl_deinitOpenGL;
	oglrender_beginOpenGL = &sdl_beginOpenGL;
	oglrender_endOpenGL = &sdl_endOpenGL;
	oglrender_framebufferDidResizeCallback = &sdl_framebufferDidResizeCallback;
	#endif
#endif // ENABLE_OPENGL_STANDARD || ENABLE_OPENGL_ES

    //Set the 3D emulation to use
    int core = my_config->engine_3d;
    // setup the gdk 3D emulation;

    if (core == -2)
    {
    	// --3d-render=wgpu: resolve to the first usable WebGPU entry.
    	core = (wgpuFirstCoreIndex >= 0) ? wgpuFirstCoreIndex : RENDERID_SOFTRASTERIZER;
    	my_config->engine_3d = core;
    }
    else if (core == -1)
	{
    	// If it was not passed, then get the Renderer config from the file
    	core = config.core3D;
        my_config->engine_3d = core;
    }

	// Check that the index still exists in the runtime renderer list (WebGPU
	// contributes a variable number of entries, and the build options may have
	// changed since the config was written).
	if ( (core < 0) || (core >= GPU3D_MAX_COUNT) || (core3DList[core] == NULL) )
	{
		core = RENDERID_SOFTRASTERIZER;
		my_config->engine_3d = core;
	}

	// OpenGL entries need their context callbacks selected first. The other
	// cores (Null, SoftRasterizer, WebGPU) need none.
#if defined(ENABLE_OPENGL_ES) && !defined(ENABLE_GLX)
	if (core == 2)
	{
	#if defined(ENABLE_EGL)
		oglrender_init = &egl_initOpenGL_ES_3_0;
	#else
		oglrender_init = &sdl_initOpenGL_ES_3_0;
	#endif
	}
#elif defined(ENABLE_OPENGL_STANDARD)
	switch (core)
	{
		case 2:
	#if defined(ENABLE_GLX)
			oglrender_init = &glx_initOpenGL_StandardAuto;
	#elif defined(ENABLE_EGL)
			oglrender_init = &egl_initOpenGL_StandardAuto;
	#else
			oglrender_init = &sdl_initOpenGL_StandardAuto;
	#endif
			break;

		case 3:
	#if defined(ENABLE_GLX)
			oglrender_init = &glx_initOpenGL_LegacyAuto;
	#elif defined(ENABLE_EGL)
			oglrender_init = &egl_initOpenGL_LegacyAuto;
	#else
			oglrender_init = &sdl_initOpenGL_LegacyAuto;
	#endif
			break;

		case 4:
	#if defined(ENABLE_GLX)
			oglrender_init = &glx_initOpenGL_3_2_CoreProfile;
	#elif defined(ENABLE_EGL)
			oglrender_init = &egl_initOpenGL_3_2_CoreProfile;
	#else
			oglrender_init = &sdl_initOpenGL_3_2_CoreProfile;
	#endif
			break;
	}
#endif

	// Everything the 3D renderer reads while it initializes has to be set
	// before it is created: the OpenGL one compiles these into its shaders.
    CommonSettings.GFX3D_HighResolutionInterpolateColor = config.highColorInterpolation;
	CommonSettings.GFX3D_Renderer_MultisampleSize = (config.multisamplingSize > 0)
														? config.multisamplingSize
														: config.multisampling ? 4 : 0;
    CommonSettings.GFX3D_Renderer_TextureDeposterize = config.textureDeposterize;
    CommonSettings.GFX3D_Renderer_TextureScalingFactor = (config.textureUpscale == 1 ||
    														config.textureUpscale == 2 ||
															config.textureUpscale == 4)
															? config.textureUpscale : 1 ;
    CommonSettings.GFX3D_Renderer_TextureSmoothing = config.textureSmoothing;

	// The remaining 3D options of the Windows port's 3D Settings dialog.
	CommonSettings.GFX3D_TXTHack = config.gfx3dTxtHack;
	CommonSettings.GFX3D_LineHack = config.gfx3dLineHack;
	CommonSettings.GFX3D_EdgeMark = config.gfx3dEdgeMark;
	CommonSettings.GFX3D_Fog = config.gfx3dFog;
	CommonSettings.GFX3D_Texture = config.gfx3dTexture;
	CommonSettings.OpenGL_Emulation_ShadowPolygon = config.gfx3dShadowPolygon;
	CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending = config.gfx3dZeroAlphaBlending;
	CommonSettings.OpenGL_Emulation_NDSDepthCalculation = config.gfx3dNdsDepthCalculation;
	CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing = config.gfx3dDepthLEqualFacing;

	// Output color depth of the 3D renderer. The handler converts whatever
	// format it gets to the 32-bit buffer the display and filters read.
	if (gtk3gpuEvent != NULL)
		gtk3gpuEvent->SetColorFormat(ColorFormatFromDepth(config.gfx3dColorDepth));

	if (!GPU->Change3DRendererByID(core)) {
		GPU->Change3DRendererByID(RENDERID_SOFTRASTERIZER);
		g_printerr("3D renderer initialization failed!\nFalling back to 3D core: %s\n", core3DList[RENDERID_SOFTRASTERIZER]->name);
		my_config->engine_3d = RENDERID_SOFTRASTERIZER;
	}

    backup_setManualBackupType(my_config->savetype);

    // Command line arg
    if( my_config->nds_file != "") {
        if(Open( my_config->nds_file.c_str()) >= 0) {
            my_config->process_movieCommands();

            if(my_config->load_slot != -1){
				int todo = my_config->load_slot;
				if(todo == 10)
					todo = 0;
              loadstate_slot(my_config->load_slot);
            }

            Launch(NULL, NULL, NULL);
        } else {
            GtkWidget *pDialog = gtk_message_dialog_new(GTK_WINDOW(pWindow),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_INFO,
                    GTK_BUTTONS_OK,
                    "Unable to load :\n%s", my_config->nds_file.c_str());
            gtk_dialog_run(GTK_DIALOG(pDialog));
            gtk_widget_destroy(pDialog);
        }
    }

    if (my_config->timeout > 0) {
        g_timeout_add_seconds(my_config->timeout, timeout_exit_cb, GINT_TO_POINTER(my_config->timeout));
    }

    SPU_SetVolume(config.audio_volume);

    /* Video filter parameters */
    video->SetFilterParameteri(VF_PARAM_SCANLINE_A, _scanline_filter_a);
    video->SetFilterParameteri(VF_PARAM_SCANLINE_B, _scanline_filter_b);
    video->SetFilterParameteri(VF_PARAM_SCANLINE_C, _scanline_filter_c);
    video->SetFilterParameteri(VF_PARAM_SCANLINE_D, _scanline_filter_d);

	RedrawScreen();
	g_timeout_add(200, OutOfLoopJoyDeviceCheckTimerFunc, 0);
}

static void Teardown() {
#ifdef HAVE_LIBAGG
	HudSaveLayout();
#endif

	// Release the WebGPU instance while the native window and the toolkit's
	// EGL/Vulkan state are still alive. The static destructor would otherwise
	// do it after exit() has started tearing down GTK and the display
	// connection, which is too late for the graphics drivers.
	WGPU_KillDisplay();

    delete video;
    
	config.save();
	avout_x264.end();
	avout_flac.end();
#ifdef HAVE_FFMPEG_RECORD
	avout_ffmpeg.end();
#endif

    desmume_free();

    delete gtk3gpuEvent;
    gtk3gpuEvent = NULL;
	
#if defined(ENABLE_OPENGL_STANDARD) || defined(ENABLE_OPENGL_ES)
    #if defined(ENABLE_GLX)
	glx_deinitOpenGL();
    #elif defined(ENABLE_EGL)
	egl_deinitOpenGL();
    #else
	sdl_deinitOpenGL();
    #endif
#endif // ENABLE_OPENGL_STANDARD || ENABLE_OPENGL_ES

    /* Unload joystick */
    uninit_joy();

    SDL_Quit();

    desmume_config_dispose(keyfile);

#ifdef GDB_STUB
    destroyStub_gdb( arm9_gdb_stub);
	arm9_gdb_stub = NULL;
	
    destroyStub_gdb( arm7_gdb_stub);
	arm7_gdb_stub = NULL;

    gdbstub_mutex_destroy();
#endif
}

static void
handle_open(GApplication *application,
            GFile **files,
            gint n_files,
            const gchar *hint,
            gpointer user_data)
{
    configured_features *my_config = static_cast<configured_features*>(user_data);
    my_config->nds_file = g_file_get_path(files[0]);
    common_gtk_main(application, user_data);
}

#ifdef AGG2D_USE_VECTORFONTS

static std::string FindFontFile(const char* fontName, bool bold)
{
	std::string fontFile;
	FcPattern* pat = FcNameParse((const FcChar8*)fontName);
	if(bold)
		FcPatternAddInteger(pat, FC_WEIGHT, FC_WEIGHT_BOLD);
	FcConfigSubstitute(fontConfig, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);
	
	// find the font
	FcResult res;
	FcPattern* font = FcFontMatch(fontConfig, pat, &res);
	if (font)
	{
		FcChar8* file = NULL;
		if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch)
		{
			// save the file to another std::string
			fontFile = (char*)file;
		}
		FcPatternDestroy(font);
	}
	FcPatternDestroy(pat);
	return fontFile;
}

#endif

int main (int argc, char *argv[])
{
  configured_features my_config;

#ifdef AGG2D_USE_VECTORFONTS
  fontConfig = FcInitLoadConfigAndFonts();
  vectorFontFile = FindFontFile("mono", true);
  if(!vectorFontFile.size())
    vectorFontFile = FindFontFile("sans", true);
#endif
  // The global menu screws up the window size...
  unsetenv("UBUNTU_MENUPROXY");

  g_set_prgname("org.desmume.DeSmuME");

  // this must be called as early as possible
  NDS_Init();

  my_config.parse(argc, argv);
  init_configured_features( &my_config);

  /* X11 multi-threading support */
  if(!XInitThreads())
    {
      fprintf(stderr, "Warning: X11 not thread-safe\n");
    }

  // TODO: pass G_APPLICATION_HANDLES_COMMAND_LINE instead.
  GtkApplication *app = gtk_application_new("org.desmume.DeSmuME", G_APPLICATION_HANDLES_OPEN);
  g_signal_connect (app, "activate", G_CALLBACK(common_gtk_main), &my_config);
  g_signal_connect (app, "open", G_CALLBACK(handle_open), &my_config);
  g_action_map_add_action_entries(G_ACTION_MAP(app),
                                  app_entries, G_N_ELEMENTS(app_entries),
                                  app);
  g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(
      G_ACTION_MAP(app), "rom-info")), FALSE);

  if ( !fill_configured_features( &my_config, argv)) {
    exit(0);
  }
#ifdef HAVE_JIT
  config.load();
  CommonSettings.jit_max_block_size=config.jit_max_block_size;
  CommonSettings.use_jit=config.use_jit;
  arm_jit_sync();
  arm_jit_reset(CommonSettings.use_jit);
#endif
  int ret = g_application_run(G_APPLICATION(app), argc, argv);
  Teardown();
  return ret;
}

Gtk3GPUEventHandler::Gtk3GPUEventHandler()
{
	_colorFormatPending = NDSColorFormat_BGR666_Rev;
	_widthPending       = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_heightPending      = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	_pageCountPending   = GTK3_FRAMEBUFFER_PAGE_COUNT;
	
	_didColorFormatChange = true;
	_didWidthChange       = true;
	_didHeightChange      = true;
	_didPageCountChange   = true;
	
	_latestBufferIndex = 0;
	
	_mutexApplyGPUSettings = slock_new();
	_mutexVideoFetch = slock_new();
	
	for (size_t i = 0; i < GTK3_FRAMEBUFFER_PAGE_COUNT; i++)
	{
		_mutexFramebufferPage[i] = slock_new();
	}
}

Gtk3GPUEventHandler::~Gtk3GPUEventHandler()
{
	slock_free(this->_mutexApplyGPUSettings);
	slock_free(this->_mutexVideoFetch);
	
	for (size_t i = 0; i < GTK3_FRAMEBUFFER_PAGE_COUNT; i++)
	{
		slock_free(this->_mutexFramebufferPage[i]);
	}
}

void Gtk3GPUEventHandler::SetFramebufferDimensions(size_t w, size_t h)
{
	if (w < GPU_FRAMEBUFFER_NATIVE_WIDTH)
	{
		w = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	}
	
	if (h < GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		h = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	}
	
	slock_lock(this->_mutexApplyGPUSettings);
	
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	this->_didWidthChange  = (w != dispInfo.customWidth);
	this->_didHeightChange = (h != dispInfo.customHeight);
	this->_widthPending  = w;
	this->_heightPending = h;
	
	slock_unlock(this->_mutexApplyGPUSettings);
}

void Gtk3GPUEventHandler::GetFramebufferDimensions(size_t &outWidth, size_t &outHeight)
{
	outWidth  = this->_widthPending;
	outHeight = this->_heightPending;
}

void Gtk3GPUEventHandler::SetColorFormat(NDSColorFormat colorFormat)
{
	slock_lock(this->_mutexApplyGPUSettings);
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	this->_didColorFormatChange = (colorFormat != dispInfo.colorFormat);
	this->_colorFormatPending = colorFormat;
	slock_unlock(this->_mutexApplyGPUSettings);
}

NDSColorFormat Gtk3GPUEventHandler::GetColorFormat()
{
	return this->_colorFormatPending;
}

void Gtk3GPUEventHandler::VideoFetchLock()
{
	slock_lock(this->_mutexVideoFetch);
}

void Gtk3GPUEventHandler::VideoFetchUnlock()
{
	slock_unlock(this->_mutexVideoFetch);
}

void Gtk3GPUEventHandler::DidFrameEnd(bool isFrameSkipped, const NDSDisplayInfo &latestDisplayInfo)
{
	if (isFrameSkipped)
	{
		return;
	}
	
	this->_latestBufferIndex = latestDisplayInfo.bufferIndex;
	
	slock_lock(this->_mutexFramebufferPage[this->_latestBufferIndex]);
	
	// The legacy x264 pipe recorder still assumes the native 16-bit buffer.
	// This currently does not work on custom-sized buffers or any 32-bit color format.
	avout_x264.updateVideo(latestDisplayInfo.masterNativeBuffer16);
#ifdef HAVE_FFMPEG_RECORD
	// Filtered recording captures the finished frame from PresentFrame()
	// instead, so that the filter, Lua and HUD are part of the video.
	if (!sRecordFilteredOutput && avout_ffmpeg.hasVideo())
	{
		// masterCustomBuffer is the buffer the frontend actually presents.
		// Unlike masterNativeBuffer16 it contains the 3D output and every
		// custom-rendered screen, which is why recording the native buffer
		// left the 3D screen black.
		//
		// 18/24-bit masterCustomBuffer memory order is R,G,B,A; the display and
		// filter buffers are B,G,R,A. Using BGRA here swapped red and blue.
		const FFmpegCaptureFormat captureFormat =
			(latestDisplayInfo.colorFormat == NDSColorFormat_BGR555_Rev)
				? FFmpegCaptureFormat_BGR555LE
				: FFmpegCaptureFormat_RGBA8888;
		avout_ffmpeg.updateVideo(latestDisplayInfo.masterCustomBuffer,
		                         (int)latestDisplayInfo.customWidth,
		                         (int)(latestDisplayInfo.customHeight * 2),
		                         captureFormat);
	}

	sFrameSerial++;
#endif
	
	this->VideoFetchLock();
	if (latestDisplayInfo.colorFormat == NDSColorFormat_BGR555_Rev)
	{
		ColorspaceConvertBuffer555xTo8888Opaque<true, false, BESwapNone>( (u16 *)latestDisplayInfo.masterCustomBuffer, (u32 *)video->GetSrcBufferPtr(), latestDisplayInfo.customWidth * latestDisplayInfo.customHeight * 2);
	}
	else
	{
		ColorspaceConvertBuffer888xTo8888Opaque<true, false>( (u32 *)latestDisplayInfo.masterCustomBuffer, (u32 *)video->GetSrcBufferPtr(), latestDisplayInfo.customWidth * latestDisplayInfo.customHeight * 2);
	}
	this->VideoFetchUnlock();
	
	slock_unlock(this->_mutexFramebufferPage[this->_latestBufferIndex]);
	
	// The frame is now in the source buffer.  Attach the HUD/Lua target and
	// mark the frame pending; the caller presents it after the Lua GUI
	// callback has drawn, so Lua pixels reach the filtered destination buffer
	// that the display actually reads.
	PrepareHudTarget();
	sFramePresentPending = true;
}

void Gtk3GPUEventHandler::DidApplyGPUSettingsBegin()
{
	slock_lock(this->_mutexApplyGPUSettings);
	
	if (this->_didWidthChange || this->_didHeightChange || this->_didColorFormatChange || this->_didPageCountChange)
	{
		if (this->_didWidthChange || this->_didHeightChange)
		{
			for (size_t i = 0; i < GTK3_FRAMEBUFFER_PAGE_COUNT; i++)
			{
				slock_lock(this->_mutexFramebufferPage[i]);
			}
		}
		
		GPU->SetCustomFramebufferSize(this->_widthPending, this->_heightPending);
		GPU->SetColorFormat(this->_colorFormatPending);
		GPU->SetFramebufferPageCount(this->_pageCountPending);
	}
}

void Gtk3GPUEventHandler::DidApplyGPUSettingsEnd()
{
	if (this->_didWidthChange || this->_didHeightChange || this->_didColorFormatChange || this->_didPageCountChange)
	{
		const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
		
		if (this->_didWidthChange || this->_didHeightChange)
		{
			video->SetSourceSize(dispInfo.customWidth, dispInfo.customHeight * 2);
			
			for (size_t i = 0; i < GTK3_FRAMEBUFFER_PAGE_COUNT; i++)
			{
				slock_unlock(this->_mutexFramebufferPage[i]);
			}
		}
		
		// Update all the change flags now that settings are applied.
		// In theory, all these flags should get cleared.
		this->_didWidthChange       = (this->_widthPending       != dispInfo.customWidth);
		this->_didHeightChange      = (this->_heightPending      != dispInfo.customHeight);
		this->_didColorFormatChange = (this->_colorFormatPending != dispInfo.colorFormat);
		this->_didPageCountChange   = (this->_pageCountPending   != dispInfo.framebufferPageCount);
	}
	
	slock_unlock(this->_mutexApplyGPUSettings);
}
