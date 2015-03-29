/*
	Copyright (C) 2008-2009 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include "guid.h"
#include "../types.h"

void Desmume_Guid::newGuid()
{
	for(int i=0;i<size;i++)
		data[i] = rand();
}

std::string Desmume_Guid::toString()
{
	char buf[37];
	sprintf(buf,"%08X-%04X-%04X-%04X-%02X%02X%02X%02X%02X%02X",
		de32lsb(data),de16lsb(data+4),de16lsb(data+6),de16lsb(data+8),data[10],data[11],data[12],data[13],data[14],data[15]);
	return std::string(buf);
}

Desmume_Guid Desmume_Guid::fromString(std::string str)
{
	Desmume_Guid ret;
	ret.scan(str);
	return ret;
}

uint8 Desmume_Guid::hexToByte(char** ptrptr)
{
	char a = toupper(**ptrptr);
	(*ptrptr)++;
	char b = toupper(**ptrptr);
	(*ptrptr)++;
	if(a>='A') a=a-'A'+10;
	else a-='0';
	if(b>='A') b=b-'A'+10;
	else b-='0';
	return ((unsigned char)a<<4)|(unsigned char)b;
}

void Desmume_Guid::scan(std::string& str)
{
	char* endptr = (char*)str.c_str();
	en32lsb(data,strtoul(endptr,&endptr,16));
	en16lsb(data+4,(u16)strtoul(endptr+1,&endptr,16));
	en16lsb(data+6,(u16)strtoul(endptr+1,&endptr,16));
	en16lsb(data+8,(u16)strtoul(endptr+1,&endptr,16));
	endptr++;
	for(int i=0;i<6;i++)
		data[10+i] = hexToByte(&endptr);
}
