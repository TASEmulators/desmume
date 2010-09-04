#include "emufile.h"

#include <vector>

bool EMUFILE::readAllBytes(std::vector<u8>* dstbuf, const std::string& fname)
{
	EMUFILE_FILE file(fname.c_str(),"rb");
	if(file.fail()) return false;
	int size = file.size();
	dstbuf->resize(size);
	file.fread(&dstbuf->at(0),size);
	return true;
}

EMUFILE* EMUFILE::memwrap(EMUFILE* fp)
{
	EMUFILE_FILE* file;
	EMUFILE_MEMORY* mem;
	file = dynamic_cast<EMUFILE_FILE*>(fp);
	mem = dynamic_cast<EMUFILE_MEMORY*>(fp);
	if(mem) return mem;
	mem = new EMUFILE_MEMORY(file->size());
	if(file->size()==0) return mem;
	file->fread(mem->buf(),file->size());
	delete file;
	return mem;
}

size_t EMUFILE_MEMORY::_fread(const void *ptr, size_t bytes){
	u32 remain = len-pos;
	u32 todo = std::min<u32>(remain,(u32)bytes);
	if(len==0)
	{
		failbit = true;
		return 0;
	}
	if(todo<=4)
	{
		u8* src = buf()+pos;
		u8* dst = (u8*)ptr;
		for(int i=0;i<todo;i++)
			*dst++ = *src++;
	}
	else
	{
		memcpy((void*)ptr,buf()+pos,todo);
	}
	pos += todo;
	if(todo<bytes)
		failbit = true;
	return todo;
}

void EMUFILE_FILE::truncate(s32 length)
{
	fflush(fp);
	#ifdef _MSC_VER
		_chsize(_fileno(fp),length);
	#else
		ftruncate(fileno(fp),length);
	#endif
	fclose(fp);
	fp = NULL;
	open(fname.c_str(),mode);
}