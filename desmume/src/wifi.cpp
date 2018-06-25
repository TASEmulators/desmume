/*  Copyright (C) 2007 Tim Seidel
    Copyright (C) 2008-2010 DeSmuME team

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

#include <assert.h>
#include "wifi.h"
#include "armcpu.h"
#include "NDSSystem.h"
#include "debug.h"
#include "utils/bits.h"
#include <driver.h>
#include <registers.h>

#ifdef HOST_WINDOWS
	#include <winsock2.h> 	 
	#include <ws2tcpip.h>
	#define socket_t    SOCKET 	 
	#define sockaddr_t  SOCKADDR
	#ifndef WXPORT
		#include "windriver.h"
	#endif
	#define PCAP_DEVICE_NAME description
#else
	#include <unistd.h> 	 
	#include <stdlib.h> 	 
	#include <string.h> 	 
	#include <arpa/inet.h> 	 
	#include <sys/socket.h> 	 
	#define socket_t    int 	 
	#define sockaddr_t  struct sockaddr
	#define closesocket close
	#define PCAP_DEVICE_NAME name
#endif

#ifndef INVALID_SOCKET 	 
	#define INVALID_SOCKET  (socket_t)-1 	 
#endif 

#define BASEPORT 7000

// PCAP file to store the Ethernet packets.
FILE* captured_packets;

const u8 BroadcastMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

bool bWFCUserWarned = false;

#ifdef EXPERIMENTAL_WIFI_COMM
socket_t wifi_socket = INVALID_SOCKET;
sockaddr_t sendAddr;
#ifndef WIN32
#include "pcap/pcap.h"
#endif
pcap_t *wifi_bridge = NULL;
#endif

//sometimes this isnt defined
#ifndef PCAP_OPENFLAG_PROMISCUOUS
#define PCAP_OPENFLAG_PROMISCUOUS 1
#endif

wifimac_t wifiMac;
Adhoc_t Adhoc;
SoftAP_t SoftAP;

/*******************************************************************************

	WIFI TODO

	- emulate transmission delays for Beacon and Extra transfers
	- emulate delays when receiving as well (may need some queuing system)
	- take transfer rate and preamble into account
	- figure out RFSTATUS and RFPINS

 *******************************************************************************/

/*******************************************************************************

	Firmware info needed for if no firmware image is available
	see: http://www.akkit.org/info/dswifi.htm#WifiInit

	written in bytes, to avoid endianess issues

 *******************************************************************************/

u8 FW_Mac[6] 			= { 0x00, 0x09, 0xBF, 0x12, 0x34, 0x56 };

const u8 FW_WIFIInit[32] 		= { 0x02,0x00,  0x17,0x00,  0x26,0x00,  0x18,0x18,
							0x48,0x00,  0x40,0x48,  0x58,0x00,  0x42,0x00,
							0x40,0x01,  0x64,0x80,  0xE0,0xE0,  0x43,0x24,
							0x0E,0x00,  0x32,0x00,  0xF4,0x01,  0x01,0x01 };
const u8 FW_BBInit[105] 		= { 0x6D, 0x9E, 0x40, 0x05,
							0x1B, 0x6C, 0x48, 0x80,
							0x38, 0x00, 0x35, 0x07,
							0x00, 0x00, 0x00, 0x00,
							0x00, 0x00, 0x00, 0x00,
							0xb0, 0x00, 0x04, 0x01,
							0xd8, 0xff, 0xff, 0xc7,
							0xbb, 0x01, 0xb6, 0x7f,
							0x5a, 0x01, 0x3f, 0x01,
							0x3f, 0x36, 0x36, 0x00,
							0x78, 0x28, 0x55, 0x08,
							0x28, 0x16, 0x00, 0x01,
							0x0e, 0x20, 0x02, 0x98,
							0x98, 0x1f, 0x0a, 0x08,
							0x04, 0x01, 0x00, 0x00,
							0x00, 0xff, 0xff, 0xfe,
							0xfe, 0xfe, 0xfe, 0xfc,
							0xfa, 0xfa, 0xf8, 0xf8,
							0xf6, 0xa5, 0x12, 0x14,
							0x12, 0x41, 0x23, 0x03,
							0x04, 0x70, 0x35, 0x0E,
							0x16, 0x16, 0x00, 0x00,
							0x06, 0x01, 0xff, 0xfe,
							0xff, 0xff, 0x00, 0x0e,
							0x13, 0x00, 0x00, 0x28,
							0x1c
						  };
const u8 FW_RFInit[36] 		= { 0x07, 0xC0, 0x00,
							0x03, 0x9C, 0x12,
							0x28, 0x17, 0x14,
							0xba, 0xe8, 0x1a,
							0x6f, 0x45, 0x1d,
							0xfa, 0xff, 0x23,
							0x30, 0x1d, 0x24,
							0x01, 0x00, 0x28,
							0x00, 0x00, 0x2c,
							0x03, 0x9c, 0x06,
							0x22, 0x00, 0x08,
							0x6f, 0xff, 0x0d
						  };
const u8 FW_RFChannel[6*14]	= { 0x28, 0x17, 0x14,		/* Channel 1 */
							0xba, 0xe8, 0x1a,		
							0x37, 0x17, 0x14,		/* Channel 2 */
							0x46, 0x17, 0x19,		
							0x45, 0x17, 0x14,		/* Channel 3 */
							0xd1, 0x45, 0x1b,		
							0x54, 0x17, 0x14,		/* Channel 4 */
							0x5d, 0x74, 0x19,		
							0x62, 0x17, 0x14,		/* Channel 5 */
							0xe9, 0xa2, 0x1b,		
							0x71, 0x17, 0x14,		/* Channel 6 */
							0x74, 0xd1, 0x19,		
							0x80, 0x17, 0x14,		/* Channel 7 */
							0x00, 0x00, 0x18,
							0x8e, 0x17, 0x14,		/* Channel 8 */
							0x8c, 0x2e, 0x1a,
							0x9d, 0x17, 0x14,		/* Channel 9 */
							0x17, 0x5d, 0x18,
							0xab, 0x17, 0x14,		/* Channel 10 */
							0xa3, 0x8b, 0x1a,
							0xba, 0x17, 0x14,		/* Channel 11 */
							0x2f, 0xba, 0x18,
							0xc8, 0x17, 0x14,		/* Channel 12 */
							0xba, 0xe8, 0x1a,
							0xd7, 0x17, 0x14,		/* Channel 13 */
							0x46, 0x17, 0x19,
							0xfa, 0x17, 0x14,		/* Channel 14 */
							0x2f, 0xba, 0x18
						  };
const u8 FW_BBChannel[14]		= { 0xb3, 0xb3, 0xb3, 0xb3, 0xb3,	/* channel  1- 6 */
							0xb4, 0xb4, 0xb4, 0xb4, 0xb4,	/* channel  7-10 */ 
							0xb5, 0xb5,						/* channel 11-12 */
							0xb6, 0xb6						/* channel 13-14 */
						  };

FW_WFCProfile FW_WFCProfile1 = {"SoftAP",
								"",
								"",
								"",
								"",
								"",
								{0, 0, 0, 0},
								{0, 0, 0, 0},
								{0, 0, 0, 0},
								{0, 0, 0, 0},
								0,
								"",
								0,
								0,
								0,
								{0, 0, 0, 0, 0, 0, 0},
								0,
								{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
								{0, 0}
							   };

FW_WFCProfile FW_WFCProfile2 = {"",
								"",
								"",
								"",
								"",
								"",
								{0, 0, 0, 0},
								{0, 0, 0, 0},
								{0, 0, 0, 0},
								{0, 0, 0, 0},
								0,
								"",
								0,
								0,
								0xFF,
								{0, 0, 0, 0, 0, 0, 0},
								0,
								{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
								{0, 0}
							   };

FW_WFCProfile FW_WFCProfile3 = {"",
								"",
								"",
								"",
								"",
								"",
								{0, 0, 0, 0},
								{0, 0, 0, 0},
								{0, 0, 0, 0},
								{0, 0, 0, 0},
								0,
								"",
								0,
								0,
								0xFF,
								{0, 0, 0, 0, 0, 0, 0},
								0,
								{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
								{0, 0}
							   };

/*******************************************************************************

	Communication interface

 *******************************************************************************/

struct WifiComInterface
{
	bool (*Init)();
	void (*DeInit)();
	void (*Reset)();
	void (*SendPacket)(u8* packet, u32 len);
	void (*usTrigger)();
};

#ifdef EXPERIMENTAL_WIFI_COMM
bool SoftAP_Init();
void SoftAP_DeInit();
void SoftAP_Reset();
void SoftAP_SendPacket(u8 *packet, u32 len);
void SoftAP_usTrigger();

WifiComInterface CI_SoftAP = {
	SoftAP_Init,
	SoftAP_DeInit,
	SoftAP_Reset,
	SoftAP_SendPacket,
	SoftAP_usTrigger
};

bool Adhoc_Init();
void Adhoc_DeInit();
void Adhoc_Reset();
void Adhoc_SendPacket(u8* packet, u32 len);
void Adhoc_usTrigger();

WifiComInterface CI_Adhoc = {
        Adhoc_Init,
        Adhoc_DeInit,
        Adhoc_Reset,
        Adhoc_SendPacket,
        Adhoc_usTrigger
};
#endif

WifiComInterface* wifiComs[] = {
#ifdef EXPERIMENTAL_WIFI_COMM
	&CI_Adhoc,
	&CI_SoftAP,
#endif
	NULL
};
WifiComInterface* wifiCom;

/*******************************************************************************

	Logging

 *******************************************************************************/

// 0: disable logging
// 1: lowest logging, shows most important messages such as errors
// 2: low logging, shows things such as warnings
// 3: medium logging, for debugging, shows lots of stuff
// 4: high logging, for debugging, shows almost everything, may slow down
// 5: highest logging, for debugging, shows everything, may slow down a lot
#define WIFI_LOGGING_LEVEL 1

#define WIFI_LOG_USE_LOGC 0

#if (WIFI_LOGGING_LEVEL >= 1)
	#if WIFI_LOG_USE_LOGC
		#define WIFI_LOG(level, ...) if(level <= WIFI_LOGGING_LEVEL) LOGC(8, "WIFI: " __VA_ARGS__);
	#else
		#define WIFI_LOG(level, ...) if(level <= WIFI_LOGGING_LEVEL) printf("WIFI: " __VA_ARGS__);
	#endif
#else
#define WIFI_LOG(level, ...) {}
#endif

/*******************************************************************************

	Helpers

 *******************************************************************************/

INLINE u32 WIFI_alignedLen(u32 len)
{
	return ((len + 3) & ~3);
}

// Fast MAC compares

INLINE bool WIFI_compareMAC(u8* a, u8* b)
{
	return ((*(u32*)&a[0]) == (*(u32*)&b[0])) && ((*(u16*)&a[4]) == (*(u16*)&b[4]));
}

INLINE bool WIFI_isBroadcastMAC(u8* a)
{
	return ((*(u32*)&a[0]) == 0xFFFFFFFF) && ((*(u16*)&a[4]) == 0xFFFF);
}

/*******************************************************************************

	CRC32 (http://www.codeproject.com/KB/recipes/crc32_large.aspx)

 *******************************************************************************/

u32 WIFI_CRC32Table[256];

static u32 reflect(u32 ref, char ch)
{
    u32 value = 0;

    for(int i = 1; i < (ch + 1); i++)
    {
        if (ref & 1)
            value |= 1 << (ch - i);
        ref >>= 1;
    } 
	
	return value;
}

static u32 WIFI_calcCRC32(u8 *data, int len)
{
	u32 crc = 0xFFFFFFFF;

	while(len--)
        crc = (crc >> 8) ^ WIFI_CRC32Table[(crc & 0xFF) ^ *data++];

	return (crc ^ 0xFFFFFFFF);
}

static void WIFI_initCRC32Table()
{
	static bool initialized = false;
	if(initialized) return;
	initialized = true;

	u32 polynomial = 0x04C11DB7;

	for(int i = 0; i < 0x100; i++)
    {
        WIFI_CRC32Table[i] = reflect(i, 8) << 24;
        for(int j = 0; j < 8; j++)
            WIFI_CRC32Table[i] = (WIFI_CRC32Table[i] << 1) ^ (WIFI_CRC32Table[i] & (1 << 31) ? polynomial : 0);
        WIFI_CRC32Table[i] = reflect(WIFI_CRC32Table[i],  32);
    }
}

/*******************************************************************************

	RF-Chip

 *******************************************************************************/

static void WIFI_resetRF(rffilter_t *rf) 
{
	/* reinitialize RF chip with the default values refer RF2958 docs */
	/* CFG1 */
	rf->CFG1.bits.IF_VGA_REG_EN = 1;
	rf->CFG1.bits.IF_VCO_REG_EN = 1;
	rf->CFG1.bits.RF_VCO_REG_EN = 1;
	rf->CFG1.bits.HYBERNATE = 0;
	rf->CFG1.bits.REF_SEL = 0;
	/* IFPLL1 */
	rf->IFPLL1.bits.DAC = 3;
	rf->IFPLL1.bits.P1 = 0;
	rf->IFPLL1.bits.LD_EN1 = 0;
	rf->IFPLL1.bits.AUTOCAL_EN1 = 0;
	rf->IFPLL1.bits.PDP1 = 1;
	rf->IFPLL1.bits.CPL1 = 0;
	rf->IFPLL1.bits.LPF1 = 0;
	rf->IFPLL1.bits.VTC_EN1 = 1;
	rf->IFPLL1.bits.KV_EN1 = 0;
	rf->IFPLL1.bits.PLL_EN1 = 0;
	/* IFPLL2 */
	rf->IFPLL2.bits.IF_N = 0x22;
	/* IFPLL3 */
	rf->IFPLL3.bits.KV_DEF1 = 8;
	rf->IFPLL3.bits.CT_DEF1 = 7;
	rf->IFPLL3.bits.DN1 = 0x1FF;
	/* RFPLL1 */
	rf->RFPLL1.bits.DAC = 3;
	rf->RFPLL1.bits.P = 0;
	rf->RFPLL1.bits.LD_EN = 0;
	rf->RFPLL1.bits.AUTOCAL_EN = 0;
	rf->RFPLL1.bits.PDP = 1;
	rf->RFPLL1.bits.CPL = 0;
	rf->RFPLL1.bits.LPF = 0;
	rf->RFPLL1.bits.VTC_EN = 0;
	rf->RFPLL1.bits.KV_EN = 0;
	rf->RFPLL1.bits.PLL_EN = 0;
	/* RFPLL2 */
	rf->RFPLL2.bits.NUM2 = 0;
	rf->RFPLL2.bits.N2 = 0x5E;
	/* RFPLL3 */
	rf->RFPLL3.bits.NUM2 = 0;
	/* RFPLL4 */
	rf->RFPLL4.bits.KV_DEF = 8;
	rf->RFPLL4.bits.CT_DEF = 7;
	rf->RFPLL4.bits.DN = 0x145;
	/* CAL1 */
	rf->CAL1.bits.LD_WINDOW = 2;
	rf->CAL1.bits.M_CT_VALUE = 8;
	rf->CAL1.bits.TLOCK = 7;
	rf->CAL1.bits.TVCO = 0x0F;
	/* TXRX1 */
	rf->TXRX1.bits.TXBYPASS = 0;
	rf->TXRX1.bits.INTBIASEN = 0;
	rf->TXRX1.bits.TXENMODE = 0;
	rf->TXRX1.bits.TXDIFFMODE = 0;
	rf->TXRX1.bits.TXLPFBW = 2;
	rf->TXRX1.bits.RXLPFBW = 2;
	rf->TXRX1.bits.TXVGC = 0;
	rf->TXRX1.bits.PCONTROL = 0;
	rf->TXRX1.bits.RXDCFBBYPS = 0;
	/* PCNT1 */
	rf->PCNT1.bits.TX_DELAY = 0;
	rf->PCNT1.bits.PC_OFFSET = 0;
	rf->PCNT1.bits.P_DESIRED = 0;
	rf->PCNT1.bits.MID_BIAS = 0;
	/* PCNT2 */
	rf->PCNT2.bits.MIN_POWER = 0;
	rf->PCNT2.bits.MID_POWER = 0;
	rf->PCNT2.bits.MAX_POWER = 0;
	/* VCOT1 */
	rf->VCOT1.bits.AUX1 = 0;
	rf->VCOT1.bits.AUX = 0;
}


void WIFI_setRF_CNT(u16 val)
{
	if (!wifiMac.rfIOStatus.bits.busy)
		wifiMac.rfIOCnt.val = val;
}

void WIFI_setRF_DATA(u16 val, u8 part)
{
	if (!wifiMac.rfIOStatus.bits.busy)
	{
        rfIOData_t *rfreg = (rfIOData_t *)&wifiMac.RF;
		switch (wifiMac.rfIOCnt.bits.readOperation)
		{
			case 1: /* read from RF chip */
				/* low part of data is ignored on reads */
				/* on high part, the address is read, and the data at this is written back */
				if (part==1)
				{
					wifiMac.rfIOData.array16[part] = val;
					if (wifiMac.rfIOData.bits.address > (sizeof(wifiMac.RF) / 4)) return; /* out of bound */
					/* get content of the addressed register */
					wifiMac.rfIOData.bits.content = rfreg[wifiMac.rfIOData.bits.address].bits.content;
				}
				break;
			case 0: /* write to RF chip */
				wifiMac.rfIOData.array16[part] = val;
				if (wifiMac.rfIOData.bits.address > (sizeof(wifiMac.RF) / 4)) return; /* out of bound */
				/* the actual transfer is done on high part write */
				if (part==1)
				{
					switch (wifiMac.rfIOData.bits.address)
					{
						case 5:		/* write to upper part of the frequency filter */
						case 6:		/* write to lower part of the frequency filter */
							{
								u32 channelFreqN;
								rfreg[wifiMac.rfIOData.bits.address].bits.content = wifiMac.rfIOData.bits.content;
								/* get the complete rfpll.n */
								channelFreqN = (u32)wifiMac.RF.RFPLL3.bits.NUM2 + ((u32)wifiMac.RF.RFPLL2.bits.NUM2 << 18) + ((u32)wifiMac.RF.RFPLL2.bits.N2 << 24);
								/* frequency setting is out of range */
								if (channelFreqN<0x00A2E8BA) return;
								/* substract base frequency (channel 1) */
								channelFreqN -= 0x00A2E8BA;
							}
							return;
						case 13:
							/* special purpose register: TEST1, on write, the RF chip resets */
							WIFI_resetRF(&wifiMac.RF);
							return;
					}
					/* set content of the addressed register */
					rfreg[wifiMac.rfIOData.bits.address].bits.content = wifiMac.rfIOData.bits.content;
				}
				break;
		}
	}
}

u16 WIFI_getRF_DATA(u8 part)
{
	if (!wifiMac.rfIOStatus.bits.busy)
		return wifiMac.rfIOData.array16[part];
	else
		/* data is not (yet) available */
		return 0;
 }

u16 WIFI_getRF_STATUS()
{
	return wifiMac.rfIOStatus.val;
}

/*******************************************************************************

	BB-Chip

 *******************************************************************************/

void WIFI_setBB_CNT(u16 val)
{
	wifiMac.bbIOCnt.val = val;

	if(wifiMac.bbIOCnt.bits.mode == 1)
		wifiMac.BB.data[wifiMac.bbIOCnt.bits.address] = WIFI_IOREG(REG_WIFI_BBWRITE);
}

u8 WIFI_getBB_DATA()
{
	if((!wifiMac.bbIOCnt.bits.enable) || (wifiMac.bbIOCnt.bits.mode != 2))
		return 0;

	return wifiMac.BB.data[wifiMac.bbIOCnt.bits.address];
}

/*******************************************************************************

	wifimac IO: a lot of the wifi regs are action registers, that are mirrored
				without action, so the default IO via MMU.c does not seem to
				be very suitable

				all registers are 16 bit

 *******************************************************************************/

static void WIFI_BeaconTXStart();

static void WIFI_triggerIRQMask(u16 mask)
{
	u16 oResult,nResult;
	oResult = wifiMac.IE & wifiMac.IF;
	wifiMac.IF = wifiMac.IF | (mask & ~0x0400);
	nResult = wifiMac.IE & wifiMac.IF;

	if (!oResult && nResult)
	{
		NDS_makeIrq(ARMCPU_ARM7,IRQ_BIT_ARM7_WIFI);   /* cascade it via arm7 wifi irq */
	}
}

static void WIFI_triggerIRQ(u8 irq)
{
	switch (irq)
	{
	case WIFI_IRQ_TXSTART:
		wifiMac.TXSeqNo++;
		break;
	case WIFI_IRQ_TIMEPREBEACON:
		if (wifiMac.TXPower & 0x0001)
		{
			wifiMac.rfStatus = 1;
			wifiMac.rfPins = 0x0084;
		}
		break;
	case WIFI_IRQ_TIMEBEACON:
		wifiMac.BeaconCount1 = wifiMac.BeaconInterval;

		if (wifiMac.ucmpEnable)
		{
			wifiMac.BeaconCount2 = 0xFFFF;
			wifiMac.TXCnt &= 0xFFF2;

			WIFI_BeaconTXStart();

			if (wifiMac.ListenCount == 0) wifiMac.ListenCount = wifiMac.ListenInterval;
			wifiMac.ListenCount--;
		}
		break;
	case WIFI_IRQ_TIMEPOSTBEACON:
		if (wifiMac.TXPower & 0x0002)
		{
			wifiMac.rfStatus = 9;
			wifiMac.rfPins = 0x0004;
		}
		break;
	case WIFI_IRQ_UNK:
		WIFI_LOG(2, "IRQ 12 triggered.\n");
		wifiMac.TXSeqNo++;
		break;
	}

	WIFI_triggerIRQMask(1<<irq);
}


bool WIFI_Init()
{
	memset(&wifiMac, 0, sizeof(wifimac_t));

	WIFI_initCRC32Table();

	WIFI_resetRF(&wifiMac.RF);

	wifiMac.powerOn = FALSE;
	wifiMac.powerOnPending = FALSE;
	
	wifiMac.rfStatus = 0x0000;
	wifiMac.rfPins = 0x0004;

	if((u32)CommonSettings.wifi.mode >= ARRAY_SIZE(wifiComs))
		CommonSettings.wifi.mode = 0;
	wifiCom = wifiComs[CommonSettings.wifi.mode];
	if(wifiCom)
		wifiCom->Init();

	bWFCUserWarned = false;

	return true;
}

void WIFI_DeInit()
{
	if(wifiCom)
		wifiCom->DeInit();
}

void WIFI_Reset()
{
	memset(&wifiMac, 0, sizeof(wifimac_t));

	WIFI_resetRF(&wifiMac.RF);

	wifiMac.powerOn = FALSE;
	wifiMac.powerOnPending = FALSE;
	
	wifiMac.rfStatus = 0x0000;
	wifiMac.rfPins = 0x0004;

	if((u32)CommonSettings.wifi.mode >= ARRAY_SIZE(wifiComs))
		CommonSettings.wifi.mode = 0;
	wifiCom = wifiComs[CommonSettings.wifi.mode];
	if(wifiCom)
		wifiCom->Reset();

	bWFCUserWarned = false;
}


INLINE u16 WIFI_GetRXFlags(u8* packet)
{
	u16 ret = 0x0010;
	u16 frameCtl = *(u16*)&packet[0];

	switch(frameCtl & 0x000C)
	{
	case 0x0000:  // Management frame
		if ((frameCtl & 0x00F0) == 0x0080)
			ret |= 0x0001;
		break;

	case 0x0004:  // Control frame
		ret |= 0x0005;
		break;

	case 0x0008:  // Data frame
		if (frameCtl == 0x0228)
			ret |= 0x000C;
		else
			ret |= 0x0008;
		break;
	}

	if (frameCtl & 0x0400)
		ret |= 0x0100;

	if (!memcmp(&packet[10], &wifiMac.bss.bytes[0], 6))
		ret |= 0x8000;

	//printf("----- Computing RX flags for received frame: FrameCtl = %04X, Flags = %04X -----\n", frameCtl, ret);

	return ret;
}

INLINE void WIFI_MakeRXHeader(u8* buf, u16 flags, u16 xferRate, u16 len, u8 maxRSSI, u8 minRSSI)
{
	*(u16*)&buf[0] = flags;

	// Unknown (usually 0x0040)
	buf[2] = 0x40;
	buf[3] = 0x00;

	// Time since last packet??? Random??? Left unchanged???
	buf[4] = 0x01;
	buf[5] = 0x00;

	*(u16*)&buf[6] = xferRate;

	*(u16*)&buf[8] = len;

	buf[10] = 253;//maxRSSI;
	buf[11] = 255;//minRSSI;
}

#ifdef EXPERIMENTAL_WIFI_COMM
static void WIFI_RXPutWord(u16 val)
{
	/* abort when RX data queuing is not enabled */
	if (!(wifiMac.RXCnt & 0x8000)) return;
	/* abort when ringbuffer is full */
	//if (wifiMac.RXReadCursor == wifiMac.RXWriteCursor) return;
	/*if(wifiMac.RXWriteCursor >= wifiMac.RXReadCursor) 
	{
		printf("WIFI: write cursor (%04X) above READCSR (%04X). Cannot write received packet.\n", 
			wifiMac.RXWriteCursor, wifiMac.RXReadCursor);
		return;
	}*/
	/* write the data to cursor position */
	wifiMac.RAM[wifiMac.RXWriteCursor & 0xFFF] = val;
//	printf("wifi: written word %04X to circbuf addr %04X\n", val, (wifiMac.RXWriteCursor << 1));
	/* move cursor by one */
	//printf("written one word to %04X (start %04X, end %04X), ", wifiMac.RXWriteCursor, wifiMac.RXRangeBegin, wifiMac.RXRangeEnd);
	wifiMac.RXWriteCursor++;
	/* wrap around */
//	wifiMac.RXWriteCursor %= (wifiMac.RXRangeEnd - wifiMac.RXRangeBegin) >> 1;
//	printf("new addr=%04X\n", wifiMac.RXWriteCursor);
	if(wifiMac.RXWriteCursor >= ((wifiMac.RXRangeEnd & 0x1FFE) >> 1))
		wifiMac.RXWriteCursor = ((wifiMac.RXRangeBegin & 0x1FFE) >> 1);
}
#endif

static void WIFI_TXStart(u8 slot)
{
	WIFI_LOG(3, "TX slot %i trying to send a packet: TXCnt = %04X, TXBufLoc = %04X\n", 
		slot, wifiMac.TXCnt, wifiMac.TXSlot[slot]);

	if (BIT15(wifiMac.TXSlot[slot]))	/* is slot enabled? */
	{
		//printf("send packet at %08X, lr=%08X\n", NDS_ARM7.instruct_adr, NDS_ARM7.R[14]);
		u16 txLen;
		// the address has to be somewhere in the circular buffer, so drop the other bits
		u16 address = (wifiMac.TXSlot[slot] & 0x0FFF);
		// is there even enough space for the header (6 hwords) in the tx buffer?
		if (address > 0x1000-6)
		{
			WIFI_LOG(1, "TX slot %i trying to send a packet overflowing from the TX buffer (address %04X). Attempt ignored.\n", 
				slot, (address << 1));
			return;
		}

		//printf("---------- SENDING A PACKET ON SLOT %i, FrameCtl = %04X ----------\n",
		//	slot, wifiMac.RAM[address+6]);

		// 12 byte header TX Header: http://www.akkit.org/info/dswifi.htm#FmtTx
		txLen = wifiMac.RAM[address+5];
		// zero length
		if (txLen == 0)
		{
			WIFI_LOG(1, "TX slot %i trying to send a packet with length field set to zero. Attempt ignored.\n", 
				slot);
			return;
		}

		// Align packet length
		txLen = WIFI_alignedLen(txLen);

		// unsupported txRate
		switch (wifiMac.RAM[address+4] & 0xFF)
		{
			case 10: // 1 mbit
			case 20: // 2 mbit
				break;
			default: // other rates
				WIFI_LOG(1, "TX slot %i trying to send a packet with transfer rate field set to an invalid value of %i. Attempt ignored.\n", 
					slot, wifiMac.RAM[address+4] & 0xFF);
				return;
		}

		// Set sequence number if required
		if (!BIT13(wifiMac.TXSlot[slot]))
		{
		//	u16 seqctl = wifiMac.RAM[address + 6 + 22];
		//	wifiMac.RAM[address + 6 + 11] = (seqctl & 0x000F) | (wifiMac.TXSeqNo << 4);
			wifiMac.RAM[address + 6 + 11] = wifiMac.TXSeqNo << 4;
		}

		// Calculate and set FCS
		u32 crc32 = WIFI_calcCRC32((u8*)&wifiMac.RAM[address + 6], txLen - 4);
		*(u32*)&wifiMac.RAM[address + 6 + ((txLen-4) >> 1)] = crc32;

		WIFI_triggerIRQ(WIFI_IRQ_TXSTART);

		if(slot > wifiMac.txCurSlot)
			wifiMac.txCurSlot = slot;

		wifiMac.txSlotBusy[slot] = 1;
		wifiMac.txSlotAddr[slot] = address;
		wifiMac.txSlotLen[slot] = txLen;
		wifiMac.txSlotRemainingBytes[slot] = (txLen + 12);

		wifiMac.RXTXAddr = address;

		wifiMac.rfStatus = 0x0003;
		wifiMac.rfPins = 0x0046;

#if 0
		WIFI_SoftAP_RecvPacketFromDS((u8*)&wifiMac.RAM[address+6], txLen);
		WIFI_triggerIRQ(WIFI_IRQ_TXEND);

		wifiMac.RAM[address] = 0x0001;
		wifiMac.RAM[address+4] &= 0x00FF;
#endif
	}
}

static void WIFI_ExtraTXStart()
{
	if (BIT15(wifiMac.TXSlotExtra))
	{
		u16 txLen;
		u16 address = wifiMac.TXSlotExtra & 0x0FFF;
		// is there even enough space for the header (6 hwords) in the tx buffer?
		if (address > 0x1000-6)
		{
			return;
		}

		//printf("---------- SENDING A PACKET ON EXTRA SLOT, FrameCtl = %04X ----------\n",
		//	wifiMac.RAM[address+6]);

		// 12 byte header TX Header: http://www.akkit.org/info/dswifi.htm#FmtTx
		txLen = wifiMac.RAM[address+5];
		// zero length 
		if (txLen == 0)
		{
			return;
		}

		// Align packet length
		txLen = WIFI_alignedLen(txLen);

		// unsupported txRate
		switch (wifiMac.RAM[address+4] & 0xFF)
		{
			case 10: // 1 mbit
			case 20: // 2 mbit
				break;
			default: // other rates
				return;
		}

		// Set sequence number if required
		if (!BIT13(wifiMac.TXSlotExtra))
		{
			//u16 seqctl = wifiMac.RAM[address + 6 + 22];
			//wifiMac.RAM[address + 6 + 11] = (seqctl & 0x000F) | (wifiMac.TXSeqNo << 4);
			wifiMac.RAM[address + 6 + 11] = wifiMac.TXSeqNo << 4;
		}

		// Calculate and set FCS
		u32 crc32 = WIFI_calcCRC32((u8*)&wifiMac.RAM[address + 6], txLen - 4);
		*(u32*)&wifiMac.RAM[address + 6 + ((txLen-4) >> 1)] = crc32;

		// Note: Extra transfers trigger two TX start interrupts according to GBATek
		WIFI_triggerIRQ(WIFI_IRQ_TXSTART);
		if(wifiCom)
			wifiCom->SendPacket((u8*)&wifiMac.RAM[address+6], txLen);
		WIFI_triggerIRQ(WIFI_IRQ_UNK);

		if (BIT13(wifiMac.TXStatCnt))
		{
			WIFI_triggerIRQ(WIFI_IRQ_TXEND);
			wifiMac.TXStat = 0x0B01;
		}
		else if (BIT14(wifiMac.TXStatCnt))
		{
			WIFI_triggerIRQ(WIFI_IRQ_TXEND);
			wifiMac.TXStat = 0x0801;
		}

		wifiMac.TXSlotExtra &= 0x7FFF;

		wifiMac.RAM[address] = 0x0001;
		wifiMac.RAM[address+4] &= 0x00FF;
	}
}

static void WIFI_BeaconTXStart()
{
	if (wifiMac.BeaconEnable)
	{
		u16 txLen;
		u16 address = wifiMac.BeaconAddr;
		// is there even enough space for the header (6 hwords) in the tx buffer?
		if (address > 0x1000-6)
		{
			return;
		}

		// 12 byte header TX Header: http://www.akkit.org/info/dswifi.htm#FmtTx
		txLen = wifiMac.RAM[address+5];
		// zero length 
		if (txLen == 0)
		{
			return;
		}

		// Align packet length
		txLen = WIFI_alignedLen(txLen);

		// unsupported txRate
		switch (wifiMac.RAM[address+4] & 0xFF)
		{
			case 10: // 1 mbit
			case 20: // 2 mbit
				break;
			default: // other rates
				return;
		}

		// Set sequence number
		//u16 seqctl = wifiMac.RAM[address + 6 + 22];
		//wifiMac.RAM[address + 6 + 11] = (seqctl & 0x000F) | (wifiMac.TXSeqNo << 4);
		wifiMac.RAM[address + 6 + 11] = wifiMac.TXSeqNo << 4;

		// Set timestamp
		*(u64*)&wifiMac.RAM[address + 6 + 12] = wifiMac.usec;

		// Calculate and set FCS
		u32 crc32 = WIFI_calcCRC32((u8*)&wifiMac.RAM[address + 6], txLen - 4);
		*(u32*)&wifiMac.RAM[address + 6 + ((txLen-4) >> 1)] = crc32;

		WIFI_triggerIRQ(WIFI_IRQ_TXSTART);
		if(wifiCom)
			wifiCom->SendPacket((u8*)&wifiMac.RAM[address+6], txLen);

		if (BIT15(wifiMac.TXStatCnt))
		{
			WIFI_triggerIRQ(WIFI_IRQ_TXEND);
			wifiMac.TXStat = 0x0301;
		}

		wifiMac.RAM[address] = 0x0001;
		wifiMac.RAM[address+4] &= 0x00FF;
	}
}

void WIFI_write16(u32 address, u16 val)
{
	BOOL action = FALSE;
	if (!nds.power2.wifi) return;

	u32 page = address & 0x7000;

	// 0x2000 - 0x3FFF: unused
	if ((page >= 0x2000) && (page < 0x4000))
		return;

	WIFI_LOG(5, "Write at address %08X, %04X\n", address, val);
	//printf("WIFI: Write at address %08X, %04X, pc=%08X\n", address, val, NDS_ARM7.instruct_adr);

	// 0x4000 - 0x5FFF: wifi RAM
	if ((page >= 0x4000) && (page < 0x6000))
	{
		/* access to the circular buffer */
		address &= 0x1FFF;
        wifiMac.RAM[address >> 1] = val;
		return;
	}

	// anything else: I/O ports
	// only the first mirror (0x0000 - 0x0FFF) causes a special action
	if (page == 0x0000) action = TRUE;

	address &= 0x0FFF;
	switch (address)
	{
		case REG_WIFI_ID:
			break;
		case REG_WIFI_MODE:
			{
				u16 oldval = wifiMac.macMode;

				if (!BIT0(oldval) && BIT0(val))
				{
					WIFI_IOREG(0x034)					= 0x0002;
					wifiMac.rfPins 						= 0x0046;
					wifiMac.rfStatus 					= 0x0009;
					WIFI_IOREG(0x27C)					= 0x0005;
				}

				if (BIT0(oldval) && !BIT0(val))
				{
					WIFI_IOREG(0x27C)		 			= 0x000A;
				}

				if (BIT13(val))
				{
					WIFI_IOREG(REG_WIFI_WRITECSRLATCH) 	= 0x0000;
					WIFI_IOREG(0x0C0)		 			= 0x0000;
					WIFI_IOREG(0x0C4)		 			= 0x0000;
					WIFI_IOREG(REG_WIFI_MAYBE_RATE)		= 0x0000;
					WIFI_IOREG(0x278)		 			= 0x000F;
				}

				if (BIT14(val))
				{
					wifiMac.wepMode 					= 0x0000;
					wifiMac.TXStatCnt					= 0x0000;
					WIFI_IOREG(REG_WIFI_0A)				= 0x0000;
					wifiMac.mac.words[0]				= 0x0000;
					wifiMac.mac.words[1]				= 0x0000;
					wifiMac.mac.words[2]				= 0x0000;
					wifiMac.bss.words[0]				= 0x0000;
					wifiMac.bss.words[1]				= 0x0000;
					wifiMac.bss.words[2]				= 0x0000;
					wifiMac.pid							= 0x0000;
					wifiMac.aid 						= 0x0000;
					WIFI_IOREG(REG_WIFI_RETRYLIMIT)		= 0x0707;
					WIFI_IOREG(0x02E)		 			= 0x0000;
					wifiMac.RXRangeBegin				= 0x4000;
					wifiMac.RXRangeEnd					= 0x4800;
					WIFI_IOREG(0x084)					= 0x0000;
					WIFI_IOREG(REG_WIFI_PREAMBLE) 		= 0x0001;
					WIFI_IOREG(REG_WIFI_RXFILTER) 		= 0x0401;
					WIFI_IOREG(0x0D4)					= 0x0001;
					WIFI_IOREG(REG_WIFI_RXFILTER2)		= 0x0008;
					WIFI_IOREG(0x0EC)		 			= 0x3F03;
					WIFI_IOREG(0x194)		 			= 0x0000;
					WIFI_IOREG(0x198) 					= 0x0000;
					WIFI_IOREG(0x1A2)		 			= 0x0001;
					WIFI_IOREG(0x224)		 			= 0x0003;
					WIFI_IOREG(0x230)		 			= 0x0047;
				}

				wifiMac.macMode = val & 0xAFFF;
			}
			break;
		case REG_WIFI_WEP:
			wifiMac.wepMode = val;
			break;
		case REG_WIFI_TXSTATCNT:
			wifiMac.TXStatCnt = val;
			//printf("txstatcnt=%04X\n", val);
			break;
		case REG_WIFI_IE:
			wifiMac.IE = val;
			//printf("wifi ie write %04X\n", val);
			break;
		case REG_WIFI_IF:
			wifiMac.IF &= ~val;		/* clear flagging bits */
			break;
		case REG_WIFI_MAC0:
		case REG_WIFI_MAC1:
		case REG_WIFI_MAC2:
			wifiMac.mac.words[(address - REG_WIFI_MAC0) >> 1] = val;
			break;
		case REG_WIFI_BSS0:
		case REG_WIFI_BSS1:
		case REG_WIFI_BSS2:
			wifiMac.bss.words[(address - REG_WIFI_BSS0) >> 1] = val;
			break;
		case REG_WIFI_RETRYLIMIT:
			wifiMac.retryLimit = val;
			break;
		case REG_WIFI_WEPCNT:
			wifiMac.WEP_enable = (val & 0x8000) != 0;
			break;
		case REG_WIFI_POWERSTATE:
			wifiMac.powerOn = ((val & 0x0002)?TRUE:FALSE);
			if(wifiMac.powerOn) WIFI_triggerIRQ(WIFI_IRQ_RFWAKEUP);
			break;
		case REG_WIFI_POWERFORCE:
			if((val & 0x8000) && (!wifiMac.powerOnPending))
			{
			/*	BOOL newPower = ((val & 0x0001)?FALSE:TRUE);
				if(newPower != wifiMac.powerOn)
				{
					if(!newPower)
						wifiMac.powerOn = FALSE;
					else
						wifiMac.powerOnPending = TRUE;
				}*/
				wifiMac.powerOn = ((val & 0x0001) ? FALSE : TRUE);
			}
			break;
		case REG_WIFI_POWERACK:
			if((val == 0x0000) && wifiMac.powerOnPending)
			{
				wifiMac.powerOn = TRUE;
				wifiMac.powerOnPending = FALSE;
			}
			break;
		case REG_WIFI_POWER_TX:
			wifiMac.TXPower = val & 0x0007;
			break;
		case REG_WIFI_RXCNT:
			wifiMac.RXCnt = val & 0xFF0E;
			if(BIT0(val))
			{
				wifiMac.RXWriteCursor = WIFI_IOREG(REG_WIFI_WRITECSRLATCH);
				WIFI_IOREG(REG_WIFI_RXHWWRITECSR) = wifiMac.RXWriteCursor;
			}
			break;
		case REG_WIFI_RXRANGEBEGIN:
			wifiMac.RXRangeBegin = val;
			if(wifiMac.RXWriteCursor < ((val & 0x1FFE) >> 1))
				wifiMac.RXWriteCursor = ((val & 0x1FFE) >> 1);
			break;
		case REG_WIFI_RXRANGEEND:
			wifiMac.RXRangeEnd = val;
			if(wifiMac.RXWriteCursor >= ((val & 0x1FFE) >> 1))
				wifiMac.RXWriteCursor = ((wifiMac.RXRangeBegin & 0x1FFE) >> 1);
			break;

		case REG_WIFI_CIRCBUFRADR:
			wifiMac.CircBufReadAddress = (val & 0x1FFE);
			break;
		case REG_WIFI_RXREADCSR:
			wifiMac.RXReadCursor = val;
			break;
		case REG_WIFI_CIRCBUFWADR:
			wifiMac.CircBufWriteAddress = val;
			break;
		case REG_WIFI_CIRCBUFWRITE:
			/* set value into the circ buffer, and move cursor to the next hword on action */
			//printf("wifi: circbuf fifo write at %04X, %04X (action=%i)\n", (wifiMac.CircBufWriteAddress & 0x1FFF), val, action);
			wifiMac.RAM[(wifiMac.CircBufWriteAddress >> 1) & 0xFFF] = val;
			if (action)
			{
				/* move to next hword */
                wifiMac.CircBufWriteAddress+=2;
				if (wifiMac.CircBufWriteAddress == wifiMac.CircBufWrEnd)
				{
					/* on end of buffer, add skip hwords to it */
					wifiMac.CircBufWrEnd += wifiMac.CircBufWrSkip * 2;
				}
			}
			break;
		case REG_WIFI_CIRCBUFWR_SKIP:
			wifiMac.CircBufWrSkip = val;
			break;
		case REG_WIFI_TXLOCBEACON:
			wifiMac.BeaconAddr = val & 0x0FFF;
			wifiMac.BeaconEnable = BIT15(val);
			if (wifiMac.BeaconEnable) 
				WIFI_LOG(3, "Beacon transmission enabled to send the packet at %08X every %i milliseconds.\n",
					0x04804000 + (wifiMac.BeaconAddr << 1), wifiMac.BeaconInterval);
			break;
		case REG_WIFI_TXLOCEXTRA:
			wifiMac.TXSlotExtra = val;
			WIFI_LOG(2, "Write to port %03X: %04X\n", address, val);
			break;
		case 0x094:
		//	printf("write to 094 port\n");
			break;
		case 0x098:
		case 0x0C0:
		case 0x0C4:
		case 0x0C8:
		case 0x244:
		case 0x228:
		case 0x290:
		case 0x1A0:
		case 0x1A2:
		case 0x1A4:
		case 0x194:
			WIFI_LOG(2, "Write to port %03X: %04X\n", address, val);
			break;
		case REG_WIFI_TXLOC1:
		case REG_WIFI_TXLOC2:
		case REG_WIFI_TXLOC3:
			wifiMac.TXSlot[(address - REG_WIFI_TXLOC1) >> 2] = val;
			WIFI_LOG(2, "Write to port %03X: %04X\n", address, val);
			break;
		case REG_WIFI_TXRESET:
			WIFI_LOG(3, "Write to TXRESET: %04X\n", val);
			//if (val & 0x0001) wifiMac.TXSlot[0] &= 0x7FFF;
			//if (val & 0x0004) wifiMac.TXSlot[1] &= 0x7FFF;
			//if (val & 0x0008) wifiMac.TXSlot[2] &= 0x7FFF;
			break;
		case REG_WIFI_TXREQ_RESET:
			wifiMac.TXCnt &= ~val;
			break;
		case REG_WIFI_TXREQ_SET:
			wifiMac.TXCnt |= val;
			if (BIT0(val)) WIFI_TXStart(0);
			if (BIT1(val)) WIFI_ExtraTXStart();
			if (BIT2(val)) WIFI_TXStart(1);
			if (BIT3(val)) WIFI_TXStart(2);
			//if (val) printf("TXReq: %04X\n", val);
			break;
		case REG_WIFI_RFCNT:
			WIFI_setRF_CNT(val);
			break;
		case REG_WIFI_RFBUSY:
			/* CHECKME: read only? */
			break;
		case REG_WIFI_RFDATA1:
			WIFI_setRF_DATA(val,0);
			break;
		case REG_WIFI_RFDATA2:
			WIFI_setRF_DATA(val,1);
			break;
		case REG_WIFI_USCOUNTERCNT:
			wifiMac.usecEnable = BIT0(val);
			break;
		case REG_WIFI_USCOMPARECNT:
			wifiMac.ucmpEnable = BIT0(val);
			break;
		case REG_WIFI_USCOUNTER0:
			wifiMac.usec = (wifiMac.usec & 0xFFFFFFFFFFFF0000ULL) | (u64)val;
			break;
		case REG_WIFI_USCOUNTER1:
			wifiMac.usec = (wifiMac.usec & 0xFFFFFFFF0000FFFFULL) | (u64)val << 16;
			break;
		case REG_WIFI_USCOUNTER2:
			wifiMac.usec = (wifiMac.usec & 0xFFFF0000FFFFFFFFULL) | (u64)val << 32;
			break;
		case REG_WIFI_USCOUNTER3:
			wifiMac.usec = (wifiMac.usec & 0x0000FFFFFFFFFFFFULL) | (u64)val << 48;
			break;
		case REG_WIFI_USCOMPARE0:
			wifiMac.ucmp = (wifiMac.ucmp & 0xFFFFFFFFFFFF0000ULL) | (u64)(val & 0xFFFE);
			//if (BIT0(val))
		//		WIFI_triggerIRQ(14);
			//	wifiMac.usec = wifiMac.ucmp;
			break;
		case REG_WIFI_USCOMPARE1:
			wifiMac.ucmp = (wifiMac.ucmp & 0xFFFFFFFF0000FFFFULL) | (u64)val << 16;
			break;
		case REG_WIFI_USCOMPARE2:
			wifiMac.ucmp = (wifiMac.ucmp & 0xFFFF0000FFFFFFFFULL) | (u64)val << 32;
			break;
		case REG_WIFI_USCOMPARE3:
			wifiMac.ucmp = (wifiMac.ucmp & 0x0000FFFFFFFFFFFFULL) | (u64)val << 48;
			break;
		case REG_WIFI_BEACONPERIOD:
			wifiMac.BeaconInterval = val & 0x03FF;
			break;
		case REG_WIFI_BEACONCOUNT1:
			wifiMac.BeaconCount1 = val;
			break;
		case REG_WIFI_BEACONCOUNT2:
			wifiMac.BeaconCount2 = val;
			break;
		case REG_WIFI_BBCNT:
            WIFI_setBB_CNT(val);
			break;
		case REG_WIFI_RXBUF_COUNT:
			wifiMac.RXBufCount = val & 0x0FFF;
			break;
		case REG_WIFI_EXTRACOUNTCNT:
			wifiMac.eCountEnable = BIT0(val);
			break;
		case REG_WIFI_EXTRACOUNT:
			wifiMac.eCount = (u32)val * 10;
			break;
		case REG_WIFI_LISTENINT:
			wifiMac.ListenInterval = val & 0x00FF;
			break;
		case REG_WIFI_LISTENCOUNT:
			wifiMac.ListenCount = val & 0x00FF;
			break;
		case REG_WIFI_POWER_US:
			wifiMac.crystalEnabled = !BIT0(val);
			break;
		case REG_WIFI_IF_SET:
			WIFI_triggerIRQMask(val);
			break;
		case REG_WIFI_CIRCBUFRD_END:
			wifiMac.CircBufRdEnd = (val & 0x1FFE);
			break;
		case REG_WIFI_CIRCBUFRD_SKIP:
			wifiMac.CircBufRdSkip = val & 0xFFF;
			break;
		case REG_WIFI_AID_LOW:
			wifiMac.pid = val & 0x0F;
			break;
		case REG_WIFI_AID_HIGH:
			wifiMac.aid = val & 0x07FF;
			break;
		case 0xD0:
		//	printf("wifi: rxfilter=%04X\n", val);
			break;
		case 0x0E0:
		//	printf("wifi: rxfilter2=%04X\n", val);
			break;

		default:
			break;
	}

	WIFI_IOREG(address) = val;
}

u16 WIFI_read16(u32 address)
{
	BOOL action = FALSE;
	if (!nds.power2.wifi) return 0;

	u32 page = address & 0x7000;

	// 0x2000 - 0x3FFF: unused
	if ((page >= 0x2000) && (page < 0x4000))
		return 0xFFFF;

	WIFI_LOG(5, "Read at address %08X\n", address);

	// 0x4000 - 0x5FFF: wifi RAM
	if ((page >= 0x4000) && (page < 0x6000))
	{
        return wifiMac.RAM[(address & 0x1FFF) >> 1];
	}

	// anything else: I/O ports
	// only the first mirror causes a special action
	if (page == 0x0000) action = TRUE;

	address &= 0x0FFF;
	switch (address)
	{
		case REG_WIFI_ID:
			return WIFI_CHIPID;
		case REG_WIFI_MODE:
			return wifiMac.macMode;
		case REG_WIFI_WEP:
			return wifiMac.wepMode;
		case REG_WIFI_IE:
			return wifiMac.IE;
		case REG_WIFI_IF:
			return wifiMac.IF;
		case REG_WIFI_POWERSTATE:
			return ((wifiMac.powerOn ? 0x0000 : 0x0200) | (wifiMac.powerOnPending ? 0x0102 : 0x0000));
		case REG_WIFI_RFDATA1:
			return WIFI_getRF_DATA(0);
		case REG_WIFI_RFDATA2:
			return WIFI_getRF_DATA(1);
		case REG_WIFI_RFBUSY:
		case REG_WIFI_BBBUSY:
			return 0;	/* we are never busy :p */
		case REG_WIFI_BBREAD:
			return WIFI_getBB_DATA();
		case REG_WIFI_RANDOM:
			// probably not right, but it's better than using the unsaved and shared rand().
			// at the very least, rand() shouldn't be used when movieMode is active.
			{
				u16 returnValue = wifiMac.randomSeed;
				wifiMac.randomSeed = (wifiMac.randomSeed & 1) ^ (((wifiMac.randomSeed << 1) & 0x7FE) | ((wifiMac.randomSeed >> 10) & 0x1));
				return returnValue;
			}

			return 0;
		case REG_WIFI_MAC0:
		case REG_WIFI_MAC1:
		case REG_WIFI_MAC2:
			//printf("read mac addr: word %i = %02X\n", (address - REG_WIFI_MAC0) >> 1, wifiMac.mac.words[(address - REG_WIFI_MAC0) >> 1]);
			return wifiMac.mac.words[(address - REG_WIFI_MAC0) >> 1];
		case REG_WIFI_BSS0:
		case REG_WIFI_BSS1:
		case REG_WIFI_BSS2:
			//printf("read bssid addr: word %i = %02X\n", (address - REG_WIFI_BSS0) >> 1, wifiMac.bss.words[(address - REG_WIFI_BSS0) >> 1]);
			return wifiMac.bss.words[(address - REG_WIFI_BSS0) >> 1];
		case REG_WIFI_RXCNT:
			return wifiMac.RXCnt;
		case REG_WIFI_RXRANGEBEGIN:
			return wifiMac.RXRangeBegin;
		case REG_WIFI_CIRCBUFREAD:
			{
				u16 val = wifiMac.RAM[wifiMac.CircBufReadAddress >> 1];
				if (action)
				{
					wifiMac.CircBufReadAddress += 2;

					if (wifiMac.CircBufReadAddress >= wifiMac.RXRangeEnd) 
					{ 
						wifiMac.CircBufReadAddress = wifiMac.RXRangeBegin;
					} 
					else
					{
						/* skip does not fire after a reset */
						if (wifiMac.CircBufReadAddress == wifiMac.CircBufRdEnd)
						{
							wifiMac.CircBufReadAddress += wifiMac.CircBufRdSkip * 2;
							wifiMac.CircBufReadAddress &= 0x1FFE;
							if (wifiMac.CircBufReadAddress + wifiMac.RXRangeBegin == wifiMac.RXRangeEnd) wifiMac.CircBufReadAddress = 0;
						}
					}

					if (wifiMac.RXBufCount > 0)
					{
						if (wifiMac.RXBufCount == 1)
						{
							WIFI_triggerIRQ(WIFI_IRQ_RXCOUNTEXP);
						}
						wifiMac.RXBufCount--;
					}				
				}
				return val;
			}
		case REG_WIFI_CIRCBUFRADR:
			return wifiMac.CircBufReadAddress;
		case REG_WIFI_RXBUF_COUNT:
			return wifiMac.RXBufCount;
		case REG_WIFI_TXREQ_READ:
			//printf("read TX reg %04X\n", address);
			return wifiMac.TXCnt | 0x10;
		case REG_WIFI_TXBUSY:
			//printf("read TX reg %04X\n", address);
			return ((wifiMac.txSlotBusy[0] ? 0x01 : 0x00) | (wifiMac.txSlotBusy[1] ? 0x04 : 0x00) | (wifiMac.txSlotBusy[2] ? 0x08 : 0x00));
		case REG_WIFI_TXSTAT:
			//printf("read TX reg %04X\n", address);
			return wifiMac.TXStat;
		case REG_WIFI_TXLOCEXTRA:
			//printf("read TX reg %04X\n", address);
			return wifiMac.TXSlotExtra;
		case REG_WIFI_TXLOC1:
		case REG_WIFI_TXLOC2:
		case REG_WIFI_TXLOC3:
			//printf("read TX reg %04X\n", address);
			return wifiMac.TXSlot[(address - REG_WIFI_TXLOC1) >> 2];
		case REG_WIFI_TXLOCBEACON:
			//printf("read TX reg %04X\n", address);
			break;
		case REG_WIFI_EXTRACOUNTCNT:
			return wifiMac.eCountEnable?1:0;
		case REG_WIFI_EXTRACOUNT:
			return (u16)((wifiMac.eCount + 9) / 10);
		case REG_WIFI_USCOUNTER0:
			return (u16)wifiMac.usec;
		case REG_WIFI_USCOUNTER1:
			return (u16)(wifiMac.usec >> 16);
		case REG_WIFI_USCOUNTER2:
			return (u16)(wifiMac.usec >> 32);
		case REG_WIFI_USCOUNTER3:
			return (u16)(wifiMac.usec >> 48);
		case REG_WIFI_USCOMPARE0:
			return (u16)wifiMac.ucmp;
		case REG_WIFI_USCOMPARE1:
			return (u16)(wifiMac.ucmp >> 16);
		case REG_WIFI_USCOMPARE2:
			return (u16)(wifiMac.ucmp >> 32);
		case REG_WIFI_USCOMPARE3:
			return (u16)(wifiMac.ucmp >> 48);
		case REG_WIFI_BEACONCOUNT1:
			return wifiMac.BeaconCount1;
		case REG_WIFI_BEACONCOUNT2:
			return wifiMac.BeaconCount2;
		case REG_WIFI_LISTENCOUNT:
			return wifiMac.ListenCount;
		case REG_WIFI_POWER_US:
			return wifiMac.crystalEnabled?0:1;
		case REG_WIFI_CIRCBUFRD_END:
			return wifiMac.CircBufRdEnd;
		case REG_WIFI_CIRCBUFRD_SKIP:
			return wifiMac.CircBufRdSkip;
		case REG_WIFI_AID_LOW:
			return wifiMac.pid;
		case REG_WIFI_AID_HIGH:
			return wifiMac.aid;

			// RFSTATUS, RFPINS
			// TODO: figure out how to emulate those correctly
			// without breaking Nintendo's games
		case REG_WIFI_RFSTATUS:
			return 0x0009;
		case REG_WIFI_RFPINS:
			return 0x00C6;

		case 0x268:
			return wifiMac.RXTXAddr;

		default:
		//	printf("wifi: read unhandled reg %03X\n", address);
			break;
	}

	return WIFI_IOREG(address);
}


void WIFI_usTrigger()
{
	if (wifiMac.crystalEnabled)
	{
		/* a usec has passed */
		if (wifiMac.usecEnable)
			wifiMac.usec++;

		// Note: the extra counter is decremented every 10 microseconds.
		// To avoid a modulo every microsecond, we multiply the counter
		// value by 10 and decrement it every microsecond :)
		if (wifiMac.eCountEnable)
		{
			if (wifiMac.eCount > 0)
			{
				wifiMac.eCount--;
				if (wifiMac.eCount == 0)
					WIFI_ExtraTXStart();
			}
		}

		// The beacon counters are in milliseconds
		// GBATek says they're decremented every 1024 usecs
		if (!(wifiMac.usec & 1023))
		{
			wifiMac.BeaconCount1--;

			if (wifiMac.BeaconCount1 == (WIFI_IOREG(REG_WIFI_PREBEACONCOUNT) >> 10))
				WIFI_triggerIRQ(WIFI_IRQ_TIMEPREBEACON);
			else if (wifiMac.BeaconCount1 == 0)
				WIFI_triggerIRQ(WIFI_IRQ_TIMEBEACON);

			if (wifiMac.BeaconCount2 > 0)
			{
				wifiMac.BeaconCount2--;
				if (wifiMac.BeaconCount2 == 0)
					WIFI_triggerIRQ(WIFI_IRQ_TIMEPOSTBEACON);
			}
		}
	}

	if ((wifiMac.ucmpEnable) && (wifiMac.ucmp == wifiMac.usec))
	{
		//printf("ucmp irq14\n");
		WIFI_triggerIRQ(WIFI_IRQ_TIMEBEACON);
	}

	if((wifiMac.usec & 3) == 0)
	{
		int slot = wifiMac.txCurSlot;

		if(wifiMac.txSlotBusy[slot])
		{
			wifiMac.txSlotRemainingBytes[slot]--;
			wifiMac.RXTXAddr++;
			if(wifiMac.txSlotRemainingBytes[slot] == 0)
			{
				wifiMac.txSlotBusy[slot] = 0;
				wifiMac.TXSlot[slot] &= 0x7FFF;

				if(wifiCom)
					wifiCom->SendPacket((u8*)&wifiMac.RAM[wifiMac.txSlotAddr[slot]+6], wifiMac.txSlotLen[slot]);

				while((wifiMac.txSlotBusy[wifiMac.txCurSlot] == 0) && (wifiMac.txCurSlot > 0))
					wifiMac.txCurSlot--;

				wifiMac.RAM[wifiMac.txSlotAddr[slot]] = 0x0001;
				wifiMac.RAM[wifiMac.txSlotAddr[slot]+4] &= 0x00FF;

				wifiMac.TXStat = (0x0001 | (slot << 12));

				WIFI_triggerIRQ(WIFI_IRQ_TXEND);
				
				//wifiMac.rfStatus = 0x0001;
				//wifiMac.rfPins = 0x0084;
				wifiMac.rfStatus = 0x0009;
				wifiMac.rfPins = 0x0004;

				WIFI_LOG(3, "TX slot %i finished sending its packet. Next is slot %i. TXStat = %04X\n", 
					slot, wifiMac.txCurSlot, wifiMac.TXStat);
			}
		}
	}

	if(wifiCom)
		wifiCom->usTrigger();
}

/*******************************************************************************

	Ad-hoc communication interface

 *******************************************************************************/

#ifdef EXPERIMENTAL_WIFI_COMM
#define ADHOC_MAGIC					"NDSWIFI\0"
#define ADHOC_PROTOCOL_VERSION 		0x0100  // v1.0

typedef struct _Adhoc_FrameHeader
{
	char magic[8];			// "NDSWIFI\0" (null terminated string)
	u16 version;			// Ad-hoc protocol version (for example 0x0502 = v5.2)
	u16 packetLen;			// Length of the packet

} Adhoc_FrameHeader;


bool Adhoc_Init()
{
	BOOL opt_true = TRUE;
	int res;

	if (!driver->WIFI_SocketsAvailable())
	{
		WIFI_LOG(1, "Ad-hoc: failed to initialize sockets.\n");
		wifi_socket = INVALID_SOCKET;
		return false;
	}

	// Create an UDP socket
	wifi_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (wifi_socket < 0)
	{
		WIFI_LOG(1, "Ad-hoc: Failed to create socket.\n");
		return false;
	}

	// Enable the socket to be bound to an address/port that is already in use
	// This enables us to communicate with another DeSmuME instance running on the same computer.
	res = setsockopt(wifi_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt_true, sizeof(BOOL));

	// Bind the socket to any address on port 7000
	sockaddr_t saddr;
	saddr.sa_family = AF_INET;
	*(u32*)&saddr.sa_data[2] = htonl(INADDR_ANY); 
	*(u16*)&saddr.sa_data[0] = htons(BASEPORT);
	res = bind(wifi_socket, &saddr, sizeof(sockaddr_t));
	if (res < 0)
	{
		WIFI_LOG(1, "Ad-hoc: failed to bind the socket.\n");
		closesocket(wifi_socket); wifi_socket = INVALID_SOCKET;
		return false;
	}

	// Enable broadcast mode
	// Not doing so results in failure when sendto'ing to broadcast address
	res = setsockopt(wifi_socket, SOL_SOCKET, SO_BROADCAST, (const char*)&opt_true, sizeof(BOOL));
	if (res < 0)
	{
		WIFI_LOG(1, "Ad-hoc: failed to enable broadcast mode.\n");
		closesocket(wifi_socket); wifi_socket = INVALID_SOCKET;
		return false;
	}

	// Prepare an address structure for sending packets
	sendAddr.sa_family = AF_INET;
	*(u32*)&sendAddr.sa_data[2] = htonl(INADDR_BROADCAST); 
	*(u16*)&sendAddr.sa_data[0] = htons(BASEPORT);

	Adhoc_Reset();

	WIFI_LOG(1, "Ad-hoc: initialization successful.\n");

	return true;
}

void Adhoc_DeInit()
{
	if (wifi_socket >= 0)
		closesocket(wifi_socket);
}

void Adhoc_Reset()
{
	Adhoc.usecCounter = 0;

	driver->WIFI_GetUniqueMAC(FW_Mac);
	NDS_PatchFirmwareMAC();

	printf("WIFI: ADHOC: MAC = %02X:%02X:%02X:%02X:%02X:%02X\n",
		FW_Mac[0], FW_Mac[1], FW_Mac[2], FW_Mac[3], FW_Mac[4], FW_Mac[5]);
}

void Adhoc_SendPacket(u8* packet, u32 len)
{
	if (wifi_socket < 0)
		return;

	WIFI_LOG(2, "Ad-hoc: sending a packet of %i bytes, frame control: %04X\n", len, *(u16*)&packet[0]);

	u32 frameLen = sizeof(Adhoc_FrameHeader) + len;

	u8* frame = new u8[frameLen];
	u8* ptr = frame;

	Adhoc_FrameHeader header;
	strncpy(header.magic, ADHOC_MAGIC, 8);
	header.version = ADHOC_PROTOCOL_VERSION;
	header.packetLen = len;
	memcpy(ptr, &header, sizeof(Adhoc_FrameHeader));
	ptr += sizeof(Adhoc_FrameHeader);

	memcpy(ptr, packet, len);

	int nbytes = sendto(wifi_socket, (const char*)frame, frameLen, 0, &sendAddr, sizeof(sockaddr_t));
	
	WIFI_LOG(4, "Ad-hoc: sent %i/%i bytes of packet.\n", nbytes, frameLen);

	delete[] frame;
}

void Adhoc_usTrigger()
{
	Adhoc.usecCounter++;

	if (wifi_socket < 0)
		return;

	// Check every millisecond if we received a packet
	if (!(Adhoc.usecCounter & 1023))
	{
		fd_set fd;
		struct timeval tv;

		FD_ZERO(&fd);
		FD_SET(wifi_socket, &fd);
		tv.tv_sec = 0; 
		tv.tv_usec = 0;
 
		if (select(1, &fd, 0, 0, &tv))
		{
			sockaddr_t fromAddr;
			socklen_t fromLen = sizeof(sockaddr_t);
			u8 buf[1536];
			u8* ptr;
			u16 packetLen;

			int nbytes = recvfrom(wifi_socket, (char*)buf, 1536, 0, &fromAddr, &fromLen);

			// No packet arrived (or there was an error)
			if (nbytes <= 0)
				return;

			ptr = buf;
			Adhoc_FrameHeader header = *(Adhoc_FrameHeader*)ptr;
			
			// Check the magic string in header
			if (strncmp(header.magic, ADHOC_MAGIC, 8))
				return;

			// Check the ad-hoc protocol version
			if (header.version != ADHOC_PROTOCOL_VERSION)
				return;

			packetLen = header.packetLen;
			ptr += sizeof(Adhoc_FrameHeader);

			// If the packet is for us, send it to the wifi core
			if (!WIFI_compareMAC(&ptr[10], &wifiMac.mac.bytes[0]))
			{
				if (WIFI_isBroadcastMAC(&ptr[16]) ||
					WIFI_compareMAC(&ptr[16], &wifiMac.bss.bytes[0]) ||
					WIFI_isBroadcastMAC(&wifiMac.bss.bytes[0]))
				{
				/*	printf("packet was for us: mac=%02X:%02X.%02X.%02X.%02X.%02X, bssid=%02X:%02X.%02X.%02X.%02X.%02X\n",
						wifiMac.mac.bytes[0], wifiMac.mac.bytes[1], wifiMac.mac.bytes[2], wifiMac.mac.bytes[3], wifiMac.mac.bytes[4], wifiMac.mac.bytes[5],
						wifiMac.bss.bytes[0], wifiMac.bss.bytes[1], wifiMac.bss.bytes[2], wifiMac.bss.bytes[3], wifiMac.bss.bytes[4], wifiMac.bss.bytes[5]);
					printf("da=%02X:%02X.%02X.%02X.%02X.%02X, sa=%02X:%02X.%02X.%02X.%02X.%02X, bssid=%02X:%02X.%02X.%02X.%02X.%02X\n",
						ptr[4], ptr[5], ptr[6], ptr[7], ptr[8], ptr[9],
						ptr[10], ptr[11], ptr[12], ptr[13], ptr[14], ptr[15],
						ptr[16], ptr[17], ptr[18], ptr[19], ptr[20], ptr[21]);*/
				/*	WIFI_LOG(3, "Ad-hoc: received a packet of %i bytes from %i.%i.%i.%i (port %i).\n",
						nbytes,
						(u8)fromAddr.sa_data[2], (u8)fromAddr.sa_data[3], 
						(u8)fromAddr.sa_data[4], (u8)fromAddr.sa_data[5],
						ntohs(*(u16*)&fromAddr.sa_data[0]));*/
					WIFI_LOG(2, "Ad-hoc: received a packet of %i bytes, frame control: %04X\n", packetLen, *(u16*)&ptr[0]);
					//WIFI_LOG(2, "Storing packet at %08X.\n", 0x04804000 + (wifiMac.RXWriteCursor<<1));

					//if (((*(u16*)&ptr[0]) != 0x0080) && ((*(u16*)&ptr[0]) != 0x0228))
					//	printf("received packet, framectl=%04X\n", (*(u16*)&ptr[0]));

					//if ((*(u16*)&ptr[0]) == 0x0228)
					//	printf("wifi: received fucking packet!\n");

					WIFI_triggerIRQ(WIFI_IRQ_RXSTART);

					u8* packet = new u8[12 + packetLen];

					WIFI_MakeRXHeader(packet, WIFI_GetRXFlags(ptr), 20, packetLen, 0, 0);
					memcpy(&packet[12], ptr, packetLen);

					for (int i = 0; i < (12 + packetLen); i += 2)
					{
						u16 word = *(u16*)&packet[i];
						WIFI_RXPutWord(word);
					}

					wifiMac.RXWriteCursor = ((wifiMac.RXWriteCursor + 1) & (~1));
					WIFI_IOREG(REG_WIFI_RXHWWRITECSR) = wifiMac.RXWriteCursor;
					wifiMac.RXNum++;
					WIFI_triggerIRQ(WIFI_IRQ_RXEND);
				}
			}
		}
	}
}

/*******************************************************************************

	SoftAP (fake wifi access point)

 *******************************************************************************/

// Note on the CRC32 field in received packets:
// The wifi hardware doesn't store the CRC32 in memory when receiving a packet
// so the RX header length field is indeed header+body
// Hence the CRC32 has been removed from those templates.

// If you wanna change SoftAP's MAC address, change this
// Warning, don't mistake this for an array, it isn't
#define SOFTAP_MACADDR 0x00, 0xF0, 0x1A, 0x2B, 0x3C, 0x4D

const u8 SoftAP_MACAddr[6] = {SOFTAP_MACADDR};

const u8 SoftAP_Beacon[] = {
	/* 802.11 header */
	0x80, 0x00,											// Frame control
	0x00, 0x00,											// Duration ID
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,					// Receiver
	SOFTAP_MACADDR,										// Sender
	SOFTAP_MACADDR,										// BSSID
	0x00, 0x00,											// Sequence control

	/* Frame body */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		// Timestamp (modified later)
	0x80, 0x00,											// Beacon interval
	0x21, 0x00,											// Capablilty information
	0x01, 0x02, 0x82, 0x84,								// Supported rates
	0x03, 0x01, 0x06,									// Current channel
	0x05, 0x04, 0x02, 0x01, 0x00, 0x00,					// TIM (no idea what the hell it is)
	0x00, 0x06, 'S', 'o', 'f', 't', 'A', 'P',			// SSID
};

const u8 SoftAP_ProbeResponse[] = {
	/* 802.11 header */
	0x50, 0x00,											// Frame control
	0x00, 0x00,											// Duration ID
	0x00, 0x09, 0xBF, 0x12, 0x34, 0x56,					// Receiver
	SOFTAP_MACADDR,										// Sender
	SOFTAP_MACADDR,										// BSSID
	0x00, 0x00,											// Sequence control

	/* Frame body */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		// Timestamp (modified later)
	0x80, 0x00,											// Beacon interval
	0x21, 0x00,											// Capablilty information
	0x01, 0x02, 0x82, 0x84,								// Supported rates
	0x03, 0x01, 0x06,									// Current channel
	0x00, 0x06, 'S', 'o', 'f', 't', 'A', 'P',			// SSID
};


const u8 SoftAP_AuthFrame[] = {
	/* 802.11 header */
	0xB0, 0x00,											// Frame control
	0x00, 0x00,											// Duration ID
	0x00, 0x09, 0xBF, 0x12, 0x34, 0x56,					// Receiver
	SOFTAP_MACADDR,										// Sender
	SOFTAP_MACADDR,										// BSSID
	0x00, 0x00,											// Sequence control

	/* Frame body */
	0x00, 0x00,											// Authentication algorithm
	0x02, 0x00,											// Authentication sequence
	0x00, 0x00,											// Status
};

const u8 SoftAP_AssocResponse[] = {
	/* 802.11 header */
	0x10, 0x00,											// Frame control
	0x00, 0x00,											// Duration ID
	0x00, 0x09, 0xBF, 0x12, 0x34, 0x56,					// Receiver
	SOFTAP_MACADDR,										// Sender
	SOFTAP_MACADDR,										// BSSID
	0x00, 0x00,											// Sequence control

	/* Frame body */
	0x21, 0x00,											// Capability information
	0x00, 0x00,											// Status
	0x01, 0xC0,											// Assocation ID
	0x01, 0x02, 0x82, 0x84,								// Supported rates
};

// Deauthentication frame - sent if the user chose not to connect to WFC
const u8 SoftAP_DeauthFrame[] = {
	/* 802.11 header */
	0xC0, 0x00,											// Frame control
	0x00, 0x00,											// Duration ID
	0x00, 0x09, 0xBF, 0x12, 0x34, 0x56,					// Receiver
	SOFTAP_MACADDR,										// Sender
	SOFTAP_MACADDR,										// BSSID
	0x00, 0x00,											// Sequence control

	/* Frame body */
	0x01, 0x00,											// Reason code (is "unspecified" ok?)
};

//todo - make a class to wrap this
//todo - zeromus - inspect memory leak safety of all this
static pcap_if_t * WIFI_index_device(pcap_if_t *alldevs, int index)
{
	pcap_if_t *curr = alldevs;

	for(int i = 0; i < index; i++)
		curr = curr->next;

	WIFI_LOG(2, "SoftAP: using %s as device.\n", curr->PCAP_DEVICE_NAME);

	return curr;
}

bool SoftAP_Init()
{
	SoftAP.usecCounter = 0;

	SoftAP.curPacketSize = 0;
	SoftAP.curPacketPos = 0;
	SoftAP.curPacketSending = FALSE;

	SoftAP.status = APStatus_Disconnected;
	SoftAP.seqNum = 0;

	if (!driver->WIFI_PCapAvailable())
		{
		WIFI_LOG(1, "SoftAP: PCap library not available on your system.\n");
		wifi_bridge = NULL;
		return false;
		}
	else
		{
		WIFI_LOG(1, "SoftAP: PCap library available on your system.\n");	
		}
	
	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_if_t *alldevs;
	int ret;

	ret = driver->PCAP_findalldevs(&alldevs, errbuf);
	if (ret == -1 || alldevs == NULL)
	{
		WIFI_LOG(1, "SoftAP: PCap: failed to find any network adapter: %s\n", errbuf);
		return false;
	}
	else
		{
		WIFI_LOG(1, "SoftAP: PCap: Find a network adapter.\n");	
		}

	pcap_if_t* dev = WIFI_index_device(alldevs,CommonSettings.wifi.infraBridgeAdapter);
	wifi_bridge = driver->PCAP_open(dev->name, PACKET_SIZE, PCAP_OPENFLAG_PROMISCUOUS, 1, errbuf);
	if(wifi_bridge == NULL)
	{
		WIFI_LOG(1, "SoftAP: PCap: failed to open %s: %s\n", dev->PCAP_DEVICE_NAME, errbuf);
		return false;
	}
	else
		{
		WIFI_LOG(1, "SoftAP: PCap: Device %s successfully opened.\n", dev->PCAP_DEVICE_NAME);	
		}

	driver->PCAP_freealldevs(alldevs);

	// Set non-blocking mode
	if (driver->PCAP_setnonblock(wifi_bridge, 1, errbuf) == -1)
	{
		WIFI_LOG(1, "SoftAP: PCap: failed to set non-blocking mode: %s\n", errbuf);
		driver->PCAP_close(wifi_bridge); wifi_bridge = NULL;
		return false;
	}
	else
		{
		WIFI_LOG(1, "SoftAP: PCap: Non-blocking mode successfully activated.\n");	
		}

	return true;
}

void SoftAP_DeInit()
{
	if(wifi_bridge != NULL)
		driver->PCAP_close(wifi_bridge);
}

void SoftAP_Reset()
{
	SoftAP.usecCounter = 0;

	SoftAP.curPacketSize = 0;
	SoftAP.curPacketPos = 0;
	SoftAP.curPacketSending = FALSE;

	SoftAP.status = APStatus_Disconnected;
	SoftAP.seqNum = 0;
}

static bool SoftAP_IsDNSRequestToWFC(u16 ethertype, u8* body)
{
	// Check the various headers...
	if (ntohs(ethertype) != 0x0800) return false;		// EtherType: IP
	if (body[0] != 0x45) return false;					// Version: 4, header len: 5
	if (body[9] != 0x11) return false;					// Protocol: UDP
	if (ntohs(*(u16*)&body[22]) != 53) return false;	// Dest. port: 53 (DNS)
	if (htons(ntohs(*(u16*)&body[28+2])) & 0x8000) return false;	// must be a query
	
	// Analyze each question
	u16 numquestions = ntohs(*(u16*)&body[28+4]);
	u32 curoffset = 28+12;
	for (u16 curquestion = 0; curquestion < numquestions; curquestion++)
	{
		// Assemble the requested domain name
		u8 bitlength = 0; char domainname[256] = "";
		while ((bitlength = body[curoffset++]) != 0)
		{
			strncat(domainname, (const char*)&body[curoffset], bitlength);
			
			curoffset += bitlength;
			if (body[curoffset] != 0)
				strcat(domainname, ".");
		}

		// if the domain name contains nintendowifi.net
		// it is most likely a WFC server
		// (note, conntest.nintendowifi.net just contains a dummy HTML page and
		// is used for connection tests, I think we can let this one slide)
		if ((strstr(domainname, "nintendowifi.net") != NULL) && 
			(strcmp(domainname, "conntest.nintendowifi.net") != 0))
			return true;

		// Skip the type and class - we don't care about that
		curoffset += 4;
	}

	return false;
}

static void SoftAP_Deauthenticate()
{
	u32 packetLen = sizeof(SoftAP_DeauthFrame);

	memcpy(&SoftAP.curPacket[12], SoftAP_DeauthFrame, packetLen);

	memcpy(&SoftAP.curPacket[12 + 4], FW_Mac, 6); // Receiver MAC

	*(u16*)&SoftAP.curPacket[12 + 22] = SoftAP.seqNum << 4;		// Sequence number
	SoftAP.seqNum++;

	u16 rxflags = 0x0010;
	if (WIFI_compareMAC(wifiMac.bss.bytes, &SoftAP.curPacket[12 + 16]))
		rxflags |= 0x8000;

	WIFI_MakeRXHeader(SoftAP.curPacket, rxflags, 20, packetLen, 0, 0);

	// Let's prepare to send
	SoftAP.curPacketSize = packetLen + 12;
	SoftAP.curPacketPos = 0;
	SoftAP.curPacketSending = TRUE;

	SoftAP.status = APStatus_Disconnected;
}

void SoftAP_SendPacket(u8 *packet, u32 len)
{
	u16 frameCtl = *(u16*)&packet[0];

	WIFI_LOG(3, "SoftAP: Received a packet of length %i bytes. Frame control = %04X\n",
		len, frameCtl);

	//use this to log wifi messages easily
	/*static int ctr=0;
	char buf[100];
	sprintf(buf,"wifi%04d.txt",ctr);
	FILE* outf = fopen(buf,"wb");
	fwrite(packet,1,len,outf);
	fclose(outf);
	ctr++;*/

	switch((frameCtl >> 2) & 0x3)
	{
	case 0x0:				// Management frame
		{
			u32 packetLen;

			switch((frameCtl >> 4) & 0xF)
			{
			case 0x4:		// Probe request (WFC)
				{
					packetLen = sizeof(SoftAP_ProbeResponse);
					memcpy(&SoftAP.curPacket[12], SoftAP_ProbeResponse, packetLen);

					// Add the timestamp
					u64 timestamp = SoftAP.usecCounter;
					*(u64*)&SoftAP.curPacket[12 + 24] = timestamp;
				}
				break;

			case 0xB:		// Authentication
				{
					packetLen = sizeof(SoftAP_AuthFrame);
					memcpy(&SoftAP.curPacket[12], SoftAP_AuthFrame, packetLen);

					SoftAP.status = APStatus_Authenticated;
				}
				break;

			case 0x0:		// Association request
				{
					if (SoftAP.status != APStatus_Authenticated)
						return;

					packetLen = sizeof(SoftAP_AssocResponse);
					memcpy(&SoftAP.curPacket[12], SoftAP_AssocResponse, packetLen);

					SoftAP.status = APStatus_Associated;
					WIFI_LOG(1, "SoftAP connected!\n");

					create_packet();
				}
				break;

			case 0xA:		// Disassociation
				SoftAP.status = APStatus_Authenticated;
				return;

			case 0xC:		// Deauthentication
				SoftAP.status = APStatus_Disconnected;
				WIFI_LOG(1, "SoftAP disconnected\n");

				if (captured_packets != NULL)
				{
					fclose(captured_packets);
					captured_packets = NULL;
				}

				return;

			default:
				WIFI_LOG(2, "SoftAP: unknown management frame type %04X\n", (frameCtl >> 4) & 0xF);
				return;
			}

			memcpy(&SoftAP.curPacket[12 + 4], FW_Mac, 6); // Receiver MAC

			*(u16*)&SoftAP.curPacket[12 + 22] = SoftAP.seqNum << 4; // Sequence number
			SoftAP.seqNum++;

			u16 rxflags = 0x0010;
			if (WIFI_compareMAC(wifiMac.bss.bytes, &SoftAP.curPacket[12 + 16]))
				rxflags |= 0x8000;

			WIFI_MakeRXHeader(SoftAP.curPacket, rxflags, 20, packetLen, 0, 0); // make the RX header

			// Let's prepare to send
			SoftAP.curPacketSize = packetLen + 12;
			SoftAP.curPacketPos = 0;
			SoftAP.curPacketSending = TRUE;
		}
		break;

	case 0x2:				// Data frame
		{
			// If it has a LLC/SLIP header, send it over the Ethernet
			if (((*(u16*)&packet[24]) == 0xAAAA) && ((*(u16*)&packet[26]) == 0x0003) && ((*(u16*)&packet[28]) == 0x0000))
			{
				if (SoftAP.status != APStatus_Associated)
					return;

				if (SoftAP_IsDNSRequestToWFC(*(u16*)&packet[30], &packet[32]))
				{
					// Removed to allow WFC communication.
					//SoftAP_Deauthenticate();
					//return;
				}

				u32 epacketLen = ((len - 30 - 4) + 14);
				u8 epacket[2048];

				//printf("----- SENDING ETHERNET PACKET: len=%i, ethertype=%04X -----\n",
				//	len, *(u16*)&packet[30]);

				memcpy(&epacket[0], &packet[16], 6);
				memcpy(&epacket[6], &packet[10], 6);
				*(u16*)&epacket[12] = *(u16*)&packet[30];
				memcpy(&epacket[14], &packet[32], epacketLen - 14);

				if(wifi_bridge != NULL) {
					// Store the packet in the PCAP file.
					save_packet(epacket, epacketLen, SoftAP.usecCounter / 1000000, SoftAP.usecCounter % 1000000, false);

					// Send it
					driver->PCAP_sendpacket(wifi_bridge, epacket, epacketLen);
				}
			}
			else
			{
				WIFI_LOG(1, "SoftAP: received non-Ethernet data frame. wtf?\n");
			}
		}
		break;
	}
}

INLINE void SoftAP_SendBeacon()
{
	u32 packetLen = sizeof(SoftAP_Beacon);

	memcpy(&SoftAP.curPacket[12], SoftAP_Beacon, packetLen);	// Copy the beacon template

	*(u16*)&SoftAP.curPacket[12 + 22] = SoftAP.seqNum << 4;		// Sequence number
	SoftAP.seqNum++;

	u64 timestamp = SoftAP.usecCounter;
	*(u64*)&SoftAP.curPacket[12 + 24] = timestamp;				// Timestamp

	u16 rxflags = 0x0011;
	if (WIFI_compareMAC(wifiMac.bss.bytes, &SoftAP.curPacket[12 + 16]))
		rxflags |= 0x8000;

	WIFI_MakeRXHeader(SoftAP.curPacket, rxflags, 20, packetLen, 0, 0);

	// Let's prepare to send
	SoftAP.curPacketSize = packetLen + 12;
	SoftAP.curPacketPos = 0;
	SoftAP.curPacketSending = TRUE;
}

static void SoftAP_RXHandler(u_char* user, const struct pcap_pkthdr* h, const u_char* _data)
{
	// safety checks
	if ((_data == NULL) || (h == NULL))
		return;

	u8* data = (u8*)_data;

	// reject the packet if it wasn't for us
	if (!(WIFI_isBroadcastMAC(&data[0]) || WIFI_compareMAC(&data[0], wifiMac.mac.bytes)))
		return;

	// reject the packet if we just sent it
	if (WIFI_compareMAC(&data[6], wifiMac.mac.bytes))
		return;

	if (SoftAP.curPacketSending)
	{
		printf("crap we're gonna nuke a packet at %i/%i (%04X) (%04X)\n", SoftAP.curPacketPos, SoftAP.curPacketSize, *(u16*)&SoftAP.curPacket[12], wifiMac.RXWriteCursor<<1);
	}

	// The packet was for us. Let's process it then.
	WIFI_triggerIRQ(WIFI_IRQ_RXSTART);

	int wpacketLen = WIFI_alignedLen(26 + 6 + (h->len-14));
	u8 wpacket[2048];

	// Save ethernet packet into the PCAP file.
	// Filter broadcast because of privacy. They aren't needed to study the protocol with the nintendo server
	// and can include PC Discovery protocols
	if (!WIFI_isBroadcastMAC(&data[0]))
		save_packet(data, h->len, SoftAP.usecCounter / 1000000, SoftAP.usecCounter % 1000000, true);

	//printf("RECEIVED DATA FRAME: len=%i/%i, src=%02X:%02X:%02X:%02X:%02X:%02X, dst=%02X:%02X:%02X:%02X:%02X:%02X, ethertype=%04X\n",
	//	24+ (h->caplen-12), 24 + (h->len-12), data[6], data[7], data[8], data[9], data[10], data[11],
	//	data[0], data[1], data[2], data[3], data[4], data[5], *(u16*)&data[12]);

	u16 rxflags = 0x0018;
	if (WIFI_compareMAC(wifiMac.bss.bytes, (u8*)SoftAP_MACAddr))
		rxflags |= 0x8000;

	// Make a valid 802.11 frame
	WIFI_MakeRXHeader(wpacket, rxflags, 20, wpacketLen, 0, 0);
	*(u16*)&wpacket[12+0] = 0x0208;
	*(u16*)&wpacket[12+2] = 0x0000;
	memcpy(&wpacket[12+4], &data[0], 6);
	memcpy(&wpacket[12+10], SoftAP_MACAddr, 6);
	memcpy(&wpacket[12+16], &data[6], 6);
	*(u16*)&wpacket[12+22] = SoftAP.seqNum << 4;
	*(u16*)&wpacket[12+24] = 0xAAAA;
	*(u16*)&wpacket[12+26] = 0x0003;
	*(u16*)&wpacket[12+28] = 0x0000;
	*(u16*)&wpacket[12+30] = *(u16*)&data[12];
	memcpy(&wpacket[12+32], &data[14], h->len-14);

	SoftAP.seqNum++;

	// put it in the RX buffer
	for (int i = 0; i < (12 + wpacketLen); i += 2)
	{
		u16 word = *(u16*)&wpacket[i];
		WIFI_RXPutWord(word);
	}

	// Done!
	wifiMac.RXWriteCursor = ((wifiMac.RXWriteCursor + 1) & (~1));
	WIFI_IOREG(REG_WIFI_RXHWWRITECSR) = wifiMac.RXWriteCursor;

	WIFI_triggerIRQ(WIFI_IRQ_RXEND);
}

void SoftAP_usTrigger()
{
	SoftAP.usecCounter++;

	// other packets will have priority over beacons
	// 'cause they might be only once of them
	// whereas there will be sooo much beacons
	if(!SoftAP.curPacketSending) // && SoftAP.status != APStatus_Associated)
	{
		//if(wifiMac.ioMem[0xD0 >> 1] & 0x0400)
		{
			//zero sez: every 1/10 second? does it have to be precise? this is so costly..
			// Okay for 128 ms then
			if((SoftAP.usecCounter & 131071) == 0)
			{
				SoftAP_SendBeacon();
			}
		}
	}

	/* Given a connection of 2 megabits per second, */
	/* we take ~4 microseconds to transfer a byte, */
	/* ie ~8 microseconds to transfer a word. */
	if((SoftAP.curPacketSending) && !(SoftAP.usecCounter & 7))
	{
		if(SoftAP.curPacketPos == 0)
			WIFI_triggerIRQ(WIFI_IRQ_RXSTART);

		u16 word = *(u16*)&SoftAP.curPacket[SoftAP.curPacketPos];
		WIFI_RXPutWord(word);

		SoftAP.curPacketPos += 2;
		if(SoftAP.curPacketPos >= SoftAP.curPacketSize)
		{
			SoftAP.curPacketSize = 0;
			SoftAP.curPacketPos = 0;
			SoftAP.curPacketSending = FALSE;

			wifiMac.RXWriteCursor = ((wifiMac.RXWriteCursor + 1) & (~1));
			WIFI_IOREG(REG_WIFI_RXHWWRITECSR) = wifiMac.RXWriteCursor;

			WIFI_triggerIRQ(WIFI_IRQ_RXEND);
		}
	}

	// EXTREMELY EXPERIMENTAL packet receiving code
	// Can now receive 64 packets per millisecond. Completely arbitrary limit. Todo: tweak if needed.
	// But due to using non-blocking mode, this shouldn't be as slow as it used to be.
	if ((SoftAP.usecCounter & 1023) == 0)
		if(wifi_bridge != NULL)
			driver->PCAP_dispatch(wifi_bridge, 64, SoftAP_RXHandler, NULL);
}

#endif

/*
 Create and open a PCAP file to store different Ethernet packets
 of the current connection.
*/
static void create_packet()
{
	// Create file using as name the current time and game code
	time_t ti;
	time(&ti);
	tm* t = localtime(&ti);
	
	char* gamecd = gameInfo.header.gameCode;
	char file_name[50];
	sprintf(
		file_name,
		"%c%c%c%c [%02d-%02d-%02d-%02d].pcap", 
		gamecd[0], gamecd[1], gamecd[2], gamecd[3],
		t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec
	);

	// Open as binary write
	captured_packets = fopen(file_name, "wb");
	if (captured_packets == NULL)
	{
		printf("Can't create capture log file: %s\n", file_name);
	}
	else
	{
		// Create global header of LCAP packet
		// More info here: http://www.kroosec.com/2012/10/a-look-at-pcap-file-format.html
		const u32 magic_header  = 0xa1b2c3d4;
		const u16 major_version = 0x02;
		const u16 minor_version = 0x04;
		const u32 gmt_time      = 0x00000000; // usually not used
		const u32 pre_time      = 0x00000000; // usually not used
		const u32 snapshot_len  = 0x0000ffff; // Maximum length of each packet
		const u32 ll_header_typ = 0x00000001; // For Ethernet

		fwrite(&magic_header,  sizeof(char), 4, captured_packets);
		fwrite(&major_version, sizeof(char), 2, captured_packets);
		fwrite(&minor_version, sizeof(char), 2, captured_packets);
		fwrite(&gmt_time,      sizeof(char), 4, captured_packets);
		fwrite(&pre_time,      sizeof(char), 4, captured_packets);
		fwrite(&snapshot_len,  sizeof(char), 4, captured_packets);
		fwrite(&ll_header_typ, sizeof(char), 4, captured_packets);

		fflush(captured_packets);
	}
}

/*
 Save an Ethernet packet into the PCAP file of the current connection.
*/
static void save_packet(u8* packet, u32 len, u32 seconds, u32 millis, bool isReceived)
{
	if (captured_packets == NULL)
	{
		printf("Can't save packet... %d\n", isReceived);
		return;
	}

	// Add the packet
	// more info: http://www.kroosec.com/2012/10/a-look-at-pcap-file-format.html
	printf("WIFI: Saving packet of %04x bytes | %d\n", len, isReceived);

	// First create the header
	fwrite(&seconds, sizeof(char), 4, captured_packets); // This should be seconds since Unix Epoch, but it works :D
	fwrite(&millis,  sizeof(char), 4, captured_packets);
	fwrite(&len,     sizeof(char), 4, captured_packets);
	fwrite(&len,     sizeof(char), 4, captured_packets);

	// Then write the packet
	fwrite(packet, sizeof(char), len, captured_packets);

	// Flush the file
	fflush(captured_packets);
}
