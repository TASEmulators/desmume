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

#ifndef _WINDRIVER_H_
#define _WINDRIVER_H_

#define WIN32_LEAN_AND_MEAN
#include "../common.h"
#include "CWindow.h"

extern WINCLASS	*MainWindow;

class Lock {
public:
	Lock(); // defaults to the critical section around NDS_exec
	Lock(CRITICAL_SECTION& cs);
	~Lock();
private:
	CRITICAL_SECTION* m_cs;
};

#endif