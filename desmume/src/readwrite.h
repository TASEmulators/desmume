#ifndef _READWRITE_H_
#define _READWRITE_H_

#include "types.h"
#include <iostream>
#include <cstdio>
#include <vector>

//well. just for the sake of consistency
int write8le(u8 b, FILE *fp);
int write8le(u8 b, std::ostream *os);
int write16le(u16 b, FILE *fp);
int write16le(u16 b, std::ostream* os);
int write32le(u32 b, FILE *fp);
int write32le(u32 b, std::ostream* os);
int write64le(u64 b, std::ostream* os);
int read64le(u64 *Bufo, std::istream *is);
int read32le(u32 *Bufo, std::istream *is);
inline int read32le(int *Bufo, std::istream *is) { return read32le((u32*)Bufo,is); }
int read32le(u32 *Bufo, FILE *fp);
int read16le(u16 *Bufo, std::istream *is);
inline int read16le(s16 *Bufo, std::istream* is) { return read16le((u16*)Bufo,is); }
int read8le(u8 *Bufo, std::istream *is);

int readbuffer(std::vector<u8> &vec, std::istream* is);
int writebuffer(std::vector<u8>& vec, std::ostream* os);

#endif
