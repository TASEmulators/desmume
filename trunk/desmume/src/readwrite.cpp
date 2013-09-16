/*
	Copyright (C) 2006-2009 DeSmuME team

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

#include "readwrite.h"
#include "types.h"

//well. just for the sake of consistency
int write8le(u8 b, EMUFILE*os)
{
	os->fwrite((char*)&b,1);
	return 1;
}

//well. just for the sake of consistency
int read8le(u8 *Bufo, EMUFILE*is)
{
	if(is->_fread((char*)Bufo,1) != 1)
		return 0;
	return 1;
}

///writes a little endian 16bit value to the specified file
int write16le(u16 b, EMUFILE *fp)
{
	u8 s[2];
	s[0]=(u8)b;
	s[1]=(u8)(b>>8);
	fp->fwrite(s,2);
	return 2;
}


///writes a little endian 32bit value to the specified file
int write32le(u32 b, EMUFILE *fp)
{
	u8 s[4];
	s[0]=(u8)b;
	s[1]=(u8)(b>>8);
	s[2]=(u8)(b>>16);
	s[3]=(u8)(b>>24);
	fp->fwrite(s,4);
	return 4;
}

void writebool(bool b, EMUFILE* os) { write32le(b?1:0,os); }

int write64le(u64 b, EMUFILE* os)
{
	u8 s[8];
	s[0]=(u8)b;
	s[1]=(u8)(b>>8);
	s[2]=(u8)(b>>16);
	s[3]=(u8)(b>>24);
	s[4]=(u8)(b>>32);
	s[5]=(u8)(b>>40);
	s[6]=(u8)(b>>48);
	s[7]=(u8)(b>>56);
	os->fwrite((char*)&s,8);
	return 8;
}


int read32le(u32 *Bufo, EMUFILE *fp)
{
	u32 buf = 0;
	if(fp->_fread(&buf,4)<4)
		return 0;
#ifdef LOCAL_LE
	*(u32*)Bufo=buf;
#else
	*(u32*)Bufo=((buf&0xFF)<<24)|((buf&0xFF00)<<8)|((buf&0xFF0000)>>8)|((buf&0xFF000000)>>24);
#endif
	return 1;
}

int read16le(u16 *Bufo, EMUFILE *is)
{
	u16 buf;
	if(is->_fread((char*)&buf,2) != 2)
		return 0;
#ifdef LOCAL_LE
	*Bufo=buf;
#else
	*Bufo = LE_TO_LOCAL_16(buf);
#endif
	return 1;
}

int read64le(u64 *Bufo, EMUFILE *is)
{
	u64 buf;
	if(is->_fread((char*)&buf,8) != 8)
		return 0;
#ifdef LOCAL_LE
	*Bufo=buf;
#else
	*Bufo = LE_TO_LOCAL_64(buf);
#endif
	return 1;
}

static int read32le(u32 *Bufo, std::istream *is)
{
	u32 buf;
	if(is->read((char*)&buf,4).gcount() != 4)
		return 0;
#ifdef LOCAL_LE
	*(u32*)Bufo=buf;
#else
	*(u32*)Bufo=((buf&0xFF)<<24)|((buf&0xFF00)<<8)|((buf&0xFF0000)>>8)|((buf&0xFF000000)>>24);
#endif
	return 1;
}

int readbool(bool *b, EMUFILE* is)
{
	u32 temp;
	int ret = read32le(&temp,is);
	*b = temp!=0;
	return ret;
}

int readbuffer(std::vector<u8> &vec, EMUFILE* is)
{
	u32 size;
	if(read32le(&size,is) != 1) return 0;
	vec.resize(size);
	if(size>0) is->fread((char*)&vec[0],size);
	return 1;
}

int writebuffer(std::vector<u8>& vec, EMUFILE* os)
{
	u32 size = vec.size();
	write32le(size,os);
	if(size>0) os->fwrite((char*)&vec[0],size);
	return 1;
}
