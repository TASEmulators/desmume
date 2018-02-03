/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2018 DeSmuME Team

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
#define STRING_DESMUME_BUG_REPORT_SITE				"https://github.com/TASVideos/desmume/issues"

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
#define NSSTRING_TITLE_SELECT_MPCF_FOLDER_PANEL		NSLocalizedString(@"Select MPCF Folder", nil)
#define NSSTRING_TITLE_SELECT_MPCF_DISK_IMAGE_PANEL	NSLocalizedString(@"Select MPCF Disk Image", nil)
#define NSSTRING_TITLE_CHOOSE_GBA_CARTRIDGE_PANEL	NSLocalizedString(@"Choose GBA Cartridge", nil)
#define NSSTRING_TITLE_CHOOSE_GBA_SRAM_PANEL		NSLocalizedString(@"Choose GBA SRAM File", nil)
#define NSSTRING_TITLE_SAVE_SCREENSHOT_PANEL		NSLocalizedString(@"Screenshot Save Location", nil)

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
#define NSSTRING_ALERT_SCREENSHOT_FAILED_TITLE		NSLocalizedString(@"DeSmuME could not create the screenshot file.", nil)
#define NSSTRING_ALERT_SCREENSHOT_FAILED_MESSAGE	NSLocalizedString(@"The screenshot file could not be written using the selected format.", nil)

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
#define NSSTRING_STATUS_AUTOHOLD_SET				NSLocalizedString(@"Autohold set...", nil)
#define NSSTRING_STATUS_AUTOHOLD_SET_RELEASE		NSLocalizedString(@"Autohold set released.", nil)
#define NSSTRING_STATUS_AUTOHOLD_CLEAR				NSLocalizedString(@"Autohold cleared.", nil)
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

#define NSSTRING_STATUS_SLOT2_LOADED_NONE			NSLocalizedString(@"No SLOT-2 device loaded.", nil)
#define NSSTRING_STATUS_SLOT2_LOADED_AUTOMATIC		NSLocalizedString(@"Loaded SLOT-2 device using automatic selection.\nSelected device type: %@", nil)
#define NSSTRING_STATUS_SLOT2_LOADED_MPCF_WITH_ROM		NSLocalizedString(@"Compact flash device loaded with data from the ROM's directory.", nil)
#define NSSTRING_STATUS_SLOT2_LOADED_MPCF_DIRECTORY		NSLocalizedString(@"Compact flash device loaded with data from directory path:\n%s", nil)
#define NSSTRING_STATUS_SLOT2_LOADED_MPCF_DISK_IMAGE	NSLocalizedString(@"Compact flash device loaded using disk image:\n%s", nil)
#define NSSTRING_STATUS_SLOT2_LOADED_GBA_CART_WITH_SRAM	NSLocalizedString(@"GBA cartridge loaded with SRAM file:\n%s", nil)
#define NSSTRING_STATUS_SLOT2_LOADED_GBA_CART_NO_SRAM	NSLocalizedString(@"GBA cartridge loaded. (No associated SRAM file loaded.)", nil)
#define NSSTRING_STATUS_SLOT2_LOADED_GENERIC_DEVICE	NSLocalizedString(@"Loaded SLOT-2 device:\n%@", nil)
#define NSSTRING_STATUS_SLOT2_LOADED_UNKNOWN		NSLocalizedString(@"An unknown SLOT-2 device has been loaded.", nil)

#define NSSTRING_STATUS_NO_ROM_CHOSEN				NSLocalizedString(@"No ROM chosen.", nil)
#define NSSTRING_STATUS_NO_FOLDER_CHOSEN			NSLocalizedString(@"No folder chosen.", nil)
#define NSSTRING_STATUS_NO_DISK_IMAGE_CHOSEN		NSLocalizedString(@"No disk image chosen.", nil)
#define NSSTRING_STATUS_NO_GBA_CART_CHOSEN			NSLocalizedString(@"No GBA cartridge chosen.", nil)
#define NSSTRING_STATUS_NO_GBA_SRAM_CHOSEN			NSLocalizedString(@"No GBA SRAM chosen.", nil)
#define NSSTRING_STATUS_NO_GBA_SRAM_FOUND			NSLocalizedString(@"No GBA SRAM found.", nil)

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
#define FILE_EXT_ACTION_REPLAY_SAVE					"dss"
#define FILE_EXT_ACTION_REPLAY_MAX_SAVE				"duc"
#define FILE_EXT_ROM_DS								"nds"
#define FILE_EXT_ROM_GBA							"ds.gba"
#define FILE_EXT_HW_IMAGE_FILE						"bin"
#define FILE_EXT_ADVANSCENE_DB						"xml"
#define FILE_EXT_R4_CHEAT_DB						"dat"
#define FILE_EXT_GBA_ROM							"gba"
#define FILE_EXT_GBA_SRAM							"sav"

#define MAX_SAVESTATE_SLOTS							10

#define MAX_VOLUME									100.0f
#define MAX_BRIGHTNESS								100.0f

#define CHEAT_DESCRIPTION_LENGTH					1024

#define WINDOW_STATUS_BAR_HEIGHT					24		// Height of an emulation window status bar in pixels.

//#define SPU_SAMPLE_RATE								(44100.0 * DS_FRAMES_PER_SECOND / 60.0)	// Samples per second
#define SPU_SAMPLE_RATE								44100.0	// Samples per second
#define SPU_SAMPLE_RESOLUTION						16		// Bits per sample; must be a multiple of 8
#define SPU_NUMBER_CHANNELS							2		// Number of channels
#define SPU_SAMPLE_SIZE								((SPU_SAMPLE_RESOLUTION / 8) * SPU_NUMBER_CHANNELS) // Bytes per sample, multiplied by the number of channels
#define SPU_BUFFER_BYTES							((SPU_SAMPLE_RATE / DS_FRAMES_PER_SECOND) * SPU_SAMPLE_SIZE) // Note that this value may be returned as floating point

#define VOLUME_THRESHOLD_LOW						35.0f
#define VOLUME_THRESHOLD_HIGH						75.0f

#define ROM_ICON_WIDTH								32
#define ROM_ICON_HEIGHT								32

#define ROMINFO_GAME_TITLE_LENGTH					12
#define ROMINFO_GAME_CODE_LENGTH					4
#define ROMINFO_GAME_BANNER_LENGTH					128

#define COCOA_DIALOG_CANCEL							0
#define COCOA_DIALOG_DEFAULT						1
#define COCOA_DIALOG_OK								1
#define COCOA_DIALOG_OPTION							2

#define RUMBLE_ITERATIONS_RUMBLE_PAK				2
#define RUMBLE_ITERATIONS_ENABLE					1
#define RUMBLE_ITERATIONS_TEST						3

enum
{
	ROMAUTOLOADOPTION_LOAD_LAST						= 0,
	ROMAUTOLOADOPTION_LOAD_SELECTED					= 1,
	ROMAUTOLOADOPTION_LOAD_NONE						= 10000,
	ROMAUTOLOADOPTION_CHOOSE_ROM					= 10001
};

enum
{
	MPCF_OPTION_LOAD_WITH_ROM						= 0,
	MPCF_OPTION_LOAD_DIRECTORY						= 1,
	MPCF_OPTION_LOAD_DISK_IMAGE						= 2,
	MPCF_ACTION_CHOOSE_DIRECTORY					= 10000,
	MPCF_ACTION_CHOOSE_DISK_IMAGE					= 10001
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
	MESSAGE_NONE = 0,
	
	MESSAGE_CHECK_FOR_RESPONSE = 100,		// Message to check if a port is responding. Usually sent to make sure that a thread is alive.
	MESSAGE_CHECK_RESPONSE_ECHO,			// Response message when another port sends MESSAGE_CHECK_FOR_RESPONSE. Sent to confirm that a thread is indeed alive.
	MESSAGE_EXIT_THREAD,					// Sent whenever there is a need to stop a thread.
	
	MESSAGE_EMU_FRAME_PROCESSED = 1000,		// Sent whenever an emulation frame is fully processed. This is mostly used to signal outputs to update themselves based on the new emulation frame.
	MESSAGE_OUTPUT_FINISHED_FRAME,			// Sent from an output device whenever it finishes processing the frame data.
	MESSAGE_SET_EMULATION_FLAGS,
	
	// Video Messages
	MESSAGE_FETCH_AND_PUSH_VIDEO,
	MESSAGE_RECEIVE_GPU_FRAME,
	MESSAGE_CHANGE_VIEW_PROPERTIES,
	MESSAGE_REDRAW_VIEW,
	MESSAGE_RELOAD_REPROCESS_REDRAW,
	MESSAGE_REPROCESS_AND_REDRAW,
	MESSAGE_SET_GPU_STATE_FLAGS,
	MESSAGE_SET_RENDER3D_METHOD,
	MESSAGE_SET_RENDER3D_HIGH_PRECISION_COLOR_INTERPOLATION,
	MESSAGE_SET_RENDER3D_EDGE_MARKING,
	MESSAGE_SET_RENDER3D_FOG,
	MESSAGE_SET_RENDER3D_TEXTURES,
	MESSAGE_SET_RENDER3D_DEPTH_COMPARISON_THRESHOLD,
	MESSAGE_SET_RENDER3D_THREADS,
	MESSAGE_SET_RENDER3D_LINE_HACK,
	MESSAGE_SET_RENDER3D_MULTISAMPLE,
	
	MESSAGE_COPY_TO_PASTEBOARD
};

enum
{
	VIDEO_SOURCE_INTERNAL	= 0,
	VIDEO_SOURCE_EMULATOR	= 1
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

enum
{
	PADDLE_CONTROL_RELATIVE = 0,
	PADDLE_CONTROL_DIRECT
};
