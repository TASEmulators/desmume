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

#include <glib.h>

#include <stdio.h>
#include "commandline.h"

CommandLine::CommandLine()
: error(NULL)
, ctx(g_option_context_new (""))
{
	load_slot = 0;
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

	return true;
}

void CommandLine::errorHelp(const char* binName)
{
	//TODO - strip this down to just the filename
	g_printerr("USAGE: %s [options] [nds-file]\n", binName);
	g_printerr("USAGE: %s --help    - for help\n", binName);
}

void foo()
{
	g_option_context_free(NULL);
}
