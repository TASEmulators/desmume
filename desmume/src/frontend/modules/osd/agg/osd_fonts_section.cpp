/*
	Copyright (C) 2026 DeSmuME team

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

#include "osd_fonts_section.h"

#include <string.h>
#include <vector>

// ---------------------------------------------------------------------------
// ELF linker symbols. `desmume_osd_fonts_anchor` is defined by the generated
// assembly at the first byte of the section. Referencing it here has two
// purposes:
//   1. it pulls the section object out of a static archive, because a plain
//      __start_/__stop_ reference is not a normal undefined symbol; and
//   2. it proves the linker-created start symbol points at our payload.
// The start/stop declarations are weak so a build that links the provider
// without the section fails clearly at the anchor reference instead of
// silently producing an empty font table.
// ---------------------------------------------------------------------------
#if defined(__ELF__)
extern "C" const unsigned char desmume_osd_fonts_anchor[];
extern "C" const unsigned char __start_desmume_osd_fonts[] __attribute__((weak));
extern "C" const unsigned char __stop_desmume_osd_fonts[] __attribute__((weak));
#endif

namespace
{
	const char *g_error = "not initialized";
	bool g_initialized = false;
	bool g_valid = false;
	std::vector<OSDEmbeddedFont> g_fonts;
	std::vector<char> g_namePool;

	uint16_t ReadU16(const uint8_t *p)
	{
		return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
	}

	uint32_t ReadU32(const uint8_t *p)
	{
		return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
	}

	bool Align4(uint32_t &offset, uint32_t total)
	{
		const uint32_t aligned = (offset + 3u) & ~3u;
		if (aligned < offset || aligned > total) return false;
		offset = aligned;
		return true;
	}

	bool IsNameByte(uint8_t c)
	{
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		       (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
	}

	bool Parse(const uint8_t *start, uint32_t size)
	{
		g_fonts.clear();
		g_namePool.clear();

		if (!start || size < 16)
		{
			g_error = "section is smaller than the 16-byte header";
			return false;
		}
		if (memcmp(start, "DSF1", 4) != 0)
		{
			g_error = "bad section magic";
			return false;
		}
		if (ReadU16(start + 4) != 1)
		{
			g_error = "unsupported container version";
			return false;
		}
		if (ReadU16(start + 6) != 16)
		{
			g_error = "unexpected header size";
			return false;
		}
		if (ReadU32(start + 8) != size)
		{
			g_error = "header totalSize does not match section size";
			return false;
		}
		if (ReadU16(start + 14) != 0)
		{
			g_error = "nonzero reserved header field";
			return false;
		}

		const uint32_t count = ReadU16(start + 12);
		if (count > 256)
		{
			g_error = "implausible font count";
			return false;
		}

		g_fonts.reserve(count);
		g_namePool.reserve(size);

		uint32_t offset = 16;
		for (uint32_t i = 0; i < count; ++i)
		{
			if (offset > size || size - offset < 8)
			{
				g_error = "truncated entry header";
				return false;
			}
			const uint32_t nameLen = ReadU16(start + offset);
			const uint32_t reserved = ReadU16(start + offset + 2);
			const uint32_t dataLen = ReadU32(start + offset + 4);
			offset += 8;

			if (reserved != 0)
			{
				g_error = "nonzero reserved entry field";
				return false;
			}
			if (nameLen == 0 || nameLen > 127)
			{
				g_error = "invalid font name length";
				return false;
			}
			if (dataLen < 4 || dataLen > size)
			{
				g_error = "invalid font data length";
				return false;
			}
			if (offset > size || size - offset < nameLen)
			{
				g_error = "truncated font name";
				return false;
			}
			for (uint32_t n = 0; n < nameLen; ++n)
			{
				if (!IsNameByte(start[offset + n]))
				{
					g_error = "font name contains a non-identifier byte";
					return false;
				}
			}

			const size_t nameOffset = g_namePool.size();
			g_namePool.insert(g_namePool.end(),
			                  (const char *)(start + offset),
			                  (const char *)(start + offset) + nameLen);
			g_namePool.push_back('\0');
			const char *name = &g_namePool[nameOffset];
			offset += nameLen;

			if (!Align4(offset, size))
			{
				g_error = "name padding overruns the section";
				return false;
			}
			if (offset > size || size - offset < dataLen)
			{
				g_error = "truncated font data";
				return false;
			}

			for (size_t n = 0; n < g_fonts.size(); ++n)
			{
				if (strcmp(g_fonts[n].name, name) == 0)
				{
					g_error = "duplicate font name";
					return false;
				}
			}

			const uint8_t *data = start + offset;
			const uint32_t charCount = data[3] ? data[3] : 256;
			if (data[0] == 0 || data[0] > 64 ||
			    charCount == 0 || 4 + charCount * 2 > dataLen)
			{
				g_error = "invalid AGG font header";
				return false;
			}

			OSDEmbeddedFont font;
			font.name = name;
			font.data = data;
			font.size = dataLen;
			g_fonts.push_back(font);

			offset += dataLen;
			if (!Align4(offset, size))
			{
				g_error = "data padding overruns the section";
				return false;
			}
		}

		if (offset != size)
		{
			g_error = "trailing bytes after the last font entry";
			return false;
		}

		g_error = "";
		return true;
	}
}

bool OSDEmbeddedFonts_Init()
{
	if (g_initialized) return g_valid;
	g_initialized = true;
	g_valid = false;
	g_error = "not initialized";

#if defined(__ELF__)
	const uint8_t *start = __start_desmume_osd_fonts;
	const uint8_t *stop = __stop_desmume_osd_fonts;
	if (!start || !stop)
	{
		g_error = "linker did not provide section start/stop symbols";
		return false;
	}
	if (stop <= start)
	{
		g_error = "empty desmume_osd_fonts section";
		return false;
	}
	if (desmume_osd_fonts_anchor != start)
	{
		g_error = "section anchor does not match __start symbol";
		return false;
	}
	const uintptr_t bytes = (uintptr_t)(stop - start);
	if (bytes > 0x7FFFFFFFu)
	{
		g_error = "section size is implausible";
		return false;
	}
	if (!Parse(start, (uint32_t)bytes))
	{
		g_fonts.clear();
		g_namePool.clear();
		return false;
	}
	g_valid = true;
	return true;
#else
	g_error = "custom-section font embedding is ELF-only";
	return false;
#endif
}

#ifdef DESMUME_ELF_FONTS_TEST
bool OSDEmbeddedFonts_ParseSection(const uint8_t *data, uint32_t size)
{
	const bool ok = Parse(data, size);
	if (!ok)
	{
		g_fonts.clear();
		g_namePool.clear();
	}
	return ok;
}
#endif

int OSDEmbeddedFonts_Count()
{
	return (int)g_fonts.size();
}

const OSDEmbeddedFont *OSDEmbeddedFonts_At(int index)
{
	if (index < 0 || (size_t)index >= g_fonts.size()) return NULL;
	return &g_fonts[(size_t)index];
}

const OSDEmbeddedFont *OSDEmbeddedFonts_Find(const char *name)
{
	if (!name) return NULL;
	for (size_t i = 0; i < g_fonts.size(); ++i)
	{
		if (strcmp(g_fonts[i].name, name) == 0) return &g_fonts[i];
	}
	return NULL;
}

const char *OSDEmbeddedFonts_LastError()
{
	return g_error;
}
