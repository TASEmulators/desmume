/*  commandline.h

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

#ifndef _COMMANDLINE_H_
#define _COMMANDLINE_H_

#include <string>

//I hate C. we have to forward declare these with more detail than I like
typedef struct _GOptionContext GOptionContext;
typedef struct _GError GError;

//this class will also eventually try to take over the responsibility of using the args that it handles
//for example: preparing the emulator run by loading the rom, savestate, and/or movie in the correct pattern.
//it should also populate CommonSettings with its initial values

class CommandLine
{
public:
	//actual options: these may move to another sturct
	int load_slot;
	std::string nds_file;
	std::string play_movie_file;
	std::string record_movie_file;
	int arm9_gdb_port, arm7_gdb_port;
	int start_paused;
	std::string cflash_image;
	std::string cflash_path;

	//load up the common commandline options
	void loadCommonOptions();
	
	bool parse(int argc,char **argv);

	//validate the common commandline options
	bool validate();

	//process movie play/record commands
	void process_movieCommands();
	//etc.
	void process_addonCommands();
	bool is_cflash_configured;
	
	//print a little help message for cases when erroneous commandlines are entered
	void errorHelp(const char* binName);

	CommandLine();
	~CommandLine();

	GError *error;
	GOptionContext *ctx;

private:
	char* _play_movie_file;
	char* _record_movie_file;
	char* _cflash_image;
	char* _cflash_path;
	char* _bios_arm9, *_bios_arm7;
	int _bios_swi;
	int _num_cores;
};

#endif
