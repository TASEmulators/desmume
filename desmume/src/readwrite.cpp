#include "readwrite.h"
#include "types.h"

//well. just for the sake of consistency
int write8le(u8 b, FILE *fp)
{
	return((fwrite(&b,1,1,fp)<1)?0:1);
}

//well. just for the sake of consistency
int write8le(u8 b, std::ostream *os)
{
	os->write((char*)&b,1);
	return 1;
}

//well. just for the sake of consistency
int read8le(u8 *Bufo, std::istream *is)
{
	if(is->read((char*)Bufo,1).gcount() != 1)
		return 0;
	return 1;
}

///writes a little endian 16bit value to the specified file
int write16le(u16 b, FILE *fp)
{
	u8 s[2];
	s[0]=b;
	s[1]=b>>8;
	return((fwrite(s,1,2,fp)<2)?0:2);
}


///writes a little endian 16bit value to the specified file
int write16le(u16 b, std::ostream *os)
{
	u8 s[2];
	s[0]=b;
	s[1]=b>>8;
	os->write((char*)&s,2);
	return 2;
}

///writes a little endian 32bit value to the specified file
int write32le(u32 b, FILE *fp)
{
	u8 s[4];
	s[0]=b;
	s[1]=b>>8;
	s[2]=b>>16;
	s[3]=b>>24;
	return((fwrite(s,1,4,fp)<4)?0:4);
}

int write32le(u32 b, std::ostream* os)
{
	u8 s[4];
	s[0]=b;
	s[1]=b>>8;
	s[2]=b>>16;
	s[3]=b>>24;
	os->write((char*)&s,4);
	return 4;
}

int write64le(u64 b, std::ostream* os)
{
	u8 s[8];
	s[0]=b;
	s[1]=b>>8;
	s[2]=b>>16;
	s[3]=b>>24;
	s[4]=b>>32;
	s[5]=b>>40;
	s[6]=b>>48;
	s[7]=b>>56;
	os->write((char*)&s,8);
	return 8;
}


///reads a little endian 32bit value from the specified file
int read32le(u32 *Bufo, FILE *fp)
{
	u32 buf;
	if(fread(&buf,1,4,fp)<4)
		return 0;
#ifdef LOCAL_LE
	*(u32*)Bufo=buf;
#else
	*(u32*)Bufo=((buf&0xFF)<<24)|((buf&0xFF00)<<8)|((buf&0xFF0000)>>8)|((buf&0xFF000000)>>24);
#endif
	return 1;
}

int read16le(u16 *Bufo, std::istream *is)
{
	u16 buf;
	if(is->read((char*)&buf,2).gcount() != 2)
		return 0;
#ifdef LOCAL_LE
	*Bufo=buf;
#else
	*Bufo = LE_TO_LOCAL_16(buf);
#endif
	return 1;
}

///reads a little endian 64bit value from the specified file
int read64le(u64 *Bufo, std::istream *is)
{
	u64 buf;
	if(is->read((char*)&buf,8).gcount() != 8)
		return 0;
#ifdef LOCAL_LE
	*Bufo=buf;
#else
	*Bufo = LE_TO_LOCAL_64(buf);
#endif
	return 1;
}

int read32le(u32 *Bufo, std::istream *is)
{
	u32 buf;
	if(is->read((char*)&buf,4).gcount() != 4)
		return 0;
#ifdef LOCAL_LE
	*(u32*)Bufo=buf;
#else
	*(u32*)Bufo=((buf&0xFF)<<24)|((buf&0xFF00)<<8)|((buf&0xFF0000)>>8)|((buf&0xFF000000)>>24);
#endif
	return 1;
}

