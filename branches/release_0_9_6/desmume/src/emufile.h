 /* Copyright (C) 2009 DeSmuME team
 *
 * This file is part of DeSmuME
 *
 * DeSmuME is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * DeSmuME is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DeSmuME; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef EMUFILE_H
#define EMUFILE_H

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include <vector>
#include <algorithm>
#include <string>
#include <stdarg.h>

#ifdef _XBOX
#undef min;
#undef max;
#endif

class EMUFILE {
protected:
	bool failbit;

public:
	EMUFILE()
		: failbit(false)
	{}

	virtual ~EMUFILE() {}
	
	static bool readAllBytes(std::vector<u8>* buf, const std::string& fname);

	bool fail() { return failbit; }

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

	EMUFILE_MEMORY(std::vector<u8> *underlying) : vec(underlying), ownvec(false), pos(0), len(underlying->size()) { }
	EMUFILE_MEMORY(u32 preallocate) : vec(new std::vector<u8>()), ownvec(true), pos(0), len(0) { vec->reserve(preallocate); }
	EMUFILE_MEMORY() : vec(new std::vector<u8>()), ownvec(true), pos(0), len(0) { vec->reserve(1024); }

	~EMUFILE_MEMORY() {
		if(ownvec) delete vec;
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
		if(_fread(&temp,1) != 1)
			return EOF;
		else return temp;
	}
	virtual int fputc(int c) {
		u8 temp = (u8)c;
		//TODO
		//if(fwrite(&temp,1)!=1) return EOF;
		fwrite(&temp,1);

		return 0;
	}

	virtual size_t _fread(const void *ptr, size_t bytes){
		u32 remain = len-pos;
		u32 todo = std::min<u32>(remain,(u32)bytes);
		memcpy((void*)ptr,buf()+pos,todo);
		pos += todo;
		if(todo<bytes)
			failbit = true;
		return todo;
	}

	//removing these return values for now so we can find any code that might be using them and make sure
	//they handle the return values correctly

	virtual void fwrite(const void *ptr, size_t bytes){
		reserve(pos+bytes);
		memcpy(buf()+pos,ptr,bytes);
		pos += bytes;
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

	virtual int size() { return (int)len; }
};

class EMUFILE_FILE : public EMUFILE { 
protected:
	FILE* fp;

public:

	EMUFILE_FILE(const char* fname, const char* mode)
	{
		fp = fopen(fname,mode);
		if(!fp)
			failbit = true;
	};

	virtual ~EMUFILE_FILE() {
		if(NULL != fp)
			fclose(fp);
	}

	virtual FILE *get_fp() {
		return fp; 
	}

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
