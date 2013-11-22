//taken from fceux on 10/27/08
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

#include "xstring.h"
#include <string>

//a vc-style substring operation (very kind and lenient)
std::string strsub(const std::string& str, int pos, int len) {
	int strlen = str.size();
	
	if(strlen==0) return str; //empty strings always return empty strings
	if(pos>=strlen) return str; //if you start past the end of the string, return the entire string. this is unusual, but there you have it

	//clipping
	if(pos<0) {
		len += pos;
		pos = 0;
	}

	if (pos+len>=strlen)
		len=strlen-pos+1;
	
	//return str.str().substr(pos,len);
	return str.substr(pos,len);
}

std::string strmid(const std::string& str, int pos, int len) { return strsub(str,pos,len); }
std::string strleft(const std::string& str, int len) { return strsub(str,0,len); }
std::string strright(const std::string& str, int len) { return len ? strsub(str,str.size()-len,len) : ""; }
std::string toupper(const std::string& str)
{
	std::string ret = str;
	for(u32 i=0;i<str.size();i++)
		ret[i] = toupper(ret[i]);
	return ret;
}

///Upper case routine. Returns number of characters modified
int str_ucase(char *str) {
	u32 i=0,j=0;

	while (i < strlen(str)) {
		if ((str[i] >= 'a') && (str[i] <= 'z')) {
			str[i] &= ~0x20;
			j++;
		}
		i++;
	}
	return j;
}


///Lower case routine. Returns number of characters modified
int str_lcase(char *str) {
	u32 i=0,j=0;

	while (i < strlen(str)) {
		if ((str[i] >= 'A') && (str[i] <= 'Z')) {
			str[i] |= 0x20;
			j++;
		}
		i++;
	}
	return j;
}


///White space-trimming routine

///Removes whitespace from left side of string, depending on the flags set (See STRIP_x definitions in xstring.h)
///Returns number of characters removed
int str_ltrim(char *str, int flags) {
	u32 i=0;

	while (str[0]) {
		if ((str[0] != ' ') || (str[0] != '\t') || (str[0] != '\r') || (str[0] != '\n')) break;

		if ((flags & STRIP_SP) && (str[0] == ' ')) {
			i++;
			strcpy(str,str+1);
		}
		if ((flags & STRIP_TAB) && (str[0] == '\t')) {
			i++;
			strcpy(str,str+1);
		}
		if ((flags & STRIP_CR) && (str[0] == '\r')) {
			i++;
			strcpy(str,str+1);
		}
		if ((flags & STRIP_LF) && (str[0] == '\n')) {
			i++;
			strcpy(str,str+1);
		}
	}
	return i;
}


///White space-trimming routine

///Removes whitespace from right side of string, depending on the flags set (See STRIP_x definitions in xstring.h)
///Returns number of characters removed
int str_rtrim(char *str, int flags) {
	u32 i=0;

	while (strlen(str)) {
		if ((str[strlen(str)-1] != ' ') ||
			(str[strlen(str)-1] != '\t') ||
			(str[strlen(str)-1] != '\r') ||
			(str[strlen(str)-1] != '\n')) break;

		if ((flags & STRIP_SP) && (str[0] == ' ')) {
			i++;
			str[strlen(str)-1] = 0;
		}
		if ((flags & STRIP_TAB) && (str[0] == '\t')) {
			i++;
			str[strlen(str)-1] = 0;
		}
		if ((flags & STRIP_CR) && (str[0] == '\r')) {
			i++;
			str[strlen(str)-1] = 0;
		}
		if ((flags & STRIP_LF) && (str[0] == '\n')) {
			i++;
			str[strlen(str)-1] = 0;
		}
	}
	return i;
}


///White space-stripping routine

///Removes whitespace depending on the flags set (See STRIP_x definitions in xstring.h)
///Returns number of characters removed, or -1 on error
int str_strip(char *str, int flags) {
	u32 i=0,j=0;
	char *astr,chr;

	if (!strlen(str)) return -1;
	if (!(flags & (STRIP_SP|STRIP_TAB|STRIP_CR|STRIP_LF))) return -1;
	if (!(astr = (char*)malloc(strlen(str)+1))) return -1;
	while (i < strlen(str)) {
		chr = str[i++];
		if ((flags & STRIP_SP) && (chr == ' ')) chr = 0;
		if ((flags & STRIP_TAB) && (chr == '\t')) chr = 0;
		if ((flags & STRIP_CR) && (chr == '\r')) chr = 0;
		if ((flags & STRIP_LF) && (chr == '\n')) chr = 0;

		if (chr) astr[j++] = chr;
	}
	astr[j] = 0;
	strcpy(str,astr);
	free(astr);
	return j;
}


///Character replacement routine

///Replaces all instances of 'search' with 'replace'
///Returns number of characters modified
int chr_replace(char *str, char search, char replace) {
	u32 i=0,j=0;

	while (i < strlen(str)) {
		if (str[i] == search) {
			str[i] = replace;
			j++;
		}
		i++;
	}
	return j;
}


///Sub-String replacement routine

///Replaces all instances of 'search' with 'replace'
///Returns number of sub-strings modified, or -1 on error
int str_replace(char *str, char *search, char *replace) {
	u32 i=0,j=0;
	int searchlen,replacelen;
	char *astr;

	searchlen = strlen(search);
	replacelen = strlen(replace);
	if ((!strlen(str)) || (!searchlen)) return -1; //note: allow *replace to have a length of zero!
	if (!(astr = (char*)malloc(strlen(str)+1))) return -1;
	while (i < strlen(str)) {
		if (!strncmp(str+i,search,searchlen)) {
			if (replacelen) memcpy(astr+j,replace,replacelen);
			i += searchlen;
			j += replacelen;
		}
		else astr[j++] = str[i++];
	}
	astr[j] = 0;
	strcpy(str,astr);
	free(astr);
	return j;
}

static const struct Base64Table
{
	Base64Table()
	{
		size_t a=0;
		for(a=0; a<256; ++a) data[a] = 0xFF; // mark everything as invalid by default
		// create value->ascii mapping
		a=0;
		for(unsigned char c='A'; c<='Z'; ++c) data[a++] = c; // 0..25
		for(unsigned char c='a'; c<='z'; ++c) data[a++] = c; // 26..51
		for(unsigned char c='0'; c<='9'; ++c) data[a++] = c; // 52..61
		data[62] = '+';                             // 62
		data[63] = '/';                             // 63
		// create ascii->value mapping (but due to overlap, write it to highbit region)
		for(a=0; a<64; ++a) data[data[a]^0x80] = a; // 
		data[((unsigned char)'=') ^ 0x80] = 0;
	}
	unsigned char operator[] (size_t pos) const { return data[pos]; }
private:
	unsigned char data[256];
} Base64Table;

std::string u32ToHexString(u32 val)
{
	char temp[16];
	sprintf(temp,"%08X",val);
	return temp;
}

///Converts the provided data to a string in a standard, user-friendly, round-trippable format
std::string BytesToString(const void* data, int len)
{
	char temp[16];
	if(len==1) {
		sprintf(temp,"%d",*(const unsigned char*)data);
		return temp;
	} else if(len==2) {
		sprintf(temp,"%d",*(const unsigned short*)data);
		return temp;
	} else if(len==4) {
		sprintf(temp,"%d",*(const unsigned int*)data);
		return temp;		
	}
	
	std::string ret;
	if(1) // use base64
	{
		const unsigned char* src = (const unsigned char*)data;
		ret = "base64:";
		for(int n; len > 0; len -= n)
		{
			unsigned char input[3] = {0,0,0};
			for(n=0; n<3 && n<len; ++n)
				input[n] = *src++;
			unsigned char output[4] =
			{
				Base64Table[ input[0] >> 2 ],
				Base64Table[ ((input[0] & 0x03) << 4) | (input[1] >> 4) ],
				n<2 ? '=' : Base64Table[ ((input[1] & 0x0F) << 2) | (input[2] >> 6) ],
				n<3 ? '=' : Base64Table[ input[2] & 0x3F ]
			};
			ret.append(output, output+4);
		}
	}
	else // use hex
	{
		ret.resize(len*2+2);
		ret[0] = '0';
		ret[1] = 'x';
		for(int i=0;i<len;i++)
		{
			int a = (((const unsigned char*)data)[i]>>4);
			int b = (((const unsigned char*)data)[i])&15;
			if(a>9) a += 'A'-10;
			else a += '0';
			if(b>9) b += 'A'-10;
			else b += '0';
			ret[2+i*2] = a;
			ret[2+i*2+1] = b;
		}
	}
	return ret;
}

///returns -1 if this is not a hex string
int HexStringToBytesLength(const std::string& str)
{
	if(str.size()>2 && str[0] == '0' && toupper(str[1]) == 'X')
		return str.size()/2-1;
	else return -1;
}

int Base64StringToBytesLength(const std::string& str)
{
	if(str.size() < 7 || (str.size()-7) % 4 || str.substr(0,7) != "base64:") return -1;
	
	size_t c = ((str.size() - 7) / 4) * 3;
	if(str[str.size()-1] == '=') { --c;
	if(str[str.size()-2] == '=') --c; }
	return c;
}

///parses a string in the same format as BytesToString
///returns true if success.
bool StringToBytes(const std::string& str, void* data, int len)
{
	if(str.substr(0,7) == "base64:")
	{
		// base64
		unsigned char* tgt = (unsigned char*)data;
		for(size_t pos = 7; pos < str.size() && len > 0; )
		{
			unsigned char input[4], converted[4];
			for(int i=0; i<4; ++i)
			{
				if(pos >= str.size() && i > 0) return false; // invalid data
				input[i]	 = str[pos++];
				if(input[i] & 0x80) return false;	  // illegal character
				converted[i] = Base64Table[input[i]^0x80];
				if(converted[i] & 0x80) return false; // illegal character
			}
			unsigned char outpacket[3] =
			{
				(converted[0] << 2) | (converted[1] >> 4),
				(converted[1] << 4) | (converted[2] >> 2),
				(converted[2] << 6) | (converted[3])
			};
			int outlen = (input[2] == '=') ? 1 : (input[3] == '=' ? 2 : 3);
			if(outlen > len) outlen = len;
			memcpy(tgt, outpacket, outlen);
			tgt += outlen;
			len -= outlen;
		}
		return true;
	}
	if(str.size()>2 && str[0] == '0' && toupper(str[1]) == 'X')
	{
		// hex
		int amt = len;
		int bytesAvailable = str.size()/2;
		if(bytesAvailable < amt)
			amt = bytesAvailable;
		const char* cstr = str.c_str()+2;
		for(int i=0;i<amt;i++) {
			char a = toupper(cstr[i*2]);
			char b = toupper(cstr[i*2+1]);
			if(a>='A') a=a-'A'+10;
			else a-='0';
			if(b>='A') b=b-'A'+10;
			else b-='0';
			unsigned char val = ((unsigned char)a<<4)|(unsigned char)b; 
			((unsigned char*)data)[i] = val;
		}
		return true;
	}
	
	if(len==1) {
		int x = atoi(str.c_str());
		*(unsigned char*)data = x;
		return true;
	} else if(len==2) {
		int x = atoi(str.c_str());
		*(unsigned short*)data = x;
		return true;
	} else if(len==4) {
		int x = atoi(str.c_str());
		*(unsigned int*)data = x;
		return true;
	}
	//we can't handle it
	return false;
}

#include <string>
#include <vector>
/// \brief convert input string into vector of string tokens
///
/// \note consecutive delimiters will be treated as single delimiter
/// \note delimiters are _not_ included in return data
///
/// \param input string to be parsed
/// \param delims list of delimiters.

std::vector<std::string> tokenize_str(const std::string & str,
                                      const std::string & delims=", \t")
{
  using namespace std;
  // Skip delims at beginning, find start of first token
  string::size_type lastPos = str.find_first_not_of(delims, 0);
  // Find next delimiter @ end of token
  string::size_type pos     = str.find_first_of(delims, lastPos);

  // output vector
  vector<string> tokens;

  while (string::npos != pos || string::npos != lastPos)
    {
      // Found a token, add it to the vector.
      tokens.push_back(str.substr(lastPos, pos - lastPos));
      // Skip delims.  Note the "not_of". this is beginning of token
      lastPos = str.find_first_not_of(delims, pos);
      // Find next delimiter at end of token.
      pos     = str.find_first_of(delims, lastPos);
    }

  return tokens;
}

//this code was taken from WINE (LGPL)
//http://google.com/codesearch?hl=en&q=+lang:c+splitpath+show:CPvw9Z-euls:_RSotQzmLeU:KGzljMEYFbY&sa=N&cd=9&ct=rc&cs_p=http://gentoo.osuosl.org/distfiles/Wine-20050524.tar.gz&cs_f=wine-20050524/programs/winefile/splitpath.c
void splitpath(const char* path, char* drv, char* dir, char* name, char* ext)
{
    const char* end; /* end of processed string */
	const char* p;	 /* search pointer */
	const char* s;	 /* copy pointer */

	/* extract drive name */
	if (path[0] && path[1]==':') {
		if (drv) {
			*drv++ = *path++;
			*drv++ = *path++;
			*drv = '\0';
		} else path+=2;
	} else if (drv)
		*drv = '\0';

	/* search for end of string or stream separator */
	for(end=path; *end && *end!=':'; )
		end++;

	/* search for begin of file extension */
	for(p=end; p>path && *--p!='\\' && *p!='/'; )
		if (*p == '.') {
			end = p;
			break;
		}

	if (ext)
		for(s=end; (*ext=*s++); )
			ext++;
	else
		for(s=end; *s++; ) {}

	/* search for end of directory name */
	for(p=end; p>path; )
		if (*--p=='\\' || *p=='/') {
			p++;
			break;
		}

	if (name) {
		for(s=p; s<end; )
			*name++ = *s++;

		*name = '\0';
	} else
		for(s=p; s<end; )
			*s++;

	if (dir) {
		for(s=path; s<p; )
			*dir++ = *s++;

		*dir = '\0';
	}
}

//mbg 5/12/08
//for the curious, I tested U16ToHexStr and it was 10x faster than printf.
//so the author of these dedicated functions is not insane, and I will leave them.

static char TempArray[11];

uint16 FastStrToU16(char* s, bool& valid)
{
	int i;
	uint16 v=0;
	for(i=0; i < 4; i++)
	{
		if(s[i] == 0) return v;
		v<<=4;
		if(s[i] >= '0' && s[i] <= '9')
		{
			v+=s[i]-'0';
		}
		else if(s[i] >= 'a' && s[i] <= 'f')
		{
			v+=s[i]-'a'+10;
		}
		else if(s[i] >= 'A' && s[i] <= 'F')
		{
			v+=s[i]-'A'+10;
		}
		else
		{
			valid = false;
			return 0xFFFF;
		}
	}
	valid = true;
	return v;
}

char *U8ToDecStr(uint8 a)
{
	TempArray[0] = '0' + a/100;
	TempArray[1] = '0' + (a%100)/10;
	TempArray[2] = '0' + (a%10);
	TempArray[3] = 0;
	return TempArray;
}

char *U16ToDecStr(uint16 a)
{
	TempArray[0] = '0' + a/10000;
	TempArray[1] = '0' + (a%10000)/1000;
	TempArray[2] = '0' + (a%1000)/100;
	TempArray[3] = '0' + (a%100)/10;
	TempArray[4] = '0' + (a%10);
	TempArray[5] = 0;
	return TempArray;
}

char *U32ToDecStr(char* buf, uint32 a)
{
	buf[0] = '0' + a/1000000000;
	buf[1] = '0' + (a%1000000000)/100000000;
	buf[2] = '0' + (a%100000000)/10000000;
	buf[3] = '0' + (a%10000000)/1000000;
	buf[4] = '0' + (a%1000000)/100000;
	buf[5] = '0' + (a%100000)/10000;
	buf[6] = '0' + (a%10000)/1000;
	buf[7] = '0' + (a%1000)/100;
	buf[8] = '0' + (a%100)/10;
	buf[9] = '0' + (a%10);
	buf[10] = 0;
	return buf;
}
char *U32ToDecStr(uint32 a)
{
	return U32ToDecStr(TempArray,a);
}

char *U16ToHexStr(uint16 a)
{
	TempArray[0] = a/4096 > 9?'A'+a/4096-10:'0' + a/4096;
	TempArray[1] = (a%4096)/256 > 9?'A'+(a%4096)/256 - 10:'0' + (a%4096)/256;
	TempArray[2] = (a%256)/16 > 9?'A'+(a%256)/16 - 10:'0' + (a%256)/16;
	TempArray[3] = a%16 > 9?'A'+(a%16) - 10:'0' + (a%16);
	TempArray[4] = 0;
	return TempArray;
}

char *U8ToHexStr(uint8 a)
{
	TempArray[0] = a/16 > 9?'A'+a/16 - 10:'0' + a/16;
	TempArray[1] = a%16 > 9?'A'+(a%16) - 10:'0' + (a%16);
	TempArray[2] = 0;
	return TempArray;
}

std::string stditoa(int n)
{
	char tempbuf[16];
	sprintf(tempbuf, "%d", n);
	return tempbuf;
}


std::string readNullTerminatedAscii(std::istream* is)
{
	std::string ret;
	ret.reserve(50);
	for(;;) 
	{
		int c = is->get();
		if(c == 0) break;
		else ret += (char)c;
	}
	return ret;
}

// replace all instances of victim with replacement
std::string mass_replace(const std::string &source, const std::string &victim, const std::string &replacement)
{
	std::string answer = source;
	std::string::size_type j = 0;
	while ((j = answer.find(victim, j)) != std::string::npos )
	{
		answer.replace(j, victim.length(), replacement);
		j+= replacement.length();
	}
	return answer;
}

//http://www.codeproject.com/KB/string/UtfConverter.aspx
#include "ConvertUTF.h"
namespace UtfConverter
{
    static std::wstring FromUtf8(const std::string& utf8string)
    {
        size_t widesize = utf8string.length();
        if (sizeof(wchar_t) == 2)
        {
            wchar_t* widestringnative = new wchar_t[widesize+1];
            const UTF8* sourcestart = reinterpret_cast<const UTF8*>(utf8string.c_str());
            const UTF8* sourceend = sourcestart + widesize;
            UTF16* targetstart = reinterpret_cast<UTF16*>(widestringnative);
            UTF16* targetend = targetstart + widesize+1;
            ConversionResult res = ConvertUTF8toUTF16(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
            if (res != conversionOK)
            {
                delete [] widestringnative;
                throw std::exception();
            }
            *targetstart = 0;
            std::wstring resultstring(widestringnative);
            delete [] widestringnative;
            return resultstring;
        }
        else if (sizeof(wchar_t) == 4)
        {
            wchar_t* widestringnative = new wchar_t[widesize+1];
            const UTF8* sourcestart = reinterpret_cast<const UTF8*>(utf8string.c_str());
            const UTF8* sourceend = sourcestart + widesize;
            UTF32* targetstart = reinterpret_cast<UTF32*>(widestringnative);
            UTF32* targetend = targetstart + widesize+1;
            ConversionResult res = ConvertUTF8toUTF32(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
            if (res != conversionOK)
            {
                delete [] widestringnative;
                throw std::exception();
            }
            *targetstart = 0;
            std::wstring resultstring(widestringnative);
            delete [] widestringnative;
            return resultstring;
        }
        else
        {
            throw std::exception();
        }
        return L"";
    }

    static std::string ToUtf8(const std::wstring& widestring)
    {
        size_t widesize = widestring.length();

        if (sizeof(wchar_t) == 2)
        {
            size_t utf8size = 3 * widesize + 1;
            char* utf8stringnative = new char[utf8size];
            const UTF16* sourcestart = reinterpret_cast<const UTF16*>(widestring.c_str());
            const UTF16* sourceend = sourcestart + widesize;
            UTF8* targetstart = reinterpret_cast<UTF8*>(utf8stringnative);
            UTF8* targetend = targetstart + utf8size;
            ConversionResult res = ConvertUTF16toUTF8(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
            if (res != conversionOK)
            {
                delete [] utf8stringnative;
                throw std::exception();
            }
            *targetstart = 0;
            std::string resultstring(utf8stringnative);
            delete [] utf8stringnative;
            return resultstring;
        }
        else if (sizeof(wchar_t) == 4)
        {
            size_t utf8size = 4 * widesize + 1;
            char* utf8stringnative = new char[utf8size];
            const UTF32* sourcestart = reinterpret_cast<const UTF32*>(widestring.c_str());
            const UTF32* sourceend = sourcestart + widesize;
            UTF8* targetstart = reinterpret_cast<UTF8*>(utf8stringnative);
            UTF8* targetend = targetstart + utf8size;
            ConversionResult res = ConvertUTF32toUTF8(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
            if (res != conversionOK)
            {
                delete [] utf8stringnative;
                throw std::exception();
            }
            *targetstart = 0;
            std::string resultstring(utf8stringnative);
            delete [] utf8stringnative;
            return resultstring;
        }
        else
        {
            throw std::exception();
        }
        return "";
    }
}
  
//convert a std::string to std::wstring
std::wstring mbstowcs(std::string str)
{
	try {
		return UtfConverter::FromUtf8(str);
	} catch(std::exception) {
		return L"(failed UTF-8 conversion)";
	}
}

std::string wcstombs(std::wstring str)
{
	return UtfConverter::ToUtf8(str);
}


//TODO - dont we already have another  function that can do this
std::string getExtension(const char* input) {
	char buf[1024];
	strcpy(buf,input);
	char* dot=strrchr(buf,'.');
	if(!dot)
		return "";
	char ext [512];
	strcpy(ext, dot+1);
	int k, extlen=strlen(ext);
	for(k=0;k<extlen;k++)
		ext[k]=tolower(ext[k]);
	return ext;
}

