#ifndef _READWRITE_H_
#define _READWRITE_H_

#include "types.h"
#include <iostream>

int write16le(u16 b, FILE *fp);
int write32le(u32 b, FILE *fp);
int write32le(u32 b, std::ostream* os);
int write64le(u64 b, std::ostream* os);
int read64le(u64 *Bufo, std::istream *is);
int read32le(u32 *Bufo, std::istream *is);
int read32le(u32 *Bufo, FILE *fp);
int read16le(u16 *Bufo, std::istream *is);

#endif
