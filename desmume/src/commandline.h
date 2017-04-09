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

#ifndef _COMMANDLINE_H_
#define _COMMANDLINE_H_

#include <string>
#include "types.h"

//hacky commandline options that i didnt want to route through commonoptions
extern int _commandline_linux_nojoy;

#define COMMANDLINE_RENDER3D_DEFAULT 0
#define COMMANDLINE_RENDER3D_NONE 1
#define COMMANDLINE_RENDER3D_SW 2
#define COMMANDLINE_RENDER3D_OLDGL 3
#define COMMANDLINE_RENDER3D_GL 4
#define COMMANDLINE_RENDER3D_AUTOGL 5

//this class will also eventually try to take over the responsibility of using the args that it handles
//for example: preparing the emulator run by loading the rom, savestate, and/or movie in the correct pattern.
//it should also populate CommonSettings with its initial values
//EDIT: not really. combining this with what a frontend wants to do is complicated. 
//you might design the API so that the frontend sets all those up, but I'm not sure I like that
//Really, this should be a passive structure that just collects the results provided by the shared command line processing, to be used later as appropriate
//(and the CommonSettings setup REMOVED or at least refactored into a separate method)

class CommandLine
{
public:
	//actual options: these may move to another struct
	int load_slot;
	int autodetect_method;
	int render3d;
	int texture_upscale;
	int gpu_resolution_multiplier;
	int language;
	std::string nds_file;
	std::string play_movie_file;
	std::string record_movie_file;
	int arm9_gdb_port, arm7_gdb_port;
	int start_paused;
	std::string cflash_image;
	std::string cflash_path;
	std::string gbaslot_rom;
	std::string slot1;
	std::string console_type;
	std::string slot1_fat_dir;
	bool _slot1_fat_dir_type;
	int disable_sound;
	int disable_limiter;

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

	int _spu_sync_mode;
	int _spu_sync_method;
private:
	char* _play_movie_file;
	char* _record_movie_file;
	char* _cflash_image;
	char* _cflash_path;
	char* _gbaslot_rom;
	char* _bios_arm9, *_bios_arm7;
	int _load_to_memory;
	int _bios_swi;
	int _spu_advanced;
	int _num_cores;
	int _rigorous_timing;
	int _advanced_timing;
	int _gamehacks;
	int _texture_deposterize;
	int _texture_smooth;
#ifdef HAVE_JIT
	int _cpu_mode;
	int _jit_size;
#endif
	char* _slot1;
	char *_slot1_fat_dir;
	char* _console_type;
	char* _advanscene_import;
	int _rtc_day;
	int _rtc_hour;
};

#endif
