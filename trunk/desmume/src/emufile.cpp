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