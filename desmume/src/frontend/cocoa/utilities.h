/*
	Copyright (C) 2013-2017 DeSmuME team

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

#ifndef _COCOA_PORT_UTILITIES_
#define _COCOA_PORT_UTILITIES_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

bool IsOSXVersionSupported(const unsigned int major, const unsigned int minor, const unsigned int revision);
bool IsOSXVersion(const unsigned int major, const unsigned int minor, const unsigned int revision);
uint32_t GetNearestPositivePOT(uint32_t value);

#ifdef __cplusplus
}
#endif

#endif // _COCOA_PORT_UTILITIES_
