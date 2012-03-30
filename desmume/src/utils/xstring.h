//taken from fceux on 27-oct-2008
//subsequently modified for desmume

/*
	Copyright (C) 2008-2009 DeSmuME team

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

#ifndef _STRINGUTIL_H_
#define _STRINGUTIL_H_

#include <string>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <cstdio>

#include "../types.h"
#include "../emufile.h"


//definitions for str_strip() flags
#define STRIP_SP	0x01 // space
#define STRIP_TAB	0x02 // tab
#define STRIP_CR	0x04 // carriage return
#define STRIP_LF	0x08 // line feed


int str_ucase(char *str);
int str_lcase(char *str);
int str_ltrim(char *str, int flags);
int str_rtrim(char *str, int flags);
int str_strip(char *str, int flags);
int chr_replace(char *str, char search, char replace);
int str_replace(char *str, char *search, char *replace);

std::string strsub(const std::string& str, int pos, int len);
std::string strmid(const std::string& str, int pos, int len);
std::string strleft(const std::string& str, int len);
std::string strright(const std::string& str, int len);
std::string toupper(const std::string& str);

int HexStringToBytesLength(const std::string& str);
int Base64StringToBytesLength(const std::string& str);
std::string u32ToHexString(u32 val);
std::string BytesToString(const void* data, int len);
bool StringToBytes(const std::string& str, void* data, int len);

std::vector<std::string> tokenize_str(const std::string & str,const std::string & delims);
void splitpath(const char* path, char* drv, char* dir, char* name, char* ext);

uint16 FastStrToU16(char* s, bool& valid);
char *U16ToDecStr(uint16 a);
char *U32ToDecStr(uint32 a);
char *U32ToDecStr(char* buf, uint32 a);
char *U8ToDecStr(uint8 a);
char *U8ToHexStr(uint8 a);
char *U16ToHexStr(uint16 a);

std::string stditoa(int n);

std::string readNullTerminatedAscii(std::istream* is);

//extracts a decimal uint from an istream
template<typename T> T templateIntegerDecFromIstream(EMUFILE* is)
{
	unsigned int ret = 0;
	bool pre = true;

	for(;;)
	{
		int c = is->fgetc();
		if(c == -1) return ret;
		int d = c - '0';
		if((d<0 || d>9))
		{
			if(!pre)
				break;
		}
		else
		{
			pre = false;
			ret *= 10;
			ret += d;
		}
	}
	is->unget();
	return ret;
}

inline u32 u32DecFromIstream(EMUFILE* is) { return templateIntegerDecFromIstream<u32>(is); }
inline u64 u64DecFromIstream(EMUFILE* is) { return templateIntegerDecFromIstream<u64>(is); }

//puts an optionally 0-padded decimal integer of type T into the ostream (0-padding is quicker)
template<typename T, int DIGITS, bool PAD> void putdec(EMUFILE* os, T dec)
{
	char temp[DIGITS];
	int ctr = 0;
	for(int i=0;i<DIGITS;i++)
	{
		int quot = dec/10;
		int rem = dec%10;
		temp[DIGITS-1-i] = '0' + rem;
		if(!PAD)
		{
			if(rem != 0) ctr = i;
		}
		dec = quot;
	}
	if(!PAD)
		os->fwrite(temp+DIGITS-ctr-1,ctr+1);
	else
		os->fwrite(temp,DIGITS);
}

std::string mass_replace(const std::string &source, const std::string &victim, const std::string &replacement);

std::wstring mbstowcs(std::string str);
std::string wcstombs(std::wstring str);



//TODO - dont we already have another  function that can do this
std::string getExtension(const char* input);


#endif
