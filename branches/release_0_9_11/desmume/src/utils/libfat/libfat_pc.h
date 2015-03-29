#ifndef _LIBFAT_PC_H
#define _LIBFAT_PC_H

#ifdef LIBFAT_PC

#include "../../types.h"

#ifdef _MSC_VER

#define ENOTSUP 0
#define EOVERFLOW 0

#include <time.h>
#include <stdio.h>

#define strcasecmp(x,y) _stricmp(x,y)
#define strncasecmp(x, y, l) strnicmp(x, y, l)

typedef unsigned __int64 uint64_t;
typedef __int64 int64_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned char uint8_t;
typedef char int8_t;

typedef int32_t ssize_t;

#include <sys/stat.h>

#define S_IRUSR S_IREAD
#define S_IRGRP S_IREAD
#define S_IROTH S_IREAD
#define S_IWUSR S_IWRITE
#define S_IWGRP S_IWRITE
#define S_IWOTH S_IWRITE

//struct stat {
//	u32 st_dev;
//	u32 st_ino;
//	u32 st_mode;
//	u32 st_nlink;
//	u32 st_uid;
//	u32 st_gid;
//	u32 st_rdev;
//	s64 st_size;
//	time_t st_atime;
//	time_t st_mtime;
//	time_t st_ctime;
//	s32 st_blksize;
//	s32 st_blocks;
//	u32 st_attr;
//}; 

#else // (!_MSC_VER)

#include <unistd.h>

#endif //_MSC_VER

struct _reent {
	intptr_t _errno;
};

#ifdef __APPLE__
typedef __darwin_mode_t mode_t;
#elif defined(_MSC_VER)
typedef uint32_t mode_t;
#else
#include <sys/types.h>
#endif

struct DIR_ITER {
	void* dirStruct;
};

struct devoptab_t {
	const char *name;
	int	structSize;
	intptr_t (*open_r)(struct _reent *r, void *fileStruct, const char *path, int flags, int mode);
	intptr_t (*close_r)(struct _reent *r, intptr_t fd);
	ssize_t (*write_r)(struct _reent *r, intptr_t fd, const char *ptr, size_t len);
	ssize_t (*read_r)(struct _reent *r, intptr_t fd, char *ptr, size_t len);
	off_t (*seek_r)(struct _reent *r, intptr_t fd, off_t pos, int dir);
	int (*fstat_r)(struct _reent *r, intptr_t fd, struct stat *st);
	int (*stat_r)(struct _reent *r, const char *file, struct stat *st);
	int (*link_r)(struct _reent *r, const char *existing, const char  *newLink);
	int (*unlink_r)(struct _reent *r, const char *name);
	int (*chdir_r)(struct _reent *r, const char *name);
	int (*rename_r) (struct _reent *r, const char *oldName, const char *newName);
	int (*mkdir_r) (struct _reent *r, const char *path, int mode);
		
	int dirStateSize;
	
	DIR_ITER* (*diropen_r)(struct _reent *r, DIR_ITER *dirState, const char *path);
	int (*dirreset_r)(struct _reent *r, DIR_ITER *dirState);
	int (*dirnext_r)(struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat);
	int (*dirclose_r)(struct _reent *r, DIR_ITER *dirState);
	#ifndef LIBFAT_PC
	int (*statvfs_r)(struct _reent *r, const char *path, struct statvfs *buf);
	#endif
	int (*ftruncate_r)(struct _reent *r, intptr_t fd, off_t len);
	int (*fsync_r)(struct _reent *r, intptr_t fd);

	void *deviceData;

	int (*chmod_r)(struct _reent *r, const char *path, mode_t mode);
	int (*fchmod_r)(struct _reent *r, int fd, mode_t mode);

};


devoptab_t* GetDeviceOpTab(const char* name);

#define _ATTR_WEAK_ 

#endif //LIBFAT_PC

#endif //_LIBFAT_PC_H
