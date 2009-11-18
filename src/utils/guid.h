#ifndef _guid_h_
#define _guid_h_

#include <string>
#include <cstdio>
#include "../types.h"
#include "valuearray.h"

struct Desmume_Guid : public ValueArray<u8,16>
{
	void newGuid();
	std::string toString();
	static Desmume_Guid fromString(std::string str);
	static uint8 hexToByte(char** ptrptr);
	void scan(std::string& str);
};


#endif
