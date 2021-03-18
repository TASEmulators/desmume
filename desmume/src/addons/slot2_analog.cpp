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
/* Open-Bus Value (mask) */
#define OPEN_BUS 0xAFFF

static u16 analog_x, analog_y, analog_magnitude, analog_angle;

class Slot2_Analog : public ISlot2Interface {
public:
	Slot2Info const* info() override {
		static Slot2InfoSimple info("Analog Stick", "Analog Stick Input", 0x09);
		return &info;
	}

	void connect() override {
		analog_x = analog_y = 0;
		analog_magnitude = analog_angle = 0;
	}

	u16 readWord(u8 PROCNUM, u32 addr) override {
		if ((addr & 0xFF000000) != 0x09000000) return OPEN_BUS;
		switch ((addr >> 8) & 0xFFFF) {
		case 0: // Generic
			switch (addr & 0xFF) {
			// 0x00 and 0x01 are reserved to match the output
			// of any physical device made for this purpose.
			// 0x02 through 0x07 are reserved for future DeSmuME use
			case 0x08: return analog_x;
			case 0x0A: return analog_y;
			default: return OPEN_BUS;
			}

		case 1: // AM64DS
			// Matches the layout of values used in SM64DS
			switch (addr & 0xFF) {
			case 0x00: return analog_magnitude;
			case 0x02: return analog_x;
			case 0x04: return analog_y;
			case 0x06: return analog_angle;
			default: return OPEN_BUS;
			}

		default:
			// Available for future game-specific interfaces
			return OPEN_BUS;
		}
	}

	u8 readByte(u8 PROCNUM, u32 addr) override {
		return (readWord(PROCNUM, addr & ~((u32) 1)) >> ((addr & 1) * 8)) & 0xFF;
	}

	u32 readLong(u8 PROCNUM, u32 addr) override {
		return (u32) readWord(PROCNUM, addr) | ((u32) readWord(PROCNUM, addr | 2) << 16);
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

	analog_x = static_cast<u16>(std::lround(x * FLOAT_TO_FIXED));
	analog_y = static_cast<u16>(std::lround(y * FLOAT_TO_FIXED));
	analog_magnitude = static_cast<u16>(std::lround(mag * FLOAT_TO_FIXED));
	analog_angle = static_cast<u16>(std::lround(ang * ANGLE_TO_SHORT));
}
