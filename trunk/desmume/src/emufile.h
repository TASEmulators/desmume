 /* Copyright (C) 2009-2010 DeSmuME team

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

#ifdef _MSC_VER
#include <io.h>
#endif

class EMUFILE {
protected:
	bool failbit;

public:
	EMUFILE()
		: failbit(false)
	{}


	//takes control of the provided EMUFILE and returns a new EMUFILE which is guranteed to be in memory
	static EMUFILE* memwrap(EMUFILE* fp);

	virtual ~EMUFILE() {}
	
	static bool readAllBytes(std::vector<u8>* buf, const std::string& fname);

	bool fail(bool unset=false) { bool ret = failbit; if(unset) unfail(); return ret; }
	void unfail() { failbit=false; }

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

	virtual size_t _fread(const void *ptr, size_t bytes) = 0;

	//removing these return values for now so we can find any code that might be using them and make sure
	//they handle the return values correctly

	virtual void fwrite(const void *ptr, size_t bytes) = 0;

	virtual int fseek(int offset, int origin) = 0;

	virtual int ftell() = 0;
	virtual int size() = 0;

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
			memcpy(&vec[0],buf,size);
	}

	~EMUFILE_MEMORY() {
		if(ownvec) delete vec;
	}

	virtual void truncate(s32 length)
	{
		vec->resize(length);
		len = length;
		if(pos>length) pos=length;
	}

	u8* buf() { return &(*vec)[0]; }

	std::vector<u8>* get_vec() { return vec; };

	virtual FILE *get_fp() { return NULL; }

	virtual int fprintf(const char *format, ...) {
		va_list argptr;
		va_start(argptr, format);
		
		//we dont generate straight into the buffer because it will null terminate (one more byte than we want)
		int amt = vsnprintf(0,0,format,argptr);
		char* tempbuf = new char[amt+1];
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
			failbit = true;
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

	virtual size_t _fread(const void *ptr, size_t bytes);

	//removing these return values for now so we can find any code that might be using them and make sure
	//they handle the return values correctly

	virtual void fwrite(const void *ptr, size_t bytes){
		reserve(pos+(s32)bytes);
		memcpy(buf()+pos,ptr,bytes);
		pos += (s32)bytes;
		len = std::max(pos,len);
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

	void trim()
	{
		vec->resize(len);
	}

	virtual int size() { return (int)len; }
};

class EMUFILE_FILE : public EMUFILE { 
protected:
	FILE* fp;
	std::string fname;
	char mode[16];

private:
	void open(const char* fname, const char* mode)
	{
		fp = fopen(fname,mode);
		if(!fp)
			failbit = true;
		this->fname = fname;
		strcpy(this->mode,mode);
	}

public:

	EMUFILE_FILE(const std::string& fname, const char* mode) { open(fname.c_str(),mode); }
	EMUFILE_FILE(const char* fname, const char* mode) { open(fname,mode); }

	virtual ~EMUFILE_FILE() {
		if(NULL != fp)
			fclose(fp);
	}

	virtual FILE *get_fp() {
		return fp; 
	}

	bool is_open() { return fp != NULL; }

	virtual void truncate(s32 length);

	virtual int fprintf(const char *format, ...) {
		va_list argptr;
		va_start(argptr, format);
		int ret = ::vfprintf(fp, format, argptr);
		va_end(argptr);
		return ret;
	};

	virtual int fgetc() {
		return ::fgetc(fp);
	}
	virtual int fputc(int c) {
		return ::fputc(c, fp);
	}

	virtual size_t _fread(const void *ptr, size_t bytes){
		size_t ret = ::fread((void*)ptr, 1, bytes, fp);
		if(ret < bytes)
			failbit = true;
		return ret;
	}

	//removing these return values for now so we can find any code that might be using them and make sure
	//they handle the return values correctly

	virtual void fwrite(const void *ptr, size_t bytes){
		size_t ret = ::fwrite((void*)ptr, 1, bytes, fp);
		if(ret < bytes)
			failbit = true;
	}

	virtual int fseek(int offset, int origin){ 
		return ::fseek(fp, offset, origin);
	}

	virtual int ftell() {
		return (u32)::ftell(fp);
	}

	virtual int size() { 
		int oldpos = ftell();
		fseek(0,SEEK_END);
		int len = ftell();
		fseek(oldpos,SEEK_SET);
		return len;
	}

};

#endif
