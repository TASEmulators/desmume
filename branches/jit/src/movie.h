/*
	Copyright 2008-2010 DeSmuME team

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

#ifndef __MOVIE_H_
#define __MOVIE_H_

#include <vector>
#include <map>
#include <string>
#include <stdlib.h>
#include <time.h>
#include "emufile.h"

#include "utils/datetime.h"
#include "utils/guid.h"
#include "utils/md5.h"

typedef struct
{
	int movie_version;					// version of the movie format in the file
	u32 num_frames;
	u32 rerecord_count;
	bool poweron;
	u32 emu_version_used;
	MD5DATA md5_of_rom_used;
	std::string name_of_rom_used;

	std::vector<std::wstring> comments;
	std::vector<std::string> subtitles;
} MOVIE_INFO;

enum EMOVIEMODE
{
	MOVIEMODE_INACTIVE = 0,
	MOVIEMODE_RECORD = 1,
	MOVIEMODE_PLAY = 2,
	MOVIEMODE_FINISHED = 3,
};

enum EMOVIECMD
{
	MOVIECMD_MIC = 1,
	MOVIECMD_RESET = 2,
	MOVIECMD_LID = 4,
};

//RLDUTSBAYXWEG

class MovieData;
class MovieRecord
{

public:
	u16 pad;
	
	union {
		struct {
			u8 x, y;
			u8 touch;
		};

		u32 padding;
	} touch;
	
	//misc commands like reset, etc.
	//small now to save space; we might need to support more commands later.
	//the disk format will support up to 64bit if necessary
	uint8 commands;
	bool command_reset() { return (commands&MOVIECMD_RESET)!=0; }
	bool command_microphone() { return (commands&MOVIECMD_MIC)!=0; }
	bool command_lid() { return (commands&MOVIECMD_LID)!=0; }

	void toggleBit(int bit)
	{
		pad ^= mask(bit);
	}

	void setBit(int bit)
	{
		pad |= mask(bit);
	}

	void clearBit(int bit)
	{
		pad &= ~mask(bit);
	}

	void setBitValue(int bit, bool val)
	{
		if(val) setBit(bit);
		else clearBit(bit);
	}

	bool checkBit(int bit)
	{
		return (pad & mask(bit))!=0;
	}

	bool Compare(MovieRecord& compareRec);
	void clear();
	
	void parse(MovieData* md, EMUFILE* fp);
	bool parseBinary(MovieData* md, EMUFILE* fp);
	void dump(MovieData* md, EMUFILE* fp, int index);
	void dumpBinary(MovieData* md, EMUFILE* fp, int index);
	void parsePad(EMUFILE* fp, u16& pad);
	void dumpPad(EMUFILE* fp, u16 pad);
	
	static const char mnemonics[13];

private:
	int mask(int bit) { return 1<<bit; }
};


class MovieData
{
public:
	MovieData();
	

	int version;
	int emuVersion;
	//todo - somehow force mutual exclusion for poweron and reset (with an error in the parser)
	
	u32 romChecksum;
	std::string romSerial;
	std::string romFilename;
	std::vector<u8> savestate;
	std::vector<u8> sram;
	std::vector<MovieRecord> records;
	std::vector<std::wstring> comments;
	
	int rerecordCount;
	Desmume_Guid guid;

	DateTime rtcStart;

	//was the frame data stored in binary?
	bool binaryFlag;

	int getNumRecords() { return records.size(); }

	class TDictionary : public std::map<std::string,std::string>
	{
	public:
		bool containsKey(std::string key)
		{
			return find(key) != end();
		}

		void tryInstallBool(std::string key, bool& val)
		{
			if(containsKey(key))
				val = atoi(operator [](key).c_str())!=0;
		}

		void tryInstallString(std::string key, std::string& val)
		{
			if(containsKey(key))
				val = operator [](key);
		}

		void tryInstallInt(std::string key, int& val)
		{
			if(containsKey(key))
				val = atoi(operator [](key).c_str());
		}

	};

	void truncateAt(int frame);
	void installValue(std::string& key, std::string& val);
	int dump(EMUFILE* fp, bool binary);
	void clearRecordRange(int start, int len);
	void insertEmpty(int at, int frames);
	
	static bool loadSavestateFrom(std::vector<u8>* buf);
	static void dumpSavestateTo(std::vector<u8>* buf, int compressionLevel);

	static bool loadSramFrom(std::vector<u8>* buf);
	//void TryDumpIncremental();

private:
	void installInt(std::string& val, int& var)
	{
		var = atoi(val.c_str());
	}

	void installBool(std::string& val, bool& var)
	{
		var = atoi(val.c_str())!=0;
	}
};

extern int currFrameCounter;
extern EMOVIEMODE movieMode;		//adelikat: main needs this for frame counter display
extern MovieData currMovieData;		//adelikat: main needs this for frame counter display

extern bool movie_reset_command;

bool FCEUI_MovieGetInfo(EMUFILE* fp, MOVIE_INFO& info, bool skipFrameCount);
void FCEUI_SaveMovie(const char *fname, std::wstring author, int flag, std::string sramfname, const DateTime &rtcstart);
const char* _CDECL_ FCEUI_LoadMovie(const char *fname, bool _read_only, bool tasedit, int _pauseframe); // returns NULL on success, errmsg on failure
void FCEUI_StopMovie();
void FCEUMOV_AddInputState();
void FCEUMOV_HandlePlayback();
void FCEUMOV_HandleRecording();
void mov_savestate(EMUFILE* fp);
bool mov_loadstate(EMUFILE* fp, int size);
void LoadFM2_binarychunk(MovieData& movieData, EMUFILE* fp, int size);
bool LoadFM2(MovieData& movieData, EMUFILE* fp, int size, bool stopAfterHeader);
extern bool movie_readonly;
extern bool ShowInputDisplay;
void FCEUI_MakeBackupMovie(bool dispMessage);
DateTime FCEUI_MovieGetRTCDefault();
#endif
