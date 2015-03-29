#ifndef _LIBFAT_PUBLIC_API_H_
#define _LIBFAT_PUBLIC_API_H_

namespace LIBFAT
{
	void Init(void* buffer, int size_bytes);
	void Shutdown();
	bool MkDir(const char *path);
	bool WriteFile(const char *path, const void* data, int len);
};

#endif //_LIBFAT_PUBLIC_API_H_
