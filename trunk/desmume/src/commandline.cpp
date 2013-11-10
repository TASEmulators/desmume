/*
	Copyright (C) 2009-2013 DeSmuME team

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

#include <glib.h>
#include <algorithm>
#include <stdio.h>
#include "commandline.h"
#include "types.h"
#include "movie.h"
#include "slot1.h"
#include "slot2.h"
#include "NDSSystem.h"
#include "utils/xstring.h"

int _scanline_filter_a = 0, _scanline_filter_b = 2, _scanline_filter_c = 2, _scanline_filter_d = 4;
int _commandline_linux_nojoy = 0;

CommandLine::CommandLine()
: is_cflash_configured(false)
, _load_to_memory(-1)
, error(NULL)
, ctx(g_option_context_new (""))
, _play_movie_file(0)
, _record_movie_file(0)
, _cflash_image(0)
, _cflash_path(0)
, _gbaslot_rom(0)
, _bios_arm9(NULL)
, _bios_arm7(NULL)
, _bios_swi(0)
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
	if(error) g_error_free (error);
	g_option_context_free (ctx);
}

void CommandLine::loadCommonOptions()
{
	//these options should be available in every port.
	//my advice is, do not be afraid of using #ifdef here if it makes sense.
	//but also see the gtk port for an example of how to combine this with other options
	//(you may need to use ifdefs to cause options to be entered in the desired order)
	static const GOptionEntry options[] = {
		{ "load-type", 0, 0, G_OPTION_ARG_INT, &_load_to_memory, "ROM loading method, 0 - stream from disk (like an iso), 1 - load entirely to RAM (default 1)", "LOAD_TYPE"},
		{ "load-slot", 0, 0, G_OPTION_ARG_INT, &load_slot, "Loads savestate from slot NUM", "NUM"},
		{ "play-movie", 0, 0, G_OPTION_ARG_FILENAME, &_play_movie_file, "Specifies a dsm format movie to play", "PATH_TO_PLAY_MOVIE"},
		{ "record-movie", 0, 0, G_OPTION_ARG_FILENAME, &_record_movie_file, "Specifies a path to a new dsm format movie", "PATH_TO_RECORD_MOVIE"},
		{ "start-paused", 0, 0, G_OPTION_ARG_NONE, &start_paused, "Indicates that emulation should start paused", "START_PAUSED"},
		{ "cflash-image", 0, 0, G_OPTION_ARG_FILENAME, &_cflash_image, "Requests cflash in gbaslot with fat image at this path", "CFLASH_IMAGE"},
		{ "cflash-path", 0, 0, G_OPTION_ARG_FILENAME, &_cflash_path, "Requests cflash in gbaslot with filesystem rooted at this path", "CFLASH_PATH"},
		{ "gbaslot-rom", 0, 0, G_OPTION_ARG_FILENAME, &_gbaslot_rom, "Requests this GBA rom in gbaslot", "GBASLOT_ROM"},
		{ "bios-arm9", 0, 0, G_OPTION_ARG_FILENAME, &_bios_arm9, "Uses the arm9 bios provided at the specified path", "BIOS_ARM9_PATH"},
		{ "bios-arm7", 0, 0, G_OPTION_ARG_FILENAME, &_bios_arm7, "Uses the arm7 bios provided at the specified path", "BIOS_ARM7_PATH"},
		{ "bios-swi", 0, 0, G_OPTION_ARG_INT, &_bios_swi, "Uses SWI from the provided bios files", "BIOS_SWI"},
		{ "spu-advanced", 0, 0, G_OPTION_ARG_INT, &_spu_advanced, "Uses advanced SPU capture functions", "SPU_ADVANCED"},
		{ "num-cores", 0, 0, G_OPTION_ARG_INT, &_num_cores, "Override numcores detection and use this many", "NUM_CORES"},
		{ "scanline-filter-a", 0, 0, G_OPTION_ARG_INT, &_scanline_filter_a, "Intensity of fadeout for scanlines filter (topleft) (default 0)", "SCANLINE_FILTER_A"},
		{ "scanline-filter-b", 0, 0, G_OPTION_ARG_INT, &_scanline_filter_b, "Intensity of fadeout for scanlines filter (topright) (default 2)", "SCANLINE_FILTER_B"},
		{ "scanline-filter-c", 0, 0, G_OPTION_ARG_INT, &_scanline_filter_c, "Intensity of fadeout for scanlines filter (bottomleft) (default 2)", "SCANLINE_FILTER_C"},
		{ "scanline-filter-d", 0, 0, G_OPTION_ARG_INT, &_scanline_filter_d, "Intensity of fadeout for scanlines filter (bottomright) (default 4)", "SCANLINE_FILTER_D"},
		{ "rigorous-timing", 0, 0, G_OPTION_ARG_INT, &_rigorous_timing, "Use some rigorous timings instead of unrealistically generous (default 0)", "RIGOROUS_TIMING"},
		{ "advanced-timing", 0, 0, G_OPTION_ARG_INT, &_advanced_timing, "Use advanced BUS-level timing (default 1)", "ADVANCED_TIMING"},
		{ "slot1", 0, 0, G_OPTION_ARG_STRING, &_slot1, "Device to mount in slot 1 (default retail)", "SLOT1"},
		{ "slot1-fat-dir", 0, 0, G_OPTION_ARG_STRING, &_slot1_fat_dir, "Directory to scan for slot 1", "SLOT1_DIR"},
		{ "depth-threshold", 0, 0, G_OPTION_ARG_INT, &depth_threshold, "Depth comparison threshold (default 0)", "DEPTHTHRESHOLD"},
		{ "console-type", 0, 0, G_OPTION_ARG_STRING, &_console_type, "Select console type: {fat,lite,ique,debug,dsi}", "CONSOLETYPE" },
#ifdef HAVE_JIT
		{ "cpu-mode", 0, 0, G_OPTION_ARG_INT, &_cpu_mode, "ARM CPU emulation mode: 0 - interpreter, 1 - dynarec (default 1)", NULL},
		{ "jit-size", 0, 0, G_OPTION_ARG_INT, &_jit_size, "ARM JIT block size: 1..100 (1 - accuracy, 100 - faster) (default 100)", NULL},
#endif
#ifndef HOST_WINDOWS 
		{ "disable-sound", 0, 0, G_OPTION_ARG_NONE, &disable_sound, "Disables the sound emulation", NULL},
		{ "disable-limiter", 0, 0, G_OPTION_ARG_NONE, &disable_limiter, "Disables the 60fps limiter", NULL},
		{ "nojoy", 0, 0, G_OPTION_ARG_INT, &_commandline_linux_nojoy, "Disables joystick support", "NOJOY"},
#endif
#ifdef GDB_STUB
		{ "arm9gdb", 0, 0, G_OPTION_ARG_INT, &arm9_gdb_port, "Enable the ARM9 GDB stub on the given port", "PORT_NUM"},
		{ "arm7gdb", 0, 0, G_OPTION_ARG_INT, &arm7_gdb_port, "Enable the ARM7 GDB stub on the given port", "PORT_NUM"},
#endif
		{ "autodetect_method", 0, 0, G_OPTION_ARG_INT, &autodetect_method, "Autodetect backup method (0 - internal, 1 - from database)", "AUTODETECT_METHOD"},
		{ NULL }
	};

	g_option_context_add_main_entries (ctx, options, "options");
}

static char mytoupper(char c) { return ::toupper(c); }

static std::string strtoupper(const std::string& str)
{
	std::string ret = str;
	std::transform(ret.begin(), ret.end(), ret.begin(), ::mytoupper);
	return ret;
}

bool CommandLine::parse(int argc,char **argv)
{
	g_option_context_parse (ctx, &argc, &argv, &error);
	if (error)
	{
		g_printerr("Error parsing command line arguments: %s\n", error->message);
		return false;
	}

	if(_slot1_fat_dir) slot1_fat_dir = _slot1_fat_dir;
	if(_slot1) slot1 = _slot1; slot1 = strtoupper(slot1);
	if(_console_type) console_type = _console_type;
	if(_load_to_memory != -1) CommonSettings.loadToMemory = (_load_to_memory == 1)?true:false;
	if(_play_movie_file) play_movie_file = _play_movie_file;
	if(_record_movie_file) record_movie_file = _record_movie_file;
	if(_cflash_image) cflash_image = _cflash_image;
	if(_cflash_path) cflash_path = _cflash_path;
	if(_gbaslot_rom) gbaslot_rom = _gbaslot_rom;

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
	if(_spu_advanced) CommonSettings.spu_advanced = true;

	if (argc == 2)
		nds_file = argv[1];
	if (argc > 2)
		return false;

	return true;
}

bool CommandLine::validate()
{


	if(slot1 != "")
	{
		if(slot1 != "R4" && slot1 != "RETAIL" && slot1 != "NONE" && slot1 != "RETAILNAND") {
			g_printerr("Invalid slot1 device specified.\n");
			return false;
		}
	}

	if (_load_to_memory < -1 || _load_to_memory > 1) {
		g_printerr("Invalid parameter (0 - stream from disk, 1 - from RAM)\n");
		return false;
	}

	if (load_slot < -1 || load_slot > 10) {
		g_printerr("I only know how to load from slots 0-10; -1 means 'do not load savegame' and is default\n");
		return false;
	}

	if(play_movie_file != "" && record_movie_file != "") {
		g_printerr("Cannot both play and record a movie.\n");
		return false;
	}

	if(record_movie_file != "" && load_slot != -1) {
		g_printerr("Cannot both record a movie and load a savestate.\n");
		return false;
	}

	if(cflash_path != "" && cflash_image != "") {
		g_printerr("Cannot specify both cflash-image and cflash-path.\n");
		return false;
	}

	if((_bios_arm9 && !_bios_arm7) || (_bios_arm7 && !_bios_arm9)) {
		g_printerr("If either bios-arm7 or bios-arm9 are specified, both must be.\n");
		return false;
	}

	if(_bios_swi && (!_bios_arm7 || !_bios_arm9)) {
		g_printerr("If either bios-swi is used, bios-arm9 and bios-arm7 must be specified.\n");
	}

	if((_cflash_image && _gbaslot_rom) || (_cflash_path && _gbaslot_rom)) {
		g_printerr("Cannot specify both cflash and gbaslot rom (both occupy SLOT-2)\n");
	}

	if (autodetect_method < -1 || autodetect_method > 1) {
		g_printerr("Invalid autodetect save method (0 - internal, 1 - from database)\n");
	}

#ifdef HAVE_JIT
	if (_cpu_mode < -1 || _cpu_mode > 1) {
		g_printerr("Invalid cpu mode emulation (0 - interpreter, 1 - dynarec)\n");
	}
	if (_jit_size < -1 && (_jit_size == 0 || _jit_size > 100)) {
		g_printerr("Invalid jit block size [1..100]. set to 100\n");
	}
#endif

	return true;
}

void CommandLine::errorHelp(const char* binName)
{
	//TODO - strip this down to just the filename
	g_printerr("USAGE: %s [options] [nds-file]\n", binName);
	g_printerr("USAGE: %s --help    - for help\n", binName);
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

