/*
	Copyright (C) 2006-2026 DeSmuME team

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

#include "ram_debug_core.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../MMU.h"

uint32_t RAMDebugCore::ReadValue(uint32_t address, int sizeInBytes)
{
	if (sizeInBytes != 1 && sizeInBytes != 2 && sizeInBytes != 4)
	{
		return 0;
	}

	// Both old frontends dumped a full word and then narrowed it. Reading four
	// bytes also keeps halfword/byte watches at the end of a region safe.
	u8 buffer[4] = { 0, 0, 0, 0 };
	MMU_DumpMemBlock(ARMCPU_ARM9, (u32)address, 4, buffer);

	const uint32_t word = (uint32_t)buffer[0] |
	                      ((uint32_t)buffer[1] << 8) |
	                      ((uint32_t)buffer[2] << 16) |
	                      ((uint32_t)buffer[3] << 24);

	switch (sizeInBytes)
	{
		case 1: return word & 0x000000FFu;
		case 2: return word & 0x0000FFFFu;
		default: return word;
	}
}

bool RAMDebugCore::WriteValue(uint32_t address, uint32_t value, int sizeInBytes)
{
	switch (sizeInBytes)
	{
		case 1: MMU_write8(ARMCPU_ARM9, (u32)address, (u8)(value & 0xFFu));       return true;
		case 2: MMU_write16(ARMCPU_ARM9, (u32)address, (u16)(value & 0xFFFFu));   return true;
		case 4: MMU_write32(ARMCPU_ARM9, (u32)address, (u32)value);               return true;
		default: return false;
	}
}

uint32_t RAMDebugCore::ReadSoftwareValue(const uint8_t *address, int sizeInBytes, bool byteSwapped)
{
	if (address == 0)
	{
		return 0;
	}

	uint32_t value = 0;
	for (int i = 0; i < sizeInBytes; i++)
	{
		value <<= 8;
		if (byteSwapped)
		{
			value |= *((const uint8_t *)((uintptr_t)(address + i) ^ 1));
		}
		else
		{
			value |= address[i];
		}
	}
	return value;
}

void RAMDebugCore::WriteSoftwareValue(uint8_t *address, uint32_t value, int sizeInBytes, bool byteSwapped)
{
	if (address == 0)
	{
		return;
	}

	for (int i = sizeInBytes - 1; i >= 0; i--)
	{
		if (byteSwapped)
		{
			*((uint8_t *)((uintptr_t)(address + i) ^ 1)) = (uint8_t)(value & 0xFFu);
		}
		else
		{
			address[i] = (uint8_t)(value & 0xFFu);
		}
		value >>= 8;
	}
}

void RAMDebugCore::NormalizeAddress(uint32_t& address)
{
	if ((address & 0xFF000000u) == 0xFF000000u)
	{
		address &= 0x00FFFFFFu;
	}
}

bool RAMDebugCore::IsRAMAddressValid(uint32_t address)
{
	// See the header: Windows accepts everything here and unmapped reads are
	// zero. Keep that behaviour so .wch files remain interchangeable.
	(void)address;
	return true;
}

bool RAMDebugCore::IsSearchedRAMAddress(uint32_t address)
{
	return ((address >= kMainMemAddress) && (address < kMainMemAddress + kMainMemSize)) ||
	       ((address >= kItcmAddress)   && (address < kItcmAddress + kItcmSize)) ||
	       ((address >= kDtcmAddress)   && (address < kDtcmAddress + kDtcmSize));
}

int RAMDebugCore::SizeInBytes(char sizeCode)
{
	switch (sizeCode)
	{
		case 'b': return 1;
		case 'w': return 2;
		case 'd': return 4;
		default:  return 0;
	}
}

char RAMDebugCore::SizeCode(int sizeInBytes)
{
	switch (sizeInBytes)
	{
		case 1: return 'b';
		case 2: return 'w';
		case 4: return 'd';
		default: return 0;
	}
}

bool RAMDebugCore::IsValidSize(char sizeCode)
{
	return SizeInBytes(sizeCode) != 0;
}

bool RAMDebugCore::IsValidType(char typeCode)
{
	return (typeCode == 's') || (typeCode == 'u') || (typeCode == 'h') || (typeCode == 'f');
}

int32_t RAMDebugCore::AsSigned(char sizeCode, uint32_t value)
{
	switch (SizeInBytes(sizeCode))
	{
		case 1: return (int8_t)(value & 0xFFu);
		case 2: return (int16_t)(value & 0xFFFFu);
		default: return (int32_t)value;
	}
}

std::string RAMDebugCore::FormatValue(char sizeCode, char typeCode, uint32_t value)
{
	if (sizeCode == 'S' || typeCode == 'S')
	{
		return std::string();
	}

	char buffer[64];
	buffer[0] = '\0';

	if (typeCode == 'f')
	{
		// 20.12 fixed point (Windows' "IDC_2012" radio button).
		snprintf(buffer, sizeof(buffer), "%f", (double)AsSigned('d', value) / 4096.0);
		return std::string(buffer);
	}

	uint32_t narrowed = value;
	switch (SizeInBytes(sizeCode))
	{
		case 1: narrowed = value & 0xFFu;   break;
		case 2: narrowed = value & 0xFFFFu; break;
		default: break;
	}

	if (typeCode == 's')
	{
		snprintf(buffer, sizeof(buffer), "%d", (int)AsSigned(sizeCode, narrowed));
	}
	else if (typeCode == 'u')
	{
		snprintf(buffer, sizeof(buffer), "%u", (unsigned int)narrowed);
	}
	else
	{
		const char *format = (sizeCode == 'd') ? "%08X" :
		                     (sizeCode == 'w') ? "%04X" : "%02X";
		snprintf(buffer, sizeof(buffer), format, (unsigned int)narrowed);
	}

	return std::string(buffer);
}

std::string RAMDebugCore::FormatAddress(uint32_t address)
{
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%08X", (unsigned int)address);
	return std::string(buffer);
}

uint32_t RAMDebugCore::ParseAddress(const std::string& text, bool *outValid)
{
	if (outValid != 0)
	{
		*outValid = false;
	}

	size_t start = 0;
	while ((start < text.size()) && isspace((unsigned char)text[start]))
	{
		start++;
	}
	// Strip an optional 0x prefix before collecting hex digits, otherwise the
	// leading zero would become part of the address.
	if ((start + 1 < text.size()) && (text[start] == '0') &&
	    (text[start + 1] == 'x' || text[start + 1] == 'X'))
	{
		start += 2;
	}

	std::string cleaned;
	cleaned.reserve(text.size());
	for (size_t i = start; i < text.size(); i++)
	{
		const char c = (char)toupper((unsigned char)text[i]);
		if (isxdigit((unsigned char)c))
		{
			cleaned.push_back(c);
		}
		else if (c == 'O')
		{
			// The Windows edit dialog explicitly repairs a mistyped letter O.
			cleaned.push_back('0');
		}
	}

	if (cleaned.empty())
	{
		return 0;
	}
	if (cleaned.size() > 8)
	{
		cleaned = cleaned.substr(cleaned.size() - 8);
	}

	if (outValid != 0)
	{
		*outValid = true;
	}
	return (uint32_t)strtoul(cleaned.c_str(), 0, 16);
}
