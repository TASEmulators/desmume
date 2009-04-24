/*
  CFLASH.H
  Mic, 2006
*/

#ifndef __CFLASH_H__
#define __CFLASH_H__

#include "common.h"

#ifndef EXPERIMENTAL_GBASLOT
#include "fat.h"

typedef struct {
	int level,parent,filesInDir;
} FILE_INFO;


BOOL cflash_init( const char *disk_image_filename);

unsigned int cflash_read(unsigned int address);

void cflash_write(unsigned int address,unsigned int data);

void cflash_close( void);
#endif

#endif
