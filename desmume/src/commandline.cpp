/*
	Copyright (C) 2009-2016 DeSmuME team

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
#include "slot1.h"
#include "slot2.h"
#include "NDSSystem.h"
#include "utils/xstring.h"
#include "compat/getopt.h"
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
, _spu_sync_mode(-1)
, _spu_sync_method(-1)
, _spu_advanced(0)
, _num_cores(-1)
, _rigorous_timing(0)
, _advanced_timing(-1)
, _slot1(NULL)
, _slot1_fat_dir(NULL)
, _slot1_fat_dir_type(false)
#ifdef HAVE_JIT
, _cpu_mode(-1)
, _jit_size(-1)
#endif
, _console_type(NULL)
, _advanscene_import(NULL)
, depth_threshold(-1)
, load_slot(-1)
, arm9_gdb_port(0)
, arm7_gdb_port(0)
, start_paused(FALSE)
, autodetect_method(-1)
{
#ifndef HOST_WINDOWS 
	disable_sound = 0;
	disable_limiter = 0;
#endif
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
#ifndef HOST_WINDOWS 
" --disable-sound            Disables the sound output" ENDL
" --disable-limiter          Disables the 60fps limiter" ENDL
" --nojoy                    Disables joystick support" ENDL
#endif
ENDL
"Arguments affecting overall emulation parameters (`sync settings`): " ENDL
#ifdef HAVE_JIT
" --jit-enable               Formerly --cpu-mode; default OFF" ENDL
" --jit-size N               JIT block size 1-100; 1:accurate 100:fast (default)" ENDL
#endif
" --advanced-timing          Use advanced bus-level timing; default ON" ENDL
" --rigorous-timing          Use more realistic component timings; default OFF" ENDL
" --spu-advanced             Enable advanced SPU capture functions (reverb)" ENDL
" --backupmem-db             Use DB for autodetecting backup memory type" ENDL
ENDL
"Arguments affecting the emulated requipment:" ENDL
" --console-type [FAT|LITE|IQUE|DEBUG|DSI]" ENDL
"                            Select basic console type; default FAT" ENDL
" --bios-arm9 BIN_FILE       Uses the ARM9 BIOS provided at the specified path" ENDL
" --bios-arm7 BIN_FILE       Uses the ARM7 BIOS provided at the specified path" ENDL
" --bios-swi                 Uses SWI from the provided bios files (else HLE)" ENDL
ENDL
"Arguments affecting contents of SLOT-1:" ENDL
" --slot1 [RETAIL|RETAILAUTO|R4|RETAILNAND|RETAILMCDROM|RETAILDEBUG]" ENDL
"                            Device type to be used SLOT-1; default RETAILAUTO" ENDL
" --preload-rom              precache ROM to RAM instead of streaming from disk" ENDL
" --slot1-fat-dir DIR        Directory to mount for SLOT-1 flash cards" ENDL
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
"These arguments may be reorganized/renamed in the future." ENDL
;

//https://github.com/mono/mono/blob/b7a308f660de8174b64697a422abfc7315d07b8c/eglib/test/driver.c

#define OPT_NUMCORES 1
#define OPT_SPU_METHOD 2
#define OPT_JIT_SIZE 100

#define OPT_CONSOLE_TYPE 200
#define OPT_ARM9 201
#define OPT_ARM7 202

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

#define OPT_ADVANSCENE 900

bool CommandLine::parse(int argc,char **argv)
{
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
			{ "num-cores", required_argument, nullptr, OPT_NUMCORES },
			{ "spu-synch", no_argument, &_spu_sync_mode, 1 },
			{ "spu-method", required_argument, nullptr, OPT_SPU_METHOD },
			#ifndef HOST_WINDOWS 
				{ "disable-sound", no_argument, &disable_sound, 1},
				{ "disable-limiter", no_argument, &disable_limiter, 1},
				{ "nojoy", no_argument, &_commandline_linux_nojoy, 1},
			#endif

			//sync settings
			#ifdef HAVE_JIT
				{ "jit-enable", no_argument, &_cpu_mode, 1},
				{ "jit-size", required_argument, &_jit_size}, 
			#endif
			{ "rigorous-timing", no_argument, &_spu_advanced, 1},
			{ "advanced-timing", no_argument, &_rigorous_timing, 1},
			{ "spu-advanced", no_argument, &_advanced_timing, 1},
			{ "backupmem-db", no_argument, &autodetect_method, 1},

			//system equipment
			{ "console-type", required_argument, nullptr, OPT_CONSOLE_TYPE },
			{ "bios-arm9", required_argument, nullptr, OPT_ARM9},
			{ "bios-arm7", required_argument, nullptr, OPT_ARM7},
			{ "bios-swi", required_argument, &_bios_swi, 1},

			//slot-1 contents
			{ "slot1", required_argument, nullptr, OPT_SLOT1},
			{ "preload-rom", no_argument, &_load_to_memory, 1},
			{ "slot1-fat-dir", required_argument, nullptr, OPT_SLOT1_FAT_DIR},

			//slot-2 contents
			{ "cflash-image", required_argument, nullptr, OPT_SLOT2_CFLASH_IMAGE},
			{ "cflash-path", required_argument, nullptr, OPT_SLOT2_CFLASH_DIR},
			{ "gbaslot-rom", required_argument, nullptr, OPT_SLOT2_GBAGAME},

			//commands
			{ "start-paused", no_argument, &start_paused, 1},
			{ "load-slot", required_argument, nullptr, OPT_LOAD_SLOT},
			{ "play-movie", required_argument, nullptr, OPT_PLAY_MOVIE},
			{ "record-movie", required_argument, nullptr, OPT_RECORD_MOVIE},

			//video filters
			{ "scanline-filter-a", required_argument, nullptr, OPT_SCANLINES_A},
			{ "scanline-filter-b", required_argument, nullptr, OPT_SCANLINES_B},
			{ "scanline-filter-c", required_argument, nullptr, OPT_SCANLINES_C},
			{ "scanline-filter-d", required_argument, nullptr, OPT_SCANLINES_D},

			//debugging
			#ifdef GDB_STUB
				{ "arm9gdb", required_argument, nullptr, OPT_ARM9GDB},
				{ "arm7gdb", required_argument, nullptr, OPT_ARM7GDB},
			#endif

			//utilities
			{ "advanscene-import", required_argument, nullptr, OPT_ADVANSCENE},
				
			{0,0,0,0}
		};

		int c = getopt_long(argc,argv,"",long_options,&option_index);
		if(c == -1)
			break;

		switch(c)
		{
		case 0: break;

		//user settings
		case OPT_NUMCORES: _num_cores = atoi(optarg); break;
		case OPT_SPU_METHOD: _spu_sync_method = atoi(optarg); break;

		//sync settings
		case OPT_JIT_SIZE: _jit_size = atoi(optarg); break;

		//system equipment
		case OPT_CONSOLE_TYPE: console_type = optarg; break;
		case OPT_ARM9: _bios_arm9 = strdup(_bios_arm9); break;
		case OPT_ARM7: _bios_arm7 = strdup(_bios_arm7); break;

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
		}
	} //arg parsing loop

	if(opt_help)
	{
		printf(help_string);
		exit(1);
	}

	if(_load_to_memory != -1) CommonSettings.loadToMemory = (_load_to_memory == 1)?true:false;
	if(_num_cores != -1) CommonSettings.num_cores = _num_cores;
	if(_rigorous_timing) CommonSettings.rigorous_timing = true;
	if(_advanced_timing != -1) CommonSettings.advanced_timing = _advanced_timing==1;

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
	if(depth_threshold != -1)
		CommonSettings.GFX3D_Zelda_Shadow_Depth_Hack = depth_threshold;


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

	if (autodetect_method != -1)
		CommonSettings.autodetectBackupMethod = autodetect_method;

	//TODO NOT MAX PRIORITY! change ARM9BIOS etc to be a std::string
	if(_bios_arm9) { CommonSettings.UseExtBIOS = true; strcpy(CommonSettings.ARM9BIOS,_bios_arm9); }
	if(_bios_arm7) { CommonSettings.UseExtBIOS = true; strcpy(CommonSettings.ARM7BIOS,_bios_arm7); }
	if(_bios_swi) CommonSettings.SWIFromBIOS = true;
	if(_spu_sync_mode != -1) CommonSettings.SPU_sync_mode = _spu_sync_mode;
	if(_spu_sync_method != -1) CommonSettings.SPU_sync_method = _spu_sync_method;
	if(_spu_advanced) CommonSettings.spu_advanced = true;

	free(_bios_arm9);
	free(_bios_arm7);
	_bios_arm9 = _bios_arm7 = nullptr;

	//remaining argument should be an NDS file, and nothing more
	int remain = argc-optind;
	if(remain==1)
		nds_file = argv[optind];
	else if(remain>1)
		return false;
	
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

	if((_cflash_image && _gbaslot_rom) || (_cflash_path && _gbaslot_rom)) {
		printerror("Cannot specify both cflash and gbaslot rom (both occupy SLOT-2)\n");
	}

	if (autodetect_method < -1 || autodetect_method > 1) {
		printerror("Invalid autodetect save method (0 - internal, 1 - from database)\n");
	}

#ifdef HAVE_JIT
	if (_cpu_mode < -1 || _cpu_mode > 1) {
		printerror("Invalid cpu mode emulation (0 - interpreter, 1 - dynarec)\n");
	}
	if (_jit_size < -1 && (_jit_size == 0 || _jit_size > 100)) {
		printerror("Invalid jit block size [1..100]. set to 100\n");
	}
#endif

	return true;
}

void CommandLine::errorHelp(const char* binName)
{
	//TODO - strip this down to just the filename
	printerror("USAGE: %s [options] [nds-file]\n", binName);
	printerror("USAGE: %s --help    - for help\n", binName);
}

void CommandLine::process_movieCommands()
{
	if(play_movie_file != "")
	{
		FCEUI_LoadMovie(play_movie_file.c_str(),true,false,-1);
	}
	else if(record_movie_file != "")
	{
		FCEUI_SaveMovie(record_movie_file.c_str(), L"", 0, NULL, FCEUI_MovieGetRTCDefault());
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
}

