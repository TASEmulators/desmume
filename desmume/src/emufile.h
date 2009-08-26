#ifndef EMUFILE_H
#define EMUFILE_H

class EMUFILE{
private:
	FILE* fp;

public:
	EMUFILE(const char* fname, const char* mode)
	{
		fp = fopen(fname,mode);
	};

	FILE *get_fp(){ 
		return fp; 
	}

	int fprintf(const char *format, ...) {
		return ::fprintf(fp, format);
	};

	int fgetc() {
		return ::fgetc(fp);
	}
	int fputc(int c) {
		return ::fputc(c, fp);
	}

	size_t fread(void *ptr, size_t size, size_t nobj){
		return ::fread(ptr, size, nobj, fp);
	}

	size_t fwrite(void *ptr, size_t size, size_t nobj){
		return ::fwrite(ptr, size, nobj, fp);
	}

	int fseek(long int offset, int origin){ 
		return ::fseek(fp, offset, origin);
	}

	long int ftell() {
		return ::ftell(fp);
	}

};

#endif