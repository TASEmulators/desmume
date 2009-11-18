/*  commandline.cpp

    Copyright (C) 2009 DeSmuME team

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

//windows note: make sure this file gets compiled with _cdecl

#include <glib.h>

#include <stdio.h>
#include "commandline.h"
#include "types.h"
#include "movie.h"
#include "addons.h"
#include "NDSSystem.h"

int scanline_filter_a = 2, scanline_filter_b = 4;

CommandLine::CommandLine()
: is_cflash_configured(false)
, error(NULL)
, ctx(g_option_context_new (""))
, _play_movie_file(0)
, _record_movie_file(0)
, _cflash_image(0)
, _cflash_path(0)
, _bios_arm9(NULL)
, _bios_arm7(NULL)
, _bios_swi(0)
, _num_cores(-1)
{
	load_slot = 0;
	arm9_gdb_port = arm7_gdb_port = 0;
	start_paused = FALSE;
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
		{ "load-slot", 0, 0, G_OPTION_ARG_INT, &load_slot, "Loads savegame from slot NUM", "NUM"},
		{ "play-movie", 0, 0, G_OPTION_ARG_FILENAME, &_play_movie_file, "Specifies a dsm format movie to play", "PATH_TO_PLAY_MOVIE"},
		{ "record-movie", 0, 0, G_OPTION_ARG_FILENAME, &_record_movie_file, "Specifies a path to a new dsm format movie", "PATH_TO_RECORD_MOVIE"},
		{ "start-paused", 0, 0, G_OPTION_ARG_NONE, &start_paused, "Indicates that emulation should start paused", "START_PAUSED"},
		{ "cflash-image", 0, 0, G_OPTION_ARG_FILENAME, &_cflash_image, "Requests cflash in gbaslot with fat image at this path", "CFLASH_IMAGE"},
		{ "cflash-path", 0, 0, G_OPTION_ARG_FILENAME, &_cflash_path, "Requests cflash in gbaslot with filesystem rooted at this path", "CFLASH_PATH"},
		{ "bios-arm9", 0, 0, G_OPTION_ARG_FILENAME, &_bios_arm9, "Uses the arm9 bios provided at the specified path", "BIOS_ARM9_PATH"},
		{ "bios-arm7", 0, 0, G_OPTION_ARG_FILENAME, &_bios_arm7, "Uses the arm7 bios provided at the specified path", "BIOS_ARM7_PATH"},
		{ "bios-swi", 0, 0, G_OPTION_ARG_INT, &_bios_swi, "Uses SWI from the provided bios files", "BIOS_SWI"},
#ifdef _MSC_VER
		{ "num-cores", 0, 0, G_OPTION_ARG_INT, &_num_cores, "Override numcores detection and use this many", "NUM_CORES"},
		{ "scanline-filter-a", 0, 0, G_OPTION_ARG_INT, &scanline_filter_a, "Intensity of fadeout for scanlines filter (edge) (default 2)", "SCANLINE_FILTER_A"},
		{ "scanline-filter-b", 0, 0, G_OPTION_ARG_INT, &scanline_filter_b, "Intensity of fadeout for scanlines filter (corner) (default 4)", "SCANLINE_FILTER_B"},
#endif
		#ifdef GDB_STUB
		{ "arm9gdb", 0, 0, G_OPTION_ARG_INT, &arm9_gdb_port, "Enable the ARM9 GDB stub on the given port", "PORT_NUM"},
		{ "arm7gdb", 0, 0, G_OPTION_ARG_INT, &arm7_gdb_port, "Enable the ARM7 GDB stub on the given port", "PORT_NUM"},
		#endif
		{ NULL }
	};

	g_option_context_add_main_entries (ctx, options, "options");
}

bool CommandLine::parse(int argc,char **argv)
{
	g_option_context_parse (ctx, &argc, &argv, &error);
	if (error)
	{
		g_printerr("Error parsing command line arguments: %s\n", error->message);
		return false;
	}

	if(_play_movie_file) play_movie_file = _play_movie_file;
	if(_record_movie_file) record_movie_file = _record_movie_file;
	if(_cflash_image) cflash_image = _cflash_image;
	if(_cflash_path) cflash_path = _cflash_path;

	if(_num_cores != -1) CommonSettings.num_cores = _num_cores;

	//TODO MAX PRIORITY! change ARM9BIOS etc to be a std::string
	if(_bios_arm9) { CommonSettings.UseExtBIOS = true; strcpy(CommonSettings.ARM9BIOS,_bios_arm9); }
	if(_bios_arm7) { CommonSettings.UseExtBIOS = true; strcpy(CommonSettings.ARM7BIOS,_bios_arm7); }
	if(_bios_swi) CommonSettings.SWIFromBIOS = true;

	if (argc == 2)
		nds_file = argv[1];
	if (argc > 2)
		return false;

	return true;
}

bool CommandLine::validate()
{
	if (load_slot < 0 || load_slot > 10) {
		g_printerr("I only know how to load from slots 1-10, 0 means 'do not load savegame' and is default\n");
		return false;
	}

	if(play_movie_file != "" && record_movie_file != "") {
		g_printerr("Cannot both play and record a movie.\n");
		return false;
	}

	if(record_movie_file != "" && load_slot != 0) {
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
		FCEUI_SaveMovie(record_movie_file.c_str(), L"", 0, NULL);
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

}

