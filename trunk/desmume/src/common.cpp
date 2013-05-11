/*
	Copyright (C) 2008-2010 DeSmuME team

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

//TODO - move this into ndssystem where it belongs probably

#include <string.h>
#include <string>
#include "common.h"

char *trim(char *s, int len)
{
	char *ptr = NULL;
	if (!s) return NULL;
	if (!*s) return s;
	
	if(len==-1)
		ptr = s + strlen(s) - 1;
	else ptr = s+len - 1;
	for (; (ptr >= s) && (!*ptr || isspace((u8)*ptr)) ; ptr--);
	ptr[1] = '\0';
	return s;
}

char *removeSpecialChars(char *s)
{
	char	*buf = s;
	if (!s) return NULL;
	if (!*s) return s;

	for (u32 i = 0; i < strlen(s); i++)
	{
		if (isspace((u8)s[i]) && (s[i] != 0x20))
			*buf = 0x20;
		else
			*buf = s[i];
		buf++;
	}
	*buf = 0;
	return s;
}

// ===============================================================================
// Message dialogs
// ===============================================================================
#define MSG_PRINT { \
	va_list args; \
	va_start (args, fmt); \
	vprintf (fmt, args); \
	va_end (args); \
}
void msgFakeInfo(const char *fmt, ...)
{
	MSG_PRINT;
}

bool msgFakeConfirm(const char *fmt, ...)
{
	MSG_PRINT;
	return true;
}

void msgFakeError(const char *fmt, ...)
{
	MSG_PRINT;
}

void msgFakeWarn(const char *fmt, ...)
{
	MSG_PRINT;
}

msgBoxInterface msgBoxFake = {
	msgFakeInfo,
	msgFakeConfirm,
	msgFakeError,
	msgFakeWarn,
};

msgBoxInterface *msgbox = &msgBoxFake;
