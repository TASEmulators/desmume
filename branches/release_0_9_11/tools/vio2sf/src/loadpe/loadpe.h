/*
	libloadpe
    Copyright (C)2007  Ku-Zu

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifdef __cplusplus
extern "C" {
#endif

void *XLoadLibraryA(const char *lpszFileName);
void *XLoadLibraryW(const WCHAR *lpszFileName);
void XFreeLibrary(void *hModule);
FARPROC XGetProcAddress(void *hModule, const char *lpfn);

#ifdef __cplusplus
}
#endif

