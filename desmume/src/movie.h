/*
	Copyright 2008-2023 DeSmuME team

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

#include "utils/datetime.h"
#include "utils/guid.h"
#include "utils/md5.h"


struct UserInput;
class EMUFILE;

typedef struct
{
	int movie_version;					// version of the movie format in the file
	u32 num_frames;
	u32 rerecord_count;
	bool poweron;
	u32 emu_version_used;
	//MD5DATA md5_of_rom_used;
	std::string name_of_rom_used;

	std::vector<std::wstring> comments;
	std::vector<std::string> subtitles;
} MOVIE_INFO;

enum START_FROM
{
	START_BLANK = 0,
	START_SRAM,
	START_SAVESTATE
};

enum EMOVIEMODE
{
	MOVIEMODE_INACTIVE = 0,
	MOVIEMODE_RECORD   = 1,
	MOVIEMODE_PLAY     = 2,
	MOVIEMODE_FINISHED = 3
};

enum EMOVIECMD
{
	MOVIECMD_MIC   = 1,
	MOVIECMD_RESET = 2,
	MOVIECMD_LID   = 4
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
			u8 micsample;
		};

		u32 padding;
	} touch;
	
	//misc commands like reset, etc.
	//small now to save space; we might need to support more commands later.
	//the disk format will support up to 64bit if necessary
	u8 commands;
	bool command_reset() const { return (commands&MOVIECMD_RESET)!=0; }
	bool command_microphone() const { return (commands&MOVIECMD_MIC)!=0; }
	bool command_lid() const { return (commands&MOVIECMD_LID)!=0; }

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
	
	void parse(EMUFILE &fp);
	bool parseBinary(EMUFILE &fp);
	void dump(EMUFILE &fp);
	void dumpBinary(EMUFILE &fp);
	void parsePad(EMUFILE &fp, u16 &outPad);
	void dumpPad(EMUFILE &fp, u16 inPad);
	
	static const char mnemonics[13];

private:
	int mask(int bit) { return 1<<bit; }
};


class MovieData
{
public:
	MovieData(bool fromCurrentSettings = false);

	int version;
	int emuVersion;
	//todo - somehow force mutual exclusion for poweron and reset (with an error in the parser)
	
	u32 romChecksum;
	std::string romSerial;
	std::string romFilename;
	bool savestate;
	std::vector<u8> sram;
	std::vector<MovieRecord> records;
	std::vector<std::wstring> comments;
	std::vector<std::vector<u8> > micSamples;
	
	int rerecordCount;
	Desmume_Guid guid;

	DateTime rtcStart;

	// Optional header field: the wall-clock time the movie was recorded, as a
	// decimal Unix timestamp. Older builds ignore this key when loading, and
	// movies written before it existed simply leave it empty.
	std::string recordDate;

	//was the frame data stored in binary?
	bool binaryFlag;

	int useExtBios;
	int swiFromBios;
	int useExtFirmware;
	int bootFromFirmware;

	std::string firmNickname;
	std::string firmMessage;
	int firmFavColour;
	int firmBirthMonth;
	int firmBirthDay;
	int firmLanguage;

	int advancedTiming;
	int jitBlockSize;

	int getNumRecords() { return (int)records.size(); }

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
	int dump(EMUFILE &fp, bool binary);
	void clearRecordRange(int start, int len);
	void insertEmpty(int at, int frames);
	
	static bool loadSramFrom(std::vector<u8>* buf);
	//void TryDumpIncremental();

private:
	void installVersion(std::string& key, std::string& val) { version = atoi(val.c_str()); }
	void installEmuVersion(std::string& key, std::string& val) { emuVersion = atoi(val.c_str()); }
	void installRerecordCount(std::string& key, std::string& val) { rerecordCount = atoi(val.c_str()); }
	void installRomFilename(std::string& key, std::string& val) { romFilename = val; }
	void installRomSerial(std::string& key, std::string& val) { romSerial = val; }
	void installGuid(std::string& key, std::string& val) { guid = Desmume_Guid::fromString(val); }
	void installRtcStartNew(std::string& key, std::string& val) { DateTime::TryParse(val.c_str(), rtcStart); }
	void installRecordDate(std::string& key, std::string& val) { recordDate = val; }
	void installBinary(std::string& key, std::string& val) { binaryFlag = atoi(val.c_str()) != 0; }
	void installUseExtBios(std::string& key, std::string& val) { useExtBios = atoi(val.c_str()) != 0; }
	void installSwiFromBios(std::string& key, std::string& val) { swiFromBios = atoi(val.c_str()) != 0; }
	void installUseExtFirmware(std::string& key, std::string& val) { useExtFirmware = atoi(val.c_str()) != 0; }
	void installBootFromFirmware(std::string& key, std::string& val) { bootFromFirmware = atoi(val.c_str()) != 0; }
	void installFirmNickname(std::string& key, std::string& val) { firmNickname = val; }
	void installFirmMessage(std::string& key, std::string& val) { firmMessage = val; }
	void installFirmFavColour(std::string& key, std::string& val) { firmFavColour = atoi(val.c_str()); }
	void installFirmBirthMonth(std::string& key, std::string& val) { firmBirthMonth = atoi(val.c_str()); }
	void installFirmBirthDay(std::string& key, std::string& val) { firmBirthDay = atoi(val.c_str()); }
	void installFirmLanguage(std::string& key, std::string& val) { firmLanguage = atoi(val.c_str()); }
	void installAdvancedTiming(std::string& key, std::string& val) { advancedTiming = atoi(val.c_str()) != 0; }
	void installJitBlockSize(std::string& key, std::string& val) { jitBlockSize = atoi(val.c_str()); }
	void installSavestate(std::string& key, std::string& val) { savestate = atoi(val.c_str()) != 0; }

	void installRomChecksum(std::string& key, std::string& val);
	void installRtcStart(std::string& key, std::string& val);
	void installComment(std::string& key, std::string& val);
	void installSram(std::string& key, std::string& val);
	void installMicSample(std::string& key, std::string& val);

	typedef void(MovieData::* ivm)(std::string&,std::string&);
	std::map<std::string, ivm> installValueMap;
};

extern int currFrameCounter;
extern EMOVIEMODE movieMode;		//adelikat: main needs this for frame counter display
extern MovieData currMovieData;		//adelikat: main needs this for frame counter display

extern bool movie_reset_command;

bool FCEUI_MovieGetInfo(EMUFILE &fp, MOVIE_INFO &info, bool skipFrameCount);
// description is written as one ordinary "comment" line; the default keeps the
// other frontends' call sites unchanged. No new DSM header key is involved.
void FCEUI_SaveMovie(const char *fname, std::wstring author, START_FROM startFrom, std::string sramfname, const DateTime &rtcstart, const std::wstring &description = L"");
const char* _CDECL_ FCEUI_LoadMovie(const char *fname, bool _read_only, bool tasedit, int _pauseframe); // returns NULL on success, errmsg on failure
void UnloadMovieEmulationSettings();
bool AreMovieEmulationSettingsActive();
void FCEUI_StopMovie();
void FCEUMOV_AddInputState();
void FCEUMOV_HandlePlayback();
void FCEUMOV_HandleRecording();
void mov_savestate(EMUFILE &fp);
bool mov_loadstate(EMUFILE &fp, int size);
void LoadFM2_binarychunk(MovieData& movieData, EMUFILE &fp, int size);
bool LoadFM2(MovieData &movieData, EMUFILE &fp, int size, bool stopAfterHeader);
extern bool movie_readonly;
extern bool ShowInputDisplay;
void FCEUI_MakeBackupMovie(bool dispMessage);
DateTime FCEUI_MovieGetRTCDefault();
void BinaryDataFromString(std::string &inStringData, std::vector<u8> *outBinaryData);
void ReplayRecToDesmumeInput(const MovieRecord &inRecord, UserInput &outInput);
void DesmumeInputToReplayRec(const UserInput &inInput, MovieRecord &outRecord);

//------------------------------------------------------------------------------
// Timeline editing, for the DSM Editor (frontend/windows/dsmeditor.cpp).
//
// The editor works on the movie that is currently loaded, in memory, and it is
// the only writer: everything below either reads a record, replaces one, or
// moves the whole timeline.  Nothing here seeks, resets or changes movieMode -
// that stays with the frontend, exactly as the VBM editor leaves VBA's player
// alone.
//
// The emulator reads currMovieData.records[currFrameCounter] once per frame, on
// the same thread the frontend runs on.  Replacing one record is therefore safe
// at any time; inserting or deleting moves the frame the playhead points at, so
// the caller is expected to pause first.
//------------------------------------------------------------------------------
enum MOVIE_EDIT_RESULT
{
	MOVIE_EDIT_OK = 0,
	MOVIE_EDIT_NO_MOVIE,	// nothing is loaded
	MOVIE_EDIT_RECORDING,	// the file is open for appending - the timeline is not ours
	MOVIE_EDIT_RANGE,		// a frame number is past the end of the movie
	MOVIE_EDIT_NO_MEMORY,
	MOVIE_EDIT_IO
};

// What the movie actually contains, so the editor can decide which columns are
// worth showing.  Collected in one pass over the records.
struct MovieInputUsage
{
	u16	 padMask;		// pad bits that are set in at least one frame
	u8	 commandMask;	// MOVIECMD_* bits that appear in at least one frame
	bool touch;			// some frame touches the screen
	bool micSample;		// some frame carries a non-zero mic sample index
};

bool				FCEUMOV_IsEditable();
u32					FCEUMOV_GetLength();
u32					FCEUMOV_GetGeneration();	// changes when the timeline is rebuilt
MovieInputUsage		FCEUMOV_GetInputUsage();
bool				FCEUMOV_GetRecord(u32 frame, MovieRecord &record);
MOVIE_EDIT_RESULT	FCEUMOV_SetRecord(u32 frame, const MovieRecord &record);
// records may be NULL, in which case count blank frames are inserted - which is
// what "insert a frame" means in the editor.
MOVIE_EDIT_RESULT	FCEUMOV_InsertRecords(u32 at, const MovieRecord *records, u32 count);
// frames must be ascending and unique; the whole selection is compacted in one
// pass, so deleting ten thousand rows is one rewrite and not ten thousand.
MOVIE_EDIT_RESULT	FCEUMOV_DeleteRecords(const u32 *frames, u32 count);
MOVIE_EDIT_RESULT	FCEUMOV_SaveMovie(const char *fname);

#endif
