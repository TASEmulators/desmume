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

// OSD bitmap fonts embedded in a custom ELF section (Scheme B).
//
// The section is named `desmume_osd_fonts` (a C identifier, so the ELF linker
// synthesizes __start_desmume_osd_fonts / __stop_desmume_osd_fonts). The
// payload is generated from frontend/windows/osd_fonts/fonts/*.bin by
// generate_elf_font_section.py. No libelf and no resource compiler are used.
//
// The public fonts_list[]/lookupFont() contract is untouched: Agg_init() asks
// this provider for each font by name and keeps the AGG arrays as a fallback.

#ifndef _OSD_FONTS_SECTION_H_
#define _OSD_FONTS_SECTION_H_

#include <stdint.h>

struct OSDEmbeddedFont
{
	const char *name;
	const uint8_t *data;
	uint32_t size;
};

// Parses and validates the section. Safe to call repeatedly. Returns false
// (and leaves the font list empty) when the platform is not ELF, the section
// is missing, or the container fails validation.
bool OSDEmbeddedFonts_Init();

// Number of valid entries. Zero before Init() succeeds.
int OSDEmbeddedFonts_Count();

// Entry by index, or NULL. The order is the order in the section, which the
// generator keeps identical to fonts_list[] for deterministic diagnostics.
const OSDEmbeddedFont *OSDEmbeddedFonts_At(int index);

// Name lookup, matching the fonts_list[].name contract exactly.
const OSDEmbeddedFont *OSDEmbeddedFonts_Find(const char *name);

// Static human-readable reason for the last Init() failure.
const char *OSDEmbeddedFonts_LastError();

#ifdef DESMUME_ELF_FONTS_TEST
// Validates an arbitrary container image. Used by the self-test to exercise
// malformed inputs; production callers use OSDEmbeddedFonts_Init().
bool OSDEmbeddedFonts_ParseSection(const uint8_t *data, uint32_t size);
#endif

#endif // _OSD_FONTS_SECTION_H_
