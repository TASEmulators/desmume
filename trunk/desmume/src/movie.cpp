/*
	Copyright 2008-2011 DeSmuME team

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
#include <assert.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>
#include "utils/guid.h"
#include "utils/xstring.h"
#include "utils/datetime.h"
#include "movie.h"
#include "NDSSystem.h"
#include "readwrite.h"
#include "debug.h"
#include "rtc.h"
#include "common.h"
#include "mic.h"
#include "version.h"
#include "GPU_osd.h"
#include "path.h"
#include "emufile.h"

using namespace std;
bool freshMovie = false;	  //True when a movie loads, false when movie is altered.  Used to determine if a movie has been altered since opening
bool autoMovieBackup = true;

#define FCEU_PrintError LOG

#define MOVIE_VERSION 1

#ifdef WIN32
#include ".\windows\main.h"
#endif

//----movie engine main state

EMOVIEMODE movieMode = MOVIEMODE_INACTIVE;

//this should not be set unless we are in MOVIEMODE_RECORD!
EMUFILE* osRecordingMovie = 0;

int currFrameCounter;
uint32 cur_input_display = 0;
int pauseframe = -1;
bool movie_readonly = true;

char curMovieFilename[512] = {0};
MovieData currMovieData;
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
	if (this->touch.touch != compareRec.touch.touch) return false;
	if (this->touch.x != compareRec.touch.x) return false;
	if (this->touch.y != compareRec.touch.y) return false;

	//Check comamnds
	//if new commands are ever recordable, they need to be added here if we go with this method
	if(this->command_reset() != compareRec.command_reset()) return false;
	if(this->command_microphone() != compareRec.command_microphone()) return false;
	if(this->command_lid() != compareRec.command_lid()) return false;
	
	return true;
}

const char MovieRecord::mnemonics[13] = {'R','L','D','U','T','S','B','A','Y','X','W','E','G'};
void MovieRecord::dumpPad(EMUFILE* fp, u16 pad)
{
	//these are mnemonics for each joystick bit.
	//since we usually use the regular joypad, these will be more helpful.
	//but any character other than ' ' or '.' should count as a set bit
	//maybe other input types will need to be encoded another way..
	for(int bit=0;bit<13;bit++)
	{
		int bitmask = (1<<(12-bit));
		char mnemonic = mnemonics[bit];
		//if the bit is set write the mnemonic
		if(pad & bitmask)
			fp->fputc(mnemonic);
		else //otherwise write an unset bit
			fp->fputc('.');
	}
}


void MovieRecord::parsePad(EMUFILE* fp, u16& pad)
{
	
	char buf[13];
	fp->fread(buf,13);
	pad = 0;
	for(int i=0;i<13;i++)
	{
		pad <<= 1;
		pad |= ((buf[i]=='.'||buf[i]==' ')?0:1);
	}
}


void MovieRecord::parse(MovieData* md, EMUFILE* fp)
{
	//by the time we get in here, the initial pipe has already been extracted

	//extract the commands
	commands = u32DecFromIstream(fp);
	
	fp->fgetc(); //eat the pipe

	parsePad(fp, pad);
	touch.x = u32DecFromIstream(fp);
	touch.y = u32DecFromIstream(fp);
	touch.touch = u32DecFromIstream(fp);
		
	fp->fgetc(); //eat the pipe

	//should be left at a newline
}


void MovieRecord::dump(MovieData* md, EMUFILE* fp, int index)
{
	//dump the misc commands
	//*os << '|' << setw(1) << (int)commands;
	fp->fputc('|');
	putdec<uint8,1,true>(fp,commands);

	fp->fputc('|');
	dumpPad(fp, pad);
	putdec<u8,3,true>(fp,touch.x); fp->fputc(' ');
	putdec<u8,3,true>(fp,touch.y); fp->fputc(' ');
	putdec<u8,1,true>(fp,touch.touch);
	fp->fputc('|');
	
	//each frame is on a new line
	fp->fputc('\n');
}

DateTime FCEUI_MovieGetRTCDefault()
{
	// compatible with old desmume
	return DateTime(2009,1,1,0,0,0);
}

MovieData::MovieData()
	: version(MOVIE_VERSION)
	, emuVersion(EMU_DESMUME_VERSION_NUMERIC())
	, romChecksum(0)
	, rerecordCount(0)
	, binaryFlag(false)
	, rtcStart(FCEUI_MovieGetRTCDefault())
{
}

void MovieData::truncateAt(int frame)
{
	if((int)records.size() > frame)
		records.resize(frame);
}


void MovieData::installValue(std::string& key, std::string& val)
{
	//todo - use another config system, or drive this from a little data structure. because this is gross
	if(key == "version")
		installInt(val,version);
	else if(key == "emuVersion")
		installInt(val,emuVersion);
	else if(key == "rerecordCount")
		installInt(val,rerecordCount);
	else if(key == "romFilename")
		romFilename = val;
	else if(key == "romChecksum") {
		// TODO: The current implementation of reading the checksum doesn't work correctly, and can
		// cause crashes when the MovieData object is deallocated. (This is caused by StringToBytes()
		// overrunning romChecksum into romSerial, making romSerial undefined.) Set romChecksum to
		// some dummy value for now to prevent crashing. This is okay, since romChecksum isn't actually
		// used in practice at this time. - rogerman, 2012/08/24
		//StringToBytes(val,&romChecksum,MD5DATA::size);
		
		romChecksum = 0;
	}
	else if(key == "romSerial")
		romSerial = val;
	else if(key == "guid")
		guid = Desmume_Guid::fromString(val);
	else if(key == "rtcStart") {
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
			struct tm t;
			const char *s = val.data();
			int year = atoi(&s[0]);
			int mon = atoi(&s[5]);
			int day = atoi(&s[8]);
			int hour = atoi(&s[11]);
			int min = atoi(&s[14]);
			int sec = atoi(&s[17]);
			rtcStart = DateTime(year,mon,day,hour,min,sec);
		}
	}
	else if(key == "rtcStartNew") {
		DateTime::TryParse(val.c_str(),rtcStart);
	}
	else if(key == "comment")
		comments.push_back(mbstowcs(val));
	else if(key == "binary")
		installBool(val,binaryFlag);
	else if(key == "savestate")
	{
		int len = Base64StringToBytesLength(val);
		if(len == -1) len = HexStringToBytesLength(val); // wasn't base64, try hex
		if(len >= 1)
		{
			savestate.resize(len);
			StringToBytes(val,&savestate[0],len); // decodes either base64 or hex
		}
	}
	else if(key == "sram")
	{
		int len = Base64StringToBytesLength(val);
		if(len == -1) len = HexStringToBytesLength(val); // wasn't base64, try hex
		if(len >= 1)
		{
			sram.resize(len);
			StringToBytes(val,&sram[0],len); // decodes either base64 or hex
		}
	}
}


int MovieData::dump(EMUFILE* fp, bool binary)
{
	int start = fp->ftell();
	fp->fprintf("version %d\n", version);
	fp->fprintf("emuVersion %d\n", emuVersion);
	fp->fprintf("rerecordCount %d\n", rerecordCount);

	fp->fprintf("romFilename %s\n", romFilename.c_str());
	fp->fprintf("romChecksum %s\n", u32ToHexString(gameInfo.crc).c_str());
	fp->fprintf("romSerial %s\n", romSerial.c_str());
	fp->fprintf("guid %s\n", guid.toString().c_str());
	fp->fprintf("useExtBios %d\n", CommonSettings.UseExtBIOS?1:0);
	fp->fprintf("advancedTiming %d\n", CommonSettings.advanced_timing?1:0);

	if(CommonSettings.UseExtBIOS)
		fp->fprintf("swiFromBios %d\n", CommonSettings.SWIFromBIOS?1:0);

	fp->fprintf("useExtFirmware %d\n", CommonSettings.UseExtFirmware?1:0);

	if(CommonSettings.UseExtFirmware) {
		fp->fprintf("bootFromFirmware %d\n", CommonSettings.BootFromFirmware?1:0);
	}
	else {
		char temp_str[27];
		int i;

		/* FIXME: harshly only use the lower byte of the UTF-16 character.
		 * This would cause strange behaviour if the user could set UTF-16 but
		 * they cannot yet.
		 */
		for (i = 0; i < CommonSettings.fw_config.nickname_len; i++) {
			temp_str[i] = CommonSettings.fw_config.nickname[i];
		}
		temp_str[i] = '\0';
		fp->fprintf("firmNickname %s\n", temp_str);
		for (i = 0; i < CommonSettings.fw_config.message_len; i++) {
			temp_str[i] = CommonSettings.fw_config.message[i];
		}
		temp_str[i] = '\0';
		fp->fprintf("firmMessage %s\n", temp_str);
		fp->fprintf("firmFavColour %d\n", CommonSettings.fw_config.fav_colour);
		fp->fprintf("firmBirthMonth %d\n", CommonSettings.fw_config.birth_month);
		fp->fprintf("firmBirthDay %d\n", CommonSettings.fw_config.birth_day);
		fp->fprintf("firmLanguage %d\n", CommonSettings.fw_config.language);
	}

	fp->fprintf("rtcStartNew %s\n", rtcStart.ToString().c_str());

	for(uint32 i=0;i<comments.size();i++)
		fp->fprintf("comment %s\n", wcstombs(comments[i]).c_str());
	
	if(binary)
		fp->fprintf("binary 1\n");

	if(savestate.size() != 0)
		fp->fprintf("savestate %s\n", BytesToString(&savestate[0],savestate.size()).c_str());
	if(sram.size() != 0)
		fp->fprintf("sram %s\n", BytesToString(&sram[0],sram.size()).c_str());

	if(binary)
	{
		//put one | to start the binary dump
		fp->fputc('|');
		for(int i=0;i<(int)records.size();i++)
			records[i].dumpBinary(this,fp,i);
	}
	else
		for(int i=0;i<(int)records.size();i++)
			records[i].dump(this,fp,i);

	int end = fp->ftell();
	return end-start;
}

//yuck... another custom text parser.
bool LoadFM2(MovieData& movieData, EMUFILE* fp, int size, bool stopAfterHeader)
{
	//TODO - start with something different. like 'desmume movie version 1"
	int curr = fp->ftell();

	//movie must start with "version 1"
	char buf[9];
	curr = fp->ftell();
	fp->fread(buf,9);
	fp->fseek(curr, SEEK_SET);
//	if(fp->fail()) return false;
	if(memcmp(buf,"version 1",9)) 
		return false;

	std::string key,value;
	enum {
		NEWLINE, KEY, SEPARATOR, VALUE, RECORD, COMMENT
	} state = NEWLINE;
	bool bail = false;
	for(;;)
	{
		bool iswhitespace, isrecchar, isnewline;
		int c;
		if(size--<=0) goto bail;
		c = fp->fgetc();
		if(c == -1)
			goto bail;
		iswhitespace = (c==' '||c=='\t');
		isrecchar = (c=='|');
		isnewline = (c==10||c==13);
		if(isrecchar && movieData.binaryFlag && !stopAfterHeader)
		{
			LoadFM2_binarychunk(movieData, fp, size);
			return true;
		}
		switch(state)
		{
		case NEWLINE:
			if(isnewline) goto done;
			if(iswhitespace) goto done;
			if(isrecchar) 
				goto dorecord;
			//must be a key
			key = "";
			value = "";
			goto dokey;
			break;
		case RECORD:
			{
				dorecord:
				if (stopAfterHeader) return true;
				int currcount = movieData.records.size();
				movieData.records.resize(currcount+1);
				int preparse = fp->ftell();
				movieData.records[currcount].parse(&movieData, fp);
				int postparse = fp->ftell();
				size -= (postparse-preparse);
				state = NEWLINE;
				break;
			}

		case KEY:
			dokey: //dookie
			state = KEY;
			if(iswhitespace) goto doseparator;
			if(isnewline) goto commit;
			key += c;
			break;
		case SEPARATOR:
			doseparator:
			state = SEPARATOR;
			if(isnewline) goto commit;
			if(!iswhitespace) goto dovalue;
			break;
		case VALUE:
			dovalue:
			state = VALUE;
			if(isnewline) goto commit;
			value += c;
			break;
		case COMMENT:
		default:
			break;
		}
		goto done;

		bail:
		bail = true;
		if(state == VALUE) goto commit;
		goto done;
		commit:
		movieData.installValue(key,value);
		state = NEWLINE;
		done: ;
		if(bail) break;
	}

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

/// Stop movie playback.
static void StopPlayback()
{
	driver->USR_InfoMessage("Movie playback stopped.");
	movieMode = MOVIEMODE_INACTIVE;
}

/// Stop movie playback without closing the movie.
static void FinishPlayback()
{
	driver->USR_InfoMessage("Movie finished playing.");
	movieMode = MOVIEMODE_FINISHED;
}


/// Stop movie recording
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
	//FCEUFILE *fp = FCEU_fopen(fname,0,"rb",0);
	//if (!fp) return;
	//if(fp->isArchive() && !_read_only) {
	//	FCEU_PrintError("Cannot open a movie in read+write from an archive.");
	//	return;
	//}

	//LoadFM2(currMovieData, fp->stream, INT_MAX, false);
	
	
	bool loadedfm2 = false;
	bool opened = false;
//	{
		EMUFILE* fp = new EMUFILE_FILE(fname, "rb");
//		if(fs.is_open())
//		{
			loadedfm2 = LoadFM2(currMovieData, fp, INT_MAX, false);
			opened = true;
//		}
//		fs.close();
		delete fp;
//	}
	if(!opened)
	{
		// for some reason fs.open doesn't work, it has to be a whole new fstream object
//		fstream fs (fname, std::ios_base::in);
		loadedfm2 = LoadFM2(currMovieData, fp, INT_MAX, false);
//		fs.close();
		delete fp;
	}

	if(!loadedfm2)
		return "failed to load movie";

	//TODO
	//fully reload the game to reinitialize everything before playing any movie
	//poweron(true);

	// reset firmware (some games can write to it)
	if (CommonSettings.UseExtFirmware == false)
	{
		NDS_CreateDummyFirmware(&CommonSettings.fw_config);
	}

	NDS_Reset();

	////WE NEED TO LOAD A SAVESTATE
	//if(currMovieData.savestate.size() != 0)
	//{
	//	bool success = MovieData::loadSavestateFrom(&currMovieData.savestate);
	//	if(!success) return;
	//}
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
void FCEUI_SaveMovie(const char *fname, std::wstring author, int flag, std::string sramfname, const DateTime &rtcstart)
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

	// reset firmware (some games can write to it)
	if (CommonSettings.UseExtFirmware == false)
	{
		NDS_CreateDummyFirmware(&CommonSettings.fw_config);
	}

	NDS_Reset();

	//todo ?
	//poweron(true);
	//else
	//	MovieData::dumpSavestateTo(&currMovieData.savestate,Z_BEST_COMPRESSION);

	if(flag == 1)
		EMUFILE::readAllBytes(&currMovieData.sram, sramfname);

	//we are going to go ahead and dump the header. from now on we will only be appending frames
	currMovieData.dump(osRecordingMovie, false);

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
			 UserInput& input = NDS_getProcessingUserInput();

			 MovieRecord* mr = &currMovieData.records[currFrameCounter];

			 if(mr->command_microphone()) input.mic.micButtonPressed = 1;
			 else input.mic.micButtonPressed = 0;

			 if(mr->command_reset()) NDS_Reset();

			 if(mr->command_lid()) input.buttons.F = true;
			 else input.buttons.F = false;

			 u16 pad = mr->pad;
			 input.buttons.R = (((pad>>12)&1)!=0);
			 input.buttons.L = (((pad>>11)&1)!=0);
			 input.buttons.D = (((pad>>10)&1)!=0);
			 input.buttons.U = (((pad>>9)&1)!=0);
			 input.buttons.T = (((pad>>8)&1)!=0);
			 input.buttons.S = (((pad>>7)&1)!=0);
			 input.buttons.B = (((pad>>6)&1)!=0);
			 input.buttons.A = (((pad>>5)&1)!=0);
			 input.buttons.Y = (((pad>>4)&1)!=0);
			 input.buttons.X = (((pad>>3)&1)!=0);
			 input.buttons.W = (((pad>>2)&1)!=0);
			 input.buttons.E = (((pad>>1)&1)!=0);
			 input.buttons.G = (((pad>>0)&1)!=0);

			 input.touch.touchX = mr->touch.x << 4;
			 input.touch.touchY = mr->touch.y << 4;
			 input.touch.isTouch = mr->touch.touch != 0;
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
		 const UserInput& input = NDS_getFinalUserInput();

		 MovieRecord mr;

		 mr.commands = 0;

		 if(input.mic.micButtonPressed == 1)
			 mr.commands = MOVIECMD_MIC;

		 mr.pad = nds.pad;

		 if(input.buttons.F)
			 mr.commands = MOVIECMD_LID;

		 if(movie_reset_command) {
			 mr.commands = MOVIECMD_RESET;
			 movie_reset_command = false;
		 }

		 mr.touch.touch = input.touch.isTouch ? 1 : 0;
		 mr.touch.x = input.touch.isTouch ? input.touch.touchX >> 4 : 0;
		 mr.touch.y = input.touch.isTouch ? input.touch.touchY >> 4 : 0;

		 assert(mr.touch.touch || (!mr.touch.x && !mr.touch.y));
		 //assert(nds.touchX == input.touch.touchX && nds.touchY == input.touch.touchY);
		 //assert((mr.touch.x << 4) == nds.touchX && (mr.touch.y << 4) == nds.touchY);

		 mr.dump(&currMovieData, osRecordingMovie,currMovieData.records.size());
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
static const int kMOVI = 0x49564F4D;
static const int kNOMO = 0x4F4D4F4E;

void mov_savestate(EMUFILE* fp)
{
	//we are supposed to dump the movie data into the savestate
	//if(movieMode == MOVIEMODE_RECORD || movieMode == MOVIEMODE_PLAY)
	//	return currMovieData.dump(os, true);
	//else return 0;
	if(movieMode != MOVIEMODE_INACTIVE)
	{
		write32le(kMOVI,fp);
		currMovieData.dump(fp, true);
	}
	else
	{
		write32le(kNOMO,fp);
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

bool mov_loadstate(EMUFILE* fp, int size)
{
	load_successful = false;

	u32 cookie;
	if(read32le(&cookie,fp) != 1) return false;
	if(cookie == kNOMO)
	{
		if(movieMode == MOVIEMODE_RECORD || movieMode == MOVIEMODE_PLAY)
			FinishPlayback();
		return true;
	}
	else if(cookie != kMOVI)
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
		
	//	is->seekg((uint32)curr+size);
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
			#if defined(WIN32)
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
			osd->setLineColor(255,0,0); // let's make the text red too to hopefully catch the user's attention a bit.
			FinishPlayback();
			osd->setLineColor(255,255,255);

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
			   osd->setLineColor(255, 0, 0);
			   osd->addLine("Can't save movie file!");
			}

			//printf("DUMPING MOVIE: %d FRAMES\n",currMovieData.records.size());
			currMovieData.dump(osRecordingMovie, false);
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


bool FCEUI_MovieGetInfo(EMUFILE* fp, MOVIE_INFO& info, bool skipFrameCount)
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

bool MovieRecord::parseBinary(MovieData* md, EMUFILE* fp)
{
	commands=fp->fgetc();
	fp->fread((char *) &pad, sizeof pad);
	fp->fread((char *) &touch.x, sizeof touch.x);
	fp->fread((char *) &touch.y, sizeof touch.y);
	fp->fread((char *) &touch.touch, sizeof touch.touch);
	return true;
}


void MovieRecord::dumpBinary(MovieData* md, EMUFILE* fp, int index)
{
	fp->fputc(md->records[index].commands);
	fp->fwrite((char *) &md->records[index].pad, sizeof md->records[index].pad);
	fp->fwrite((char *) &md->records[index].touch.x, sizeof md->records[index].touch.x);
	fp->fwrite((char *) &md->records[index].touch.y, sizeof md->records[index].touch.y);
	fp->fwrite((char *) &md->records[index].touch.touch, sizeof md->records[index].touch.touch);
}

void LoadFM2_binarychunk(MovieData& movieData, EMUFILE* fp, int size)
{
	int recordsize = 1; //1 for the command

	recordsize = 6;

	assert(size%6==0);

	//find out how much remains in the file
	int curr = fp->ftell();
	fp->fseek(0,SEEK_END);
	int end = fp->ftell();
	int flen = end-curr;
	fp->fseek(curr,SEEK_SET);

	//the amount todo is the min of the limiting size we received and the remaining contents of the file
	int todo = std::min(size, flen);

	int numRecords = todo/recordsize;
	//printf("LOADED MOVIE: %d records; currFrameCounter: %d\n",numRecords,currFrameCounter);
	movieData.records.resize(numRecords);
	for(int i=0;i<numRecords;i++)
	{
		movieData.records[i].parseBinary(&movieData,fp);
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
	EMUFILE* outf = new EMUFILE_FILE(backupFn.c_str(),"wb"); //FCEUD_UTF8_fstream(backupFn, "wb");	//open/create file
	md.dump(outf,false);										//dump movie data
	delete outf;												//clean up, delete file object
	
	//TODO, decide if fstream successfully opened the file and print error message if it doesn't

	if (dispMessage)	//If we should inform the user 
	{
//		if (overflow)
//			FCEUI_DispMessage("Backup overflow, overwriting %s",backupFn.c_str()); //Inform user of overflow
//		else
//			FCEUI_DispMessage("%s created",backupFn.c_str()); //Inform user of backup filename
	}
}

