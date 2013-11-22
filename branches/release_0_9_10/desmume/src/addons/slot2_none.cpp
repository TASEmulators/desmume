/*
	Copyright (C) 2009 CrazyMax
	Copyright (C) 2009-2013 DeSmuME team

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

#include "../slot2.h"

class Slot2_None : public ISlot2Interface
{
public:
	virtual Slot2Info const* info()
	{
		static Slot2InfoSimple info("None", "Slot2 no-device emulation", 0xFF);
		return &info;
	}
};

ISlot2Interface* construct_Slot2_None() { return new Slot2_None(); }
