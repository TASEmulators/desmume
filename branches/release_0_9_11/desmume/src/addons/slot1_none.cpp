/*
	Copyright (C) 2010-2015 DeSmuME team

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

#include "../slot1.h"

class Slot1_None : public ISlot1Interface
{
public:
	virtual Slot1Info const* info()
	{
		static Slot1InfoSimple info("None","Slot1 no-card emulation", 0xFF);
		return &info;
	}

	//pretty much every access to the card should just be ignored and reading HIGH-Z off the GC bus.
	//so, nothing really to do here
	//(notably, it results in a 0xFFFFFFFF card ID)

};

ISlot1Interface* construct_Slot1_None() { return new Slot1_None(); }