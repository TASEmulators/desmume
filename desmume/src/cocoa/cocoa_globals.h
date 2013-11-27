/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2013 DeSmuME team

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

#define STRING_DESMUME_WEBSITE						"http://desmume.org"
#define STRING_DESMUME_SHORT_DESCRIPTION			"Nintendo DS Emulator"
#define STRING_DESMUME_FORUM_SITE					"http://forums.desmume.org/index.php"
#define STRING_DESMUME_TECH_SUPPORT_SITE			"http://forums.desmume.org/viewforum.php?id=4"
#define STRING_DESMUME_BUG_REPORT_SITE				"http://sourceforge.net/p/desmume/bugs"

// User Interface Localized Strings
#define NSSTRING_TITLE_OPEN_ROM_PANEL				NSLocalizedString(@"Open ROM", nil)
#define NSSTRING_TITLE_OPEN_STATE_FILE_PANEL		NSLocalizedString(@"Open State File", nil)
#define NSSTRING_TITLE_SAVE_STATE_FILE_PANEL		NSLocalizedString(@"Save State File", nil)
#define NSSTRING_TITLE_IMPORT_ROM_SAVE_PANEL		NSLocalizedString(@"Import ROM Save File", nil)
#define NSSTRING_TITLE_EXPORT_ROM_SAVE_PANEL		NSLocalizedString(@"Export ROM Save File", nil)
#define NSSTRING_TITLE_SELECT_ROM_PANEL				NSLocalizedString(@"Select ROM", nil)
#define NSSTRING_TITLE_SELECT_ADVANSCENE_DB_PANEL	NSLocalizedString(@"Select ADVANsCEne Database", nil)
#define NSSTRING_TITLE_SELECT_R4_CHEAT_DB_PANEL		NSLocalizedString(@"Select R4 Cheat Database", nil)
#define NSSTRING_TITLE_SELECT_ARM7_IMAGE_PANEL		NSLocalizedString(@"Select ARM7 BIOS Image", nil)
#define NSSTRING_TITLE_SELECT_ARM9_IMAGE_PANEL		NSLocalizedString(@"Select ARM9 BIOS Image", nil)
#define NSSTRING_TITLE_SELECT_FIRMWARE_IMAGE_PANEL	NSLocalizedString(@"Select Firmware Image", nil)
#define NSSTRING_TITLE_SAVE_SCREENSHOT_PANEL		NSLocalizedString(@"Save Screenshot", nil)

#define NSSTRING_TITLE_EXECUTE_CONTROL				NSLocalizedString(@"Execute", nil)
#define NSSTRING_TITLE_PAUSE_CONTROL				NSLocalizedString(@"Pause", nil)
#define NSSTRING_TITLE_DISABLE_SPEED_LIMIT			NSLocalizedString(@"Disable Speed Limit", nil)
#define NSSTRING_TITLE_ENABLE_SPEED_LIMIT			NSLocalizedString(@"Enable Speed Limit", nil)
#define NSSTRING_TITLE_DISABLE_AUTO_FRAME_SKIP		NSLocalizedString(@"Disable Auto Frame Skip", nil)
#define NSSTRING_TITLE_ENABLE_AUTO_FRAME_SKIP		NSLocalizedString(@"Enable Auto Frame Skip", nil)
#define NSSTRING_TITLE_DISABLE_CHEATS				NSLocalizedString(@"Disable Cheats", nil)
#define NSSTRING_TITLE_ENABLE_CHEATS				NSLocalizedString(@"Enable Cheats", nil)
#define NSSTRING_TITLE_DISABLE_HUD					NSLocalizedString(@"Disable HUD", nil)
#define NSSTRING_TITLE_ENABLE_HUD					NSLocalizedString(@"Enable HUD", nil)
#define NSSTRING_TITLE_EXIT_FULL_SCREEN				NSLocalizedString(@"Exit Full Screen", nil)
#define NSSTRING_TITLE_ENTER_FULL_SCREEN			NSLocalizedString(@"Enter Full Screen", nil)
#define NSSTRING_TITLE_HIDE_STATUS_BAR				NSLocalizedString(@"Hide Status Bar", nil)
#define NSSTRING_TITLE_SHOW_STATUS_BAR				NSLocalizedString(@"Show Status Bar", nil)
#define NSSTRING_TITLE_HIDE_TOOLBAR					NSLocalizedString(@"Hide Toolbar", nil)
#define NSSTRING_TITLE_SHOW_TOOLBAR					NSLocalizedString(@"Show Toolbar", nil)
#define NSSTRING_TITLE_SPEED_1X						NSLocalizedString(@"Speed 1x", nil)
#define NSSTRING_TITLE_SPEED_2X						NSLocalizedString(@"Speed 2x", nil)
#define NSSTRING_TITLE_SLOT_NUMBER					NSLocalizedString(@"Slot %ld", nil)

#define NSSTRING_TITLE_TECH_SUPPORT_WINDOW_TITLE	NSLocalizedString(@"Support Request Form", nil)
#define NSSTRING_TITLE_BUG_REPORT_WINDOW_TITLE		NSLocalizedString(@"Bug Report Form", nil)
#define NSSTRING_TITLE_GO_TECH_SUPPORT_WEBPAGE_TITLE	NSLocalizedString(@"Go to Tech Support Webpage", nil)
#define NSSTRING_TITLE_GO_BUG_REPORT_WEBPAGE_TITLE	NSLocalizedString(@"Go to Bug Report Webpage", nil)
#define NSSTRING_HELP_COPY_PASTE_TECH_SUPPORT		NSLocalizedString(@"Please copy-paste the above information into our tech support webpage. This will ensure the fastest response time from us.", nil)
#define NSSTRING_HELP_COPY_PASTE_BUG_REPORT			NSLocalizedString(@"Please copy-paste the above information into our bug report webpage. This will ensure the fastest response time from us.", nil)

#define NSSTRING_ALERT_CRITICAL_FILE_MISSING_PRI	NSLocalizedString(@"A critical file is missing. DeSmuME will now quit.", nil)
#define NSSTRING_ALERT_CRITICAL_FILE_MISSING_SEC	NSLocalizedString(@"The file \"DefaultUserPrefs.plist\" is missing. Please reinstall DeSmuME.", nil)

#define NSSTRING_STATUS_READY						NSLocalizedString(@"Ready.", nil)
#define NSSTRING_STATUS_SAVESTATE_LOADING_FAILED	NSLocalizedString(@"Save state file loading failed!", nil)
#define NSSTRING_STATUS_SAVESTATE_LOADED			NSLocalizedString(@"Save state file loaded.", nil)
#define NSSTRING_STATUS_SAVESTATE_SAVING_FAILED		NSLocalizedString(@"Save state file saving failed!", nil)
#define NSSTRING_STATUS_SAVESTATE_SAVED				NSLocalizedString(@"Save state file saved.", nil)
#define NSSTRING_STATUS_SAVESTATE_REVERTING_FAILED	NSLocalizedString(@"Save state file reverting failed!", nil)
#define NSSTRING_STATUS_SAVESTATE_REVERTED			NSLocalizedString(@"Save state file reverted.", nil)
#define NSSTRING_STATUS_ROM_SAVE_IMPORT_FAILED		NSLocalizedString(@"ROM save file import failed!", nil)
#define NSSTRING_STATUS_ROM_SAVE_IMPORTED			NSLocalizedString(@"ROM save file imported.", nil)
#define NSSTRING_STATUS_ROM_SAVE_EXPORT_FAILED		NSLocalizedString(@"ROM save file export failed!", nil)
#define NSSTRING_STATUS_ROM_SAVE_EXPORTED			NSLocalizedString(@"ROM save file exported.", nil)
#define NSSTRING_STATUS_ROM_LOADING					NSLocalizedString(@"Loading ROM...", nil)
#define NSSTRING_STATUS_ROM_LOADING_FAILED			NSLocalizedString(@"ROM loading failed!", nil)
#define NSSTRING_STATUS_ROM_LOADED					NSLocalizedString(@"ROM loaded.", nil)
#define NSSTRING_STATUS_ROM_UNLOADING				NSLocalizedString(@"Unloading ROM...", nil)
#define NSSTRING_STATUS_ROM_UNLOADED				NSLocalizedString(@"ROM unloaded.", nil)
#define NSSTRING_STATUS_EMULATOR_RESETTING			NSLocalizedString(@"Emulator resetting...", nil)
#define NSSTRING_STATUS_EMULATOR_RESET				NSLocalizedString(@"Emulator reset.", nil)
#define NSSTRING_STATUS_CANNOT_GENERATE_SAVE_PATH	NSLocalizedString(@"Cannot generate the save file path!", nil)
#define NSSTRING_STATUS_CANNOT_CREATE_SAVE_DIRECTORY	NSLocalizedString(@"Cannot create the save directory!", nil)

#define NSSTRING_STATUS_SPEED_LIMIT_DISABLED		NSLocalizedString(@"Speed limit disabled.", nil)
#define NSSTRING_STATUS_SPEED_LIMIT_ENABLED			NSLocalizedString(@"Speed limit enabled.", nil)
#define NSSTRING_STATUS_AUTO_FRAME_SKIP_DISABLED	NSLocalizedString(@"Auto frame skip disabled.", nil)
#define NSSTRING_STATUS_AUTO_FRAME_SKIP_ENABLED		NSLocalizedString(@"Auto frame skip enabled.", nil)
#define NSSTRING_STATUS_CHEATS_DISABLED				NSLocalizedString(@"Cheats disabled.", nil)
#define NSSTRING_STATUS_CHEATS_ENABLED				NSLocalizedString(@"Cheats enabled.", nil)
#define NSSTRING_STATUS_HUD_DISABLED				NSLocalizedString(@"HUD disabled.", nil)
#define NSSTRING_STATUS_HUD_ENABLED					NSLocalizedString(@"HUD enabled.", nil)
#define NSSTRING_STATUS_VOLUME						NSLocalizedString(@"Volume: %1.1f%%", nil)
#define NSSTRING_STATUS_NO_ROM_LOADED				NSLocalizedString(@"No ROM loaded.", nil)
#define NSSTRING_STATUS_SIZE_BYTES					NSLocalizedString(@"%i bytes", nil)

#define NSSTRING_STATUS_EMULATION_NOT_RUNNING		NSLocalizedString(@"Emulation is not running.", nil)
#define NSSTRING_STATUS_SLOT1_UNKNOWN_STATE			NSLocalizedString(@"Unknown state.", nil)
#define NSSTRING_STATUS_SLOT1_NO_DEVICE				NSLocalizedString(@"No device inserted.", nil)
#define NSSTRING_STATUS_SLOT1_RETAIL_INSERTED		NSLocalizedString(@"Retail cartridge inserted. (Auto-detect)", nil)
#define NSSTRING_STATUS_SLOT1_RETAIL_NAND_INSERTED	NSLocalizedString(@"Retail cartridge w/ NAND flash inserted.", nil)
#define NSSTRING_STATUS_SLOT1_R4_INSERTED			NSLocalizedString(@"R4 cartridge interface inserted.", nil)
#define NSSTRING_STATUS_SLOT1_STANDARD_INSERTED		NSLocalizedString(@"Standard retail cartridge inserted.", nil)

#define NSSTRING_STATUS_AUTOLOAD_ROM_NAME_NONE		NSLocalizedString(@"No ROM chosen.", nil)

#define NSSTRING_DISPLAYMODE_MAIN					NSLocalizedString(@"Main", nil)
#define NSSTRING_DISPLAYMODE_TOUCH					NSLocalizedString(@"Touch", nil)
#define NSSTRING_DISPLAYMODE_DUAL					NSLocalizedString(@"Dual Screen", nil)

#define NSSTRING_INPUTPREF_NUM_INPUTS_MAPPED		NSLocalizedString(@"%ld Input Mapped", nil)
#define NSSTRING_INPUTPREF_NUM_INPUTS_MAPPED_PLURAL	NSLocalizedString(@"%ld Inputs Mapped", nil)

#define NSSTRING_INPUTPREF_NO_SAVED_PROFILES		NSLocalizedString(@"No saved profiles.", nil)

#define NSSTRING_INPUTPREF_USE_DEVICE_COORDINATES	NSLocalizedString(@"Use Device Coordinates", nil)
#define NSSTRING_INPUTPREF_MIC_NONE					NSLocalizedString(@"None", nil)
#define NSSTRING_INPUTPREF_MIC_INTERNAL_NOISE		NSLocalizedString(@"Internal Noise Samples", nil)
#define NSSTRING_INPUTPREF_MIC_AUDIO_FILE_NONE_SELECTED	NSLocalizedString(@"No audio file selected.", nil)
#define NSSTRING_INPUTPREF_MIC_WHITE_NOISE			NSLocalizedString(@"White Noise", nil)
#define NSSTRING_INPUTPREF_MIC_SINE_WAVE			NSLocalizedString(@"%1.1f Hz Sine Wave", nil)
#define NSSTRING_INPUTPREF_SPEED_SCALAR				NSLocalizedString(@"%1.2fx Speed", nil)
#define NSSTRING_INPUTPREF_GPU_STATE_ALL_MAIN		NSLocalizedString(@"Main GPU - All Layers", nil)
#define NSSTRING_INPUTPREF_GPU_STATE_ALL_SUB		NSLocalizedString(@"Sub GPU - All Layers", nil)

// LEGACY STRINGS
#define NSSTRING_TITLE_OPEN_ROM_PANEL_LEGACY		NSLocalizedString(@"Open ROM...", nil)
#define NSSTRING_TITLE_OPEN_STATE_FILE_PANEL_LEGACY	NSLocalizedString(@"Load State From...", nil)
#define NSSTRING_TITLE_SAVE_STATE_FILE_PANEL_LEGACY	NSLocalizedString(@"Save State...", nil)
#define NSSTRING_STATUS_ROM_UNLOADED_LEGACY			NSLocalizedString(@"No ROM Loaded", nil)
#define NSSTRING_STATUS_ROM_LOADED_LEGACY			NSLocalizedString(@"ROM Loaded", nil)
#define NSSTRING_STATUS_ROM_LOADING_FAILED_LEGACY	NSLocalizedString(@"Couldn't load ROM", nil)
#define NSSTRING_STATUS_EMULATOR_EXECUTING_LEGACY	NSLocalizedString(@"Emulation Executing", nil)
#define NSSTRING_STATUS_EMULATOR_PAUSED_LEGACY		NSLocalizedString(@"Emulation Paused", nil)
#define NSSTRING_STATUS_EMULATOR_RESET_LEGACY		NSLocalizedString(@"Emulation Reset", nil)
#define NSSTRING_ERROR_TITLE_LEGACY					NSLocalizedString(@"Error", nil)
#define NSSTRING_ERROR_GENERIC_LEGACY				NSLocalizedString(@"An emulation error occurred", nil)
#define NSSTRING_ERROR_SCREENSHOT_FAILED_LEGACY		NSLocalizedString(@"Couldn't create the screenshot image", nil)
#define NSSTRING_MESSAGE_TITLE_LEGACY				NSLocalizedString(@"DeSmuME Emulator", nil)
#define NSSTRING_MESSAGE_ASK_CLOSE_LEGACY			NSLocalizedString(@"Are you sure you want to close the ROM?", nil)

#define FILENAME_README								"README"
#define FILENAME_COPYING							"COPYING"
#define FILENAME_AUTHORS							"AUTHORS"
#define FILENAME_CHANGELOG							"ChangeLog"

#define PATH_CONFIG_DIRECTORY_0_9_6					"~/.config/desmume"
#define PATH_USER_APP_SUPPORT						"${APPSUPPORT}"
#define PATH_OPEN_EMU								"${OPENEMU}"
#define PATH_WITH_ROM								"${WITHROM}"

#define FILE_EXT_FIRMWARE_CONFIG					"dfc"
#define FILE_EXT_SAVE_STATE							"dst"
#define FILE_EXT_ROM_SAVE							"dsv"
#define FILE_EXT_CHEAT								"dct"
#define FILE_EXT_ROM_SAVE_NOGBA						"sav*"
#define FILE_EXT_ROM_SAVE_RAW						"sav"
#define FILE_EXT_ACTION_REPLAY_SAVE					"duc"
#define FILE_EXT_ROM_DS								"nds"
#define FILE_EXT_ROM_GBA							"ds.gba"
#define FILE_EXT_HW_IMAGE_FILE						"bin"
#define FILE_EXT_ADVANSCENE_DB						"xml"
#define FILE_EXT_R4_CHEAT_DB						"dat"

#define MAX_SAVESTATE_SLOTS							10

#define MAX_VOLUME									100.0f
#define MAX_BRIGHTNESS								100.0f

#define CHEAT_DESCRIPTION_LENGTH					1024

#define GPU_DISPLAY_WIDTH							256
#define GPU_DISPLAY_HEIGHT							192
#define GPU_DISPLAY_COLOR_DEPTH						sizeof(UInt16)
#define GPU_SCREEN_SIZE_BYTES						(GPU_DISPLAY_WIDTH * GPU_DISPLAY_HEIGHT * GPU_DISPLAY_COLOR_DEPTH) // The numbers are: 256px width, 192px height, 16bit color depth

#define DS_DISPLAY_VERTICAL_GAP_TO_HEIGHT_RATIO		(21.0/46.0) // Based on the official DS specification: 21mm/46mm
#define DS_DISPLAY_GAP								(GPU_DISPLAY_HEIGHT * DS_DISPLAY_VERTICAL_GAP_TO_HEIGHT_RATIO)

#define WINDOW_STATUS_BAR_HEIGHT					24		// Height of an emulation window status bar in pixels.

#define SPEED_SCALAR_QUARTER						0.25	// Speed scalar for quarter execution speed.
#define SPEED_SCALAR_HALF							0.5		// Speed scalar for half execution speed.
#define SPEED_SCALAR_THREE_QUARTER					0.75	// Speed scalar for three quarters execution speed.
#define SPEED_SCALAR_NORMAL							1.0		// Speed scalar for normal execution speed.
#define SPEED_SCALAR_DOUBLE							2.0		// Speed scalar for double execution speed.
#define SPEED_SCALAR_MIN							0.005	// Lower limit for the speed multiplier.

#define DS_FRAMES_PER_SECOND						59.8261	// Number of DS frames per second.
#define DS_SECONDS_PER_FRAME						(1.0 / DS_FRAMES_PER_SECOND) // The length of time in seconds that, ideally, a frame should be processed within.

#define FRAME_SKIP_AGGRESSIVENESS					10.0	// Must be a value between 0.0 (inclusive) and positive infinity.
															// This value acts as a scalar multiple of the frame skip.
#define FRAME_SKIP_SMOOTHNESS						0.90	// Must be a value between 0.00 (inclusive) and 1.00 (exclusive).
															// Values closer to 0.00 give better video smoothness, but makes the emulation timing more "jumpy."
															// Values closer to 1.00 makes the emulation timing more accurate, but makes the video look more "choppy."
#define FRAME_SKIP_BIAS								0.5		// May be any real number. This value acts as a vector addition to the frame skip.
#define MAX_FRAME_SKIP								(DS_FRAMES_PER_SECOND / 3.0)

#define SPU_SAMPLE_RATE								44100.0	// Samples per second
#define SPU_SAMPLE_RESOLUTION						16		// Bits per sample; must be a multiple of 8
#define SPU_NUMBER_CHANNELS							2		// Number of channels
#define SPU_SAMPLE_SIZE								((SPU_SAMPLE_RESOLUTION / 8) * SPU_NUMBER_CHANNELS) // Bytes per sample, multiplied by the number of channels
#define SPU_BUFFER_BYTES							((SPU_SAMPLE_RATE / DS_FRAMES_PER_SECOND) * SPU_SAMPLE_SIZE) // Note that this value may be returned as floating point

#define CLOCKWISE_DEGREES(x)						(360.0 - x) // Converts an angle in degrees from normal-direction to clockwise-direction.

#define VOLUME_THRESHOLD_LOW						35.0f
#define VOLUME_THRESHOLD_HIGH						75.0f

#define ROM_ICON_WIDTH								32
#define ROM_ICON_HEIGHT								32

#define ROMINFO_GAME_TITLE_LENGTH					12
#define ROMINFO_GAME_CODE_LENGTH					4
#define ROMINFO_GAME_BANNER_LENGTH					128

#define MIC_SAMPLE_RATE								16000
#define MIC_MAX_BUFFER_SAMPLES						(MIC_SAMPLE_RATE / DS_FRAMES_PER_SECOND)

#define COCOA_DIALOG_CANCEL							0
#define COCOA_DIALOG_DEFAULT						1
#define COCOA_DIALOG_OK								1
#define COCOA_DIALOG_OPTION							2

enum
{
	ROMAUTOLOADOPTION_LOAD_LAST						= 0,
	ROMAUTOLOADOPTION_LOAD_SELECTED					= 1,
	ROMAUTOLOADOPTION_LOAD_NONE						= 10000,
	ROMAUTOLOADOPTION_CHOOSE_ROM					= 10001
};

enum
{
	REASONFORCLOSE_NORMAL = 0,
	REASONFORCLOSE_OPEN,
	REASONFORCLOSE_TERMINATE
};

enum
{
	ROMSAVEFORMAT_DESMUME							= 0,
	ROMSAVEFORMAT_NOGBA								= 1,
	ROMSAVEFORMAT_RAW								= 2
};

enum
{
	ROMSAVETYPE_AUTOMATIC							= 0
};

enum
{
	EMULATION_ENSATA_BIT							= 0,
	EMULATION_ADVANCED_BUS_LEVEL_TIMING_BIT			= 1,
	EMULATION_USE_EXTERNAL_BIOS_BIT					= 2,
	EMULATION_BIOS_SWI_BIT							= 3,
	EMULATION_PATCH_DELAY_LOOP_BIT					= 4,
	EMULATION_USE_EXTERNAL_FIRMWARE_BIT				= 5,
	EMULATION_BOOT_FROM_FIRMWARE_BIT				= 6,
	EMULATION_SLEEP_BIT								= 7,
	EMULATION_CARD_EJECT_BIT						= 8,
	EMULATION_DEBUG_CONSOLE_BIT						= 9,
	EMULATION_RIGOROUS_TIMING_BIT					= 10
};

enum
{
	EMULATION_ENSATA_MASK							= 1 << EMULATION_ENSATA_BIT,
	EMULATION_ADVANCED_BUS_LEVEL_TIMING_MASK		= 1 << EMULATION_ADVANCED_BUS_LEVEL_TIMING_BIT,
	EMULATION_USE_EXTERNAL_BIOS_MASK				= 1 << EMULATION_USE_EXTERNAL_BIOS_BIT,
	EMULATION_BIOS_SWI_MASK							= 1 << EMULATION_BIOS_SWI_BIT,
	EMULATION_PATCH_DELAY_LOOP_MASK					= 1 << EMULATION_PATCH_DELAY_LOOP_BIT,
	EMULATION_USE_EXTERNAL_FIRMWARE_MASK			= 1 << EMULATION_USE_EXTERNAL_FIRMWARE_BIT,
	EMULATION_BOOT_FROM_FIRMWARE_MASK				= 1 << EMULATION_BOOT_FROM_FIRMWARE_BIT,
	EMULATION_SLEEP_MASK							= 1 << EMULATION_SLEEP_BIT,
	EMULATION_CARD_EJECT_MASK						= 1 << EMULATION_CARD_EJECT_BIT,
	EMULATION_DEBUG_CONSOLE_MASK					= 1 << EMULATION_DEBUG_CONSOLE_BIT,
	EMULATION_RIGOROUS_TIMING_MASK					= 1 << EMULATION_RIGOROUS_TIMING_BIT
};

enum
{
	CPU_EMULATION_ENGINE_INTERPRETER				= 0,
	CPU_EMULATION_ENGINE_DYNAMIC_RECOMPILER			= 1
};

enum
{
	GPUSTATE_MAIN_GPU_BIT							= 0,
	GPUSTATE_MAIN_BG0_BIT							= 1,
	GPUSTATE_MAIN_BG1_BIT							= 2,
	GPUSTATE_MAIN_BG2_BIT							= 3,
	GPUSTATE_MAIN_BG3_BIT							= 4,
	GPUSTATE_MAIN_OBJ_BIT							= 5,
	GPUSTATE_SUB_GPU_BIT							= 6,
	GPUSTATE_SUB_BG0_BIT							= 7,
	GPUSTATE_SUB_BG1_BIT							= 8,
	GPUSTATE_SUB_BG2_BIT							= 9,
	GPUSTATE_SUB_BG3_BIT							= 10,
	GPUSTATE_SUB_OBJ_BIT							= 11
};

enum
{
	GPUSTATE_MAIN_GPU_MASK							= 1 << GPUSTATE_MAIN_GPU_BIT,
	GPUSTATE_MAIN_BG0_MASK							= 1 << GPUSTATE_MAIN_BG0_BIT,
	GPUSTATE_MAIN_BG1_MASK							= 1 << GPUSTATE_MAIN_BG1_BIT,
	GPUSTATE_MAIN_BG2_MASK							= 1 << GPUSTATE_MAIN_BG2_BIT,
	GPUSTATE_MAIN_BG3_MASK							= 1 << GPUSTATE_MAIN_BG3_BIT,
	GPUSTATE_MAIN_OBJ_MASK							= 1 << GPUSTATE_MAIN_OBJ_BIT,
	GPUSTATE_SUB_GPU_MASK							= 1 << GPUSTATE_SUB_GPU_BIT,
	GPUSTATE_SUB_BG0_MASK							= 1 << GPUSTATE_SUB_BG0_BIT,
	GPUSTATE_SUB_BG1_MASK							= 1 << GPUSTATE_SUB_BG1_BIT,
	GPUSTATE_SUB_BG2_MASK							= 1 << GPUSTATE_SUB_BG2_BIT,
	GPUSTATE_SUB_BG3_MASK							= 1 << GPUSTATE_SUB_BG3_BIT,
	GPUSTATE_SUB_OBJ_MASK							= 1 << GPUSTATE_SUB_OBJ_BIT
};

enum
{
	SPU_SYNC_MODE_DUAL_SYNC_ASYNC					= 0,
	SPU_SYNC_MODE_SYNCHRONOUS						= 1
};

enum
{
	SPU_SYNC_METHOD_N								= 0,
	SPU_SYNC_METHOD_Z								= 1,
	SPU_SYNC_METHOD_P								= 2
};

enum
{
	CHEAT_TYPE_INTERNAL								= 0,
	CHEAT_TYPE_ACTION_REPLAY						= 1,
	CHEAT_TYPE_CODE_BREAKER							= 2
};

enum
{
	CHEATSEARCH_SEARCHSTYLE_EXACT_VALUE				= 0,
	CHEATSEARCH_SEARCHSTYLE_COMPARATIVE				= 1
};

enum
{
	CHEATSEARCH_COMPARETYPE_GREATER_THAN			= 0,
	CHEATSEARCH_COMPARETYPE_LESSER_THAN				= 1,
	CHEATSEARCH_COMPARETYPE_EQUALS_TO				= 2,
	CHEATSEARCH_COMPARETYPE_NOT_EQUALS_TO			= 3
};

enum
{
	CHEATSEARCH_UNSIGNED							= 0,
	CHEATSEARCH_SIGNED								= 1
};

enum
{
	CHEATEXPORT_ERROR_FILE_NOT_FOUND				= 1,
	CHEATEXPORT_ERROR_WRONG_FILE_FORMAT				= 2,
	CHEATEXPORT_ERROR_SERIAL_NOT_FOUND				= 3,
	CHEATEXPORT_ERROR_EXPORT_FAILED					= 4
};

/*
 PORT MESSAGES
 */
enum
{
	MESSAGE_CHECK_FOR_RESPONSE = 100,		// Message to check if a port is responding. Usually sent to make sure that a thread is alive.
	MESSAGE_CHECK_RESPONSE_ECHO,			// Response message when another port sends MESSAGE_CHECK_FOR_RESPONSE. Sent to confirm that a thread is indeed alive.
	MESSAGE_EXIT_THREAD,					// Sent whenever there is a need to stop a thread.
	
	MESSAGE_EMU_FRAME_PROCESSED = 1000,		// Sent whenever an emulation frame is fully processed. This is mostly used to signal outputs to update themselves based on the new emulation frame.
	MESSAGE_OUTPUT_FINISHED_FRAME,			// Sent from an output device whenever it finishes processing the frame data.
	MESSAGE_SET_EMULATION_FLAGS,
	
	// Video Messages
	MESSAGE_RESIZE_VIEW,
	MESSAGE_TRANSFORM_VIEW,
	MESSAGE_REDRAW_VIEW,
	MESSAGE_SET_GPU_STATE_FLAGS,
	MESSAGE_CHANGE_DISPLAY_TYPE,
	MESSAGE_CHANGE_DISPLAY_ORIENTATION,
	MESSAGE_CHANGE_DISPLAY_ORDER,
	MESSAGE_CHANGE_DISPLAY_GAP,
	MESSAGE_CHANGE_BILINEAR_OUTPUT,
	MESSAGE_CHANGE_VERTICAL_SYNC,
	MESSAGE_CHANGE_VIDEO_FILTER,
	MESSAGE_SET_RENDER3D_METHOD,
	MESSAGE_SET_RENDER3D_HIGH_PRECISION_COLOR_INTERPOLATION,
	MESSAGE_SET_RENDER3D_EDGE_MARKING,
	MESSAGE_SET_RENDER3D_FOG,
	MESSAGE_SET_RENDER3D_TEXTURES,
	MESSAGE_SET_RENDER3D_DEPTH_COMPARISON_THRESHOLD,
	MESSAGE_SET_RENDER3D_THREADS,
	MESSAGE_SET_RENDER3D_LINE_HACK,
	MESSAGE_SET_RENDER3D_MULTISAMPLE,
	MESSAGE_SET_VIEW_TO_BLACK,
	MESSAGE_SET_VIEW_TO_WHITE,
	
	MESSAGE_SET_AUDIO_PROCESS_METHOD,
	MESSAGE_SET_SPU_ADVANCED_LOGIC,
	MESSAGE_SET_SPU_SYNC_MODE,
	MESSAGE_SET_SPU_SYNC_METHOD,
	MESSAGE_SET_SPU_INTERPOLATION_MODE,
	MESSAGE_SET_VOLUME,
	
	MESSAGE_REQUEST_SCREENSHOT,
	MESSAGE_COPY_TO_PASTEBOARD
};

/*
 DS DISPLAY TYPES
 */
enum
{
	DS_DISPLAY_TYPE_MAIN = 0,
	DS_DISPLAY_TYPE_TOUCH,
	DS_DISPLAY_TYPE_DUAL
};

enum
{
	DS_DISPLAY_ORIENTATION_VERTICAL = 0,
	DS_DISPLAY_ORIENTATION_HORIZONTAL
};

enum
{
	DS_DISPLAY_ORDER_MAIN_FIRST = 0,
	DS_DISPLAY_ORDER_TOUCH_FIRST
};

/*
 DS GPU TYPES
 */
enum
{
	DS_GPU_TYPE_MAIN = 0,
	DS_GPU_TYPE_SUB,
	DS_GPU_TYPE_MAIN_AND_SUB
};

/*
 COCOA DS CORE STATES
 */
enum
{
	CORESTATE_EXECUTE = 0,
	CORESTATE_PAUSE
};

/*
 DESMUME 3D RENDERER TYPES
 */
enum
{
	CORE3DLIST_NULL = 0,
	CORE3DLIST_SWRASTERIZE,
	CORE3DLIST_OPENGL
};

/*
 MICROPHONE MODE
 */
enum
{
	MICMODE_NONE = 0,
	MICMODE_INTERNAL_NOISE,
	MICMODE_AUDIO_FILE,
	MICMODE_WHITE_NOISE,
	MICMODE_PHYSICAL,
	MICMODE_SINE_WAVE
};
