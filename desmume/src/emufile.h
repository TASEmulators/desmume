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

//don't use emufile for files bigger than 2GB! you have been warned! some day this will be fixed.

#ifndef EMUFILE_H
#define EMUFILE_H

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <string>
#include <stdarg.h>

#include "types.h"

#ifdef HOST_WINDOWS 
#include <io.h>
#else
#include <unistd.h>
#endif

class EMUFILE_MEMORY;

class EMUFILE {
protected:
	bool _failbit;

public:
	EMUFILE()
		: _failbit(false)
	{}


	//returns a new EMUFILE which is guranteed to be in memory. the EMUFILE you call this on may be deleted. use the returned EMUFILE in its place
	virtual EMUFILE* memwrap() = 0;

	virtual ~EMUFILE() {}
	
	static bool readAllBytes(std::vector<u8>* buf, const std::string& fname);

	bool fail(bool unset=false) { bool ret = this->_failbit; if(unset) unfail(); return ret; }
	void unfail() { this->_failbit = false; }

	bool eof() { return size()==ftell(); }

	size_t fread(const void *ptr, size_t bytes){
		return _fread(ptr,bytes);
	}

	void unget() { fseek(-1,SEEK_CUR); }

	//virtuals
public:

	virtual FILE *get_fp() = 0;

	virtual int fprintf(const char *format, ...) = 0;

	virtual int fgetc() = 0;
	virtual int fputc(int c) = 0;

	virtual char* fgets(char* str, int num) = 0;

	virtual size_t _fread(const void *ptr, size_t bytes) = 0;
	virtual size_t fwrite(const void *ptr, size_t bytes) = 0;
	
	size_t write_64LE(s64 s64valueIn);
	size_t write_64LE(u64 u64valueIn);
	size_t read_64LE(s64 &s64valueOut);
	size_t read_64LE(u64 &u64valueOut);
	s64 read_s64LE();
	u64 read_u64LE();
	
	size_t write_32LE(s32 s32valueIn);
	size_t write_32LE(u32 u32valueIn);
	size_t read_32LE(s32 &s32valueOut);
	size_t read_32LE(u32 &u32valueOut);
	s32 read_s32LE();
	u32 read_u32LE();
	
	size_t write_16LE(s16 s16valueIn);
	size_t write_16LE(u16 u16valueIn);
	size_t read_16LE(s16 &s16valueOut);
	size_t read_16LE(u16 &u16valueOut);
	s16 read_s16LE();
	u16 read_u16LE();
	
	size_t write_u8(u8 u8valueIn);
	size_t read_u8(u8 &u8valueOut);
	u8 read_u8();
	
	size_t write_bool32(bool boolValueIn);
	size_t read_bool32(bool &boolValueOut);
	bool read_bool32();
	
	size_t write_bool8(bool boolValueIn);
	size_t read_bool8(bool &boolValueOut);
	bool read_bool8();
	
	size_t write_doubleLE(double doubleValueIn);
	size_t read_doubleLE(double &doubleValueOut);
	double read_doubleLE();
	
	size_t write_floatLE(float floatValueIn);
	size_t read_floatLE(float &floatValueOut);
	float read_floatLE();
	
	size_t write_buffer(std::vector<u8> &vec);
	size_t read_buffer(std::vector<u8> &vec);
	
	size_t write_MemoryStream(EMUFILE_MEMORY &ms);
	size_t read_MemoryStream(EMUFILE_MEMORY &ms);
	
	virtual int fseek(int offset, int origin) = 0;

	virtual int ftell() = 0;
	virtual int size() = 0;
	virtual void fflush() = 0;

	virtual void truncate(s32 length) = 0;
};

//todo - handle read-only specially?
class EMUFILE_MEMORY : public EMUFILE { 
protected:
	std::vector<u8> *vec;
	bool ownvec;
	s32 pos, len;

	void reserve(u32 amt) {
		if(vec->size() < amt)
			vec->resize(amt);
	}

public:

	EMUFILE_MEMORY(std::vector<u8> *underlying) : vec(underlying), ownvec(false), pos(0), len((s32)underlying->size()) { }
	EMUFILE_MEMORY(u32 preallocate) : vec(new std::vector<u8>()), ownvec(true), pos(0), len(0) { 
		vec->resize(preallocate);
		len = preallocate;
	}
	EMUFILE_MEMORY() : vec(new std::vector<u8>()), ownvec(true), pos(0), len(0) { vec->reserve(1024); }
	EMUFILE_MEMORY(void* buf, s32 size) : vec(new std::vector<u8>()), ownvec(true), pos(0), len(size) { 
		vec->resize(size);
		if(size != 0)
			memcpy(&vec->front(),buf,size);
	}

	~EMUFILE_MEMORY() {
		if(ownvec) delete vec;
	}

	virtual EMUFILE* memwrap();

	virtual void truncate(s32 length)
	{
		vec->resize(length);
		len = length;
		if(pos>length) pos=length;
	}

	u8* buf() { 
		if(size()==0) reserve(1);
		return &(*vec)[0];
	}

	std::vector<u8>* get_vec() const { return vec; };

	virtual FILE *get_fp() { return NULL; }

	virtual int fprintf(const char *format, ...) {
		va_list argptr;
		va_start(argptr, format);

		//we dont generate straight into the buffer because it will null terminate (one more byte than we want)
		int amt = vsnprintf(0,0,format,argptr);
		char* tempbuf = new char[amt+1];

		va_end(argptr);
		va_start(argptr, format);
		vsprintf(tempbuf,format,argptr);
		
		fwrite(tempbuf,amt);
		delete[] tempbuf;

		va_end(argptr);
		return amt;
	};

	virtual int fgetc() {
		u8 temp;

		//need an optimized codepath
		//if(_fread(&temp,1) != 1)
		//	return EOF;
		//else return temp;
		u32 remain = len-pos;
		if(remain<1) {
			this->_failbit = true;
			return -1;
		}
		temp = buf()[pos];
		pos++;
		return temp;
	}
	virtual int fputc(int c) {
		u8 temp = (u8)c;
		//TODO
		//if(fwrite(&temp,1)!=1) return EOF;
		fwrite(&temp,1);

		return 0;
	}

	virtual char* fgets(char* str, int num)
	{
		throw "Not tested: emufile memory fgets";
	}

	virtual size_t _fread(const void *ptr, size_t bytes);
	virtual size_t fwrite(const void *ptr, size_t bytes){
		reserve(pos+(s32)bytes);
		memcpy(buf()+pos,ptr,bytes);
		pos += (s32)bytes;
		len = std::max(pos,len);
		
		return bytes;
	}

	virtual int fseek(int offset, int origin){ 
		//work differently for read-only...?
		switch(origin) {
			case SEEK_SET:
				pos = offset;
				break;
			case SEEK_CUR:
				pos += offset;
				break;
			case SEEK_END:
				pos = size()+offset;
				break;
			default:
				assert(false);
		}
		reserve(pos);
		return 0;
	}

	virtual int ftell() {
		return pos;
	}

	virtual void fflush() {}

	void trim()
	{
		vec->resize(len);
	}

	virtual int size() { return (int)len; }
};

class EMUFILE_FILE : public EMUFILE { 
protected:
	FILE* _fp;
	std::string _fname;
	char _mode[16];
	long _mFilePosition;
	bool _mPositionCacheEnabled;
	
	enum eCondition
	{
		eCondition_Clean,
		eCondition_Unknown,
		eCondition_Read,
		eCondition_Write
	} _mCondition;

private:
	void __open(const char* fname, const char* mode);

public:

	EMUFILE_FILE(const std::string& fname, const char* mode) { __open(fname.c_str(),mode); }
	EMUFILE_FILE(const char* fname, const char* mode) { __open(fname,mode); }

	void EnablePositionCache();

	virtual ~EMUFILE_FILE() {
		if (NULL != this->_fp)
			fclose(this->_fp);
	}

	virtual FILE *get_fp() {
		return this->_fp;
	}

	virtual EMUFILE* memwrap();

	bool is_open() { return this->_fp != NULL; }

	void DemandCondition(eCondition cond);

	virtual void truncate(s32 length);

	virtual int fprintf(const char *format, ...) {
		va_list argptr;
		va_start(argptr, format);
		int ret = ::vfprintf(this->_fp, format, argptr);
		va_end(argptr);
		return ret;
	};

	virtual int fgetc() {
		return ::fgetc(this->_fp);
	}
	virtual int fputc(int c) {
		return ::fputc(c, this->_fp);
	}

	virtual char* fgets(char* str, int num) {
		return ::fgets(str, num, this->_fp);
	}

	virtual size_t _fread(const void *ptr, size_t bytes);
	virtual size_t fwrite(const void *ptr, size_t bytes);

	virtual int fseek(int offset, int origin);

	virtual int ftell();

	virtual int size() { 
		int oldpos = ftell();
		fseek(0,SEEK_END);
		int len = ftell();
		fseek(oldpos,SEEK_SET);
		return len;
	}

	virtual void fflush() {
		::fflush(this->_fp);
	}

};

#endif
