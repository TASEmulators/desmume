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

#include <string>
#include "encodings/utf.h"
#include "xstring.h"

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

std::string stditoa(int n)
{
	char tempbuf[16];
	sprintf(tempbuf, "%d", n);
	return tempbuf;
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

//convert a std::string to std::wstring
std::wstring mbstowcs(std::string str)
{
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
}

std::string wcstombs(std::wstring str)
{
	size_t len = str.length();
	char *tmp8 = new char[len*4+1]; //allow space for string to inflate
	utf16_to_char_string((u16*)str.c_str(),tmp8,len*4+1);
	std::string ret = tmp8;
	delete[] tmp8;
	return ret;
}

