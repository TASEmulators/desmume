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

CommandLine::CommandLine()
: error(NULL)
, ctx(g_option_context_new (""))
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

static const char* _play_movie_file;
static const char* _record_movie_file;

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
