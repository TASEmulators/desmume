/*
	Copyright (C) 2009-2019 DeSmuME Team

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

#ifndef _FIRMWARE_H_
#define _FIRMWARE_H_

#include <string>
#include "types.h"

#define NDS_FW_SIZE_V1 (256 * 1024)		/* size of fw memory on nds v1 */
#define NDS_FW_SIZE_V2 (512 * 1024)		/* size of fw memory on nds v2 */

// the count of bytes (user settings) copied from the firmware into memory (for easy access) during the firmware boot process
#define NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT 0x70

//extension of the firmware user settings file
#define FW_CONFIG_FILE_EXT "dfc"

#define MAX_FW_NICKNAME_LENGTH 10
#define MAX_FW_MESSAGE_LENGTH 26

extern const char *defaultNickname;
extern const char *defaultMessage;

struct FirmwareConfig
{
	u8 consoleType;
	
	u8 favoriteColor;
	u8 birthdayMonth;
	u8 birthdayDay;
	
	// Add one character to these string buffers just in case clients
	// try to terminate a max length string with a null character.
	u16 nickname[MAX_FW_NICKNAME_LENGTH+1];
	u8 nicknameLength;
	u16 message[MAX_FW_MESSAGE_LENGTH+1];
	u8 messageLength;
	
	u8 language;
	u8 backlightLevel;
	
	u16 tscADC_x1;
	u16 tscADC_y1;
	u8 tscPixel_x1;
	u8 tscPixel_y1;
	u16 tscADC_x2;
	u16 tscADC_y2;
	u8 tscPixel_x2;
	u8 tscPixel_y2;
	
	u8 MACAddress[6];
	u8 WFCUserID[6];
	
	u8 ipv4Address_AP1[4];
	u8 ipv4Gateway_AP1[4];
	u8 ipv4PrimaryDNS_AP1[4];
	u8 ipv4SecondaryDNS_AP1[4];
	u8 subnetMask_AP1;
	
	u8 ipv4Address_AP2[4];
	u8 ipv4Gateway_AP2[4];
	u8 ipv4PrimaryDNS_AP2[4];
	u8 ipv4SecondaryDNS_AP2[4];
	u8 subnetMask_AP2;
	
	u8 ipv4Address_AP3[4];
	u8 ipv4Gateway_AP3[4];
	u8 ipv4PrimaryDNS_AP3[4];
	u8 ipv4SecondaryDNS_AP3[4];
	u8 subnetMask_AP3;
};
typedef FirmwareConfig FirmwareConfig;

#include "PACKED.h"
typedef union __PACKED
{
	u64 encryption;
	
	struct
	{
		u64 timestamp:40;
		u64 consoleType:8;
		u64 unused:16;
	};
} FW_HEADER_KEY;

typedef union __PACKED
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 language:3;
		u16 gbaModeScreenSelection:1;
		u16 backlightLevel:2;
		u16 bootmenuDisable:1;
		u16 :1;
		
		u16 :1;
		u16 settingsLost:1;
		u16 settingsOkay1:1;
		u16 :1;
		u16 settingsOkay2:1;
		u16 settingsOkay3:1;
		u16 settingsOkay4:1;
		u16 settingsOkay5:1;
#else
		u16 :1;
		u16 bootmenuDisable:1;
		u16 backlightLevel:2;
		u16 gbaModeScreenSelection:1;
		u16 language:3;
		
		u16 settingsOkay5:1;
		u16 settingsOkay4:1;
		u16 settingsOkay3:1;
		u16 settingsOkay2:1;
		u16 :1;
		u16 settingsOkay1:1;
		u16 settingsLost:1;
		u16 :1;
#endif
	};
} FW_USERSETTINGS_LANGUAGE_FLAGS;

typedef struct __PACKED
{
	u16 part3_rom_gui9_addr;			// 0x0000
	u16 part4_rom_wifi7_addr;			// 0x0002
	u16 part34_gui_wifi_crc16;			// 0x0004
	u16 part12_boot_crc16;				// 0x0006
	
	union
	{
		u32 identifierValue;			// 0x0008
		u8 identifier[4];				// 0x0008
	};
	
	u16 part1_rom_boot9_addr;			// 0x000C
	u16 part1_ram_boot9_addr;			// 0x000E
	u16 part2_rom_boot7_addr;			// 0x0010
	u16 part2_ram_boot7_addr;			// 0x0012
	u16 shift;							// 0x0014
	u16 part5_data_gfx_addr;			// 0x0016
	
	FW_HEADER_KEY key;					// 0x0018
	
	u16 userSettingsOffset;				// 0x0020
	u16 UNKNOWN1;						// 0x0022
	u16 UNKNOWN2;						// 0x0024
	u16 part5_crc16;					// 0x0026
	u16 unused2;						// 0x0028
} FWHeader;

typedef struct __PACKED
{
	u16 crc16;							// 0x002A
	u16 length;							// 0x002C
	u8 unused1;							// 0x002E
	u8 version;							// 0x002F
	u8 unused2[6];						// 0x0030
	u8 MACAddr[6];						// 0x0036
	u16 channels;						// 0x003C
	u16 flags;							// 0x003E
	u8 rfType;							// 0x0040
	u8 rfBits;							// 0x0041
	u8 rfEntries;						// 0x0042
	u8 UNKNOWN1;						// 0x0043
	
	u16 wifiConfig146;					// 0x0044
	u16 wifiConfig148;					// 0x0046
	u16 wifiConfig14A;					// 0x0048
	u16 wifiConfig14C;					// 0x004A
	u16 wifiConfig120;					// 0x004C
	u16 wifiConfig122;					// 0x004E
	u16 wifiConfig154;					// 0x0050
	u16 wifiConfig144;					// 0x0052
	u16 wifiConfig130;					// 0x0054
	u16 wifiConfig132;					// 0x0056
	u16 wifiConfig140;					// 0x0058
	u16 wifiConfig142;					// 0x005A
	u16 wifiConfigPowerTX;				// 0x005C
	u16 wifiConfig124;					// 0x005E
	u16 wifiConfig128;					// 0x0060
	u16 wifiConfig150;					// 0x0062
	
	u8 bbData[105];						// 0x0064
	u8 unused3;							// 0x00CD
	
	union
	{
		struct
		{
			u8 rfValue[36];				// 0x00CE
			u8 rfChannelValue24[84];	// 0x00F2
			u8 bbChannelValue8[14];		// 0x0146
			u8 rfChannelValue8[14];		// 0x0154
		} Type2;
		
		struct
		{
			u8 type3Data[148];			// 0x00CE
		} Type3;
	};
	
	u8 UNKNOWN2;						// 0x0162
	u8 unused4;							// 0x0163
	u8 unused5[156];					// 0x0164
} FWWifiInfo;

typedef struct
{
	u8 UNKNOWN1[64];
	u8 SSID[32];
	u8 SSID_WEP[32];
	u8 wepKey1[16];
	u8 wepKey2[16];
	u8 wepKey3[16];
	u8 wepKey4[16];
	u8 ipv4Address[4];
	u8 ipv4Gateway[4];
	u8 ipv4PrimaryDNS[4];
	u8 ipv4SecondaryDNS[4];
	u8 subnetMask;
	u8 wepKeyAOSS[20];
	u8 UNKNOWN2;
	u8 wepMode;
	u8 configureMode;
	u8 zero;
	u8 UNKNOWN3;
	u16 mtuSize;
	u8 UNKNOWN4[3];
	u8 connectStatus;
	u8 wfcUserID[6];
	u8 UNKNOWN5[8];
	u16 crc16;
} FWAccessPointSettings;

typedef struct
{
	u8 proxyAuthenticationUsername[32];
	u8 proxyAuthenticationPassword[32];
	u8 SSID[32];
	u8 SSID_WEP[32];
	u8 wepKey1[16];
	u8 wepKey2[16];
	u8 wepKey3[16];
	u8 wepKey4[16];
	u8 ipv4Address[4];
	u8 ipv4Gateway[4];
	u8 ipv4PrimaryDNS[4];
	u8 ipv4SecondaryDNS[4];
	u8 subnetMask;
	u8 wepKeyAOSS[20];
	u8 UNKNOWN2;
	u8 wepMode;
	u8 configureMode;
	u8 ssidLength;
	u8 UNKNOWN3;
	u16 mtuSize;
	u8 UNKNOWN4[3];
	u8 connectStatus;
	u8 unused1[14];
	u16 crc16_1;
	u8 UNKNOWN5[32];
	u8 wpaKey[16];
	u8 unused2[82];
	u8 securityMode;
	u8 proxyEnable;
	u8 proxyAuthentication;
	u8 proxyName[32];
	u8 unused3[65];
	u16 proxyPort;
	u8 unused4[21];
	u16 crc16_2;
} FWExtAccessPointSettings;

typedef struct
{
	u16 version;
	u8 favoriteColor;
	u8 birthdayMonth;
	u8 birthdayDay;
	u8 unused1;
	u16 nickname[10];
	u16 nicknameLength;
	u16 message[26];
	u16 messageLength;
	u8 alarmHour;
	u8 alarmMinute;
	u16 unused2;
	u8 alarmEnable;
	u8 unused3;
	u16 tscADC_x1;
	u16 tscADC_y1;
	u8 tscPixel_x1;
	u8 tscPixel_y1;
	u16 tscADC_x2;
	u16 tscADC_y2;
	u8 tscPixel_x2;
	u8 tscPixel_y2;
	FW_USERSETTINGS_LANGUAGE_FLAGS languageFlags;
	u8 year;
	u8 UNKNOWN1;
	u32 rtcOffset;
	u32 unused4;
	
	u16 updateCounter;
	u16 crc16;
	
	union
	{
		struct
		{
			u8 unused5[140];
		} nds;
		
		struct
		{
			u8 UNKNOWN2;
			u8 extendedLanguage;
			u16 supportedLanguage;
			u8 unused6[134];
			u16 crc16Extended;
		} dsi;
	};
	
} FWUserSettings;

typedef struct
{
	union
	{
		u64 identifierValue;
		u8 identifier[8];
	};
	
	u16 crc16Value;
	u16 crc16Length;
	u8 version;
	u8 updateCount;
	u8 bootMenuFlags;
	u8 gbaBorder;
	u16 tempCalibrationTP0;
	u16 tempCalibrationTP1;
	u16 tempCalibrationDegrees;
	u8 tempFlags;
	u8 backlightIntensity;
	u32 dateCenturyOffset;
	u8 dateMonthRecoveryValue;
	u8 dateDayRecoveryValue;
	u8 dateYearRecoveryValue;
	u8 dateTimeFlags;
	u8 dateSeparator;
	u8 timeSeparator;
	u8 decimalSeparator;
	u8 thousandsSeparator;
	u8 daylightSavingsTimeNth;
	u8 daylightSavingsTimeDay;
	u8 daylightSavingsTimeMonth;
	u8 daylightSavingsTimeFlags;
} FWExtUserSettings;

typedef union
{
	u8 _raw[262144];
	
	struct
	{
		FWHeader header;
		FWWifiInfo wifiInfo;
		
		union
		{
			struct
			{
				u8 codeAndData[260096];
			} nds;
			
			struct
			{
				u8 zeroBuffer[255];
				u8 value0x80;
				u8 ffBuffer[127231];
				u8 bootFlags;
				FWExtAccessPointSettings wifiAP4;
				FWExtAccessPointSettings wifiAP5;
				FWExtAccessPointSettings wifiAP6;
				u8 UNKNOWN1[131072];
			} dsi;
		};
		
		FWAccessPointSettings wifiAP1;
		FWAccessPointSettings wifiAP2;
		FWAccessPointSettings wifiAP3;
		
		u8 unused[256];
		
		FWUserSettings userSettings0;
		FWUserSettings userSettings1;
	};
	
} NDSFirmwareData;
#include "PACKED_END.h"

class CFIRMWARE
{
private:
	FWHeader _header;
	std::string _fwFilePath;
	bool _isLoaded;
	u32 _userDataAddr;
	
	u16		_getBootCodeCRC16(const u8 *arm9Data, const u32 arm9Size, const u8 *arm7Data, const u32 arm7Size);
	u32		_decrypt(const u8 *in, u8* &out);
	u32		_decompress(const u8 *in, u8* &out);

public:
	CFIRMWARE(): _userDataAddr(0x3FE00), _isLoaded(false) {};
	
	bool load(const char *firmwareFilePath);
	bool unpack();
	bool loadSettings(const char *firmwareUserSettingsFilePath);
	bool saveSettings(const char *firmwareUserSettingsFilePath);

	bool loaded();
	void* getTouchCalibrate();
	
	static std::string GetUserSettingsFilePath(const char *firmwareFilePath);
};

int copy_firmware_user_data( u8 *dest_buffer, const u8 *fw_data);

void NDS_GetDefaultFirmwareConfig(FirmwareConfig &outConfig);
void NDS_GetCurrentWFCUserID(u8 *outMAC, u8 *outUserID);
void NDS_ApplyFirmwareSettings(NDSFirmwareData *outFirmware,
							   const FW_HEADER_KEY *headerKey,
							   const FWUserSettings *userSettings0,
							   const FWUserSettings *userSettings1,
							   const FWWifiInfo *wifiInfo,
							   const FWAccessPointSettings *wifiAP1,
							   const FWAccessPointSettings *wifiAP2,
							   const FWAccessPointSettings *wifiAP3);
bool NDS_ApplyFirmwareSettingsWithFile(NDSFirmwareData *outFirmware, const char *inFileName);
void NDS_ApplyFirmwareSettingsWithConfig(NDSFirmwareData *outFirmware, const FirmwareConfig &inConfig);
void NDS_InitDefaultFirmware(NDSFirmwareData *outFirmware);
bool NDS_ReadFirmwareDataFromFile(const char *fileName, NDSFirmwareData *outFirmware, size_t *outFileSize, int *outConsoleType, u8 *outMACAddr);

struct fw_memory_chip
{
	u8 com;	//persistent command actually handled
	u32 addr;        //current address for reading/writing
	u8 addr_shift;   //shift for address (since addresses are transfered by 3 bytes units)
	u8 addr_size;    //size of addr when writing/reading

	BOOL write_enable;	//is write enabled ?
	
	NDSFirmwareData data;
	
	u32 size;       //memory size
	BOOL writeable_buffer;	//is "data" writeable ?
	int type; //type of Memory
	
	// needs only for firmware
	bool isFirmware;
};


void fw_reset_com(fw_memory_chip *mc);
u8 fw_transfer(fw_memory_chip *mc, u8 data);
void mc_init(fw_memory_chip *mc, int type);    /* reset and init values for memory struct */
u8 *mc_alloc(fw_memory_chip *mc, u32 size);  /* alloc mc memory */
void mc_realloc(fw_memory_chip *mc, int type, u32 size);      /* realloc mc memory */
void mc_load_file(fw_memory_chip *mc, const char* filename); /* load save file and setup fp */
void mc_free(fw_memory_chip *mc);    /* delete mc memory */

#endif
