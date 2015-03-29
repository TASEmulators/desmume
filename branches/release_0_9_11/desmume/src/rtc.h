/*
	Copyright 2006 yopyop
	Copyright 2008 CrazyMax
	Copyright 2008-2010 DeSmuME team

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

#ifndef _RTC_H_
#define _RTC_H_
#include <stdlib.h>
#include <time.h>
#include "types.h"
#include "utils/datetime.h"

DateTime rtcGetTime(void);

extern	void rtcInit();
extern	u16 rtcRead();
extern	void rtcWrite(u16 val);
#endif
