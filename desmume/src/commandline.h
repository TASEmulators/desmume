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
	int load_slot = -1;
	int autodetect_method = -1;
	int render3d = COMMANDLINE_RENDER3D_DEFAULT;
	int texture_upscale = -1;
	int gpu_resolution_multiplier = -1;
	int language = 1; // English by default
	float scale = 1.0;
	std::string nds_file;
	std::string play_movie_file;
	std::string record_movie_file;
	int arm9_gdb_port = 0;
	int arm7_gdb_port = 0;
	int start_paused = FALSE;
	std::string cflash_image;
	std::string cflash_path;
	std::string gbaslot_rom;
	std::string slot1;
	std::string console_type;
	std::string slot1_fat_dir;
	bool _slot1_fat_dir_type = false;
	int _slot1_no8000prot = 0;
	int disable_sound = 0;
	int disable_limiter = 0;
	int windowed_fullscreen = 0;
	int frameskip = 0;
	int horizontal = 0;

	bool parse(int argc,char **argv);

	//validate the common commandline options
	bool validate();

	//process movie play/record commands
	void process_movieCommands();
	//etc.
	void process_addonCommands();
	bool is_cflash_configured = false;
	
	//print a little help message for cases when erroneous commandlines are entered
	void errorHelp(const char* binName);

	int _spu_sync_mode = -1;
	int _spu_sync_method = -1;
private:
	char* _play_movie_file = 0;
	char* _record_movie_file = 0;
	char* _cflash_image = 0;
	char* _cflash_path = 0;
	char* _gbaslot_rom = 0;
	char* _bios_arm9 = nullptr;
	char* _bios_arm7 = nullptr;
	char* _fw_path = nullptr;
	int _fw_boot = 0;
	int _load_to_memory = -1;
	int _bios_swi = 0;
	int _spu_advanced = 0;
	int _num_cores = -1;
	int _rigorous_timing = 0;
	int _advanced_timing = -1;
	int _gamehacks = -1;
	int _texture_deposterize = -1;
	int _texture_smooth = -1;
#ifdef HAVE_JIT
	int _cpu_mode = -1;
	int _jit_size = -1;
#endif
	char* _slot1 = nullptr;
	char* _slot1_fat_dir = nullptr;
	char* _console_type = nullptr;
	char* _advanscene_import = nullptr;
	int _rtc_day = -1;
	int _rtc_hour = -1;
};

#endif
