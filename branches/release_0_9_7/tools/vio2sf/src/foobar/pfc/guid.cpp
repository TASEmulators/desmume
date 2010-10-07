#include "pfc.h"

namespace pfc {
/*
6B29FC40-CA47-1067-B31D-00DD010662DA
.
typedef struct _GUID {          // size is 16
    DWORD Data1;
    WORD   Data2;
    WORD   Data3;
    BYTE  Data4[8];
} GUID;

// {B296CF59-4D51-466f-8E0B-E57D3F91D908}
static const GUID <<name>> = 
{ 0xb296cf59, 0x4d51, 0x466f, { 0x8e, 0xb, 0xe5, 0x7d, 0x3f, 0x91, 0xd9, 0x8 } };

*/

unsigned GUID_from_text::read_hex(char c)
{
	if (c>='0' && c<='9') return (unsigned)c - '0';
	else if (c>='a' && c<='f') return 0xa + (unsigned)c - 'a';
	else if (c>='A' && c<='F') return 0xa + (unsigned)c - 'A';
	else return 0;
}

unsigned GUID_from_text::read_byte(const char * ptr)
{
	return (read_hex(ptr[0])<<4) | read_hex(ptr[1]);
}
unsigned GUID_from_text::read_word(const char * ptr)
{
	return (read_byte(ptr)<<8) | read_byte(ptr+2);
}

unsigned GUID_from_text::read_dword(const char * ptr)
{
	return (read_word(ptr)<<16) | read_word(ptr+4);
}

void GUID_from_text::read_bytes(BYTE * out,unsigned num,const char * ptr)
{
	for(;num;num--)
	{
		*out = read_byte(ptr);
		out++;ptr+=2;
	}
}


GUID_from_text::GUID_from_text(const char * text)
{
	if (*text=='{') text++;
	const char * max;

	{
		const char * t = strchr(text,'}');
		if (t) max = t;
		else max = text + strlen(text);
	}

	(GUID)*this = pfc::guid_null;

	
	do {
		if (text+8>max) break;
		Data1 = read_dword(text);
		text += 8;
		while(*text=='-') text++;
		if (text+4>max) break;
		Data2 = read_word(text);
		text += 4;
		while(*text=='-') text++;
		if (text+4>max) break;
		Data3 = read_word(text);
		text += 4;
		while(*text=='-') text++;
		if (text+4>max) break;
		read_bytes(Data4,2,text);
		text += 4;
		while(*text=='-') text++;
		if (text+12>max) break;
		read_bytes(Data4+2,6,text);
	} while(false);
}


static inline char print_hex_digit(unsigned val)
{
	static const char table[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
	assert((val & ~0xF) == 0);
	return table[val];
}

static void print_hex(unsigned val,char * &out,unsigned bytes)
{
	unsigned n;
	for(n=0;n<bytes;n++)
	{
		unsigned char c = (unsigned char)((val >> ((bytes - 1 - n) << 3)) & 0xFF);
		*(out++) = print_hex_digit( c >> 4 );
		*(out++) = print_hex_digit( c & 0xF );
	}
	*out = 0;
}


print_guid::print_guid(const GUID & p_guid)
{
	char * out = m_data;
	print_hex(p_guid.Data1,out,4);
	*(out++) = '-';
	print_hex(p_guid.Data2,out,2);
	*(out++) = '-';
	print_hex(p_guid.Data3,out,2);
	*(out++) = '-';
	print_hex(p_guid.Data4[0],out,1);
	print_hex(p_guid.Data4[1],out,1);
	*(out++) = '-';
	print_hex(p_guid.Data4[2],out,1);
	print_hex(p_guid.Data4[3],out,1);
	print_hex(p_guid.Data4[4],out,1);
	print_hex(p_guid.Data4[5],out,1);
	print_hex(p_guid.Data4[6],out,1);
	print_hex(p_guid.Data4[7],out,1);
	*out = 0;
}


void print_hex_raw(const void * buffer,unsigned bytes,char * p_out)
{
	char * out = p_out;
	const unsigned char * in = (const unsigned char *) buffer;
	unsigned n;
	for(n=0;n<bytes;n++)
		print_hex(in[n],out,1);
}

}



const GUID pfc::guid_null = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };