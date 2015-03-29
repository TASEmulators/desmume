/* Cheats converter

	Copyright 2009 DeSmuME team

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
// first release: 19/12/2009

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "common.h"
#include "convert.h"

int main()
{
	WIN32_FIND_DATA FindFileData;
	HANDLE	hFind = NULL;
	u32		count = 0;
	char	buf[MAX_PATH] = {0};

	SetConsoleCP(GetACP());
	SetConsoleOutputCP(GetACP());

	printf("DeSmuME cheats file converter\n");
	printf("Copyright (C) 2009 by DeSmuME Team\n");
	printf("Version 0.1\n");
	printf("\n");

	printf("All files (.cht) of deception in a current directory will be renamed\n");
	printf("Are you sure?");
	
	int	ch = 0;
	do
	{
		ch = getch();
		if ((ch == 'N') || (ch == 'n'))
		{
			printf(" no\n\n");
			return 2;
		}
	} while ((ch != 'Y') && (ch != 'y'));
	printf(" yes\n");
	printf("\n");

	hFind = FindFirstFile("*.dct", &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		printf("%s\n", error());
		return 1;
	}

	do
	{
		if (!(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			if (strncmp(FindFileData.cFileName + (strlen(FindFileData.cFileName) - 4), ".dct", 4) != 0)
				continue;

			printf("%s", FindFileData.cFileName);
			sprintf(buf, ".\\%s", FindFileData.cFileName);
			if (convertFile(buf))
			{
				printf(" - Ok\n");
				count++;
			}
			else
				printf(" - Failed\n");
		}

	} while (FindNextFile(hFind, &FindFileData));
	FindClose(hFind);
	printf("\nConverted %i files\n\n", count);
	printf("Done\n\n");

	return 0;
}