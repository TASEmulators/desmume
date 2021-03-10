/*
	Copyright (C) 2021 LRFLEW
	Copyright (C) 2021 DeSmuME team

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

#include <cmath>
#include <cstring>

#include "../debug.h"

/* 2^15 / Pi */
#define ANGLE_TO_SHORT 10430.37835047f
/* (float) 0x1000 */
#define FLOAT_TO_FIXED 4096.0f

static u16 state[4];

class Slot2_Analog : public ISlot2Interface {
public:
	Slot2Info const* info() override {
		static Slot2InfoSimple info("Analog Stick", "Analog Stick input for Modded SM64DS", 0x09);
		return &info;
	}

	void connect() override {
		std::memset(state, 0, sizeof(state));
	}

	u8 readByte(u8 PROCNUM, u32 addr) override {
		if ((addr & 0xFFFFFFF8) != 0x09000000)
			return 0xFF;
		else if ((addr & 1) == 0)
			return (u8) state[(addr & 6) >> 1];
		else
			return (u8) (state[(addr & 6) >> 1] >> 8);
	}

	u16 readWord(u8 PROCNUM, u32 addr) override {
		if ((addr & 0xFFFFFFF8) != 0x09000000)
			return 0xFFFF;
		return state[(addr & 6) >> 1];
	}

	u32 readLong(u8 PROCNUM, u32 addr) override {
		if ((addr & 0xFFFFFFF8) != 0x09000000)
			return 0xFFFFFFFF;
		int i = (addr & 4) >> 1;
		return ((u32) state[i]) | ((u32) state[i + 1] << 16);
	}
};

ISlot2Interface* construct_Slot2_Analog() { return new Slot2_Analog(); }

void analog_setValue(float x, float y) {
	float mag = std::hypot(x, y);
	float ang = std::atan2(x, y);
	if (mag > 1.0f) {
		x /= mag;
		y /= mag;
		mag = 1.0f;
	}

	state[0] = static_cast<u16>(std::lround(mag * FLOAT_TO_FIXED));
	state[1] = static_cast<u16>(std::lround(  x * FLOAT_TO_FIXED));
	state[2] = static_cast<u16>(std::lround(  y * FLOAT_TO_FIXED));
	state[3] = static_cast<u16>(std::lround(ang * ANGLE_TO_SHORT));
}
