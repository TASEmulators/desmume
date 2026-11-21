//taken from fceux on 10/27/08
//subsequently modified for desmume

/*
	Copyright (C) 2008-2025 DeSmuME team

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

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "encodings/utf.h"
#include "xstring.h"

//a vc-style substring operation (very kind and lenient)
std::string strsub(const std::string& str, int pos, int len) {
	int strlen = (int)str.size();
	
	if(strlen==0) return str; //empty strings always return empty strings
	if(pos>=strlen) return str; //if you start past the end of the string, return the entire string. this is unusual, but there you have it

	//clipping
	if(pos<0) {
		len += pos;
		pos = 0;
	}

	// Keep the original lenient semantics: a negative or oversized length
	// means "to the end". Written explicitly so a negative len never turns
	// into a huge size_t, and (pos+len) cannot overflow int.
	if (len < 0 || len >= strlen - pos)
		len = strlen - pos;

	return str.substr(pos, (size_t)len);
}

std::string strmid(const std::string& str, int pos, int len) { return strsub(str,pos,len); }
std::string strleft(const std::string& str, int len) { return strsub(str,0,len); }
std::string strright(const std::string& str, int len) { return len ? strsub(str,(int)str.size()-len,len) : ""; }
std::string toupper(const std::string& str)
{
	std::string ret = str;
	for(size_t i=0;i<str.size();i++)
		ret[i] = (char)toupper(static_cast<unsigned char>(ret[i]));
	return ret;
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
	snprintf(temp, sizeof(temp), "%08X", val);
	return temp;
}

///Converts the provided data to a string in a standard, user-friendly, round-trippable format
std::string BytesToString(const void* data, int len)
{
	char temp[16];
	if(len==1) {
		unsigned char value = 0;
		memcpy(&value, data, sizeof(value));
		snprintf(temp, sizeof(temp), "%u", (unsigned)value);
		return temp;
	} else if(len==2) {
		unsigned short value = 0;
		memcpy(&value, data, sizeof(value));
		snprintf(temp, sizeof(temp), "%u", (unsigned)value);
		return temp;
	} else if(len==4) {
		unsigned int value = 0;
		memcpy(&value, data, sizeof(value));
		snprintf(temp, sizeof(temp), "%u", value);
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
				(unsigned char)(n<2 ? '=' : Base64Table[ ((input[1] & 0x0F) << 2) | (input[2] >> 6) ]),
				(unsigned char)(n<3 ? '=' : Base64Table[ input[2] & 0x3F ])
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

static int HexNibble(unsigned char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

///returns -1 if this is not a hex string
int HexStringToBytesLength(const std::string& str)
{
	if(str.size()>2 && str[0] == '0' && toupper(static_cast<unsigned char>(str[1])) == 'X')
		return (int)(str.size() / 2) - 1;
	else return -1;
}

int Base64StringToBytesLength(const std::string& str)
{
	if(str.size() < 7 || (str.size()-7) % 4 || str.substr(0,7) != "base64:") return -1;
	
	size_t c = ((str.size() - 7) / 4) * 3;
	if(str[str.size()-1] == '=') { --c;
	if(str[str.size()-2] == '=') --c; }
	return (int)c;
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
				(unsigned char)((converted[0] << 2) | (converted[1] >> 4)),
				(unsigned char)((converted[1] << 4) | (converted[2] >> 2)),
				(unsigned char)((converted[2] << 6) | (converted[3]))
			};
			int outlen = (input[2] == '=') ? 1 : (input[3] == '=' ? 2 : 3);
			if(outlen > len) outlen = len;
			memcpy(tgt, outpacket, outlen);
			tgt += outlen;
			len -= outlen;
		}
		return true;
	}
	if(str.size()>2 && str[0] == '0' && toupper(static_cast<unsigned char>(str[1])) == 'X')
	{
		// hex. The "0x" prefix is not part of the payload, so the byte
		// count is (size-2)/2. Using size/2 here over-counted by one,
		// reading past the string end and writing past data - the bug
		// recorded in movie.cpp:installRomChecksum().
		int bytesAvailable = (int)((str.size() - 2) / 2);
		int amt = (len < bytesAvailable) ? len : bytesAvailable;
		const char* cstr = str.c_str() + 2;
		for(int i=0;i<amt;i++) {
			const int hi = HexNibble((unsigned char)cstr[i*2]);
			const int lo = HexNibble((unsigned char)cstr[i*2+1]);
			if(hi < 0 || lo < 0) return false;
			((unsigned char*)data)[i] = (unsigned char)((hi << 4) | lo);
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

std::string stditoa(int n)
{
	char tempbuf[16];
	snprintf(tempbuf, sizeof(tempbuf), "%d", n);
	return tempbuf;
}


// replace all instances of victim with replacement
std::string mass_replace(const std::string &source, const std::string &victim, const std::string &replacement)
{
	std::string answer = source;
	if (victim.empty())
		return answer; // find("") + an empty replacement would never advance
	std::string::size_type j = 0;
	while ((j = answer.find(victim, j)) != std::string::npos )
	{
		answer.replace(j, victim.length(), replacement);
		j+= replacement.length();
	}
	return answer;
}

std::string wcstombs_locale(std::wstring str)
{
#ifdef HOST_WINDOWS
	if (str.empty())
		return std::string();
	const int needed = WideCharToMultiByte(CP_ACP, 0, str.c_str(), (int)str.size(), NULL, 0, NULL, NULL);
	if (needed <= 0)
		return std::string();
	std::string ret((size_t)needed, '\0');
	WideCharToMultiByte(CP_ACP, 0, str.c_str(), (int)str.size(), &ret[0], needed, NULL, NULL);
	return ret;
#else
	return wcstombs(str);
#endif
}

std::wstring mbstowcs_locale(std::string str)
{
#ifdef HOST_WINDOWS
	if (str.empty())
		return std::wstring();
	const int needed = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str.c_str(), (int)str.size(), NULL, 0);
	if (needed <= 0)
		return std::wstring();
	std::wstring ret((size_t)needed, L'\0');
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str.c_str(), (int)str.size(), &ret[0], needed);
	return ret;
#else
	return mbstowcs(str);
#endif
}

// UTF-8 <-> std::wstring. std::wstring is UTF-16 on Windows and UTF-32
// on macOS/Linux, so Windows uses the native Win32 code-page APIs while
// other platforms keep the libretro-common / direct UTF-32 path below.
//convert a std::string to std::wstring
std::wstring mbstowcs(std::string str)
{
#ifdef HOST_WINDOWS
	// Callers pass UTF-8 (movie author/comments, paths), so use CP_UTF8
	// rather than the process ANSI code page; this also handles surrogate
	// pairs correctly instead of truncating code points to 16 bits.
	if (str.empty())
		return std::wstring();
	const int needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
	if (needed <= 0)
		return std::wstring();
	std::wstring ret((size_t)needed, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &ret[0], needed);
	return ret;
#else
	size_t len = utf8len(str.c_str());
	u32* tmp32 = new u32[len+1];
	wchar_t* tmp16 = new wchar_t[len+1];
	utf8_conv_utf32(tmp32,len+1,str.c_str(),str.size()+1);

	//truncate the utf32 output to utf16.
	//I know that's amazingly sloppy, but it should work for us, probably (libretro-common's facilities are underpowered at the present)
	for(size_t i=0;i<len+1;i++)
		tmp16[i] = tmp32[i];
	std::wstring ret = tmp16;

	delete[] tmp32;
	delete[] tmp16;

	return ret;
#endif
}

std::string wcstombs(std::wstring str)
{
#ifdef HOST_WINDOWS
	if (str.empty())
		return std::string();
	const int needed = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0, NULL, NULL);
	if (needed <= 0)
		return std::string();
	std::string ret((size_t)needed, '\0');
	WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), &ret[0], needed, NULL, NULL);
	return ret;
#else
	size_t len = str.length();
	char *tmp8 = new char[len*4+1]; //allow space for string to inflate

	if (sizeof(wchar_t) == 4)
	{
		// macOS/Linux: wchar_t holds UTF-32. Casting it to u16* (the Windows
		// path below) makes the high half of every code point look like a
		// NUL, so "author X" becomes just "a". Encode the code points here.
		char *out = tmp8;
		for (size_t i = 0; i < len; i++)
		{
			const u32 cp = (u32)str[i];
			if (cp == 0) break;
			if (cp < 0x80)
			{
				*out++ = (char)cp;
			}
			else if (cp < 0x800)
			{
				*out++ = (char)(0xC0 | (cp >> 6));
				*out++ = (char)(0x80 | (cp & 0x3F));
			}
			else if (cp < 0x10000)
			{
				*out++ = (char)(0xE0 | (cp >> 12));
				*out++ = (char)(0x80 | ((cp >> 6) & 0x3F));
				*out++ = (char)(0x80 | (cp & 0x3F));
			}
			else
			{
				*out++ = (char)(0xF0 | (cp >> 18));
				*out++ = (char)(0x80 | ((cp >> 12) & 0x3F));
				*out++ = (char)(0x80 | ((cp >> 6) & 0x3F));
				*out++ = (char)(0x80 | (cp & 0x3F));
			}
		}
		*out = '\0';
	}
	else
	{
		utf16_to_char_string((u16*)str.c_str(),tmp8,len*4+1);
	}
	std::string ret = tmp8;
	delete[] tmp8;
	return ret;
#endif
}

