/*
	Copyright 2008-2019 DeSmuME team

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

#define WIN32_LEAN_AND_MEAN
#include "movie.h"

#include <assert.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>
#include <string>

#include "utils/guid.h"
#include "utils/xstring.h"
#include "utils/datetime.h"
#include "encodings/utf.h"

#include "MMU.h"
#include "NDSSystem.h"
#include "readwrite.h"
#include "debug.h"
#include "driver.h"
#include "rtc.h"
#include "mic.h"
#include "version.h"
#include "path.h"
#include "emufile.h"
#include "saves.h"

using namespace std;
bool freshMovie = false;	  //True when a movie loads, false when movie is altered.  Used to determine if a movie has been altered since opening
bool autoMovieBackup = true;

#define FCEU_PrintError LOG

//version 1 - was the main version for a long time, most of 201x
//version 2 - march 2019, added mic sample
#define MOVIE_VERSION 2

#ifdef WIN32_FRONTEND
#include "frontend/windows/main.h"
#endif

//----movie engine main state

EMOVIEMODE movieMode = MOVIEMODE_INACTIVE;

//this should not be set unless we are in MOVIEMODE_RECORD!
EMUFILE *osRecordingMovie = NULL;

int currFrameCounter;
u32 cur_input_display = 0;
int pauseframe = -1;
bool movie_readonly = true;

char curMovieFilename[512] = {0};
MovieData currMovieData;
MovieData* oldSettings = NULL;
// Loading a movie calls NDS_Reset, which calls UnloadMovieEmulationSettings. Don't unload settings on that call.
bool firstReset = false;

int currRerecordCount;
bool movie_reset_command = false;
//--------------


void MovieData::clearRecordRange(int start, int len)
{
	for(int i=0;i<len;i++)
		records[i+start].clear();
}

void MovieData::insertEmpty(int at, int frames)
{
	if(at == -1) 
	{
		int currcount = records.size();
		records.resize(records.size()+frames);
		clearRecordRange(currcount,frames);
	}
	else
	{
		records.insert(records.begin()+at,frames,MovieRecord());
		clearRecordRange(at,frames);
	}
}

void MovieRecord::clear()
{ 
	pad = 0;
	commands = 0;
	touch.padding = 0;
}

bool MovieRecord::Compare(MovieRecord& compareRec)
{
	//Check pad
	if (this->pad != compareRec.pad) 
		return false;

	//Check Stylus
	if (this->touch.padding != compareRec.touch.padding) return false;

	//Check comamnds
	//if new commands are ever recordable, they need to be added here if we go with this method
	if(this->command_reset() != compareRec.command_reset()) return false;
	if(this->command_microphone() != compareRec.command_microphone()) return false;
	if(this->command_lid() != compareRec.command_lid()) return false;
	
	return true;
}

const char MovieRecord::mnemonics[13] = {'R','L','D','U','T','S','B','A','Y','X','W','E','G'};
void MovieRecord::dumpPad(EMUFILE &fp, u16 inPad)
{
	//these are mnemonics for each joystick bit.
	//since we usually use the regular joypad, these will be more helpful.
	//but any character other than ' ' or '.' should count as a set bit
	//maybe other input types will need to be encoded another way..
	for (int bit = 0; bit < 13; bit++)
	{
		int bitmask = (1<<(12-bit));
		char mnemonic = mnemonics[bit];
		//if the bit is set write the mnemonic
		if (pad & bitmask)
			fp.fputc(mnemonic);
		else //otherwise write an unset bit
			fp.fputc('.');
	}
}

void MovieRecord::parsePad(EMUFILE &fp, u16 &outPad)
{
	
	char buf[13] = {};
	fp.fread(buf,13);
	pad = 0;
	for (int i = 0; i < 13; i++)
	{
		pad <<= 1;
		pad |= ((buf[i] == '.' || buf[i] == ' ') ? 0 : 1);
	}
}

void MovieRecord::parse(EMUFILE &fp)
{
	//by the time we get in here, the initial pipe has already been extracted

	//extract the commands
	commands = u32DecFromIstream(fp);
	
	fp.fgetc(); //eat the pipe

	parsePad(fp, pad);
	touch.x = u32DecFromIstream(fp);
	touch.y = u32DecFromIstream(fp);
	touch.touch = u32DecFromIstream(fp);
	touch.micsample = u32DecFromIstream(fp);
		
	fp.fgetc(); //eat the pipe

	//should be left at a newline
}

void MovieRecord::dump(EMUFILE &fp)
{
	//dump the misc commands
	//*os << '|' << setw(1) << (int)commands;
	fp.fputc('|');
	putdec<uint8,1,true>(fp,commands);

	fp.fputc('|');
	dumpPad(fp, pad);
	putdec<u8,3,true>(fp,touch.x); fp.fputc(' ');
	putdec<u8,3,true>(fp,touch.y); fp.fputc(' ');
	putdec<u8,1,true>(fp,touch.touch); fp.fputc(' ');
	putdec<u8,3,true>(fp,touch.micsample);
	fp.fputc('|');
	
	//each frame is on a new line
	fp.fputc('\n');
}

DateTime FCEUI_MovieGetRTCDefault()
{
	// compatible with old desmume
	return DateTime(2009,1,1,0,0,0);
}

MovieData::MovieData(bool fromCurrentSettings)
	: version(MOVIE_VERSION)
	, emuVersion(EMU_DESMUME_VERSION_NUMERIC())
	, romChecksum(0)
	, rerecordCount(0)
	, binaryFlag(false)
	, rtcStart(FCEUI_MovieGetRTCDefault())
{
	savestate = false;
	
	useExtBios = -1;
	swiFromBios = -1;
	useExtFirmware = -1;
	bootFromFirmware = -1;
	
	firmNickname = "";
	firmMessage = "";
	firmFavColour = -1;
	firmBirthMonth = -1;
	firmBirthDay = -1;
	firmLanguage = -1;
	
	advancedTiming = -1;
	jitBlockSize = -1;
	
	installValueMap["version"] = &MovieData::installVersion;
	installValueMap["emuVersion"] = &MovieData::installEmuVersion;
	installValueMap["rerecordCount"] = &MovieData::installRerecordCount;
	installValueMap["romFilename"] = &MovieData::installRomFilename;
	installValueMap["romChecksum"] = &MovieData::installRomChecksum;
	installValueMap["romSerial"] = &MovieData::installRomSerial;
	installValueMap["guid"] = &MovieData::installGuid;
	installValueMap["rtcStart"] = &MovieData::installRtcStart;
	installValueMap["rtcStartNew"] = &MovieData::installRtcStartNew;
	
	installValueMap["comment"] = &MovieData::installComment;
	installValueMap["binary"] = &MovieData::installBinary;
	installValueMap["useExtBios"] = &MovieData::installUseExtBios;
	installValueMap["swiFromBios"] = &MovieData::installSwiFromBios;
	installValueMap["useExtFirmware"] = &MovieData::installUseExtFirmware;
	installValueMap["bootFromFirmware"] = &MovieData::installBootFromFirmware;
	
	installValueMap["firmNickname"] = &MovieData::installFirmNickname;
	installValueMap["firmMessage"] = &MovieData::installFirmMessage;
	installValueMap["firmFavColour"] = &MovieData::installFirmFavColour;
	installValueMap["firmBirthMonth"] = &MovieData::installFirmBirthMonth;
	installValueMap["firmBirthDay"] = &MovieData::installFirmBirthDay;
	installValueMap["firmLanguage"] = &MovieData::installFirmLanguage;
	
	installValueMap["advancedTiming"] = &MovieData::installAdvancedTiming;
	installValueMap["jitBlockSize"] = &MovieData::installJitBlockSize;
	installValueMap["savestate"] = &MovieData::installSavestate;
	installValueMap["sram"] = &MovieData::installSram;

	for(int i=0;i<256;i++)
	{
		char tmp[256];
		sprintf(tmp,"micsample%d",i);
		installValueMap[tmp] = &MovieData::installMicSample;
	}

	if (fromCurrentSettings)
	{
		useExtBios = CommonSettings.UseExtBIOS;
		if (useExtBios)
			swiFromBios = CommonSettings.SWIFromBIOS;
		useExtFirmware = CommonSettings.UseExtFirmware;
		if (useExtFirmware)
			bootFromFirmware = CommonSettings.BootFromFirmware;
		if (!CommonSettings.UseExtFirmware)
		{
			firmNickname.resize(CommonSettings.fwConfig.nicknameLength);
			for (int i = 0; i < CommonSettings.fwConfig.nicknameLength; i++)
				firmNickname[i] = CommonSettings.fwConfig.nickname[i];
			firmMessage.resize(CommonSettings.fwConfig.messageLength);
			for (int i = 0; i < CommonSettings.fwConfig.messageLength; i++)
				firmMessage[i] = CommonSettings.fwConfig.message[i];

			firmFavColour = CommonSettings.fwConfig.favoriteColor;
			firmBirthMonth = CommonSettings.fwConfig.birthdayMonth;
			firmBirthDay = CommonSettings.fwConfig.birthdayDay;
			firmLanguage = CommonSettings.fwConfig.language;
		}
		advancedTiming = CommonSettings.advanced_timing;
		jitBlockSize = CommonSettings.use_jit ? CommonSettings.jit_max_block_size : 0;
	}
}

void MovieData::truncateAt(int frame)
{
	if((int)records.size() > frame)
		records.resize(frame);
}

void MovieData::installRomChecksum(std::string& key, std::string& val)
{
	// TODO: The current implementation of reading the checksum doesn't work correctly, and can
	// cause crashes when the MovieData object is deallocated. (This is caused by StringToBytes()
	// overrunning romChecksum into romSerial, making romSerial undefined.) Set romChecksum to
	// some dummy value for now to prevent crashing. This is okay, since romChecksum isn't actually
	// used in practice at this time. - rogerman, 2012/08/24
	//StringToBytes(val,&romChecksum,MD5DATA::size);

	romChecksum = 0;
}
void MovieData::installRtcStart(std::string& key, std::string& val)
{
	// sloppy format check and parse
	const char *validFormatStr = "####-##-##T##:##:##Z";
	bool validFormat = true;
	for (int i = 0; validFormatStr[i] != '\0'; i++) {
		if (validFormatStr[i] != val[i] &&
			!(validFormatStr[i] == '#' && isdigit(val[i]))) {
			validFormat = false;
			break;
		}
	}
	if (validFormat) {
		const char *s = val.data();
		int year = atoi(&s[0]);
		int mon = atoi(&s[5]);
		int day = atoi(&s[8]);
		int hour = atoi(&s[11]);
		int min = atoi(&s[14]);
		int sec = atoi(&s[17]);
		rtcStart = DateTime(year, mon, day, hour, min, sec);
	}
}
void MovieData::installComment(std::string& key, std::string& val) { comments.push_back(mbstowcs(val)); }
void MovieData::installSram(std::string& key, std::string& val) { BinaryDataFromString(val, &this->sram); }

void MovieData::installMicSample(std::string& key, std::string& val)
{
	//which sample?
	int which = atoi(key.c_str()+strlen("micsample"));

	//make sure we have this many
	if(micSamples.size()<which+1)
		micSamples.resize(which+1);

	BinaryDataFromString(val, &micSamples[which]);
}

void MovieData::installValue(std::string& key, std::string& val)
{
	ivm method = installValueMap[key];
	if (method != NULL)
		(this->*method)(key,val);
}


int MovieData::dump(EMUFILE &fp, bool binary)
{
	int start = fp.ftell();
	fp.fprintf("version %d\n", version);
	fp.fprintf("emuVersion %d\n", emuVersion);
	fp.fprintf("rerecordCount %d\n", rerecordCount);

	fp.fprintf("romFilename %s\n", romFilename.c_str());
	fp.fprintf("romChecksum %s\n", u32ToHexString(gameInfo.crc).c_str());
	fp.fprintf("romSerial %s\n", romSerial.c_str());
	fp.fprintf("guid %s\n", guid.toString().c_str());
	fp.fprintf("useExtBios %d\n", CommonSettings.UseExtBIOS?1:0); // TODO: include bios file data, not just a flag saying something was used

	if (CommonSettings.UseExtBIOS)
		fp.fprintf("swiFromBios %d\n", CommonSettings.SWIFromBIOS?1:0);

	fp.fprintf("useExtFirmware %d\n", CommonSettings.UseExtFirmware?1:0); // TODO: include firmware file data, not just a flag saying something was used

	if (CommonSettings.UseExtFirmware)
	{
		fp.fprintf("bootFromFirmware %d\n", CommonSettings.BootFromFirmware?1:0);
	}
	else
	{
		std::wstring wnick((wchar_t*)CommonSettings.fwConfig.nickname, CommonSettings.fwConfig.nicknameLength);
		std::string nick = wcstombs(wnick);
		
		std::wstring wmessage((wchar_t*)CommonSettings.fwConfig.message, CommonSettings.fwConfig.messageLength);
		std::string message = wcstombs(wmessage);
		
		fp.fprintf("firmNickname %s\n", nick.c_str());
		fp.fprintf("firmMessage %s\n", message.c_str());
		fp.fprintf("firmFavColour %d\n", CommonSettings.fwConfig.favoriteColor);
		fp.fprintf("firmBirthMonth %d\n", CommonSettings.fwConfig.birthdayMonth);
		fp.fprintf("firmBirthDay %d\n", CommonSettings.fwConfig.birthdayDay);
		fp.fprintf("firmLanguage %d\n", CommonSettings.fwConfig.language);
	}

	fp.fprintf("advancedTiming %d\n", CommonSettings.advanced_timing?1:0);
	fp.fprintf("jitBlockSize %d\n", CommonSettings.use_jit ? CommonSettings.jit_max_block_size : 0);

	fp.fprintf("rtcStartNew %s\n", rtcStart.ToString().c_str());

	for (u32 i = 0; i < comments.size(); i++)
		fp.fprintf("comment %s\n", wcstombs(comments[i]).c_str());
	
	if (binary)
		fp.fprintf("binary 1\n");

	fp.fprintf("savestate %d\n", savestate?1:0);
	if (sram.size() != 0)
		fp.fprintf("sram %s\n", BytesToString(&sram[0],sram.size()).c_str());

	for(int i=0;i<256;i++)
	{
		//TODO - how do these get put in here
		if(micSamples.size() > i)
		{
			char tmp[32];
			sprintf(tmp,"micsample%d",i);
			fp.fprintf("%s %s\n",tmp, BytesToString(&micSamples[i][0],micSamples[i].size()).c_str());
		}
	}

	if (binary)
	{
		//put one | to start the binary dump
		fp.fputc('|');
		for (int i = 0; i < (int)records.size(); i++)
			records[i].dumpBinary(fp);
	}
	else
		for (int i = 0; i < (int)records.size(); i++)
			records[i].dump(fp);

	int end = fp.ftell();
	return end-start;
}

std::string readUntilWhitespace(EMUFILE &fp)
{
	std::string ret = "";
	while (true)
	{
		int c = fp.fgetc();
		switch (c)
		{
		case -1:
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			return ret;
		default:
			ret += c;
			break;
		}
	}
}
std::string readUntilNewline(EMUFILE &fp)
{
	std::string ret = "";
	while (true)
	{
		int c = fp.fgetc();
		switch (c)
		{
		case -1:
		case '\r':
		case '\n':
			return ret;
		default:
			ret += c;
			break;
		}
	}
}
void readUntilNotWhitespace(EMUFILE &fp)
{
	while (true)
	{
		int c = fp.fgetc();
		switch (c)
		{
		case -1:
			return;
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			break;
		default:
			fp.unget();
			return;
		}
	}
}
//yuck... another custom text parser.
bool LoadFM2(MovieData &movieData, EMUFILE &fp, int size, bool stopAfterHeader)
{
	int endOfMovie;
	if (size == INT_MAX)
		endOfMovie = fp.size();
	else
		endOfMovie = fp.ftell() + size;

	//movie must start with "version 1"
	//TODO - start with something different. like 'desmume movie version 1"
	char buf[9];
	fp.fread(buf, 9);
	fp.fseek(-9, SEEK_CUR);
//	if(fp->fail()) return false;
	bool version1 = memcmp(buf, "version 1", 9)==0;
	bool version2 = memcmp(buf, "version 2", 9)==0;

	if(version1) {}
	else if(version2) {}
	else return false;

	while (fp.ftell() < endOfMovie)
	{
		readUntilNotWhitespace(fp);
		int c = fp.fgetc();
		// This will be the case if there is a newline at the end of the file.
		if (c == -1) break;
		else if (c == '|')
		{
			if (stopAfterHeader) break;
			else if (movieData.binaryFlag)
			{
				LoadFM2_binarychunk(movieData, fp, endOfMovie - fp.ftell());
				break;
			}
			else
			{
				int currcount = movieData.records.size();
				movieData.records.resize(currcount + 1);
				movieData.records[currcount].parse(fp);
			}
		}
		else // key value
		{
			fp.unget();
			std::string key = readUntilWhitespace(fp);
			readUntilNotWhitespace(fp);
			std::string value = readUntilNewline(fp);
			movieData.installValue(key, value);
		}
	}

	// just in case readUntilNotWhitespace read past the limit set by size parameter
	fp.fseek(endOfMovie, SEEK_SET);

	return true;
}


static void closeRecordingMovie()
{
	if(osRecordingMovie)
	{
		delete osRecordingMovie;
		osRecordingMovie = 0;
	}
}

// Stop movie playback.
static void StopPlayback()
{
	driver->USR_InfoMessage("Movie playback stopped.");
	movieMode = MOVIEMODE_INACTIVE;
}

// Stop movie playback without closing the movie.
static void FinishPlayback()
{
	driver->USR_InfoMessage("Movie finished playing.");
	movieMode = MOVIEMODE_FINISHED;
}


// Stop movie recording
static void StopRecording()
{
	driver->USR_InfoMessage("Movie recording stopped.");
	movieMode = MOVIEMODE_INACTIVE;
	
	closeRecordingMovie();
}



void FCEUI_StopMovie()
{
	if(movieMode == MOVIEMODE_PLAY || movieMode == MOVIEMODE_FINISHED)
		StopPlayback();
	else if(movieMode == MOVIEMODE_RECORD)
		StopRecording();

	curMovieFilename[0] = 0;
	freshMovie = false;
}

static void LoadSettingsFromMovie(MovieData movieData)
{
	if (movieData.useExtBios != -1)
		CommonSettings.UseExtBIOS = movieData.useExtBios;
	if (movieData.swiFromBios != -1)
		CommonSettings.SWIFromBIOS = movieData.swiFromBios;
	if (movieData.useExtFirmware != -1)
		CommonSettings.UseExtFirmware = movieData.useExtFirmware;
	if (movieData.bootFromFirmware != -1)
		CommonSettings.BootFromFirmware = movieData.bootFromFirmware;
	if (!CommonSettings.UseExtFirmware)
	{
		if (movieData.firmNickname != "")
		{
			CommonSettings.fwConfig.nicknameLength = movieData.firmNickname.length() > MAX_FW_NICKNAME_LENGTH ? MAX_FW_NICKNAME_LENGTH : movieData.firmNickname.length();
			for (int i = 0; i < CommonSettings.fwConfig.nicknameLength; i++)
				CommonSettings.fwConfig.nickname[i] = movieData.firmNickname[i];
		}
		if (movieData.firmMessage != "")
		{
			CommonSettings.fwConfig.messageLength = movieData.firmMessage.length() > MAX_FW_MESSAGE_LENGTH ? MAX_FW_MESSAGE_LENGTH : movieData.firmMessage.length();
			for (int i = 0; i < CommonSettings.fwConfig.messageLength; i++)
				CommonSettings.fwConfig.message[i] = movieData.firmMessage[i];
		}

		if (movieData.firmFavColour != -1)
			CommonSettings.fwConfig.favoriteColor = movieData.firmFavColour;
		if (movieData.firmBirthMonth != -1)
			CommonSettings.fwConfig.birthdayMonth = movieData.firmBirthMonth;
		if (movieData.firmBirthDay != -1)
			CommonSettings.fwConfig.birthdayDay = movieData.firmBirthDay;
		if (movieData.firmLanguage != -1)
			CommonSettings.fwConfig.language = movieData.firmLanguage;

		// reset firmware (some games can write to it)
		NDS_InitDefaultFirmware(&MMU.fw.data);
		NDS_ApplyFirmwareSettingsWithConfig(&MMU.fw.data, CommonSettings.fwConfig);
	}
	if (movieData.advancedTiming != -1)
		CommonSettings.advanced_timing = movieData.advancedTiming;
	if (movieData.jitBlockSize > 0 && movieData.jitBlockSize <= 100)
	{
		CommonSettings.use_jit = true;
		CommonSettings.jit_max_block_size = movieData.jitBlockSize;
	}
	else
		CommonSettings.use_jit = false;
}
void UnloadMovieEmulationSettings()
{
	if (oldSettings && !firstReset)
	{
		LoadSettingsFromMovie(*oldSettings);
		delete oldSettings;
		oldSettings = NULL;
	}
}
bool AreMovieEmulationSettingsActive()
{
	return (bool)oldSettings;
}
//begin playing an existing movie
const char* _CDECL_ FCEUI_LoadMovie(const char *fname, bool _read_only, bool tasedit, int _pauseframe)
{
	//if(!tasedit && !FCEU_IsValidUI(FCEUI_PLAYMOVIE))
	//	return;

	// FIXME: name==null indicates to BROWSE to retrieve fname from user (or stdin if that's impossible),
	// when called from movie_play
	assert(fname);
	if(!fname)
		return "LoadMovie doesn't support browsing yet";

	//mbg 6/10/08 - we used to call StopMovie here, but that cleared curMovieFilename and gave us crashes...
	if(movieMode == MOVIEMODE_PLAY || movieMode == MOVIEMODE_FINISHED)
		StopPlayback();
	else if(movieMode == MOVIEMODE_RECORD)
		StopRecording();
	//--------------

	currMovieData = MovieData();
	
	strcpy(curMovieFilename, fname);
	
	bool loadedfm2 = false;
	EMUFILE *fp = new EMUFILE_FILE(fname, "rb");
	loadedfm2 = LoadFM2(currMovieData, *fp, INT_MAX, false);
	delete fp;

	if(!loadedfm2)
		return "failed to load movie";

	//TODO
	//fully reload the game to reinitialize everything before playing any movie
	//poweron(true);

	// set emulation/firmware settings
	oldSettings = new MovieData(true);
	LoadSettingsFromMovie(currMovieData);

	if (currMovieData.savestate)
	{
		// SS file name should be the same as the movie file name, except for extension
		std::string ssName = fname;
		ssName.erase(ssName.length() - 3, 3);
		ssName.append("dst");
		if (!savestate_load(ssName.c_str()))
			return "Could not load movie's savestate. There should be a .dst file with the same name as the movie, in the same folder.";
	}
	else
	{
		firstReset = true;
		NDS_Reset();
		firstReset = false;
	}

	lagframecounter=0;
	LagFrameFlag=0;
	lastLag=0;
	TotalLagFrames=0;

	currFrameCounter = 0;
	pauseframe = _pauseframe;
	movie_readonly = _read_only;
	movieMode = MOVIEMODE_PLAY;
	currRerecordCount = currMovieData.rerecordCount;
	MMU_new.backupDevice.movie_mode();
	if(currMovieData.sram.size() != 0)
	{
		bool success = MovieData::loadSramFrom(&currMovieData.sram);
		if(!success) return "failed to load sram";
	}
	else
		MMU_new.backupDevice.load_movie_blank();

	#ifdef WIN32_FRONTEND
	::micSamples = currMovieData.micSamples;
	#endif

	freshMovie = true;
	ClearAutoHold();

	if(movie_readonly)
		driver->USR_InfoMessage("Replay started Read-Only.");
	else
		driver->USR_InfoMessage("Replay started Read+Write.");

	return NULL; // success
}

static void openRecordingMovie(const char* fname)
{
	//osRecordingMovie = FCEUD_UTF8_fstream(fname, "wb");
	osRecordingMovie = new EMUFILE_FILE(fname, "wb");
	/*if(!osRecordingMovie)
		FCEU_PrintError("Error opening movie output file: %s",fname);*/
	strcpy(curMovieFilename, fname);
}

bool MovieData::loadSramFrom(std::vector<u8>* buf)
{
	EMUFILE_MEMORY ms(buf);
	MMU_new.backupDevice.load_movie(&ms);
	return true;
}

//static bool FCEUSS_SaveSRAM(EMUFILE* outstream, const std::string& fname)
//{
//	//a temp memory stream. we'll dump some data here and then compress
//	//TODO - support dumping directly without compressing to save a buffer copy
////TODO
///*	memorystream ms;
//
//	//size it
//	FILE * fp = fopen( fname.c_str(), "r" );
//	if(!fp)
//		return 0;
//	
//	fseek( fp, 0, SEEK_END );
//	int size = ftell(fp);
//	fclose(fp);
//
//	filebuf fb;
//	fb.open (fname.c_str(), ios::in | ios::binary);//ios::in
//	istream is(&fb);
//
//	char *buffer = new char[size];
//
//	is.read(buffer, size);
//
//	outstream->write((char*)buffer,size);
//
//	fb.close();
//*/
//
//
//	return true;
//}

//begin recording a new movie
//TODO - BUG - the record-from-another-savestate doesnt work.
void FCEUI_SaveMovie(const char *fname, std::wstring author, START_FROM startFrom, std::string sramfname, const DateTime &rtcstart)
{
	//if(!FCEU_IsValidUI(FCEUI_RECORDMOVIE))
	//	return;

	assert(fname);

	FCEUI_StopMovie();

	openRecordingMovie(fname);

	currFrameCounter = 0;
	//LagCounterReset();

	currMovieData = MovieData();
	currMovieData.guid.newGuid();

	if(author != L"") currMovieData.comments.push_back(L"author " + author);
	currMovieData.romChecksum = gameInfo.crc;
	currMovieData.romSerial = gameInfo.ROMserial;
	currMovieData.romFilename = path.GetRomName();
	currMovieData.rtcStart = rtcstart;
	currMovieData.micSamples = ::micSamples;

	// reset firmware (some games can write to it)
	if (!CommonSettings.UseExtFirmware)
	{
		NDS_InitDefaultFirmware(&MMU.fw.data);
		NDS_ApplyFirmwareSettingsWithConfig(&MMU.fw.data, CommonSettings.fwConfig);
	}

	if (startFrom == START_SAVESTATE)
	{
		// SS file name should be the same as the movie file name, except for extension
		std::string ssName = fname;
		ssName.erase(ssName.length() - 3, 3);
		ssName.append("dst");
		savestate_save(ssName.c_str());
		currMovieData.savestate = true;
	}
	else
	{
		NDS_Reset();

		//todo ?
		//poweron(true);

		if (startFrom == START_SRAM)
			EMUFILE::readAllBytes(&currMovieData.sram, sramfname);
	}

	//we are going to go ahead and dump the header. from now on we will only be appending frames
	currMovieData.dump(*osRecordingMovie, false);

	currFrameCounter=0;
	lagframecounter=0;
	LagFrameFlag=0;
	lastLag=0;
	TotalLagFrames=0;

	movieMode = MOVIEMODE_RECORD;
	movie_readonly = false;
	currRerecordCount = 0;
	MMU_new.backupDevice.movie_mode();

	if(currMovieData.sram.size() != 0)
	{
		bool success = MovieData::loadSramFrom(&currMovieData.sram);
		if(!success) return;
	}

	driver->USR_InfoMessage("Movie recording started.");
}


 //the main interaction point between the emulator and the movie system.
 //either dumps the current joystick state or loads one state from the movie.
 //deprecated, should use the two functions it has been split into directly
 void FCEUMOV_AddInputState()
 {
	 FCEUMOV_HandlePlayback();
	 FCEUMOV_HandleRecording();
 }

 void FCEUMOV_HandlePlayback()
 {
	 if(movieMode == MOVIEMODE_PLAY)
	 {
		 //stop when we run out of frames
		 if(currFrameCounter == (int)currMovieData.records.size())
		 {
			 FinishPlayback();
		 }
		 else
		 {
			 UserInput &input = NDS_getProcessingUserInput();
			 ReplayRecToDesmumeInput(currMovieData.records[currFrameCounter], input);
		 }

		 //if we are on the last frame, then pause the emulator if the player requested it
		 if(currFrameCounter == (int)currMovieData.records.size()-1)
		 {
			 /*if(FCEUD_PauseAfterPlayback())
			 {
			 FCEUI_ToggleEmulationPause();
			 }*/
		 }

		 //pause the movie at a specified frame 
		 //if(FCEUMOV_ShouldPause() && FCEUI_EmulationPaused()==0)
		 //{
		 //	FCEUI_ToggleEmulationPause();
		 //	FCEU_DispMessage("Paused at specified movie frame");
		 //}

		 // it's apparently un-threadsafe to do this here
		 // (causes crazy flickering in other OSD elements, at least)
		 // and it's also pretty annoying,
		 // and the framecounter display already conveys this info as well.
		 // so, I'm disabling this, at least for now.
//		 osd->addFixed(180, 176, "%s", "Playback");

	 }
 }

 void FCEUMOV_HandleRecording()
 {
	 if(movieMode == MOVIEMODE_RECORD)
	 {
		 MovieRecord mr;
		 const UserInput &input = NDS_getFinalUserInput();
		 DesmumeInputToReplayRec(input, mr);

		 assert(mr.touch.touch || (!mr.touch.x && !mr.touch.y));
		 //assert(nds.touchX == input.touch.touchX && nds.touchY == input.touch.touchY);
		 //assert((mr.touch.x << 4) == nds.touchX && (mr.touch.y << 4) == nds.touchY);

		 mr.dump(*osRecordingMovie);
		 currMovieData.records.push_back(mr);

		 // it's apparently un-threadsafe to do this here
		 // (causes crazy flickering in other OSD elements, at least)
		 // and it's also pretty annoying,
		 // and the framecounter display already conveys this info as well.
		 // so, I'm disabling this, at least for now.
//		 osd->addFixed(180, 176, "%s", "Recording");
	 }

	 /*extern uint8 joy[4];
	 memcpy(&cur_input_display,joy,4);*/
 }


//TODO 
static void FCEUMOV_AddCommand(int cmd)
{
	// do nothing if not recording a movie
	if(movieMode != MOVIEMODE_RECORD)
		return;
	
	//printf("%d\n",cmd);

	//MBG TODO BIG TODO TODO TODO
	//DoEncode((cmd>>3)&0x3,cmd&0x7,1);
}

//little endian 4-byte cookies
static const u32 kMOVI = 0x49564F4D;
static const u32 kNOMO = 0x4F4D4F4E;

void mov_savestate(EMUFILE &fp)
{
	//we are supposed to dump the movie data into the savestate
	//if(movieMode == MOVIEMODE_RECORD || movieMode == MOVIEMODE_PLAY)
	//	return currMovieData.dump(os, true);
	//else return 0;
	if(movieMode != MOVIEMODE_INACTIVE)
	{
		fp.write_32LE(kMOVI);
		currMovieData.dump(fp, true);
	}
	else
	{
		fp.write_32LE(kNOMO);
	}
}


bool CheckTimelines(MovieData& stateMovie, MovieData& currMovie, int& errorFr)
{
	bool isInTimeline = true;
	int length;

	//First check, make sure we are checking is for a post-movie savestate, we just want to adjust the length for now, we will handle this situation later in another function
	if (currFrameCounter <= stateMovie.getNumRecords())
		length = currFrameCounter;							//Note: currFrameCounter corresponds to the framecounter in the savestate
	else if (currFrameCounter > currMovie.getNumRecords())  //Now that we know the length of the records of the savestate we plan to load, let's match the length against the movie
		length = currMovie.getNumRecords();				    //If length < currMovie records then this is a "future" event from the current movie, againt we will handle this situation later, we just want to find the right number of frames to compare
	else
		length = stateMovie.getNumRecords();

	for (int x = 0; x < length; x++)
	{
		if (!stateMovie.records[x].Compare(currMovie.records[x]))
		{
			isInTimeline = false;
			errorFr = x;
			break;
		}
	}

	return isInTimeline;
}


static bool load_successful;

bool mov_loadstate(EMUFILE &fp, int size)
{
	load_successful = false;

	u32 cookie;
	if (fp.read_32LE(cookie) != 1) return false;
	if (cookie == kNOMO)
	{
		if(movieMode == MOVIEMODE_RECORD || movieMode == MOVIEMODE_PLAY)
			FinishPlayback();
		return true;
	}
	else if (cookie != kMOVI)
		return false;

	size -= 4;

	if (!movie_readonly && autoMovieBackup && freshMovie) //If auto-backup is on, movie has not been altered this session and the movie is in read+write mode
	{
		FCEUI_MakeBackupMovie(false);	//Backup the movie before the contents get altered, but do not display messages						  
	}

	//a little rule: cant load states in read+write mode with a movie from an archive.
	//so we are going to switch it to readonly mode in that case
//	if(!movie_readonly 
//		//*&& FCEU_isFileInArchive(curMovieFilename)*/
//		) {
//		FCEU_PrintError("Cannot loadstate in Read+Write with movie from archive. Movie is now Read-Only.");
//		movie_readonly = true;
//	}

	MovieData tempMovieData = MovieData();
	//int curr = fp->ftell();
	if(!LoadFM2(tempMovieData, fp, size, false)) {
		
	//	is->seekg((u32)curr+size);
	/*	extern bool FCEU_state_loading_old_format;
		if(FCEU_state_loading_old_format) {
			if(movieMode == MOVIEMODE_PLAY || movieMode == MOVIEMODE_RECORD) {
				FCEUI_StopMovie();			
				FCEU_PrintError("You have tried to use an old savestate while playing a movie. This is unsupported (since the old savestate has old-format movie data in it which can't be converted on the fly)");
			}
		}*/
		return false;
	}

	//----------------
	//complex TAS logic for loadstate
	//fully conforms to the savestate logic documented in the Laws of TAS
	//http://tasvideos.org/LawsOfTAS/OnSavestates.html
	//----------------
		
	/*
	Playback or Recording + Read-only

    * Check that GUID of movie and savestate-movie must match or else error
          o on error: a message informing that the savestate doesn't belong to this movie. This is a GUID mismatch error. Give user a choice to load it anyway.
                + failstate: if use declines, loadstate attempt canceled, movie can resume as if not attempted if user has backup savstates enabled else stop movie
    * Check that movie and savestate-movie are in same timeline. If not then this is a wrong timeline error.
          o on error: a message informing that the savestate doesn't belong to this movie
                + failstate: loadstate attempt canceled, movie can resume as if not attempted if user has backup savestates enabled else stop movie
    * Check that savestate-movie is not greater than movie. If not then this is a future event error and is not allowed in read-only
          o on error: message informing that the savestate is from a frame after the last frame of the movie
                + failstate - loadstate attempt cancelled, movie can resume if user has backup savesattes enabled, else stop movie
    * Check that savestate framcount <= savestate movie length. If not this is a post-movie savestate
          o on post-movie: See post-movie event section. 
    * All error checks have passed, state will be loaded
    * Movie contained in the savestate will be discarded
    * Movie is now in Playback mode 

	Playback, Recording + Read+write

    * Check that GUID of movie and savestate-movie must match or else error
          o on error: a message informing that the savestate doesn't belong to this movie. This is a GUID mismatch error. Give user a choice to load it anyway.
                + failstate: if use declines, loadstate attempt canceled, movie can resume as if not attempted (stop movie if resume fails)canceled, movie can resume if backup savestates enabled else stopmovie
    * Check that savestate framcount <= savestate movie length. If not this is a post-movie savestate
          o on post-movie: See post-movie event section. 
    * savestate passed all error checks and will now be loaded in its entirety and replace movie (note: there will be no truncation yet)
    * current framecount will be set to savestate framecount
    * on the next frame of input, movie will be truncated to framecount
          o (note: savestate-movie can be a future event of the movie timeline, or a completely new timeline and it is still allowed) 
    * Rerecord count of movie will be incremented
    * Movie is now in record mode 

	Post-movie savestate event

    * Whan a savestate is loaded and is determined that the savestate-movie length is less than the savestate framecount then it is a post-movie savestate. These occur when a savestate was made during Movie Finished mode. 
	* If read+write, the entire savestate movie will be loaded and replace current movie.
    * If read-only, the savestate movie will be discarded
    * Mode will be switched to Move Finished
    * Savestate will be loaded
    * Current framecount changes to savestate framecount
    * User will have control of input as if Movie inactive mode 
	*/


	if(movieMode != MOVIEMODE_INACTIVE)
	{
		//handle moviefile mismatch
		if(tempMovieData.guid != currMovieData.guid)
		{
			//mbg 8/18/08 - this code  can be used to turn the error message into an OK/CANCEL
			#if defined(WIN32_FRONTEND)
				std::string msg = "There is a mismatch between savestate's movie and current movie.\ncurrent: " + currMovieData.guid.toString() + "\nsavestate: " + tempMovieData.guid.toString() + "\n\nThis means that you have loaded a savestate belonging to a different movie than the one you are playing now.\n\nContinue loading this savestate anyway?";
				int result = MessageBox(MainWindow->getHWnd(),msg.c_str(),"Error loading savestate",MB_OKCANCEL);
				if(result == IDCANCEL)
					return false;
			#else
				FCEU_PrintError("Mismatch between savestate's movie and current movie.\ncurrent: %s\nsavestate: %s\n",currMovieData.guid.toString().c_str(),tempMovieData.guid.toString().c_str());
				return false;
			#endif
		}

		closeRecordingMovie();

		if(!movie_readonly)
		{
			currMovieData = tempMovieData;
			currMovieData.rerecordCount = currRerecordCount;
		}

		if(currFrameCounter > (int)currMovieData.records.size())
		{
			// if the frame counter is longer than our current movie,
			// switch to "finished" mode.
			// this is a mode that behaves like "inactive"
			// except it permits switching to play/record by loading an earlier savestate.
			// (and we continue to store the finished movie in savestates made while finished)
			driver->SetLineColor(255,0,0); // let's make the text red too to hopefully catch the user's attention a bit.
			FinishPlayback();
			driver->SetLineColor(255,255,255);

			//FCEU_PrintError("Savestate is from a frame (%d) after the final frame in the movie (%d). This is not permitted.", currFrameCounter, currMovieData.records.size()-1);
			//return false;
		}
		else if(movie_readonly)
		{
			//-------------------------------------------------------------
			//this code would reload the movie from disk. allegedly it is helpful to hexers, but
			//it is way too slow with dsm format. so it is only here as a reminder, and in case someone
			//wants to play with it
			//-------------------------------------------------------------
			//{
			//	fstream fs (curMovieFilename);
			//	if(!LoadFM2(tempMovieData, &fs, INT_MAX, false))
			//	{
			//		FCEU_PrintError("Failed to reload DSM after loading savestate");
			//	}
			//	fs.close();
			//	currMovieData = tempMovieData;
			//}
			//-------------------------------------------------------------

			movieMode = MOVIEMODE_PLAY;
		}
		else
		{
		//	#ifdef _S9XLUA_H
		//	if(!FCEU_LuaRerecordCountSkip())
				currRerecordCount++;
		//	#endif
			
			currMovieData.rerecordCount = currRerecordCount;
			currMovieData.truncateAt(currFrameCounter);

			openRecordingMovie(curMovieFilename);
			if(!osRecordingMovie)
			{
			   driver->SetLineColor(255, 0, 0);
			   driver->AddLine("Can't save movie file!");
			}

			//printf("DUMPING MOVIE: %d FRAMES\n",currMovieData.records.size());
			currMovieData.dump(*osRecordingMovie, false);
			movieMode = MOVIEMODE_RECORD;
		}
	}
	
	load_successful = true;
	freshMovie = false;

	return true;
}

static void FCEUMOV_PreLoad(void)
{
	load_successful=0;
}

static bool FCEUMOV_PostLoad(void)
{
	if(movieMode == MOVIEMODE_INACTIVE)
		return true;
	else
		return load_successful;
}


bool FCEUI_MovieGetInfo(EMUFILE &fp, MOVIE_INFO &info, bool skipFrameCount)
{
	//MovieData md;
	//if(!LoadFM2(md, fp, INT_MAX, skipFrameCount))
	//	return false;
	//
	//info.movie_version = md.version;
	//info.poweron = md.savestate.size()==0;
	//info.pal = md.palFlag;
	//info.nosynchack = true;
	//info.num_frames = md.records.size();
	//info.md5_of_rom_used = md.romChecksum;
	//info.emu_version_used = md.emuVersion;
	//info.name_of_rom_used = md.romFilename;
	//info.rerecord_count = md.rerecordCount;
	//info.comments = md.comments;

	return true;
}

bool MovieRecord::parseBinary(EMUFILE &fp)
{
	fp.read_u8(this->commands);
	fp.read_16LE(this->pad);
	fp.read_u8(this->touch.x);
	fp.read_u8(this->touch.y);
	fp.read_u8(this->touch.touch);
	
	return true;
}


void MovieRecord::dumpBinary(EMUFILE &fp)
{
	fp.write_u8(this->commands);
	fp.write_16LE(this->pad);
	fp.write_u8(this->touch.x);
	fp.write_u8(this->touch.y);
	fp.write_u8(this->touch.touch);
}

void LoadFM2_binarychunk(MovieData &movieData, EMUFILE &fp, int size)
{
	int recordsize = 1; //1 for the command

	recordsize = 6;

	assert(size%6==0);

	//find out how much remains in the file
	int curr = fp.ftell();
	fp.fseek(0,SEEK_END);
	int end = fp.ftell();
	int flen = end-curr;
	fp.fseek(curr,SEEK_SET);

	//the amount todo is the min of the limiting size we received and the remaining contents of the file
	int todo = std::min(size, flen);

	int numRecords = todo/recordsize;
	//printf("LOADED MOVIE: %d records; currFrameCounter: %d\n",numRecords,currFrameCounter);
	movieData.records.resize(numRecords);
	for(int i=0;i<numRecords;i++)
	{
		movieData.records[i].parseBinary(fp);
	}
}

#include <sstream>

static bool CheckFileExists(const char* filename)
{
	//This function simply checks to see if the given filename exists
	string checkFilename; 

	if (filename)
		checkFilename = filename;
			
	//Check if this filename exists
	FILE* fp = fopen(checkFilename.c_str(), "rb");
		
	if (!fp)
	{
		return false; 
	}
	else
	{
		fclose(fp);
		return true; 
	}
}

void FCEUI_MakeBackupMovie(bool dispMessage)
{
	//This function generates backup movie files
	string currentFn;					//Current movie fillename
	string backupFn;					//Target backup filename
	string tempFn;						//temp used in back filename creation
	stringstream stream;
	int x;								//Temp variable for string manip
	bool exist = false;					//Used to test if filename exists
	bool overflow = false;				//Used for special situation when backup numbering exceeds limit

	currentFn = curMovieFilename;		//Get current moviefilename
	backupFn = curMovieFilename;		//Make backup filename the same as current moviefilename
	x = backupFn.find_last_of(".");		 //Find file extension
	backupFn = backupFn.substr(0,x);	//Remove extension
	tempFn = backupFn;					//Store the filename at this point
	for (unsigned int backNum=0;backNum<999;backNum++) //999 = arbituary limit to backup files
	{
		stream.str("");					 //Clear stream
		if (backNum > 99)
			stream << "-" << backNum;	 //assign backNum to stream
		 else if (backNum <= 99 && backNum >= 10)
			stream << "-0" << backNum;      //Make it 010, etc if two digits
		else
			stream << "-00" << backNum;	 //Make it 001, etc if single digit
		backupFn.append(stream.str());	 //add number to bak filename
		backupFn.append(".bak");		 //add extension

		exist = CheckFileExists(backupFn.c_str());	//Check if file exists
		
		if (!exist) 
			break;						//Yeah yeah, I should use a do loop or something
		else
		{
			backupFn = tempFn;			//Before we loop again, reset the filename
			
			if (backNum == 999)			//If 999 exists, we have overflowed, let's handle that
			{
				backupFn.append("-001.bak"); //We are going to simply overwrite 001.bak
				overflow = true;		//Flag that we have exceeded limit
				break;					//Just in case
			}
		}
	}

	MovieData md = currMovieData;								//Get current movie data
	EMUFILE_FILE outf = EMUFILE_FILE(backupFn.c_str(),"wb"); //FCEUD_UTF8_fstream(backupFn, "wb");	//open/create file
	md.dump(outf,false);										//dump movie data
	
	//TODO, decide if fstream successfully opened the file and print error message if it doesn't

	if (dispMessage)	//If we should inform the user 
	{
//		if (overflow)
//			FCEUI_DispMessage("Backup overflow, overwriting %s",backupFn.c_str()); //Inform user of overflow
//		else
//			FCEUI_DispMessage("%s created",backupFn.c_str()); //Inform user of backup filename
	}
}

void BinaryDataFromString(std::string &inStringData, std::vector<u8> *outBinaryData)
{
	int len = Base64StringToBytesLength(inStringData);
	if(len == -1) len = HexStringToBytesLength(inStringData); // wasn't base64, try hex
	if(len >= 1)
	{
		outBinaryData->resize(len);
		StringToBytes(inStringData, &outBinaryData->front(), len); // decodes either base64 or hex
	}
}

void ReplayRecToDesmumeInput(const MovieRecord &inRecord, UserInput &outInput)
{
	if (inRecord.command_reset())
	{
		NDS_Reset();
		return;
	}
	else
	{
		movie_reset_command = false;
	}
	
	outInput.buttons.R = ((inRecord.pad & (1 << 12)) != 0);
	outInput.buttons.L = ((inRecord.pad & (1 << 11)) != 0);
	outInput.buttons.D = ((inRecord.pad & (1 << 10)) != 0);
	outInput.buttons.U = ((inRecord.pad & (1 <<  9)) != 0);
	outInput.buttons.T = ((inRecord.pad & (1 <<  8)) != 0);
	outInput.buttons.S = ((inRecord.pad & (1 <<  7)) != 0);
	outInput.buttons.B = ((inRecord.pad & (1 <<  6)) != 0);
	outInput.buttons.A = ((inRecord.pad & (1 <<  5)) != 0);
	outInput.buttons.Y = ((inRecord.pad & (1 <<  4)) != 0);
	outInput.buttons.X = ((inRecord.pad & (1 <<  3)) != 0);
	outInput.buttons.W = ((inRecord.pad & (1 <<  2)) != 0);
	outInput.buttons.E = ((inRecord.pad & (1 <<  1)) != 0);
	outInput.buttons.G = ((inRecord.pad & (1 <<  0)) != 0);
	outInput.buttons.F = inRecord.command_lid();
	
	outInput.touch.isTouch = (inRecord.touch.touch != 0);
	outInput.touch.touchX = inRecord.touch.x << 4;
	outInput.touch.touchY = inRecord.touch.y << 4;
	
	outInput.mic.micButtonPressed = (inRecord.command_microphone()) ? 1 : 0;
	outInput.mic.micSample = MicSampleSelection;
}

void DesmumeInputToReplayRec(const UserInput &inInput, MovieRecord &outRecord)
{
	outRecord.commands = 0;
	
	//put into the format we want for the movie system
	//fRLDUTSBAYXWEg
	outRecord.pad = ((inInput.buttons.R) ? (1 << 12) : 0) |
	                ((inInput.buttons.L) ? (1 << 11) : 0) |
	                ((inInput.buttons.D) ? (1 << 10) : 0) |
	                ((inInput.buttons.U) ? (1 <<  9) : 0) |
	                ((inInput.buttons.T) ? (1 <<  8) : 0) |
	                ((inInput.buttons.S) ? (1 <<  7) : 0) |
	                ((inInput.buttons.B) ? (1 <<  6) : 0) |
	                ((inInput.buttons.A) ? (1 <<  5) : 0) |
	                ((inInput.buttons.Y) ? (1 <<  4) : 0) |
	                ((inInput.buttons.X) ? (1 <<  3) : 0) |
	                ((inInput.buttons.W) ? (1 <<  2) : 0) |
	                ((inInput.buttons.E) ? (1 <<  1) : 0);
	
	if (inInput.buttons.F)
		outRecord.commands = MOVIECMD_LID;
	
	if (movie_reset_command)
	{
		outRecord.commands = MOVIECMD_RESET;
		movie_reset_command = false;
	}
	
	outRecord.touch.touch = (inInput.touch.isTouch) ? 1 : 0;
	outRecord.touch.x = (inInput.touch.isTouch) ? inInput.touch.touchX >> 4 : 0;
	outRecord.touch.y = (inInput.touch.isTouch) ? inInput.touch.touchY >> 4 : 0;
	outRecord.touch.micsample = MicSampleSelection;
	
	if (inInput.mic.micButtonPressed != 0)
		outRecord.commands = MOVIECMD_MIC;
}
