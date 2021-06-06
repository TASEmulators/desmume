/*
	Copyright (C) 2009-2017 DeSmuME team

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

//windows note: make sure this file gets compiled with _cdecl

#include <algorithm>
#include <stdio.h>
#include "commandline.h"
#include "types.h"
#include "movie.h"
#include "rtc.h"
#include "slot1.h"
#include "slot2.h"
#include "NDSSystem.h"
#include "utils/datetime.h"
#include "utils/xstring.h"
#include <compat/getopt.h>
//#include "frontend/modules/mGetOpt.h" //to test with this, make sure global `optind` is initialized to 1

#define printerror(...) fprintf(stderr, __VA_ARGS__)

int _scanline_filter_a = 0, _scanline_filter_b = 2, _scanline_filter_c = 2, _scanline_filter_d = 4;
int _commandline_linux_nojoy = 0;

CommandLine::CommandLine()
: is_cflash_configured(false)
, _load_to_memory(-1)
, _play_movie_file(0)
, _record_movie_file(0)
, _cflash_image(0)
, _cflash_path(0)
, _gbaslot_rom(0)
, _bios_arm9(NULL)
, _bios_arm7(NULL)
, _bios_swi(0)
, _fw_path(NULL)
, _fw_boot(0)
, _spu_sync_mode(-1)
, _spu_sync_method(-1)
, _spu_advanced(0)
, _num_cores(-1)
, _rigorous_timing(0)
, _advanced_timing(-1)
, _gamehacks(-1)
, _texture_deposterize(-1)
, _texture_smooth(-1)
, _slot1(NULL)
, _slot1_fat_dir(NULL)
, _slot1_fat_dir_type(false)
, _slot1_no8000prot(0)
#ifdef HAVE_JIT
, _cpu_mode(-1)
, _jit_size(-1)
#endif
, _console_type(NULL)
, _advanscene_import(NULL)
, load_slot(-1)
, arm9_gdb_port(0)
, arm7_gdb_port(0)
, start_paused(FALSE)
, autodetect_method(-1)
, render3d(COMMANDLINE_RENDER3D_DEFAULT)
, texture_upscale(-1)
, gpu_resolution_multiplier(-1)
, language(1) //english by default
, disable_sound(0)
, disable_limiter(0)
, windowed_fullscreen(0)
, _rtc_day(-1)
, _rtc_hour(-1)
{
}

CommandLine::~CommandLine()
{
}

static char mytoupper(char c) { return ::toupper(c); }

static std::string strtoupper(const std::string& str)
{
	std::string ret = str;
	std::transform(ret.begin(), ret.end(), ret.begin(), ::mytoupper);
	return ret;
}

#define ENDL "\n"
static const char* help_string = \
"Arguments affecting overall emulator behaviour: (`user settings`):" ENDL
" --num-cores N              Override numcores detection and use this many" ENDL
" --spu-synch                Use SPU synch (crackles; helps streams; default ON)" ENDL
" --spu-method N             Select SPU synch method: 0:N, 1:Z, 2:P; default 0" ENDL
" --3d-render [SW|AUTOGL|GL|OLDGL]" ENDL
"                            Select 3d renderer; default SW" ENDL
" --3d-texture-deposterize-enable" ENDL
"                            Enables texture deposterization." ENDL
" --3d-texture-upscale [1|2|4]" ENDL
"                            Automatically upscales textures by the specified" ENDL
"                            amount; 1:No scaling (default), 2:2x upscaling," ENDL
"                            4:4x upscaling" ENDL
" --3d-texture-smoothing-enable" ENDL
"                            Enables smooth texture sampling while rendering." ENDL
#ifdef HOST_WINDOWS
" --gpu-resolution-multiplier N" ENDL
"                            Increases the resolution of GPU rendering by this" ENDL
"                            multipler; 1:256x192 (default), 2:512x384," ENDL
"                            3:768x576, 4:1024x768, 5:1280x960" ENDL
" --windowed-fullscreen" ENDL
"                            Launches in windowed fullscreen (same as alt+enter)" ENDL
#else
" --nojoy                    Disables joystick support" ENDL
#endif
" --disable-sound            Disables the sound output" ENDL
" --disable-limiter          Disables the 60fps limiter" ENDL
" --rtc-day D                Override RTC day, 0=Sunday, 6=Saturday" ENDL
" --rtc-hour H               Override RTC hour, 0=midnight, 23=an hour before" ENDL
ENDL
"Arguments affecting overall emulation parameters (`sync settings`): " ENDL
#ifdef HAVE_JIT
" --jit-enable               Formerly --cpu-mode; default OFF" ENDL
" --jit-size N               JIT block size 1-100; 1:accurate 100:fast (default)" ENDL
#endif
" --advanced-timing          Use advanced bus-level timing; default ON" ENDL
" --rigorous-timing          Use more realistic component timings; default OFF" ENDL
" --gamehacks                Use game-specific hacks; default ON" ENDL
" --spu-advanced             Enable advanced SPU capture functions (reverb)" ENDL
" --backupmem-db             Use DB for autodetecting backup memory type" ENDL
ENDL
"Arguments affecting the emulated requipment:" ENDL
" --console-type [FAT|LITE|IQUE|DEBUG|DSI]" ENDL
"                            Select basic console type; default FAT" ENDL
" --bios-arm9 BIN_FILE       Uses the ARM9 BIOS provided at the specified path" ENDL
" --bios-arm7 BIN_FILE       Uses the ARM7 BIOS provided at the specified path" ENDL
" --firmware-path BIN_FILE   Uses the firmware provided at the specified path" ENDL
" --firmware-boot 0|1        Boot from firmware" ENDL
" --bios-swi                 Uses SWI from the provided bios files (else HLE)" ENDL
" --lang N                   Firmware language (can affect game translations)" ENDL
"                            0 = Japanese, 1 = English (default), 2 = French" ENDL
"                            3 = German, 4 = Italian, 5 = Spanish" ENDL
ENDL
"Arguments affecting contents of SLOT-1:" ENDL
" --slot1 [RETAIL|RETAILAUTO|R4|RETAILNAND|RETAILMCDROM|RETAILDEBUG]" ENDL
"                            Device type to be used SLOT-1; default RETAILAUTO" ENDL
" --preload-rom              precache ROM to RAM instead of streaming from disk" ENDL
" --slot1-fat-dir DIR        Directory to mount for SLOT-1 flash cards" ENDL
" --slot1_no8000prot         Disables retail card copy protection <8000 feature" ENDL
ENDL
"Arguments affecting contents of SLOT-2:" ENDL
" --cflash-image IMG_FILE    Mounts cflash in SLOT-2 with specified image file" ENDL
" --cflash-path DIR          Mounts cflash in SLOT-2 with FS rooted at DIR" ENDL
" --gbaslot-rom GBA_FILE     Mounts GBA specified rom in SLOT-2" ENDL
ENDL
"Commands taking place after ROM is loaded: (be sure to specify a ROM!)" ENDL
" --start-paused             emulation should start paused" ENDL
" --load-slot N              loads savestate from slot N (0-9)" ENDL
" --play-movie DSM_FILE      automatically plays movie" ENDL
" --record-movie DSM_FILE    begin recording a movie" ENDL
ENDL
"Arguments affecting video filters:" ENDL
" --scanline-filter-a N      Fadeout intensity (N/16) (topleft) (default 0)" ENDL
" --scanline-filter-b N      Fadeout intensity (N/16) (topright) (default 2)" ENDL
" --scanline-filter-c N      Fadeout intensity (N/16) (bottomleft) (default 2)" ENDL
" --scanline-filter-d N      Fadeout intensity (N/16) (bottomright) (default 4)" ENDL
ENDL
#ifdef GDB_STUB
"Arguments affecting debugging features:" ENDL
" --arm9gdb PORTNUM          Enable the ARM9 GDB stub on the given port" ENDL
" --arm7gdb PORTNUM          Enable the ARM7 GDB stub on the given port" ENDL
ENDL
#endif
"Utility commands which occur in place of emulation:" ENDL
" --advanscene-import PATH   Import advanscene, dump .ddb, and exit" ENDL
ENDL
"These arguments may be reorganized/renamed in the future." ENDL ENDL
;

//https://github.com/mono/mono/blob/b7a308f660de8174b64697a422abfc7315d07b8c/eglib/test/driver.c

#define OPT_NUMCORES 1
#define OPT_SPU_METHOD 2
#define OPT_3D_RENDER 3
#define OPT_3D_TEXTURE_UPSCALE 81
#define OPT_GPU_RESOLUTION_MULTIPLIER 82
#define OPT_JIT_SIZE 100

#define OPT_CONSOLE_TYPE 200
#define OPT_ARM9 201
#define OPT_ARM7 202
#define OPT_LANGUAGE   203
#define OPT_FIRMPATH 204
#define OPT_FIRMBOOT 205

#define OPT_SLOT1 300
#define OPT_SLOT1_FAT_DIR 301

#define OPT_LOAD_SLOT 400
#define OPT_PLAY_MOVIE 410
#define OPT_RECORD_MOVIE 411

#define OPT_SLOT2_CFLASH_IMAGE 500
#define OPT_SLOT2_CFLASH_DIR 501
#define OPT_SLOT2_GBAGAME 502

#define OPT_SCANLINES_A 600
#define OPT_SCANLINES_B 601
#define OPT_SCANLINES_C 602
#define OPT_SCANLINES_D 603

#define OPT_ARM9GDB 700
#define OPT_ARM7GDB 701

#define OPT_RTC_DAY 800
#define OPT_RTC_HOUR 801

#define OPT_ADVANSCENE 900

bool CommandLine::parse(int argc,char **argv)
{
	//closest thing to a portable main() we have, I guess.
	srand((unsigned)time(nullptr));

	std::string _render3d;

	int opt_help = 0;
	int option_index = 0;
	for(;;)
	{
		//libretro-common's optional argument is not supported presently
		static struct option long_options[] =
		{
			//stuff
			{ "help", no_argument, &opt_help, 1 },

			//user settings
			{ "num-cores", required_argument, NULL, OPT_NUMCORES },
			{ "spu-synch", no_argument, &_spu_sync_mode, 1 },
			{ "spu-method", required_argument, NULL, OPT_SPU_METHOD },
			{ "3d-render", required_argument, NULL, OPT_3D_RENDER },
			{ "3d-texture-deposterize-enable", no_argument, &_texture_deposterize, 1 },
			{ "3d-texture-upscale", required_argument, NULL, OPT_3D_TEXTURE_UPSCALE },
			{ "3d-texture-smoothing-enable", no_argument, &_texture_smooth, 1 },
			#ifdef HOST_WINDOWS
				{ "gpu-resolution-multiplier", required_argument, NULL, OPT_GPU_RESOLUTION_MULTIPLIER },
				{ "windowed-fullscreen", no_argument, &windowed_fullscreen, 1 },
			#else
				{ "nojoy", no_argument, &_commandline_linux_nojoy, 1},
			#endif
			{ "disable-sound", no_argument, &disable_sound, 1},
			{ "disable-limiter", no_argument, &disable_limiter, 1},
			{ "rtc-day", required_argument, NULL, OPT_RTC_DAY},
			{ "rtc-hour", required_argument, NULL, OPT_RTC_HOUR},

			//sync settings
			#ifdef HAVE_JIT
				{ "jit-enable", no_argument, &_cpu_mode, 1},
				{ "jit-size", required_argument, NULL, OPT_JIT_SIZE },
			#endif
			{ "rigorous-timing", no_argument, &_rigorous_timing, 1},
			{ "advanced-timing", no_argument, &_advanced_timing, 1},
			{ "gamehacks", no_argument, &_gamehacks, 1},
			{ "spu-advanced", no_argument, &_spu_advanced, 1},
			{ "backupmem-db", no_argument, &autodetect_method, 1},

			//system equipment
			{ "console-type", required_argument, NULL, OPT_CONSOLE_TYPE },
			{ "bios-arm9", required_argument, NULL, OPT_ARM9},
			{ "bios-arm7", required_argument, NULL, OPT_ARM7},
			{ "bios-swi", no_argument, &_bios_swi, 1},
			{ "firmware-path", required_argument, NULL, OPT_FIRMPATH},
			{ "firmware-boot", required_argument, NULL, OPT_FIRMBOOT},
			{ "lang", required_argument, NULL, OPT_LANGUAGE},

			//slot-1 contents
			{ "slot1", required_argument, NULL, OPT_SLOT1},
			{ "preload-rom", no_argument, &_load_to_memory, 1},
			{ "slot1-fat-dir", required_argument, NULL, OPT_SLOT1_FAT_DIR},
			//and other slot-1 option
			{ "slot1-no8000prot", no_argument, &_slot1_no8000prot, 1},

			//slot-2 contents
			{ "cflash-image", required_argument, NULL, OPT_SLOT2_CFLASH_IMAGE},
			{ "cflash-path", required_argument, NULL, OPT_SLOT2_CFLASH_DIR},
			{ "gbaslot-rom", required_argument, NULL, OPT_SLOT2_GBAGAME},

			//commands
			{ "start-paused", no_argument, &start_paused, 1},
			{ "load-slot", required_argument, NULL, OPT_LOAD_SLOT},
			{ "play-movie", required_argument, NULL, OPT_PLAY_MOVIE},
			{ "record-movie", required_argument, NULL, OPT_RECORD_MOVIE},

			//video filters
			{ "scanline-filter-a", required_argument, NULL, OPT_SCANLINES_A},
			{ "scanline-filter-b", required_argument, NULL, OPT_SCANLINES_B},
			{ "scanline-filter-c", required_argument, NULL, OPT_SCANLINES_C},
			{ "scanline-filter-d", required_argument, NULL, OPT_SCANLINES_D},

			//debugging
			#ifdef GDB_STUB
				{ "arm9gdb", required_argument, NULL, OPT_ARM9GDB},
				{ "arm7gdb", required_argument, NULL, OPT_ARM7GDB},
			#endif

			//utilities
			{ "advanscene-import", required_argument, NULL, OPT_ADVANSCENE},
				
			{0,0,0,0}
		};

		int c = getopt_long(argc,argv,"",long_options,&option_index);
		if(c == -1) break;
		if(c == '?') 
			break;

		switch(c)
		{
		case 0: break;

		//user settings
		case OPT_NUMCORES: _num_cores = atoi(optarg); break;
		case OPT_SPU_METHOD: _spu_sync_method = atoi(optarg); break;
		case OPT_3D_RENDER: _render3d = optarg; break;
		case OPT_3D_TEXTURE_UPSCALE: texture_upscale = atoi(optarg); break;
		case OPT_GPU_RESOLUTION_MULTIPLIER: gpu_resolution_multiplier = atoi(optarg); break;

		//RTC settings
		case OPT_RTC_DAY: _rtc_day = atoi(optarg); break;
		case OPT_RTC_HOUR: _rtc_hour = atoi(optarg); break;

		//sync settings
		#ifdef HAVE_JIT
		case OPT_JIT_SIZE: _jit_size = atoi(optarg); break;
		#endif

		//system equipment
		case OPT_CONSOLE_TYPE: console_type = optarg; break;
		case OPT_ARM9: _bios_arm9 = strdup(optarg); break;
		case OPT_ARM7: _bios_arm7 = strdup(optarg); break;
		case OPT_FIRMPATH: _fw_path = strdup(optarg); break;
		case OPT_FIRMBOOT: _fw_boot = atoi(optarg); break;

		//slot-1 contents
		case OPT_SLOT1: slot1 = strtoupper(optarg); break;
		case OPT_SLOT1_FAT_DIR: slot1_fat_dir = optarg; break;

		//slot-2 contents
		case OPT_SLOT2_CFLASH_IMAGE: cflash_image = optarg; break;
		case OPT_SLOT2_CFLASH_DIR: _cflash_path = optarg; break;
		case OPT_SLOT2_GBAGAME: _gbaslot_rom = optarg; break;

		//commands
		case OPT_LOAD_SLOT: load_slot = atoi(optarg);  break;
		case OPT_PLAY_MOVIE: play_movie_file = optarg; break;
		case OPT_RECORD_MOVIE: record_movie_file = optarg; break;

		//video filters
		case OPT_SCANLINES_A: _scanline_filter_a = atoi(optarg); break;
		case OPT_SCANLINES_B: _scanline_filter_b = atoi(optarg); break;
		case OPT_SCANLINES_C: _scanline_filter_c = atoi(optarg); break;
		case OPT_SCANLINES_D: _scanline_filter_d = atoi(optarg); break;

		//debugging
		case OPT_ARM9GDB: arm9_gdb_port = atoi(optarg); break;
		case OPT_ARM7GDB: arm7_gdb_port = atoi(optarg); break;

		//utilities
		case OPT_ADVANSCENE: CommonSettings.run_advanscene_import = optarg; break;
		case OPT_LANGUAGE: language = atoi(optarg); break;
		}
	} //arg parsing loop

	if(opt_help)
	{
		printf(help_string);
		exit(1);
	}

	if(_load_to_memory != -1) CommonSettings.loadToMemory = (_load_to_memory == 1)?true:false;
	//if(_num_cores != -1) CommonSettings.num_cores = _num_cores;
	if(_rigorous_timing) CommonSettings.rigorous_timing = true;
	if(_advanced_timing != -1) CommonSettings.advanced_timing = _advanced_timing==1;
	if(_gamehacks != -1) CommonSettings.gamehacks.en = _gamehacks==1;

#ifdef HAVE_JIT
	if(_cpu_mode != -1) CommonSettings.use_jit = (_cpu_mode==1);
	if(_jit_size != -1) 
	{
		if ((_jit_size < 1) || (_jit_size > 100)) 
			CommonSettings.jit_max_block_size = 100;
		else
			CommonSettings.jit_max_block_size = _jit_size;
	}
#endif

	//process console type
	CommonSettings.DebugConsole = false;
	CommonSettings.ConsoleType = NDS_CONSOLE_TYPE_FAT;
	console_type = strtoupper(console_type);
	if(console_type == "") {}
	else if(console_type == "FAT") CommonSettings.ConsoleType = NDS_CONSOLE_TYPE_FAT;
	else if(console_type == "LITE") CommonSettings.ConsoleType = NDS_CONSOLE_TYPE_LITE;
	else if(console_type == "IQUE") CommonSettings.ConsoleType = NDS_CONSOLE_TYPE_IQUE;
	else if(console_type == "DSI") CommonSettings.ConsoleType = NDS_CONSOLE_TYPE_DSI;
	else if(console_type == "DEBUG")
	{
		CommonSettings.ConsoleType = NDS_CONSOLE_TYPE_FAT;
		CommonSettings.DebugConsole = true;
	}

	//process 3d renderer 
	_render3d = strtoupper(_render3d);
	if(_render3d == "NONE") render3d = COMMANDLINE_RENDER3D_NONE;
	else if(_render3d == "SW") render3d = COMMANDLINE_RENDER3D_SW;
	else if(_render3d == "OLDGL") render3d = COMMANDLINE_RENDER3D_OLDGL;
	else if(_render3d == "AUTOGL") render3d = COMMANDLINE_RENDER3D_AUTOGL;
	else if(_render3d == "GL") render3d = COMMANDLINE_RENDER3D_GL;

	if (_texture_deposterize != -1) CommonSettings.GFX3D_Renderer_TextureDeposterize = (_texture_deposterize == 1);
	if (_texture_smooth != -1) CommonSettings.GFX3D_Renderer_TextureSmoothing = (_texture_smooth == 1);

	if (autodetect_method != -1)
		CommonSettings.autodetectBackupMethod = autodetect_method;

	//TODO NOT MAX PRIORITY! change ARM9BIOS etc to be a std::string
	if(_bios_arm9) { CommonSettings.UseExtBIOS = true; strcpy(CommonSettings.ARM9BIOS,_bios_arm9); }
	if(_bios_arm7) { CommonSettings.UseExtBIOS = true; strcpy(CommonSettings.ARM7BIOS,_bios_arm7); }
	#ifndef HOST_WINDOWS 
		if(_fw_path) { CommonSettings.UseExtFirmware = true; CommonSettings.UseExtFirmwareSettings = true; strcpy(CommonSettings.ExtFirmwarePath,_fw_path); }
	#endif
	if(_fw_boot) CommonSettings.BootFromFirmware = true;
	if(_bios_swi) CommonSettings.SWIFromBIOS = true;
	if(_slot1_no8000prot) CommonSettings.RetailCardProtection8000 = false;
	if(_spu_sync_mode != -1) CommonSettings.SPU_sync_mode = _spu_sync_mode;
	if(_spu_sync_method != -1) CommonSettings.SPU_sync_method = _spu_sync_method;
	if(_spu_advanced) CommonSettings.spu_advanced = true;

	free(_bios_arm9);
	free(_bios_arm7);
	_bios_arm9 = _bios_arm7 = NULL;

	//remaining argument should be an NDS file, and nothing more
	int remain = argc-optind;
	if(remain==1)
		nds_file = argv[optind];
	else if(remain>1) return false;
	
	return true;
}

bool CommandLine::validate()
{
	if(slot1 != "")
	{
		if(slot1 != "R4" && slot1 != "RETAIL" && slot1 != "NONE" && slot1 != "RETAILNAND") {
			printerror("Invalid slot1 device specified.\n");
			return false;
		}
	}

	if (_load_to_memory < -1 || _load_to_memory > 1) {
		printerror("Invalid parameter (0 - stream from disk, 1 - from RAM)\n");
		return false;
	}

	if (_spu_sync_mode < -1 || _spu_sync_mode > 1) {
		printerror("Invalid parameter\n");
		return false;
	}

	if (_spu_sync_method < -1 || _spu_sync_method > 2) {
		printerror("Invalid parameter\n");
		return false;
	}

	if (load_slot < -1 || load_slot > 10) {
		printerror("I only know how to load from slots 0-10; -1 means 'do not load savegame' and is default\n");
		return false;
	}

	if(play_movie_file != "" && record_movie_file != "") {
		printerror("Cannot both play and record a movie.\n");
		return false;
	}

	if(record_movie_file != "" && load_slot != -1) {
		printerror("Cannot both record a movie and load a savestate.\n");
		return false;
	}

	if(cflash_path != "" && cflash_image != "") {
		printerror("Cannot specify both cflash-image and cflash-path.\n");
		return false;
	}

	if((_bios_arm9 && !_bios_arm7) || (_bios_arm7 && !_bios_arm9)) {
		printerror("If either bios-arm7 or bios-arm9 are specified, both must be.\n");
		return false;
	}

	if(_bios_swi && (!_bios_arm7 || !_bios_arm9)) {
		printerror("If either bios-swi is used, bios-arm9 and bios-arm7 must be specified.\n");
	}

	if(_fw_boot && (!_fw_path)) {
		printerror("If either firmware boot is used, firmware path must be specified.\n");
 	}

	if((_cflash_image && _gbaslot_rom) || (_cflash_path && _gbaslot_rom)) {
		printerror("Cannot specify both cflash and gbaslot rom (both occupy SLOT-2)\n");
	}

	if (autodetect_method < -1 || autodetect_method > 1) {
		printerror("Invalid autodetect save method (0 - internal, 1 - from database)\n");
	}

	if ( (texture_upscale != -1) && (texture_upscale != 1) && (texture_upscale != 2) && (texture_upscale != 4) ) {
		printerror("Invalid texture upscaling value [1|2|4]. Ignoring command line setting.\n");
		texture_upscale = -1;
	}

	if ( (gpu_resolution_multiplier != -1) && ((gpu_resolution_multiplier < 1) || (gpu_resolution_multiplier > 5)) ) {
		printerror("Invalid GPU resolution multiplier [1..5]. Ignoring command line setting.\n");
		gpu_resolution_multiplier = -1;
	}

#ifdef HAVE_JIT
	if (_cpu_mode < -1 || _cpu_mode > 1) {
		printerror("Invalid cpu mode emulation (0 - interpreter, 1 - dynarec)\n");
	}
	if (_jit_size < -1 && (_jit_size == 0 || _jit_size > 100)) {
		printerror("Invalid jit block size [1..100]. set to 100\n");
	}
#endif
        if (_rtc_day < -1 || _rtc_day > 6) {
                printerror("Invalid rtc day override, valid values are from 0 to 6");
                return false;
        }
        if (_rtc_hour < -1 || _rtc_hour > 23) {
                printerror("Invalid rtc day override, valid values are from 0 to 23");
                return false;
        }

	return true;
}

void CommandLine::errorHelp(const char* binName)
{
	printerror(help_string);
}

void CommandLine::process_movieCommands()
{
	if(play_movie_file != "")
	{
		FCEUI_LoadMovie(play_movie_file.c_str(),true,false,-1);
	}
	else if(record_movie_file != "")
	{
		FCEUI_SaveMovie(record_movie_file.c_str(), L"", START_BLANK, NULL, FCEUI_MovieGetRTCDefault());
	}
}

void CommandLine::process_addonCommands()
{
	if (cflash_image != "")
	{
		CFlash_Mode = ADDON_CFLASH_MODE_File;
		CFlash_Path = cflash_image;
		is_cflash_configured = true;
	}
	if (cflash_path != "")
	{
		CFlash_Mode = ADDON_CFLASH_MODE_Path;
		CFlash_Path = cflash_path;
		is_cflash_configured = true;
	}

	if(slot1_fat_dir != "")
		slot1_SetFatDir(slot1_fat_dir);

	if(slot1 == "RETAIL")
		slot1_Change(NDS_SLOT1_RETAIL_AUTO);
	else if(slot1 == "RETAILAUTO")
		slot1_Change(NDS_SLOT1_RETAIL_AUTO);
	else if(slot1 == "R4")
		slot1_Change(NDS_SLOT1_R4);
	else if(slot1 == "RETAILNAND")
		slot1_Change(NDS_SLOT1_RETAIL_NAND);
		else if(slot1 == "RETAILMCROM")
			slot1_Change(NDS_SLOT1_RETAIL_MCROM);
			else if(slot1 == "RETAILDEBUG")
				slot1_Change(NDS_SLOT1_RETAIL_DEBUG);

        if (_rtc_day != -1 || _rtc_hour != -1) {
                DateTime now = DateTime::get_Now();
                int cur_day = now.get_DayOfWeek();
                int cur_hour = now.get_Hour();
                int cur_total = cur_day * 24 + cur_hour;
                int day = (_rtc_day != -1 ? _rtc_day : cur_day);
                int hour = (_rtc_hour != -1 ? _rtc_hour : cur_hour);
                int total = day * 24 + hour;
                int diff = total - cur_total;
                if (diff < 0)
                        diff += 24 * 7;
                rtcHourOverride = diff;
        }
}

