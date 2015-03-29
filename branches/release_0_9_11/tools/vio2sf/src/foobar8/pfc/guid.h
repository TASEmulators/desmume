#ifndef _PFC_GUID_H_
#define _PFC_GUID_H_


class GUID_from_text : public GUID
{
	unsigned read_hex(char c);
	unsigned read_byte(const char * ptr);
	unsigned read_word(const char * ptr);
	unsigned read_dword(const char * ptr);
	void read_bytes(unsigned char * out,unsigned num,const char * ptr);

public:
	GUID_from_text(const char * text);
};

#endif