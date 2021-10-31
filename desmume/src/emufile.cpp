/*
The MIT License

Copyright (C) 2009-2021 DeSmuME team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "emufile.h"
#include "utils/xstring.h"

#include <vector>

inline u64 double_to_u64(double d)
{
	union
	{
		u64 a;
		double b;
	} fuxor;
	
	fuxor.b = d;
	return fuxor.a;
}

inline double u64_to_double(u64 u)
{
	union
	{
		u64 a;
		double b;
	} fuxor;
	
	fuxor.a = u;
	return fuxor.b;
}

inline u32 float_to_u32(float f)
{
	union
	{
		u32 a;
		float b;
	} fuxor;
	
	fuxor.b = f;
	return fuxor.a;
}

inline float u32_to_float(u32 u)
{
	union
	{
		u32 a;
		float b;
	} fuxor;
	
	fuxor.a = u;
	return fuxor.b;
}

bool EMUFILE::readAllBytes(std::vector<u8>* dstbuf, const std::string& fname)
{
	EMUFILE_FILE file(fname.c_str(),"rb");
	if(file.fail()) return false;
	int size = file.size();
	dstbuf->resize(size);
	file.fread(&dstbuf->at(0),size);
	return true;
}

size_t EMUFILE_MEMORY::_fread(const void *ptr, size_t bytes){
	u32 remain = len-pos;
	u32 todo = std::min<u32>(remain,(u32)bytes);
	if(len==0)
	{
		this->_failbit = true;
		return 0;
	}
	if(todo<=4)
	{
		u8* src = buf()+pos;
		u8* dst = (u8*)ptr;
		for(size_t i=0;i<todo;i++)
			*dst++ = *src++;
	}
	else
	{
		memcpy((void*)ptr,buf()+pos,todo);
	}
	pos += todo;
	if(todo<bytes)
		this->_failbit = true;
	return todo;
}

void EMUFILE_FILE::truncate(s32 length)
{
	::fflush(this->_fp);
	#ifdef HOST_WINDOWS 
		_chsize(_fileno(this->_fp),length);
	#else
		ftruncate(fileno(this->_fp),length);
	#endif
	fclose(this->_fp);
	this->_fp = NULL;
	this->__open(this->_fname.c_str(), this->_mode);
}

int EMUFILE_FILE::fseek(int offset, int origin)
{
	//if the position cache is enabled, and the seek offset matches the known current position, then early exit.
	if (this->_mPositionCacheEnabled)
	{
		if (origin == SEEK_SET)
		{
			if (this->_mFilePosition == offset)
			{
				return 0;
			}
		}
	}

	this->_mCondition = eCondition_Clean;

	int ret = ::fseek(this->_fp, offset, origin);
 
	if (this->_mPositionCacheEnabled)
		this->_mFilePosition = ::ftell(this->_fp);
 
	return ret;
}


int EMUFILE_FILE::ftell()
{
	if (this->_mPositionCacheEnabled)
		return (int)this->_mFilePosition;
	return (u32)::ftell(this->_fp);
}

void EMUFILE_FILE::DemandCondition(eCondition cond)
{
	//allows switching between reading and writing; an fseek is required, under the circumstances

	if( this->_mCondition == eCondition_Clean)
		goto CONCLUDE;
	if (this->_mCondition == eCondition_Unknown)
		goto RESET;
	if (this->_mCondition != cond)
		goto RESET;

	return;

RESET:
	::fseek(this->_fp,::ftell(this->_fp),SEEK_SET);
CONCLUDE:
	this->_mCondition = cond;
}

size_t EMUFILE_FILE::_fread(const void *ptr, size_t bytes)
{
	DemandCondition(eCondition_Read);
	size_t ret = ::fread((void*)ptr, 1, bytes, this->_fp);
	this->_mFilePosition += ret;
	if (ret < bytes)
		this->_failbit = true;
	
	return ret;
}

void EMUFILE_FILE::EnablePositionCache()
{
	this->_mPositionCacheEnabled = true;
	this->_mFilePosition = ::ftell(this->_fp);
}

size_t EMUFILE_FILE::fwrite(const void *ptr, size_t bytes)
{
	DemandCondition(eCondition_Write);
	size_t ret = ::fwrite((void*)ptr, 1, bytes, this->_fp);
	this->_mFilePosition += ret;
	if (ret < bytes)
		this->_failbit = true;
	
	return ret;
}


EMUFILE* EMUFILE_FILE::memwrap()
{
	EMUFILE_MEMORY* mem = new EMUFILE_MEMORY(size());
	if(size()==0) return mem;
	fread(mem->buf(),size());
	return mem;
}

EMUFILE* EMUFILE_MEMORY::memwrap()
{
	return this;
}

size_t EMUFILE::write_64LE(s64 s64valueIn)
{
	return write_64LE(*(u64 *)&s64valueIn);
}

size_t EMUFILE::write_64LE(u64 u64valueIn)
{
	u64valueIn = LOCAL_TO_LE_64(u64valueIn);
	fwrite(&u64valueIn,8);
	return 8;
}

size_t EMUFILE::read_64LE(s64 &s64valueOut)
{
	return read_64LE(*(u64 *)&s64valueOut);
}

size_t EMUFILE::read_64LE(u64 &u64valueOut)
{
	u64 temp = 0;
	if (fread(&temp,8) != 8)
		return 0;
	
	u64valueOut = LE_TO_LOCAL_64(temp);
	
	return 1;
}

s64 EMUFILE::read_s64LE()
{
	s64 value = 0;
	read_64LE(value);
	return value;
}

u64 EMUFILE::read_u64LE()
{
	u64 value = 0;
	read_64LE(value);
	return value;
}

size_t EMUFILE::write_32LE(s32 s32valueIn)
{
	return write_32LE(*(u32 *)&s32valueIn);
}

size_t EMUFILE::write_32LE(u32 u32valueIn)
{
	u32valueIn = LOCAL_TO_LE_32(u32valueIn);
	fwrite(&u32valueIn,4);
	return 4;
}

size_t EMUFILE::read_32LE(s32 &s32valueOut)
{
	return read_32LE(*(u32 *)&s32valueOut);
}

size_t EMUFILE::read_32LE(u32 &u32valueOut)
{
	u32 temp = 0;
	if (fread(&temp,4) != 4)
		return 0;
	
	u32valueOut = LE_TO_LOCAL_32(temp);
	
	return 1;
}

s32 EMUFILE::read_s32LE()
{
	s32 value = 0;
	read_32LE(value);
	return value;
}

u32 EMUFILE::read_u32LE()
{
	u32 value = 0;
	read_32LE(value);
	return value;
}

size_t EMUFILE::write_16LE(s16 s16valueIn)
{
	return write_16LE(*(u16 *)&s16valueIn);
}

size_t EMUFILE::write_16LE(u16 u16valueIn)
{
	u16valueIn = LOCAL_TO_LE_16(u16valueIn);
	fwrite(&u16valueIn,2);
	return 2;
}

size_t EMUFILE::read_16LE(s16 &s16valueOut)
{
	return read_16LE(*(u16 *)&s16valueOut);
}

size_t EMUFILE::read_16LE(u16 &u16valueOut)
{
	u32 temp = 0;
	if (fread(&temp,2) != 2)
		return 0;
	
	u16valueOut = LE_TO_LOCAL_16(temp);
	
	return 1;
}

s16 EMUFILE::read_s16LE()
{
	s16 value = 0;
	read_16LE(value);
	return value;
}

u16 EMUFILE::read_u16LE()
{
	u16 value = 0;
	read_16LE(value);
	return value;
}

size_t EMUFILE::write_u8(u8 u8valueIn)
{
	fwrite(&u8valueIn,1);
	return 1;
}

size_t EMUFILE::read_u8(u8 &u8valueOut)
{
	return fread(&u8valueOut,1);
}

u8 EMUFILE::read_u8()
{
	u8 value = 0;
	fread(&value,1);
	return value;
}

size_t EMUFILE::write_doubleLE(double doubleValueIn)
{
	return write_64LE(double_to_u64(doubleValueIn));
}

size_t EMUFILE::read_doubleLE(double &doubleValueOut)
{
	u64 temp = 0;
	size_t ret = read_64LE(temp);
	doubleValueOut = u64_to_double(temp);
	return ret;
}

double EMUFILE::read_doubleLE()
{
	double value = 0.0;
	read_doubleLE(value);
	return value;
}

size_t EMUFILE::write_floatLE(float floatValueIn)
{
	return write_32LE(float_to_u32(floatValueIn));
}

size_t EMUFILE::read_floatLE(float &floatValueOut)
{
	u32 temp = 0;
	size_t ret = read_32LE(temp);
	floatValueOut = u32_to_float(temp);
	return ret;
}

float EMUFILE::read_floatLE()
{
	float value = 0.0f;
	read_floatLE(value);
	return value;
}

size_t EMUFILE::write_bool32(bool boolValueIn)
{
	return write_32LE((boolValueIn) ? 1 : 0);
}

size_t EMUFILE::read_bool32(bool &boolValueOut)
{
	u32 temp = 0;
	size_t ret = read_32LE(temp);
	
	if (ret != 0)
		boolValueOut = (temp != 0);
	
	return ret;
}

bool EMUFILE::read_bool32()
{
	bool value = false;
	read_bool32(value);
	return value;
}

size_t EMUFILE::write_bool8(bool boolValueIn)
{
	return write_u8((boolValueIn) ? 1 : 0);
}

size_t EMUFILE::read_bool8(bool &boolValueOut)
{
	u8 temp = 0;
	size_t ret = read_u8(temp);
	
	if (ret != 0)
		boolValueOut = (temp != 0);
	
	return ret;
}

bool EMUFILE::read_bool8()
{
	bool value = false;
	read_bool8(value);
	return value;
}

size_t EMUFILE::write_buffer(std::vector<u8> &vec)
{
	u32 size = (u32)vec.size();
	write_32LE(size);
	
	if (size > 0)
		fwrite(&vec[0],size);
	
	return (size + 4);
}

size_t EMUFILE::read_buffer(std::vector<u8> &vec)
{
	u32 size = 0;
	
	if (read_32LE(size) != 1)
		return 0;
	
	vec.resize(size);
	
	if (size > 0)
	{
		size_t ret = fread(&vec[0],size);
		
		if (ret != size)
			return 0;
	}
	
	return 1;
}

size_t EMUFILE::write_MemoryStream(EMUFILE_MEMORY &ms)
{
	u32 size = (u32)ms.size();
	write_32LE(size);
	
	if (size > 0)
	{
		std::vector<u8> *vec = ms.get_vec();
		fwrite(&vec->at(0),size);
	}
	
	return (size + 4);
}

size_t EMUFILE::read_MemoryStream(EMUFILE_MEMORY &ms)
{
	u32 size = 0;
	
	if (read_32LE(size) != 1)
		return 0;
	
	if (size > 0)
	{
		std::vector<u8> vec(size);
		size_t ret = fread(&vec[0],size);
		
		if (ret != size)
			return 0;
		
		ms.fwrite(&vec[0],size);
	}
	
	return 1;
}

void EMUFILE_FILE::__open(const char* fname, const char* mode)
{
	this->_mPositionCacheEnabled = false;
	this->_mCondition = eCondition_Clean;
	this->_mFilePosition = 0;
	
	#ifdef HOST_WINDOWS
	auto tmp = mbstowcs((std::string)fname);
	this->_fp = _wfopen(tmp.c_str(),mbstowcs(mode).c_str());
	#else
	this->_fp = fopen(fname, mode);
	#endif
	
	if (this->_fp == NULL)
		this->_failbit = true;
	
	this->_fname = fname;
	strcpy(this->_mode, mode);
}
