//taken from ndstool
//http://devkitpro.svn.sourceforge.net/viewvc/devkitpro/trunk/tools/nds/ndstool/source/header.cpp?revision=3063

/* header.cpp - this file is part of DeSmuME
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

#include "header.h"

//#include "ndstool.h"
//#include "banner.h"
//#include "sha1.h"
//#include "crc.h"
//#include "bigint.h"
//#include "arm7_sha1_homebrew.h"
//#include "arm7_sha1_nintendo.h"
//#include "encryption.h"
//
///*
// * Data
// */
//unsigned char publicKeyNintendo[] =
//{
//	0x9E, 0xC1, 0xCC, 0xC0, 0x4A, 0x6B, 0xD0, 0xA0, 0x6D, 0x62, 0xED, 0x5F, 0x15, 0x67, 0x87, 0x12,
//	0xE6, 0xF4, 0x77, 0x1F, 0xD8, 0x5C, 0x81, 0xCE, 0x0C, 0xD0, 0x22, 0x31, 0xF5, 0x89, 0x08, 0xF5,
//	0xBE, 0x04, 0xCB, 0xC1, 0x4F, 0x63, 0xD9, 0x5A, 0x98, 0xFF, 0xEB, 0x36, 0x0F, 0x9C, 0x5D, 0xAD,
//	0x15, 0xB9, 0x99, 0xFB, 0xC6, 0x86, 0x2C, 0x0A, 0x0C, 0xFC, 0xE6, 0x86, 0x03, 0x60, 0xD4, 0x87,
//	0x28, 0xD5, 0x66, 0x42, 0x9C, 0xF7, 0x04, 0x14, 0x4E, 0x6F, 0x73, 0x20, 0xC3, 0x3E, 0x3F, 0xF5,
//	0x82, 0x2E, 0x78, 0x18, 0xD6, 0xCD, 0xD5, 0xC2, 0xDC, 0xAA, 0x1D, 0x34, 0x91, 0xEC, 0x99, 0xC9,
//	0xF7, 0xBF, 0xBF, 0xA0, 0x0E, 0x1E, 0xF0, 0x25, 0xF8, 0x66, 0x17, 0x54, 0x34, 0x28, 0x2D, 0x28,
//	0xA3, 0xAE, 0xF0, 0xA9, 0xFA, 0x3A, 0x70, 0x56, 0xD2, 0x34, 0xA9, 0xC5, 0x9E, 0x5D, 0xF5, 0xE1,
//};
//
///*
// * CalcHeaderCRC
// */
//unsigned short CalcHeaderCRC(Header &header)
//{
//	return CalcCrc16((unsigned char *)&header, 0x15E);
//}
//
///*
// * CalcLogoCRC
// */
//unsigned short CalcLogoCRC(Header &header)
//{
//	return CalcCrc16((unsigned char *)&header + 0xC0, 156);
//}
//
/*
 * DetectRomType
 */
int DetectRomType(const Header& header, char* secure)
{
	unsigned int * data = (unsigned int*)(secure);

	//this is attempting to check for an utterly invalid nds header
	if(header.unitcode < 0 && header.unitcode > 3) return ROMTYPE_INVALID;

	if (header.arm9_rom_offset < 0x4000) return ROMTYPE_HOMEBREW;
	if (data[0] == 0x00000000 && data[1] == 0x00000000) return ROMTYPE_MULTIBOOT;
	if (data[0] == 0xE7FFDEFF && data[1] == 0xE7FFDEFF) return ROMTYPE_NDSDUMPED;
	//TODO
	//for (int i=0x200; i<0x4000; i++)
	//	if (romdata[i]) return ROMTYPE_MASKROM;	// found something odd ;)
	return ROMTYPE_ENCRSECURE;
}

///*
// * CalcSecureAreaCRC
// */
//unsigned short CalcSecureAreaCRC(bool encrypt)
//{
//	fseek(fNDS, 0x4000, SEEK_SET);
//	unsigned char data[0x4000];
//	fread(data, 1, 0x4000, fNDS);
//	if (encrypt) encrypt_arm9(*(u32 *)header.gamecode, data);
//	return CalcCrc16(data, 0x4000);
//}
//
///*
// * CalcSecurityDataCRC
// */
//unsigned short CalcSecurityDataCRC()
//{
//	fseek(fNDS, 0x1000, SEEK_SET);
//	unsigned char data[0x2000];
//	fread(data, 1, 0x2000, fNDS);
//	return CalcCrc16(data, 0x2000);
//}
//
///*
// * CalcSegment3CRC
// */
//unsigned short CalcSegment3CRC()
//{
//	fseek(fNDS, 0x3000, SEEK_SET);
//	unsigned char data[0x1000];
//	fread(data, 1, 0x1000, fNDS);
//	for (int i=0; i<0x1000; i+=2)	// swap bytes
//	{
//		unsigned char t = data[i+1]; data[i+1] = data[i]; data[i] = t;
//	}
//	return CalcCcitt16(data, 0x1000);	// why would they use CRC16-CCITT ?
//}
//
///*
// * FixHeaderCRC
// */
//void FixHeaderCRC(char *ndsfilename)
//{
//	fNDS = fopen(ndsfilename, "r+b");
//	if (!fNDS) { fprintf(stderr, "Cannot open file '%s'.\n", ndsfilename); exit(1); }
//	fread(&header, 512, 1, fNDS);
//	header.header_crc = CalcHeaderCRC(header);
//	fseek(fNDS, 0, SEEK_SET);
//	fwrite(&header, 512, 1, fNDS);
//	fclose(fNDS);
//}
//
///*
// * ShowHeaderInfo
// */
//void ShowHeaderInfo(Header &header, int romType, unsigned int length = 0x200)
//{
//	printf("0x00\t%-25s\t", "Game title");
//
//	for (unsigned int i=0; i<sizeof(header.title); i++)
//		if (header.title[i]) putchar(header.title[i]); printf("\n");
//
//	printf("0x0C\t%-25s\t", "Game code");
//	for (unsigned int i=0; i<sizeof(header.gamecode); i++)
//		if (header.gamecode[i]) putchar(header.gamecode[i]);
//	for (int i=0; i<NumCountries; i++)
//	{
//		if (countries[i].countrycode == header.gamecode[3])
//		{
//			printf(" (NTR-");
//			for (unsigned int j=0; j<sizeof(header.gamecode); j++)
//				if (header.gamecode[j]) putchar(header.gamecode[j]);
//			printf("-%s)", countries[i].name);
//			break;
//		}
//	}
//	printf("\n");
//	
//	printf("0x10\t%-25s\t", "Maker code"); for (unsigned int i=0; i<sizeof(header.makercode); i++)
//		if (header.makercode[i]) putchar(header.makercode[i]);
//	for (int j=0; j<NumMakers; j++)
//	{
//		if ((makers[j].makercode[0] == header.makercode[0]) && (makers[j].makercode[1] == header.makercode[1]))
//		{
//			printf(" (%s)", makers[j].name);
//			break;
//		}
//	}
//	printf("\n");
//
//	printf("0x12\t%-25s\t0x%02X\n", "Unit code", header.unitcode);
//	printf("0x13\t%-25s\t0x%02X\n", "Device type", header.devicetype);
//	printf("0x14\t%-25s\t0x%02X (%d Mbit)\n", "Device capacity", header.devicecap, 1<<header.devicecap);
//	printf("0x15\t%-25s\t", "reserved 1"); for (unsigned int i=0; i<sizeof(header.reserved1); i++) printf("%02X", header.reserved1[i]); printf("\n");
//	printf("0x1E\t%-25s\t0x%02X\n", "ROM version", header.romversion);
//	printf("0x1F\t%-25s\t0x%02X\n", "reserved 2", header.reserved2);
//	printf("0x20\t%-25s\t0x%X\n", "ARM9 ROM offset", (int)header.arm9_rom_offset);
//	printf("0x24\t%-25s\t0x%X\n", "ARM9 entry address", (int)header.arm9_entry_address);
//	printf("0x28\t%-25s\t0x%X\n", "ARM9 RAM address", (int)header.arm9_ram_address);
//	printf("0x2C\t%-25s\t0x%X\n", "ARM9 code size", (int)header.arm9_size);
//	printf("0x30\t%-25s\t0x%X\n", "ARM7 ROM offset", (int)header.arm7_rom_offset);
//	printf("0x34\t%-25s\t0x%X\n", "ARM7 entry address", (int)header.arm7_entry_address);
//	printf("0x38\t%-25s\t0x%X\n", "ARM7 RAM address", (int)header.arm7_ram_address);
//	printf("0x3C\t%-25s\t0x%X\n", "ARM7 code size", (int)header.arm7_size);
//	printf("0x40\t%-25s\t0x%X\n", "File name table offset", (int)header.fnt_offset);
//	printf("0x44\t%-25s\t0x%X\n", "File name table size", (int)header.fnt_size);
//	printf("0x48\t%-25s\t0x%X\n", "FAT offset", (int)header.fat_offset);
//	printf("0x4C\t%-25s\t0x%X\n", "FAT size", (int)header.fat_size);
//	printf("0x50\t%-25s\t0x%X\n", "ARM9 overlay offset", (int)header.arm9_overlay_offset);
//	printf("0x54\t%-25s\t0x%X\n", "ARM9 overlay size", (int)header.arm9_overlay_size);
//	printf("0x58\t%-25s\t0x%X\n", "ARM7 overlay offset", (int)header.arm7_overlay_offset);
//	printf("0x5C\t%-25s\t0x%X\n", "ARM7 overlay size", (int)header.arm7_overlay_size);
//	printf("0x60\t%-25s\t0x%08X\n", "ROM control info 1", (int)header.rom_control_info1);
//	printf("0x64\t%-25s\t0x%08X\n", "ROM control info 2", (int)header.rom_control_info2);
//	printf("0x68\t%-25s\t0x%X\n", "Icon/title offset", (int)header.banner_offset);
//	unsigned short secure_area_crc = CalcSecureAreaCRC((romType == ROMTYPE_NDSDUMPED));
//	const char *s1, *s2 = "";
//	if (romType == ROMTYPE_HOMEBREW) s1 = "-";
//	else if (secure_area_crc == header.secure_area_crc) s1 = "OK";
//	else
//	{
//		s1 = "INVALID";
//	}
//	switch (romType)
//	{
//		case ROMTYPE_HOMEBREW: s2 = "homebrew"; break;
//		case ROMTYPE_MULTIBOOT: s2 = "multiboot"; break;
//		case ROMTYPE_NDSDUMPED: s2 = "decrypted"; break;
//		case ROMTYPE_ENCRSECURE: s2 = "encrypted"; break;
//		case ROMTYPE_MASKROM: s2 = "mask ROM"; break;
//	}
//	printf("0x6C\t%-25s\t0x%04X (%s, %s)\n", "Secure area CRC", (int)header.secure_area_crc, s1, s2);
//	printf("0x6E\t%-25s\t0x%04X\n", "ROM control info 3", (int)header.rom_control_info3);
//	printf("0x70\t%-25s\t0x%X\n", "ARM9 ?", (int)header.offset_0x70);
//	printf("0x74\t%-25s\t0x%X\n", "ARM7 ?", (int)header.offset_0x74);
//	printf("0x78\t%-25s\t0x%08X\n", "Magic 1", (int)header.offset_0x78);
//	printf("0x7C\t%-25s\t0x%08X\n", "Magic 2", (int)header.offset_0x7C);
//	printf("0x80\t%-25s\t0x%08X\n", "Application end offset", (int)header.application_end_offset);
//	printf("0x84\t%-25s\t0x%08X\n", "ROM header size", (int)header.rom_header_size);
//	for (unsigned int i=0x88; i<0xC0; i+=4)
//	{
//		unsigned_int &x = ((unsigned_int *)&header)[i/4];
//		if (x != 0) printf("0x%02X\t%-25s\t0x%08X\n", i, "?", (int)x);
//	}
//	unsigned short logo_crc = CalcLogoCRC(header);
//	printf("0x15C\t%-25s\t0x%04X (%s)\n", "Logo CRC", (int)header.logo_crc, (logo_crc == header.logo_crc) ? "OK" : "INVALID");
//	unsigned short header_crc = CalcHeaderCRC(header);
//	printf("0x15E\t%-25s\t0x%04X (%s)\n", "Header CRC", (int)header.header_crc, (header_crc == header.header_crc) ? "OK" : "INVALID");
//	for (unsigned int i=0x160; i<length; i+=4)
//	{
//		unsigned_int &x = ((unsigned_int *)&header)[i/4];
//		if (x != 0) printf("0x%02X\t%-25s\t0x%08X\n", i, "?", (int)x);
//	}
//}
//
///*
// * HeaderSha1
// */
//void HeaderSha1(FILE *fNDS, unsigned char *header_sha1, int romType)
//{
//	sha1_ctx m_sha1;
//	sha1_begin(&m_sha1);
//	unsigned char buf[32 + 0x200];
//	fseek(fNDS, 0x200, SEEK_SET);		// check for 32 bytes text + alternate header
//	fread(buf, 1, sizeof(buf), fNDS);
//	if (!memcmp(buf, "DS DOWNLOAD PLAY", 16))	// found?
//	{
//		sha1_hash(buf + 0x20, 0x160, &m_sha1);	// alternate header
//		if (verbose >= 2)
//		{
//			printf("{ DS Download Play(TM) / Wireless MultiBoot header information:\n");
//			ShowHeaderInfo(*(Header *)(buf + 0x20), romType, 0x160);
//			printf("}\n");
//		}
//	}
//	else
//	{
//		fseek(fNDS, 0, SEEK_SET);
//		fread(buf, 1, sizeof(buf), fNDS);
//		sha1_hash(buf, 0x160, &m_sha1);
//	}
//	sha1_end(header_sha1, &m_sha1);
//}
//
///*
// * Arm9Sha1Multiboot
// */
//void Arm9Sha1Multiboot(FILE *fNDS, unsigned char *arm9_sha1)
//{
//	sha1_ctx m_sha1;
//	sha1_begin(&m_sha1);
//	fseek(fNDS, header.arm9_rom_offset, SEEK_SET);
//	unsigned int len = header.arm9_size;
//	unsigned char *buf = new unsigned char [len];
//	fread(buf, 1, len, fNDS);
//	//printf("%u\n", len);
//	sha1_hash(buf, len, &m_sha1);
//	delete [] buf;
//	sha1_end(arm9_sha1, &m_sha1);
//}
//
///*
// * Arm9Sha1ClearedOutArea
// */
//void Arm9Sha1ClearedOutArea(FILE *fNDS, unsigned char *arm9_sha1)
//{
//	sha1_ctx m_sha1;
//	sha1_begin(&m_sha1);
//	fseek(fNDS, header.arm9_rom_offset, SEEK_SET);
//	unsigned int len = header.arm9_size;
//	unsigned char *buf = new unsigned char [len];
//
//	int len1 = (0x5000 - header.arm9_rom_offset);		// e.g. 0x5000 - 0x4000 = 0x1000
//	int len3 = header.arm9_size - (0x7000 - header.arm9_rom_offset);	// e.g. 0x10000 - (0x7000 - 0x4000) = 0xD000
//	int len2 = header.arm9_size - len1 - len3;			// e.g. 0x10000 - 0x1000 - 0xD000 = 0x2000
//	if (len1 > 0) fread(buf, 1, len1, fNDS);
//	if (len2 > 0) { memset(buf + len1, 0, len2); fseek(fNDS, len2, SEEK_CUR); }		// gets cleared for security?
//	if (len3 > 0) fread(buf + len1 + len2, 1, len3, fNDS);
////	printf("%X %X %X\n", len1, len2, len3);
//
////		memset(buf, 0, 0x800);		// clear "secure area" too
//
//	sha1_hash(buf, len, &m_sha1);
//	delete [] buf;
//	sha1_end(arm9_sha1, &m_sha1);
//}
//
///*
// * Arm7Sha1
// */
//void Arm7Sha1(FILE *fNDS, unsigned char *arm7_sha1)
//{
//	sha1_ctx m_sha1;
//	sha1_begin(&m_sha1);
//	fseek(fNDS, header.arm7_rom_offset, SEEK_SET);
//	unsigned int len = header.arm7_size;
//	unsigned char *buf = new unsigned char [len];
//	fread(buf, 1, len, fNDS);
//	//printf("%u\n", len);
//	sha1_hash(buf, len, &m_sha1);
//	delete [] buf;
//	sha1_end(arm7_sha1, &m_sha1);
//}
//
///*
// * CompareSha1WithList
// */
//int CompareSha1WithList(unsigned char *arm7_sha1, const unsigned char *text, unsigned int textSize)
//{
//	while (1)
//	{
//		//printf("\n");
//		for (int i=0; i<SHA1_DIGEST_SIZE; i++)
//		{
//			unsigned char b = 0;
//			for (int n=0; n<2; n++)
//			{
//				//printf("%c", *text);
//				if (!*text) return -1;
//				b = b << 4 | ((*text > '9') ? ((*text - 'A') & 7) + 10 : *text - '0');
//				text++;
//			}
//			//printf("%02X", b);
//			if (b != arm7_sha1[i]) break; else if (i == 19) return 0;
//		}
//		while (*text && (*text >= ' ')) text++;		// line end
//		while (*text && (*text < ' ')) text++;		// new line
//	}
//}
//
///*
// * HashAndCompareWithList
// * -1=error, 0=match, 1=no match
// */
//int HashAndCompareWithList(char *filename, unsigned char sha1[])
//{
//	FILE *f = fopen(filename, "rb");
//	if (!f) return -1;
//	sha1_ctx m_sha1;
//	sha1_begin(&m_sha1);
//	unsigned char buf[1024];
//	unsigned int r;
//	do
//	{
//		r = fread(buf, 1, 1024, f);
//		sha1_hash(buf, r, &m_sha1);
//	} while (r > 0);
//	fclose(f);
//	sha1_end(sha1, &m_sha1);
//	if (CompareSha1WithList(sha1, arm7_sha1_homebrew, arm7_sha1_homebrew_size)) return 1;	// not yet in list
//	return 0;
//}
//
///*
// * strsepc
// */
//char *strsepc(char **s, char d)
//{
//	char *r = *s;
//	for (char *p = *s; ; p++)
//	{
//		if (*p == 0) { *s = p; break; }
//		if (*p == d) { *s = p+1; *p = 0; break; }
//	}
//	return r;
//}
//
///*
// * RomListInfo
// */
//void RomListInfo(unsigned int crc32_match)
//{
//	if (!romlistfilename) return;
//	FILE *fRomList = fopen(romlistfilename, "rt");
//	if (!fRomList) { fprintf(stderr, "Cannot open file '%s'.\n", romlistfilename); exit(1); }
//	char s[1024];
//	while (fgets(s, 1024, fRomList))	// empty, title, title, title, title, filename, CRC32
//	{
//		char *p = s;
//		if (strlen(strsepc(&p, '\xAC')) == 0)
//		{
//			char *title = strsepc(&p, '\xAC');
//			unsigned int index = strtoul(title, 0, 10);
//			title += 7;
//			char *b1 = strchr(title, '(');
//			char *b2 = b1 ? strchr(b1+1, ')') : 0;
//			char *b3 = b2 ? strchr(b2+1, '(') : 0;
//			char *b4 = b3 ? strchr(b3+1, ')') : 0;
//			char *group = 0;
//			if (b1 + 2 == b2) if (b3 && b4) { *b3 = 0; *b4 = 0; group = b3+1; }		// remove release group name
//			strsepc(&p, '\xAC'); strsepc(&p, '\xAC');
//			strsepc(&p, '\xAC'); strsepc(&p, '\xAC');
//			unsigned long crc32 = strtoul(strsepc(&p, '\xAC'), 0, 16);
//			if (crc32 == crc32_match)
//			{
//				printf("Release index: \t%u\n", index);
//				printf("Release title: \t%s\n", title);
//				printf("Release group: \t%s\n", group ? group : "");
//			}
//			//for (int i=0; i<10; i++) printf("%d %s\n", i, strsepc(&p, '\xAC'));
//		}
//	}
//}
//
///*
// * ShowVerboseInfo
// */
//void ShowVerboseInfo(FILE *fNDS, Header &header, int romType)
//{
//	// calculate SHA1 of ARM7 binary
//	unsigned char arm7_sha1[SHA1_DIGEST_SIZE];
//	Arm7Sha1(fNDS, arm7_sha1);
//
//	// find signature data
//	unsigned_int signature_id = 0;
//	fseek(fNDS, header.application_end_offset, SEEK_SET);
//	fread(&signature_id, sizeof(signature_id), 1, fNDS);
//	if (signature_id != 0x00016361)
//	{
//		fseek(fNDS, header.application_end_offset - 136, SEEK_SET);		// try again
//		fread(&signature_id, sizeof(signature_id), 1, fNDS);
//	}
//	if (signature_id == 0x00016361)
//	{
//		printf("\n");
//
//		unsigned char signature[128];
//		fread(signature, 1, sizeof(signature), fNDS);
//
//		unsigned char sha_parts[3*SHA1_DIGEST_SIZE + 4];
//		fread(sha_parts + 3*SHA1_DIGEST_SIZE, 4, 1, fNDS);		// some number
//		
//		//printf("%08X\n", *(unsigned int *)(sha_parts + 3*SHA1_DIGEST_SIZE));
//
//		unsigned char header_sha1[SHA1_DIGEST_SIZE];
//		HeaderSha1(fNDS, header_sha1, romType);
//		memcpy(sha_parts + 0*SHA1_DIGEST_SIZE, header_sha1, SHA1_DIGEST_SIZE);
//
//		unsigned char arm9_sha1[SHA1_DIGEST_SIZE];
//		if (romType == ROMTYPE_MULTIBOOT)
//		{
//			Arm9Sha1Multiboot(fNDS, arm9_sha1);
//		}
//		else
//		{
//			Arm9Sha1ClearedOutArea(fNDS, arm9_sha1);
//		}
//		memcpy(sha_parts + 1*SHA1_DIGEST_SIZE, arm9_sha1, SHA1_DIGEST_SIZE);
//
//		memcpy(sha_parts + 2*SHA1_DIGEST_SIZE, arm7_sha1, SHA1_DIGEST_SIZE);
//
//		unsigned char sha_final[SHA1_DIGEST_SIZE];
//		{
//			sha1_ctx m_sha1;
//			sha1_begin(&m_sha1);
//			unsigned int len = sizeof(sha_parts);
//			unsigned char *buf = sha_parts;
//			sha1_hash(buf, len, &m_sha1);
//			sha1_end(sha_final, &m_sha1);
//		}
//
//		// calculate SHA1 from signature
//		unsigned char sha1_from_sig[SHA1_DIGEST_SIZE];
//		{
//			BigInt _signature;
//			_signature.Set(signature, sizeof(signature));
//			//printf("signature: "); _signature.print();
//
//			BigInt _publicKey;
//			_publicKey.Set(publicKeyNintendo, sizeof(publicKeyNintendo));
//			//printf("public key: "); _publicKey.print();
//
//			BigInt big_sha1;
//			big_sha1.PowMod(_signature, _publicKey);
//			//printf("big_sha1: "); big_sha1.print();
//			big_sha1.Get(sha1_from_sig, sizeof(sha1_from_sig));
//		}
//		
//		bool ok = (memcmp(sha_final, sha1_from_sig, SHA1_DIGEST_SIZE) == 0);
//		printf("DS Download Play(TM) / Wireless MultiBoot signature: %s\n", ok ? "OK" : "INVALID");
//		if (!ok)
//		{
//			printf("header hash:    \t"); for (int i=0; i<SHA1_DIGEST_SIZE; i++) printf("%02X", (sha_parts + 0*SHA1_DIGEST_SIZE)[i]); printf("\n");
//			printf("ARM9 hash:      \t"); for (int i=0; i<SHA1_DIGEST_SIZE; i++) printf("%02X", (sha_parts + 1*SHA1_DIGEST_SIZE)[i]); printf("\n");
//			printf("ARM7 hash:      \t"); for (int i=0; i<SHA1_DIGEST_SIZE; i++) printf("%02X", (sha_parts + 2*SHA1_DIGEST_SIZE)[i]); printf("\n");
//			printf("combined hash:  \t"); for (int i=0; i<SHA1_DIGEST_SIZE; i++) printf("%02X", sha_final[i]); printf("\n");
//			printf("signature hash: \t"); for (int i=0; i<SHA1_DIGEST_SIZE; i++) printf("%02X", sha1_from_sig[i]); printf("\n");
//		}
//	}
//	
//	// CRC32
//	{
//		unsigned char *buf = new unsigned char [0x10000];
//		fseek(fNDS, 0, SEEK_SET);
//		unsigned long crc32 = ~0;
//		int r;
//		while ((r = fread(buf, 1, 0x10000, fNDS)) > 0)
//		{
//			crc32 = CalcCrc32(buf, r, crc32);
//		}
//		crc32 = ~crc32;
//		delete [] buf;
//
//		printf("\nFile CRC32:    \t%08X\n", (unsigned int)crc32);
//		RomListInfo(crc32);
//	}
//	
//	// ROM dumper 1.0 bad data
//	{
//		unsigned char buf[0x200];
//		fseek(fNDS, 0x7E00, SEEK_SET);
//		fread(buf, 1, 0x200, fNDS);
//		unsigned long crc32 = ~CalcCrc32(buf, 0x200);
//		printf("\nSMT dumper v1.0 corruption check: \t%s\n", (crc32 == 0x7E8B456F) ? "CORRUPTED" : "OK");
//	}
//
//	// Verify ARM7 SHA1 hash against known default binaries
//	int bKnownArm7 = 0;
//	{
//		printf("\nARM7 binary hash : \t"); for (int i=0; i<SHA1_DIGEST_SIZE; i++) printf("%02X", arm7_sha1[i]); printf("\n");
//
//		if (CompareSha1WithList(arm7_sha1, arm7_sha1_homebrew, arm7_sha1_homebrew_size) == 0)
//		{
//			bKnownArm7 = 1; printf("ARM7 binary is homebrew verified.\n");
//		}
//		if (CompareSha1WithList(arm7_sha1, arm7_sha1_nintendo, arm7_sha1_nintendo_size) == 0)
//		{
//			bKnownArm7 = 2; printf("ARM7 binary is Nintendo verified.\n");
//		}
//		if (!bKnownArm7) printf("WARNING! ARM7 binary is NOT verified!\n");
//	}
//
//	// check ARM7 RAM address
//	if (bKnownArm7 != 2)
//	if ((header.arm7_ram_address < 0x03000000) || (header.arm7_ram_address >= 0x04000000))
//	{
//		printf("\nWARNING! ARM7 RAM address does not point to shared memory!\n");
//	}
//
//	// check ARM7 entry address
//	if ((header.arm7_entry_address < header.arm7_ram_address) ||
//		(header.arm7_entry_address > header.arm7_ram_address + header.arm7_size))
//	{
//		printf("\nWARNING! ARM7 entry address points outside of ARM7 binary!\n");
//	}
//}
//
///*
// * ShowInfo
// */
//void ShowInfo(char *ndsfilename)
//{
//	fNDS = fopen(ndsfilename, "rb");
//	if (!fNDS) { fprintf(stderr, "Cannot open file '%s'.\n", ndsfilename); exit(1); }
//	fread(&header, 512, 1, fNDS);
//
//	int romType = DetectRomType();
//
//	printf("Header information:\n");
//	ShowHeaderInfo(header, romType);
//
//	// banner info
//	if (header.banner_offset)
//	{
//		Banner banner;
//		fseek(fNDS, header.banner_offset, SEEK_SET);
//		if (fread(&banner, 1, sizeof(banner), fNDS))
//		{
//			unsigned short banner_crc = CalcBannerCRC(banner);
//			printf("\n");
//			printf("Banner CRC:                     \t0x%04X (%s)\n", (int)banner.crc, (banner_crc == banner.crc) ? "OK" : "INVALID");
//	
//			for (int language=1; language<=1; language++)
//			{
//				int line = 1;
//				bool nextline = true;
//				for (int i=0; i<128; i++)
//				{
//					unsigned short c = banner.title[language][i];
//					if (c >= 128) c = '_';
//					if (c == 0x00) { printf("\n"); break; }
//					if (c == 0x0A)
//					{
//						nextline = true;
//					}
//					else
//					{
//						if (nextline)
//						{
//							if (line != 1) printf("\n");
//							printf("%s banner text, line %d:", bannerLanguages[language], line);
//							for (unsigned int i=0; i<11 - strlen(bannerLanguages[language]); i++) putchar(' ');
//							printf("\t");
//							nextline = false;
//							line++;
//						}
//						putchar(c);
//					}
//				}
//			}
//		}
//	}
//
//	// ARM9 footer
//	fseek(fNDS, header.arm9_rom_offset + header.arm9_size, SEEK_SET);
//	unsigned_int nitrocode;
//	if (fread(&nitrocode, sizeof(nitrocode), 1, fNDS) && (nitrocode == 0xDEC00621))
//	{
//		printf("\n");
//		printf("ARM9 footer found.\n");
//		unsigned_int x;
//		fread(&x, sizeof(x), 1, fNDS);
//		fread(&x, sizeof(x), 1, fNDS);
//	}
//
//	// show security CRCs
//	if (romType >= ROMTYPE_NDSDUMPED)
//	{
//		printf("\n");
//		unsigned short securitydata_crc = CalcSecurityDataCRC();
//		printf("Security data CRC (0x1000-0x2FFF)  0x%04X\n", (int)securitydata_crc);
//		unsigned short segment3_crc = CalcSegment3CRC();
//		printf("Segment3 CRC (0x3000-0x3FFF)       0x%04X (%s)\n", (int)segment3_crc, (segment3_crc == 0x0254) ? "OK" : "INVALID");
//	}
//
//	// more information
//	if (verbose >= 1)
//	{
//		ShowVerboseInfo(fNDS, header, romType);
//	}
//
//	fclose(fNDS);
//}
