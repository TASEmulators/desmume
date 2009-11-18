//taken from ndstool
//http://devkitpro.svn.sourceforge.net/viewvc/devkitpro/trunk/tools/nds/ndstool/include/crc.h?revision=2447

/* crc.h - this file is part of DeSmuME
 *
 * Copyright (C) 2005-2006 Rafael Vuijk
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
	Cyclic Redundancy Code (CRC) functions
	by Rafael Vuijk (aka DarkFader)
*/

#ifndef __CRC_H
#define __CRC_H

//#include "little.h"		// FixCrc is not yet big endian compatible

/*
 * Data
 */
extern unsigned short ccitt16tab[];
extern unsigned short crc16tab[];
extern unsigned long crc32tab[];

/*
 * Defines
 */
#define CRC_TEMPLATE	template <typename CrcType, CrcType *crcTable>

/*
 * CalcCcitt
 * Does not perform final inversion.
 */
#define CalcCcitt_	CalcCcitt<CrcType, crcTable>
#define CalcCcitt16	CalcCcitt<typeof(*ccitt16tab), ccitt16tab>
CRC_TEMPLATE inline CrcType CalcCcitt(unsigned char *data, unsigned int length, CrcType crc = (CrcType)0)
{
	for (unsigned int i=0; i<length; i++)
	{
		crc = (crc << 8) ^ crcTable[(crc >> 8) ^ data[i]];
	}
	return crc;
}

/*
 * CalcCrc
 * Does not perform final inversion.
 */
#define CalcCrc_	CalcCrc<CrcType, crcTable>
#define CalcCrc16	CalcCrc<typeof(*crc16tab), crc16tab>
#define CalcCrc32	CalcCrc<typeof(*crc32tab), crc32tab>
CRC_TEMPLATE inline CrcType CalcCrc(unsigned char *data, unsigned int length, CrcType crc = (CrcType)~0)
{
	for (unsigned int i=0; i<length; i++)
	{
		crc = (crc >> 8) ^ crcTable[(crc ^ data[i]) & 0xFF];
	}
	return crc;
}

/*
 * FCalcCrc
 * Does not perform final inversion.
 */
#define FCalcCrc_	FCalcCrc<CrcType, crcTable>
#define FCalcCrc16	FCalcCrc<typeof(*crc16tab), crc16tab>
#define FCalcCrc32	FCalcCrc<typeof(*crc32tab), crc32tab>
CRC_TEMPLATE inline CrcType FCalcCrc(FILE *f, unsigned int offset, unsigned int length, CrcType crc = (CrcType)~0)
{
	fseek(f, offset, SEEK_SET);
	for (unsigned int i=0; i<length; i++)
	{
		crc = (crc >> 8) ^ crcTable[(crc ^ fgetc(f)) & 0xFF];
	}
	return crc;
}

/*
 * RevCrc
 * Reverse table lookup.
 */
#define RevCrc_		RevCrc<CrcType, crcTable>
CRC_TEMPLATE inline unsigned char RevCrc(unsigned char x, CrcType *value = 0)
{
	for (int y=0; y<256; y++)
	{
		if ((crcTable[y] >> (8*sizeof(CrcType)-8)) == x)
		{
			if (value) *value = crcTable[y];
			return y;
		}
	}
	return 0;
}

/*
 * FixCrc
 */
#define FixCrc_		FixCrc<CrcType, crcTable>
#define FixCrc16	FixCrc<typeof(*crc16tab), crc16tab>
#define FixCrc32	FixCrc<typeof(*crc32tab), crc32tab>
CRC_TEMPLATE void FixCrc
(
	unsigned char *data,				// data to be patched
	unsigned int patch_offset, unsigned char *patch_data, unsigned int patch_length,	// patch data
	unsigned int fix_offset = 0,		// position to write the fix. by default, it is immediately after the patched data
	CrcType initial_crc = (CrcType)~0	// useful when manually calculating leading data
)
{
	if (!fix_offset) fix_offset = patch_offset + patch_length;

	// calculate CRC after leading data
	initial_crc = CalcCrc_(data, patch_offset, initial_crc);

	// calculate CRC before fix
	unsigned char buf[2*sizeof(CrcType)];
	CrcType crc_before_fix = CalcCrc_(data + patch_offset, fix_offset - patch_offset);
	*(CrcType *)(buf + 0) = crc_before_fix;

	// patch
	memcpy(data + patch_offset, patch_data, patch_length);

	// calculate CRC after unfixed
	CrcType crc_after_unfix = CalcCrc_(data + patch_offset, fix_offset - patch_offset + sizeof(CrcType));
	*(CrcType *)(buf + sizeof(CrcType)) = crc_after_unfix;

	// fix it
	for (int i=sizeof(CrcType); i>=1; i--)
	{
		CrcType value;
		unsigned char index = RevCrc_(buf[i + sizeof(CrcType) - 1], &value);
		*(CrcType *)(buf + i) ^= value;
		buf[i - 1] ^= index;
	}
	memcpy(data + fix_offset, buf, sizeof(CrcType));
}

/*
 * FFixCrc
 */
#define FFixCrc_	FFixCrc<CrcType, crcTable>
#define FFixCrc16	FFixCrc<typeof(*crc16tab), crc16tab>
#define FFixCrc32	FFixCrc<typeof(*crc32tab), crc32tab>
CRC_TEMPLATE void FFixCrc
(
	FILE *f,							// file to be patched
	unsigned int patch_offset, unsigned char *patch_data, unsigned int patch_length,	// patch data
	unsigned int fix_offset = 0,		// position to write the fix. by default, it is immediately after the patched data
	CrcType initial_crc = (CrcType)~0	// useful when manually calculating leading data
)
{
	if (!fix_offset) fix_offset = patch_offset + patch_length;

	// calculate CRC after leading data
	initial_crc = FCalcCrc_(f, 0, patch_offset, initial_crc);

	// calculate CRC before fix
	unsigned char buf[2*sizeof(CrcType)];
	CrcType crc_before_fix = FCalcCrc_(f, patch_offset, fix_offset - patch_offset);
	*(CrcType *)(buf + 0) = crc_before_fix;

	// patch
	fseek(f, patch_offset, SEEK_SET);
	fwrite(patch_data, 1, patch_length, f);

	// calculate CRC after unfixed
	CrcType crc_after_unfix = FCalcCrc_(f, patch_offset, fix_offset - patch_offset + sizeof(CrcType));
	*(CrcType *)(buf + sizeof(CrcType)) = crc_after_unfix;

	// fix it
	for (int i=sizeof(CrcType); i>=1; i--)
	{
		CrcType value=0;
		unsigned char index = RevCrc_(buf[i + sizeof(CrcType) - 1], &value);
		*(CrcType *)(buf + i) ^= value;
		buf[i - 1] ^= index;
	}
	fseek(f, fix_offset, SEEK_SET);
	fwrite(buf, sizeof(CrcType), 1, f);
}

#endif	// __CRC_H
