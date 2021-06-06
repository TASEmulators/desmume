/*
		Copyright (C) 2007 Tim Seidel
		Copyright (C) 2008-2018 DeSmuME team

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
#include "utils/task.h"
#include <driver.h>
#include <registers.h>
#include <rthreads/rthreads.h>

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
#include <stddef.h>
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
#define PACKET_SIZE 65535

// Some platforms need HAVE_REMOTE to work with libpcap, but
// Apple platforms are not among them.
#ifndef __APPLE__
#define HAVE_REMOTE
#endif

#ifdef HOST_WINDOWS
#define WPCAP
#endif

//#include <pcap.h>
typedef void* pcap_t;
typedef void* pcap_if_t;
typedef void* pcap_pkthdr;
#define htons
#define ntohs

//sometimes this isnt defined
#ifndef PCAP_OPENFLAG_PROMISCUOUS
#define PCAP_OPENFLAG_PROMISCUOUS 1
#endif

// PCAP_ERRBUF_SIZE should 256 bytes according to POSIX libpcap and winpcap.
// Define it if it isn't available.
#ifndef PCAP_ERRBUF_SIZE
#define PCAP_ERRBUF_SIZE 256
#endif

static const u8 BBDefaultData[105] = {
	0x6D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x00 - 0x0F
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x10 - 0x1F
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x20 - 0x2F
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x30 - 0x3F
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x40 - 0x4F
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, // 0x50 - 0x5F
	0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00 // 0x60 - 0x68
};

static const bool BBIsDataWritable[105] = {
	false,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true, false, false, false, // 0x00 - 0x0F
	false, false, false,  true,  true,  true, false, false, false, false, false,  true,  true,  true,  true,  true, // 0x10 - 0x1F
	 true,  true,  true,  true,  true,  true,  true, false,  true,  true,  true,  true,  true,  true,  true,  true, // 0x20 - 0x2F
	 true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true, // 0x30 - 0x3F
	 true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true, false,  true,  true, // 0x40 - 0x4F
	 true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true, false, false, false, // 0x50 - 0x5F
	false, false,  true,  true, false,  true, false,  true,  true // 0x60 - 0x68
};

static const u16 RFPinsLUT[10] = { 0x04, 0x84, 0, 0x46, 0, 0x84, 0x87, 0, 0x46, 0x04 };
static const WifiLLCSNAPHeader DefaultSNAPHeader = { 0xAA, 0xAA, 0x03, {0x00, 0x00, 0x00}, 0x0800 };
LegacyWifiSFormat legacyWifiSF;

DummyPCapInterface dummyPCapInterface;
WifiHandler* wifiHandler = NULL;

#define slock_lock
#define slock_unlock
#define slock_new() NULL
#define slock_free



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

u8 FW_Mac[6] = { 0x00, 0x09, 0xBF, 0x12, 0x34, 0x56 };

const u8 FW_WIFIInit[32] = { 0x02,0x00,  0x17,0x00,  0x26,0x00,  0x18,0x18,
							0x48,0x00,  0x40,0x48,  0x58,0x00,  0x42,0x00,
							0x40,0x01,  0x64,0x80,  0xE0,0xE0,  0x43,0x24,
							0x0E,0x00,  0x32,0x00,  0xF4,0x01,  0x01,0x01 };
const u8 FW_BBInit[105] = { 0x6D, 0x9E, 0x40, 0x05,
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
const u8 FW_RFInit[36] = { 0x07, 0xC0, 0x00,
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
const u8 FW_RFChannel[6 * 14] = { 0x28, 0x17, 0x14,		/* Channel 1 */
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
const u8 FW_BBChannel[14] = { 0xb3, 0xb3, 0xb3, 0xb3, 0xb3,	/* channel  1- 6 */
							0xb4, 0xb4, 0xb4, 0xb4, 0xb4,	/* channel  7-10 */
							0xb5, 0xb5,						/* channel 11-12 */
							0xb6, 0xb6						/* channel 13-14 */
};

FW_WFCProfile FW_WFCProfile1 = { "SoftAP",
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

FW_WFCProfile FW_WFCProfile2 = { "",
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

FW_WFCProfile FW_WFCProfile3 = { "",
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

// For debugging purposes, the results of libpcap can be written to a file.
// Note that enabling this setting can negatively affect emulation performance.
#define WIFI_SAVE_PCAP_TO_FILE 0

/*******************************************************************************

	Helpers

 *******************************************************************************/

 // Fast MAC compares
INLINE bool WIFI_compareMAC(const u8* a, const u8* b)
{
	return ((*(u32*)&a[0]) == (*(u32*)&b[0])) && ((*(u16*)&a[4]) == (*(u16*)&b[4]));
}

INLINE bool WIFI_isBroadcastMAC(const u8* a)
{
	return ((*(u32*)&a[0]) == 0xFFFFFFFF) && ((*(u16*)&a[4]) == 0xFFFF);
}

INLINE bool WIFI_IsLLCSNAPHeader(const u8* snapHeader)
{
	return ((*(u16*)&snapHeader[0] == *(u16*)&DefaultSNAPHeader.dsap) && (*(u32*)&snapHeader[2] == *(u32*)&DefaultSNAPHeader.control));
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
		if(ref & 1)
			value |= 1 << (ch - i);
		ref >>= 1;
	}

	return value;
}

static u32 WIFI_calcCRC32(u8* data, int len)
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
		WIFI_CRC32Table[i] = reflect(WIFI_CRC32Table[i], 32);
	}
}

/*******************************************************************************

	RF-Chip

 *******************************************************************************/

static void WIFI_resetRF(RF2958_IOREG_MAP& rf)
{
	/* reinitialize RF chip with the default values refer RF2958 docs */
	/* CFG1 */
	rf.CFG1.IF_VGA_REG_EN = 1;
	rf.CFG1.IF_VCO_REG_EN = 1;
	rf.CFG1.RF_VCO_REG_EN = 1;
	rf.CFG1.HYBERNATE = 0;
	rf.CFG1.REF_SEL = 0;
	/* IFPLL1 */
	rf.IFPLL1.DAC = 3;
	rf.IFPLL1.P1 = 0;
	rf.IFPLL1.LD_EN1 = 0;
	rf.IFPLL1.AUTOCAL_EN1 = 0;
	rf.IFPLL1.PDP1 = 1;
	rf.IFPLL1.CPL1 = 0;
	rf.IFPLL1.LPF1 = 0;
	rf.IFPLL1.VTC_EN1 = 1;
	rf.IFPLL1.KV_EN1 = 0;
	rf.IFPLL1.PLL_EN1 = 0;
	/* IFPLL2 */
	rf.IFPLL2.IF_N = 0x22;
	/* IFPLL3 */
	rf.IFPLL3.KV_DEF1 = 8;
	rf.IFPLL3.CT_DEF1 = 7;
	rf.IFPLL3.DN1 = 0x1FF;
	/* RFPLL1 */
	rf.RFPLL1.DAC = 3;
	rf.RFPLL1.P = 0;
	rf.RFPLL1.LD_EN = 0;
	rf.RFPLL1.AUTOCAL_EN = 0;
	rf.RFPLL1.PDP = 1;
	rf.RFPLL1.CPL = 0;
	rf.RFPLL1.LPF = 0;
	rf.RFPLL1.VTC_EN = 1;
	rf.RFPLL1.KV_EN = 0;
	rf.RFPLL1.PLL_EN = 0;
	/* RFPLL2 */
	rf.RFPLL2.NUM2 = 0;
	rf.RFPLL2.N2 = 0x5E;
	/* RFPLL3 */
	rf.RFPLL3.NUM2 = 0;
	/* RFPLL4 */
	rf.RFPLL4.KV_DEF = 8;
	rf.RFPLL4.CT_DEF = 7;
	rf.RFPLL4.DN = 0x145;
	/* CAL1 */
	rf.CAL1.LD_WINDOW = 2;
	rf.CAL1.M_CT_VALUE = 8;
	rf.CAL1.TLOCK = 7;
	rf.CAL1.TVCO = 0x0F;
	/* TXRX1 */
	rf.TXRX1.TXBYPASS = 0;
	rf.TXRX1.INTBIASEN = 0;
	rf.TXRX1.TXENMODE = 0;
	rf.TXRX1.TXDIFFMODE = 0;
	rf.TXRX1.TXLPFBW = 2;
	rf.TXRX1.RXLPFBW = 2;
	rf.TXRX1.TXVGC = 0;
	rf.TXRX1.PCONTROL = 0;
	rf.TXRX1.RXDCFBBYPS = 0;
	/* PCNT1 */
	rf.PCNT1.TX_DELAY = 0;
	rf.PCNT1.PC_OFFSET = 0;
	rf.PCNT1.P_DESIRED = 0;
	rf.PCNT1.MID_BIAS = 0;
	/* PCNT2 */
	rf.PCNT2.MIN_POWER = 0;
	rf.PCNT2.MID_POWER = 0;
	rf.PCNT2.MAX_POWER = 0;
	/* VCOT1 */
	rf.VCOT1.AUX1 = 0;
	rf.VCOT1.AUX = 0;
}

/*******************************************************************************

	wifimac IO: a lot of the wifi regs are action registers, that are mirrored
				without action, so the default IO via MMU.c does not seem to
				be very suitable

				all registers are 16 bit

 *******************************************************************************/

static void WIFI_TXStart(const WifiTXLocIndex txSlotIndex, IOREG_W_TXBUF_LOCATION& txLocation);

static void WIFI_SetIRQ(const WifiIRQ irq)
{
	WIFI_IOREG_MAP& io = wifiHandler->GetWifiData().io;

	u16 oldFlags = io.IF.value & io.IE.value;

	io.IF.value |= ((1 << irq) & 0xFBFF);
	u16 newFlags = io.IF.value & io.IE.value;

	if((oldFlags == 0) && (newFlags != 0))
	{
		NDS_makeIrq(ARMCPU_ARM7, IRQ_BIT_ARM7_WIFI); // cascade it via arm7 wifi irq
	}
}

static void WIFI_triggerIRQ(const WifiIRQ irq)
{
	WIFI_IOREG_MAP& io = wifiHandler->GetWifiData().io;

	switch(irq)
	{
		case WifiIRQ00_RXComplete:
		case WifiIRQ01_TXComplete:
		case WifiIRQ02_RXEventIncrement:
		case WifiIRQ03_TXErrorIncrement:
		case WifiIRQ04_RXEventOverflow:
		case WifiIRQ05_TXErrorOverflow:
		case WifiIRQ08_TXCountExpired:
		case WifiIRQ09_RXCountExpired:
		case WifiIRQ11_RFWakeup:
			WIFI_SetIRQ(irq);
			break;

		case WifiIRQ06_RXStart:
			io.RF_STATUS.RFStatus = WifiRFStatus6_RXEnabled;
			io.RF_PINS.value = RFPinsLUT[WifiRFStatus6_RXEnabled];
			WIFI_SetIRQ(irq);
			break;

		case WifiIRQ07_TXStart:
			io.TX_SEQNO.Number++;
			WIFI_SetIRQ(irq);
			break;

		case WifiIRQ10_UNUSED:
			// Do nothing.
			break;

		case WifiIRQ12_UNKNOWN:
			WIFI_LOG(2, "IRQ 12 triggered.\n");
			io.TX_SEQNO.Number++;
			WIFI_SetIRQ(irq);
			break;

		case WifiIRQ13_TimeslotPostBeacon:
			{
				WIFI_SetIRQ(irq);

				if(io.POWER_TX.AutoSleep != 0)
				{
					io.RF_STATUS.RFStatus = 0x9;
					io.RF_PINS.CarrierSense = 0;
					io.RF_PINS.TXMain = 1;
					io.RF_PINS.UNKNOWN1 = 1;
					io.RF_PINS.TX_On = 1;
					io.RF_PINS.RX_On = 0;

					io.INTERNAL_034 = 0x0002;
					io.TXREQ_READ.value &= 0x0010;
					io.POWERSTATE.WillPowerOn = 0;
					io.POWERSTATE.IsPowerOff = 1;
				}
				break;
			}

		case WifiIRQ14_TimeslotBeacon:
			{
				if(io.US_COMPARECNT.ForceIRQ14 == 0)
				{
					io.BEACONCOUNT1 = io.BEACONINT.Interval;
				}

				if(io.US_COMPARECNT.EnableCompare != 0)
				{
					WIFI_SetIRQ(irq);

					io.BEACONCOUNT2 = 0xFFFF;
					io.TXREQ_READ.Loc1 = 0;
					io.TXREQ_READ.Loc2 = 0;
					io.TXREQ_READ.Loc3 = 0;

					if(io.TXBUF_BEACON.TransferRequest != 0)
					{
						io.TXBUSY.Beacon = 1;
						io.RF_PINS.value = RFPinsLUT[WifiRFStatus3_TXEnabled];

						WIFI_TXStart(WifiTXLocIndex_BEACON, io.TXBUF_BEACON);
					}

					if(io.LISTENCOUNT.Count == 0)
					{
						io.LISTENCOUNT.Count = io.LISTENINT.Interval;
					}

					io.LISTENCOUNT.Count--;
				}
				break;
			}

		case WifiIRQ15_TimeslotPreBeacon:
			{
				WIFI_SetIRQ(irq);

				if(io.POWER_TX.AutoWakeup != 0)
				{
					io.RF_STATUS.RFStatus = 0x1;
					io.RF_PINS.RX_On = 1;
				}
				break;
			}
	}
}

TXPacketHeader WIFI_GenerateTXHeader(bool isTXRate20, size_t txLength)
{
	TXPacketHeader txHeader;
	txHeader.txStatus = 1;
	txHeader.mpSlaves = 0;
	txHeader.seqNumberControl = 0;
	txHeader.UNKNOWN1 = 0;
	txHeader.UNKNOWN2 = 0;
	txHeader.txRate = (isTXRate20) ? 20 : 10;
	txHeader.UNKNOWN3 = 0;
	txHeader.length = txLength;

	return txHeader;
}

RXPacketHeader WIFI_GenerateRXHeader(const u8* packetIEEE80211HeaderPtr, const u16 timeStamp, const bool isTXRate20, const u16 emuPacketSize)
{
	const WIFI_IOREG_MAP& io = wifiHandler->GetWifiData().io;

	RXPacketHeader rxHeader;
	rxHeader.rxFlags.value = 0;

	const WifiFrameControl& fc = (WifiFrameControl&)packetIEEE80211HeaderPtr[0];

	switch((WifiFrameType)fc.Type)
	{
		case WifiFrameType_Management:
			{
				const WifiMgmtFrameHeader& IEEE80211Header = (WifiMgmtFrameHeader&)packetIEEE80211HeaderPtr[0];
				rxHeader.rxFlags.MatchingBSSID = (WIFI_compareMAC(IEEE80211Header.BSSID, io.BSSID)) ? 1 : 0;

				if(fc.Subtype == WifiFrameManagementSubtype_Beacon)
				{
					rxHeader.rxFlags.FrameType = 0x1;
				}
				else
				{
					rxHeader.rxFlags.FrameType = 0x0;
				}
				break;
			}

		case WifiFrameType_Control:
			{
				rxHeader.rxFlags.FrameType = 0x5;

				switch((WifiFrameControlSubtype)fc.Subtype)
				{
					case WifiFrameControlSubtype_PSPoll:
						{
							const WifiCtlFrameHeaderPSPoll& IEEE80211Header = (WifiCtlFrameHeaderPSPoll&)packetIEEE80211HeaderPtr[0];
							rxHeader.rxFlags.MatchingBSSID = (WIFI_compareMAC(IEEE80211Header.BSSID, io.BSSID)) ? 1 : 0;
							break;
						}

					case WifiFrameControlSubtype_RTS:
					case WifiFrameControlSubtype_CTS:
					case WifiFrameControlSubtype_ACK:
						rxHeader.rxFlags.MatchingBSSID = 1;
						break;

					case WifiFrameControlSubtype_End:
						{
							const WifiCtlFrameHeaderEnd& IEEE80211Header = (WifiCtlFrameHeaderEnd&)packetIEEE80211HeaderPtr[0];
							rxHeader.rxFlags.MatchingBSSID = (WIFI_compareMAC(IEEE80211Header.BSSID, io.BSSID)) ? 1 : 0;
							break;
						}

					case WifiFrameControlSubtype_EndAck:
						{
							const WifiCtlFrameHeaderEndAck& IEEE80211Header = (WifiCtlFrameHeaderEndAck&)packetIEEE80211HeaderPtr[0];
							rxHeader.rxFlags.MatchingBSSID = (WIFI_compareMAC(IEEE80211Header.BSSID, io.BSSID)) ? 1 : 0;
							break;
						}

					default:
						break;
				}
				break;
			}

		case WifiFrameType_Data:
			{
				// By default, set the FrameType flags to 8 for a data frame.
				// This can be overridden based on other frame control states.
				rxHeader.rxFlags.FrameType = 0x8;

				switch((WifiFCFromToState)fc.FromToState)
				{
					case WifiFCFromToState_STA2STA:
						{
							const WifiDataFrameHeaderSTA2STA& IEEE80211Header = (WifiDataFrameHeaderSTA2STA&)packetIEEE80211HeaderPtr[0];
							rxHeader.rxFlags.MatchingBSSID = (WIFI_compareMAC(IEEE80211Header.BSSID, io.BSSID)) ? 1 : 0;
							break;
						}

					case WifiFCFromToState_STA2DS:
						{
							const WifiDataFrameHeaderSTA2DS& IEEE80211Header = (WifiDataFrameHeaderSTA2DS&)packetIEEE80211HeaderPtr[0];
							rxHeader.rxFlags.MatchingBSSID = (WIFI_compareMAC(IEEE80211Header.BSSID, io.BSSID)) ? 1 : 0;

							if(fc.Subtype == WifiFrameDataSubtype_DataAck)
							{
								rxHeader.rxFlags.FrameType = 0xE;
							}
							else if(fc.Subtype == WifiFrameDataSubtype_Ack)
							{
								rxHeader.rxFlags.FrameType = 0xF;
							}
							break;
						}

					case WifiFCFromToState_DS2STA:
						{
							const WifiDataFrameHeaderDS2STA& IEEE80211Header = (WifiDataFrameHeaderDS2STA&)packetIEEE80211HeaderPtr[0];
							rxHeader.rxFlags.MatchingBSSID = (WIFI_compareMAC(IEEE80211Header.BSSID, io.BSSID)) ? 1 : 0;

							if(fc.Subtype == WifiFrameDataSubtype_DataPoll)
							{
								rxHeader.rxFlags.FrameType = 0xC;
							}
							else if(fc.Subtype == WifiFrameDataSubtype_DataAck)
							{
								rxHeader.rxFlags.FrameType = 0xD;
							}
							break;
						}

					case WifiFCFromToState_DS2DS:
						{
							// We shouldn't be receiving any DS-to-DS packets, but include this case anyways
							// to account for all possible code paths.
							rxHeader.rxFlags.MatchingBSSID = 0;
							break;
						}
				}
				break;
			}

		default:
			break;
	}

	rxHeader.rxFlags.UNKNOWN1 = 1;
	rxHeader.rxFlags.MoreFragments = fc.MoreFragments;

	rxHeader.UNKNOWN1 = 0x0040;
	rxHeader.timeStamp = timeStamp;
	rxHeader.txRate = (isTXRate20) ? 20 : 10;
	rxHeader.length = emuPacketSize;
	rxHeader.rssiMax = 255;
	rxHeader.rssiMin = 240;

	return rxHeader;
}

static void WIFI_TXStart(const WifiTXLocIndex txSlotIndex, IOREG_W_TXBUF_LOCATION& txLocation)
{
	WifiData& wifi = wifiHandler->GetWifiData();
	WIFI_IOREG_MAP& io = wifi.io;

	WIFI_LOG(3, "TX slot %i trying to send a packet: TXCnt = %04X, TXBufLoc = %04X\n",
		(int)txSlotIndex, io.TXREQ_READ.value, txLocation.value);

	if(txLocation.TransferRequest != 0)	/* is slot enabled? */
	{
		//printf("send packet at %08X, lr=%08X\n", NDS_ARM7.instruct_adr, NDS_ARM7.R[14]);
		u32 byteAddress = txLocation.HalfwordAddress << 1;

		// is there even enough space for the header (6 hwords) in the tx buffer?
		if(byteAddress > (0x2000 - sizeof(TXPacketHeader) - sizeof(WifiFrameControl)))
		{
			WIFI_LOG(1, "TX slot %i trying to send a packet overflowing from the TX buffer (address %04X). Attempt ignored.\n",
				(int)txSlotIndex, (int)byteAddress);
			return;
		}

		TXPacketHeader& txHeader = (TXPacketHeader&)wifi.RAM[byteAddress];
		const WifiFrameControl& fc = (WifiFrameControl&)wifi.RAM[byteAddress + sizeof(TXPacketHeader)];

		//printf("---------- SENDING A PACKET ON SLOT %i, FrameCtl = %04X ----------\n",
		//	slot, wifi.RAM[byteAddress + sizeof(TXPacketHeader)]);

		// 12 byte header TX Header: http://www.akkit.org/info/dswifi.htm#FmtTx

		// The minimum possible frame length is 14 bytes.
		// - The TX frame length is header + body + FCS.
		// - Possible header sizes range from 10 - 30 bytes.
		// - It is possible for the frame body to be 0 bytes for certain frame types/subtypes.
		// - The frame check sequence (FCS) is always 4 bytes.
		if(txHeader.length < 14)
		{
			WIFI_LOG(1, "TX slot %i trying to send a packet with length field set to zero. Attempt ignored.\n",
				(int)txSlotIndex);
			return;
		}

		// Align packet length
		txHeader.length = ((txHeader.length + 3) & 0xFFFC);

		// Set sequence number if required
		if((txSlotIndex == WifiTXLocIndex_BEACON) || (txLocation.IEEESeqCtrl == 0))
		{
			if(fc.Type == WifiFrameType_Management)
			{
				WifiMgmtFrameHeader& mgmtHeader = (WifiMgmtFrameHeader&)wifi.RAM[byteAddress + sizeof(TXPacketHeader)];
				mgmtHeader.seqCtl.SequenceNumber = io.TX_SEQNO.Number;
				mgmtHeader.seqCtl.FragmentNumber = 0;
			}
			else if(fc.Type == WifiFrameType_Data)
			{
				// The frame header may not necessarily be a STA-to-STA header, but all of the data frame headers
				// place the sequence number value at the same location in memory, so we can just use the
				// STA-to-STA header to represent all of the data frame headers.
				WifiDataFrameHeaderSTA2STA& dataHeader = (WifiDataFrameHeaderSTA2STA&)wifi.RAM[byteAddress + sizeof(TXPacketHeader)];
				dataHeader.seqCtl.SequenceNumber = io.TX_SEQNO.Number;
				dataHeader.seqCtl.FragmentNumber = 0;
			}
		}

		// Calculate and set FCS
		u32 crc32 = WIFI_calcCRC32(&wifi.RAM[byteAddress + sizeof(TXPacketHeader)], txHeader.length - 4);
		*(u32*)&wifi.RAM[byteAddress + sizeof(TXPacketHeader) + txHeader.length - 4] = crc32;

		WIFI_triggerIRQ(WifiIRQ07_TXStart);

		if((txSlotIndex == WifiTXLocIndex_LOC1) || (txSlotIndex == WifiTXLocIndex_LOC2) || (txSlotIndex == WifiTXLocIndex_LOC3))
		{
			TXPacketInfo& txPacketInfo = wifiHandler->GetPacketInfoAtSlot(txSlotIndex);
			txPacketInfo.emuPacketLength = txHeader.length;
			txPacketInfo.remainingBytes = txHeader.length + sizeof(TXPacketHeader);

			switch(txSlotIndex)
			{
				case WifiTXLocIndex_LOC1: io.TXBUSY.Loc1 = 1; break;
					//case WifiTXLocIndex_CMD: io.TXBUSY.Cmd = 1; break;
				case WifiTXLocIndex_LOC2: io.TXBUSY.Loc2 = 1; break;
				case WifiTXLocIndex_LOC3: io.TXBUSY.Loc3 = 1; break;
					//case WifiTXLocIndex_BEACON: io.TXBUSY.Beacon = 1; break;

				default:
					break;
			}

			if(txSlotIndex == WifiTXLocIndex_LOC3)
			{
				wifi.txCurrentSlot = WifiTXLocIndex_LOC3;
			}
			else if((txSlotIndex == WifiTXLocIndex_LOC2) && (wifi.txCurrentSlot == WifiTXLocIndex_LOC1))
			{
				wifi.txCurrentSlot = WifiTXLocIndex_LOC2;
			}

			io.RXTX_ADDR.HalfwordAddress = byteAddress >> 1;

			io.RF_STATUS.RFStatus = 0x03;
			io.RF_PINS.CarrierSense = 0;
			io.RF_PINS.TXMain = 1;
			io.RF_PINS.UNKNOWN1 = 1;
			io.RF_PINS.TX_On = 1;
			io.RF_PINS.RX_On = 0;
			#if 0
			WIFI_SoftAP_RecvPacketFromDS(&wifi.RAM[address + sizeof(TXPacketHeader)], txHeader.length);
			WIFI_triggerIRQ(WifiIRQ01_TXComplete);

			txHeader.txStatus = 0x0001;
			txHeader.UNKNOWN3 = 0;
			#endif
		}
		else if(txSlotIndex == WifiTXLocIndex_CMD)
		{
			wifiHandler->CommSendPacket(txHeader, &wifi.RAM[byteAddress + sizeof(TXPacketHeader)]);
			WIFI_triggerIRQ(WifiIRQ12_UNKNOWN);

			// If bit 13 is set, then it has priority over bit 14
			if(io.TXSTATCNT.UpdateTXStat_0B01 != 0)
			{
				WIFI_triggerIRQ(WifiIRQ01_TXComplete);
				io.TXSTAT.value = 0x0B01;
			}
			else if(io.TXSTATCNT.UpdateTXStat_0800 != 0)
			{
				WIFI_triggerIRQ(WifiIRQ01_TXComplete);
				io.TXSTAT.value = 0x0800;
			}

			txLocation.TransferRequest = 0;

			txHeader.txStatus = 0x0001;
			txHeader.UNKNOWN3 = 0;
		}
		else if(txSlotIndex == WifiTXLocIndex_BEACON)
		{
			// Set timestamp
			*(u64*)&wifi.RAM[byteAddress + sizeof(TXPacketHeader) + sizeof(WifiMgmtFrameHeader)] = io.US_COUNT;

			wifiHandler->CommSendPacket(txHeader, &wifi.RAM[byteAddress + sizeof(TXPacketHeader)]);

			if(io.TXSTATCNT.UpdateTXStatBeacon != 0)
			{
				WIFI_triggerIRQ(WifiIRQ01_TXComplete);
				io.TXSTAT.value = 0x0301;
			}

			txHeader.txStatus = 0x0001;
			txHeader.UNKNOWN3 = 0;
		}
	}
}

void WIFI_write16(u32 address, u16 val)
{
	bool action = false;
	if(!nds.power2.wifi) return;

	WifiData& wifi = wifiHandler->GetWifiData();
	WIFI_IOREG_MAP& io = wifi.io;

	u32 page = address & 0x7000;

	// 0x2000 - 0x3FFF: unused
	if((page >= 0x2000) && (page < 0x4000))
		return;

	WIFI_LOG(5, "Write at address %08X, %04X\n", address, val);
	//printf("WIFI: Write at address %08X, %04X, pc=%08X\n", address, val, NDS_ARM7.instruct_adr);

	// 0x4000 - 0x5FFF: wifi RAM
	if((page >= 0x4000) && (page < 0x6000))
	{
		/* access to the circular buffer */
		*(u16*)&wifi.RAM[address & 0x1FFE] = val;
		return;
	}

	// anything else: I/O ports
	// only the first mirror (0x0000 - 0x0FFF) causes a special action
	if(page == 0x0000) action = true;

	address &= 0x0FFF;
	switch(address)
	{
		case REG_WIFI_MODE: // 0x004
			{
				IOREG_W_MODE_RST MODE_RST;
				MODE_RST.value = val;

				io.MODE_RST.UNKNOWN1 = MODE_RST.UNKNOWN1;
				io.MODE_RST.UNKNOWN2 = MODE_RST.UNKNOWN2;
				io.MODE_RST.UNKNOWN3 = MODE_RST.UNKNOWN3;

				if((io.MODE_RST.TXMasterEnable == 0) && (MODE_RST.TXMasterEnable != 0))
				{
					io.INTERNAL_034 = 0x0002;
					io.RF_STATUS.value = 0x0009;
					io.RF_PINS.value = 0x0046;
					io.INTERNAL_27C = 0x0005;

					// According to GBATEK, the following registers might be reset to some unknown values.
					// io.INTERNAL_2A2					= ???;
				}

				if((io.MODE_RST.TXMasterEnable != 0) && (MODE_RST.TXMasterEnable == 0))
				{
					io.INTERNAL_27C = 0x000A;
				}

				io.MODE_RST.TXMasterEnable = MODE_RST.TXMasterEnable;

				if(MODE_RST.ResetPortSet1 != 0)
				{
					io.RXBUF_WR_ADDR.value = 0x0000;
					io.CMD_TOTALTIME = 0x0000;
					io.CMD_REPLYTIME = 0x0000;
					io.X_1A4 = 0x0000;
					io.INTERNAL_278 = 0x000F;

					// According to GBATEK, the following registers might be reset to some unknown values.
					//io.TXREQ_SET.value				= ???;
					//io.INTERNAL_0BA					= ???;
					//io.INTERNAL_204					= ???;
					//io.INTERNAL_25C					= ???;
					//io.RXTX_ADDR.value				= ???;
					//io.INTERNAL_274					= ???;
				}

				if(MODE_RST.ResetPortSet2 != 0)
				{
					io.MODE_WEP.value = 0x0000;
					io.TXSTATCNT.value = 0x0000;
					io.X_00A = 0x0000;
					io.MACADDR[0] = 0x00;
					io.MACADDR[1] = 0x00;
					io.MACADDR[2] = 0x00;
					io.MACADDR[3] = 0x00;
					io.MACADDR[4] = 0x00;
					io.MACADDR[5] = 0x00;
					io.BSSID[0] = 0x00;
					io.BSSID[1] = 0x00;
					io.BSSID[2] = 0x00;
					io.BSSID[3] = 0x00;
					io.BSSID[4] = 0x00;
					io.BSSID[5] = 0x00;
					io.AID_LOW.value = 0x0000;
					io.AID_FULL.value = 0x0000;
					io.TX_RETRYLIMIT.value = 0x0707;
					io.INTERNAL_02E = 0x0000;
					io.RXBUF_BEGIN = 0x4000;
					io.RXBUF_END = 0x4800;
					io.TXBUF_TIM.value = 0x0000;
					io.PREAMBLE.value = 0x0001;
					io.RXFILTER.value = 0x0401;
					io.CONFIG_0D4 = 0x0001;
					io.RXFILTER2.value = 0x0008;
					io.CONFIG_0EC = 0x3F03;
					io.TX_HDR_CNT.value = 0x0000;
					io.INTERNAL_198 = 0x0000;
					io.X_1A2.value = 0x0001;
					io.INTERNAL_224 = 0x0003;
					io.INTERNAL_230 = 0x0047;
				}
				break;
			}

		case REG_WIFI_WEP: // 0x006
			io.MODE_WEP.value = val;
			break;

		case REG_WIFI_TXSTATCNT: // 0x008
			io.TXSTATCNT.value = val;
			break;

		case REG_WIFI_X_00A: // 0x00A
			io.X_00A = val;
			break;

		case REG_WIFI_IF: // 0x010
			io.IF.value &= ~val;
			break;

		case REG_WIFI_IE: // 0x012
			io.IE.value = val;
			break;

		case REG_WIFI_MAC0: // 0x018
			io.MACADDR0.value = val;
			break;

		case REG_WIFI_MAC1: // 0x01A
			io.MACADDR1.value = val;
			break;

		case REG_WIFI_MAC2: // 0x01C
			io.MACADDR2.value = val;
			break;

		case REG_WIFI_BSS0: // 0x020
			io.BSSID0.value = val;
			break;

		case REG_WIFI_BSS1: // 0x022
			io.BSSID1.value = val;
			break;

		case REG_WIFI_BSS2: // 0x024
			io.BSSID2.value = val;
			break;

		case REG_WIFI_AID_LOW: // 0x028
			io.AID_LOW.value = val & 0x000F;
			break;

		case REG_WIFI_AID_HIGH: // 0x02A
			io.AID_FULL.value = val & 0x07FF;
			break;

		case REG_WIFI_RETRYLIMIT: // 0x02C
			io.TX_RETRYLIMIT.value = val;
			break;

		case 0x02E: // 0x02E
			io.INTERNAL_02E = val;
			break;

		case REG_WIFI_RXCNT: // 0x030
			{
				IOREG_W_RXCNT RXCNT;
				RXCNT.value = val & 0xFF8F;

				io.RXCNT.EnableRXFIFOQueuing = RXCNT.EnableRXFIFOQueuing;
				if(io.RXCNT.EnableRXFIFOQueuing == 0)
				{
					wifiHandler->CommEmptyRXQueue();
				}

				if(io.RXCNT.UNKNOWN1 != RXCNT.UNKNOWN1)
				{
					io.RXCNT.UNKNOWN1 = RXCNT.UNKNOWN1;
					WIFI_LOG(2, "Writing value of %04Xh to RXCNT.UNKNOWN1\n", (int)io.RXCNT.UNKNOWN1);
				}

				if(io.RXCNT.UNKNOWN2 != RXCNT.UNKNOWN2)
				{
					io.RXCNT.UNKNOWN2 = RXCNT.UNKNOWN2;
					WIFI_LOG(2, "Writing value of %04Xh to RXCNT.UNKNOWN2\n", (int)io.RXCNT.UNKNOWN2);
				}

				if(RXCNT.CopyAddrToWRCSR != 0)
				{
					io.RXBUF_WRCSR.HalfwordAddress = io.RXBUF_WR_ADDR.HalfwordAddress;
				}

				if(RXCNT.CopyTXBufReply1To2 != 0)
				{
					io.TXBUF_REPLY2.HalfwordAddress = io.TXBUF_REPLY1.HalfwordAddress;
					io.TXBUF_REPLY1.value = 0x0000;
				}
				break;
			}

		case REG_WIFI_WEPCNT: // 0x032
			io.WEP_CNT.value = val & 0x8000;
			break;

		case 0x034: // 0x034
			io.INTERNAL_034 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_034\n", (int)io.INTERNAL_034);
			break;

		case REG_WIFI_POWER_US: // 0x036
			{
				IOREG_W_POWER_US POWER_US;
				POWER_US.value = val & 0x0003;

				io.POWER_US.Disable = (wifiHandler->GetCurrentEmulationLevel() == WifiEmulationLevel_Off) ? 1 : POWER_US.Disable;

				if(io.POWER_US.UNKNOWN1 != POWER_US.UNKNOWN1)
				{
					io.POWER_US.UNKNOWN1 = POWER_US.UNKNOWN1;
					WIFI_LOG(2, "Writing value of %d to POWER_US.UNKNOWN1\n", (int)POWER_US.UNKNOWN1);
				}
				break;
			}

		case REG_WIFI_POWER_TX: // 0x038
			{
				IOREG_W_POWER_TX POWER_TX;
				POWER_TX.value = val & 0x000F;

				io.POWER_TX.AutoWakeup = POWER_TX.AutoWakeup;
				io.POWER_TX.AutoSleep = POWER_TX.AutoSleep;

				if(io.POWER_TX.UNKNOWN1 != POWER_TX.UNKNOWN1)
				{
					io.POWER_TX.UNKNOWN1 = POWER_TX.UNKNOWN1;
					WIFI_LOG(2, "Writing value of %d to POWER_TX.UNKNOWN1\n", (int)POWER_TX.UNKNOWN1);
				}

				if(POWER_TX.UNKNOWN2 != 0)
				{
					WIFI_LOG(2, "Performing unknown action of POWER_TX.UNKNOWN2\n");
				}
				break;
			}

		case REG_WIFI_POWERSTATE: // 0x03C
			{
				IOREG_W_POWERSTATE newPowerState;
				newPowerState.value = val;

				io.POWERSTATE.UNKNOWN1 = newPowerState.UNKNOWN1;
				io.POWERSTATE.RequestPowerOn = newPowerState.RequestPowerOn;

				if(io.POWERSTATE.RequestPowerOn != 0)
				{
					WIFI_triggerIRQ(WifiIRQ11_RFWakeup);

					// This is supposedly the TX beacon transfer flag.
					// Since reading TXREQ_READ bit 4 should always return a 1 (unless
					// POWERFORCE clears it), we'll simply set this bit to 1 on power up
					// so that bit 4 gets its default value.
					io.TXREQ_READ.UNKNOWN1 = 1;

					// Most likely, there should be some delay between firing the IRQ and
					// setting the register bits. But we're not going to emulate the delay
					// right now, so we simply change all of the bits immediately.
					io.POWERSTATE.RequestPowerOn = 0;
					io.POWERSTATE.WillPowerOn = 0;
					io.POWERSTATE.IsPowerOff = 0;
				}
				break;
			}

		case REG_WIFI_POWERFORCE: // 0x040
			{

				io.POWERFORCE.value = val & (u16)0x8001;

				if(io.POWERFORCE.ApplyNewPowerOffState != 0)
				{
					if((io.POWERFORCE.NewPowerOffState != 0) && (io.POWERSTATE.IsPowerOff == 0))
					{
						// Immediate action
						io.INTERNAL_034 = 0x0002;
						io.TXREQ_READ.value = 0x0000; // Note that even bit 4 gets cleared here
						io.RF_STATUS.value = 0x0009;
						io.RF_PINS.value = 0x0046;

						io.POWERSTATE.WillPowerOn = 0;
						io.POWERSTATE.IsPowerOff = 1;
					}
					else if((io.POWERFORCE.NewPowerOffState == 0) && (io.POWERSTATE.IsPowerOff != 0))
					{
						// Delayed action
						io.POWERSTATE.WillPowerOn = 1;
					}

					// This probably shouldn't happen right here, but we need to write to
					// POWERSTATE.IsPowerOff because we need to keep the power on in order
					// to force certain in-game errors to occur. Otherwise, the user might
					// get caught up in an infinite loop.
					if(io.POWERSTATE.WillPowerOn != 0)
					{
						io.POWERSTATE.IsPowerOff = io.POWERFORCE.NewPowerOffState;
					}
				}
				break;
			}

		case REG_WIFI_POWER_UNK: // 0x048
			io.POWER_UNKNOWN.value = val;
			break;

		case REG_WIFI_RXRANGEBEGIN: // 0x050
			{
				io.RXBUF_BEGIN = val;
				if(io.RXBUF_WRCSR.HalfwordAddress < ((io.RXBUF_BEGIN & 0x1FFE) >> 1))
				{
					io.RXBUF_WRCSR.HalfwordAddress = ((io.RXBUF_BEGIN & 0x1FFE) >> 1);
				}
				break;
			}

		case REG_WIFI_RXRANGEEND: // 0x052
			{
				io.RXBUF_END = val;
				if(io.RXBUF_WRCSR.HalfwordAddress >= ((io.RXBUF_END & 0x1FFE) >> 1))
				{
					io.RXBUF_WRCSR.HalfwordAddress = ((io.RXBUF_BEGIN & 0x1FFE) >> 1);
				}
				break;
			}

		case REG_WIFI_WRITECSRLATCH: // 0x056
			io.RXBUF_WR_ADDR.value = val & 0x0FFF;
			break;

		case REG_WIFI_CIRCBUFRADR: // 0x058
			io.RXBUF_RD_ADDR.value = val & 0x1FFE;
			break;

		case REG_WIFI_RXREADCSR: // 0x05A
			io.RXBUF_READCSR.value = val & 0x0FFF;
			break;

		case REG_WIFI_RXBUF_COUNT: // 0x05C
			io.RXBUF_COUNT.value = val & 0x0FFF;
			break;

		case REG_WIFI_CIRCBUFRD_END: // 0x062
			io.RXBUF_GAP.value = val & 0x1FFE;
			break;

		case REG_WIFI_CIRCBUFRD_SKIP: // 0x064
			io.RXBUF_GAPDISP.value = val & 0x0FFF;
			break;

		case REG_WIFI_CIRCBUFWADR: // 0x068
			io.TXBUF_WR_ADDR.value = val & 0x1FFE;
			break;

		case REG_WIFI_TXBUFCOUNT: // 0x06C
			io.TXBUF_COUNT.value = val & 0x0FFF;
			break;

		case REG_WIFI_CIRCBUFWRITE: // 0x070
			{
				/* set value into the circ buffer, and move cursor to the next hword on action */
				//printf("wifi: circbuf fifo write at %04X, %04X (action=%i)\n", (wifiMac.CircBufWriteAddress & 0x1FFF), val, action);
				*(u16*)&wifi.RAM[io.TXBUF_WR_ADDR.HalfwordAddress << 1] = val;
				if(action)
				{
					/* move to next hword */
					io.TXBUF_WR_ADDR.HalfwordAddress++;
					if(io.TXBUF_WR_ADDR.HalfwordAddress == io.TXBUF_GAP.HalfwordAddress)
					{
						/* on end of buffer, add skip hwords to it */
						io.TXBUF_GAP.HalfwordAddress += io.TXBUF_GAPDISP.HalfwordOffset;
					}

					if(io.TXBUF_COUNT.Count > 0)
					{
						io.TXBUF_COUNT.Count--;
						if(io.TXBUF_COUNT.Count == 0)
						{
							WIFI_triggerIRQ(WifiIRQ08_TXCountExpired);
						}
					}
				}
				break;
			}

		case REG_WIFI_CIRCBUFWR_END: // 0x074
			io.TXBUF_GAP.value = val & 0x1FFF;
			break;

		case REG_WIFI_CIRCBUFWR_SKIP: // 0x076
			io.TXBUF_GAPDISP.value = val & 0x0FFF;
			break;

		case 0x078: // 0x078
			io.INTERNAL_078 = val;
			break;

		case REG_WIFI_TXLOCBEACON: // 0x080
			{
				IOREG_W_TXBUF_LOCATION TXBUF_BEACON;
				TXBUF_BEACON.value = val;

				io.TXBUF_BEACON.HalfwordAddress = TXBUF_BEACON.HalfwordAddress;
				io.TXBUF_BEACON.Bit12 = TXBUF_BEACON.Bit12;
				io.TXBUF_BEACON.IEEESeqCtrl = TXBUF_BEACON.IEEESeqCtrl;
				io.TXBUF_BEACON.TransferRequest = TXBUF_BEACON.TransferRequest;

				if(io.TXBUF_BEACON.UNKNOWN1 != TXBUF_BEACON.UNKNOWN1)
				{
					io.TXBUF_BEACON.UNKNOWN1 = TXBUF_BEACON.UNKNOWN1;
					WIFI_LOG(2, "Writing value of %d to TXBUF_BEACON.UNKNOWN1\n", (int)TXBUF_BEACON.UNKNOWN1);
				}

				if(io.TXBUF_BEACON.TransferRequest != 0)
				{
					WIFI_LOG(3, "Beacon transmission enabled to send the packet at %08X every %i milliseconds.\n",
						0x04804000 + (io.TXBUF_BEACON.HalfwordAddress << 1), io.BEACONINT.Interval);
				}
				break;
			}

		case REG_WIFI_TXBUF_TIM: // 0x084
			io.TXBUF_TIM.value = val & 0x00FF;
			break;

		case REG_WIFI_LISTENCOUNT: // 0x088
			io.LISTENCOUNT.value = val & 0x00FF;
			break;

		case REG_WIFI_BEACONPERIOD: // 0x08C
			io.BEACONINT.value = val & 0x03FF;
			break;

		case REG_WIFI_LISTENINT: // 0x08E
			io.LISTENINT.value = val & 0x00FF;
			break;

		case REG_WIFI_TXLOCEXTRA: // 0x090
			{
				IOREG_W_TXBUF_LOCATION TXBUF_CMD;
				TXBUF_CMD.value = val;

				io.TXBUF_CMD.HalfwordAddress = TXBUF_CMD.HalfwordAddress;
				io.TXBUF_CMD.Bit12 = TXBUF_CMD.Bit12;
				io.TXBUF_CMD.IEEESeqCtrl = TXBUF_CMD.IEEESeqCtrl;

				if(wifi.cmdCount_u32 != 0)
				{
					io.TXBUF_CMD.TransferRequest = TXBUF_CMD.TransferRequest;
				}

				if(io.TXBUF_CMD.UNKNOWN1 != TXBUF_CMD.UNKNOWN1)
				{
					io.TXBUF_CMD.UNKNOWN1 = TXBUF_CMD.UNKNOWN1;
					WIFI_LOG(2, "Writing value of %d to TXBUF_CMD.UNKNOWN1\n", (int)TXBUF_CMD.UNKNOWN1);
				}
				break;
			}

		case REG_WIFI_TXBUF_REPLY1: // 0x094
			io.TXBUF_REPLY1.value = val;
			break;

		case 0x09C: // 0x09C
			io.INTERNAL_09C = val;
			break;

		case REG_WIFI_TXLOC1: // 0x0A0
			{
				IOREG_W_TXBUF_LOCATION TXBUF_LOCn;
				TXBUF_LOCn.value = val;

				io.TXBUF_LOC1.HalfwordAddress = TXBUF_LOCn.HalfwordAddress;
				io.TXBUF_LOC1.Bit12 = TXBUF_LOCn.Bit12;
				io.TXBUF_LOC1.IEEESeqCtrl = TXBUF_LOCn.IEEESeqCtrl;
				io.TXBUF_LOC1.TransferRequest = TXBUF_LOCn.TransferRequest;

				if(io.TXBUF_LOC1.UNKNOWN1 != TXBUF_LOCn.UNKNOWN1)
				{
					io.TXBUF_LOC1.UNKNOWN1 = TXBUF_LOCn.UNKNOWN1;
					WIFI_LOG(2, "Writing value of %d to TXBUF_LOC1.UNKNOWN1\n", (int)TXBUF_LOCn.UNKNOWN1);
				}
				break;
			}

		case REG_WIFI_TXLOC2: // 0x0A4
			{
				IOREG_W_TXBUF_LOCATION TXBUF_LOCn;
				TXBUF_LOCn.value = val;

				io.TXBUF_LOC2.HalfwordAddress = TXBUF_LOCn.HalfwordAddress;
				io.TXBUF_LOC2.Bit12 = TXBUF_LOCn.Bit12;
				io.TXBUF_LOC2.IEEESeqCtrl = TXBUF_LOCn.IEEESeqCtrl;
				io.TXBUF_LOC2.TransferRequest = TXBUF_LOCn.TransferRequest;

				if(io.TXBUF_LOC2.UNKNOWN1 != TXBUF_LOCn.UNKNOWN1)
				{
					io.TXBUF_LOC2.UNKNOWN1 = TXBUF_LOCn.UNKNOWN1;
					WIFI_LOG(2, "Writing value of %d to TXBUF_LOC2.UNKNOWN1\n", (int)TXBUF_LOCn.UNKNOWN1);
				}
				break;
			}

		case REG_WIFI_TXLOC3: // 0x0A8
			{
				IOREG_W_TXBUF_LOCATION TXBUF_LOCn;
				TXBUF_LOCn.value = val;

				io.TXBUF_LOC3.HalfwordAddress = TXBUF_LOCn.HalfwordAddress;
				io.TXBUF_LOC3.Bit12 = TXBUF_LOCn.Bit12;
				io.TXBUF_LOC3.IEEESeqCtrl = TXBUF_LOCn.IEEESeqCtrl;
				io.TXBUF_LOC3.TransferRequest = TXBUF_LOCn.TransferRequest;

				if(io.TXBUF_LOC3.UNKNOWN1 != TXBUF_LOCn.UNKNOWN1)
				{
					io.TXBUF_LOC3.UNKNOWN1 = TXBUF_LOCn.UNKNOWN1;
					WIFI_LOG(2, "Writing value of %d to TXBUF_LOC3.UNKNOWN1\n", (int)TXBUF_LOCn.UNKNOWN1);
				}
				break;
			}

		case REG_WIFI_TXREQ_RESET: // 0x0AC
			{
				IOREG_W_TXREQ_RESET TXREQ_RESET;
				TXREQ_RESET.value = val;

				if(TXREQ_RESET.Loc1 != 0)
				{
					io.TXREQ_READ.Loc1 = 0;
				}

				if(TXREQ_RESET.Cmd != 0)
				{
					io.TXREQ_READ.Cmd = 0;
				}

				if(TXREQ_RESET.Loc2 != 0)
				{
					io.TXREQ_READ.Loc2 = 0;
				}

				if(TXREQ_RESET.Loc3 != 0)
				{
					io.TXREQ_READ.Loc3 = 0;
				}

				if(TXREQ_RESET.UNKNOWN1 != 0)
				{
					WIFI_LOG(2, "Prevented clearing of TXREQ_READ.UNKNOWN1, for beacon?\n");
				}
				break;
			}

		case REG_WIFI_TXREQ_SET: // 0x0AE
			{
				IOREG_W_TXREQ_SET TXREQ_SET;
				TXREQ_SET.value = val;

				if(TXREQ_SET.Loc1 != 0)
				{
					io.TXREQ_READ.Loc1 = 1;
					WIFI_TXStart(WifiTXLocIndex_LOC1, io.TXBUF_LOC1);
				}

				if(TXREQ_SET.Cmd != 0)
				{
					io.TXREQ_READ.Cmd = 1;
					WIFI_TXStart(WifiTXLocIndex_CMD, io.TXBUF_CMD);
				}

				if(TXREQ_SET.Loc2 != 0)
				{
					io.TXREQ_READ.Loc2 = 1;
					WIFI_TXStart(WifiTXLocIndex_LOC2, io.TXBUF_LOC2);
				}

				if(TXREQ_SET.Loc3 != 0)
				{
					io.TXREQ_READ.Loc3 = 1;
					WIFI_TXStart(WifiTXLocIndex_LOC3, io.TXBUF_LOC3);
				}

				if(TXREQ_SET.UNKNOWN1 != 0)
				{
					io.TXREQ_READ.UNKNOWN1 = 1;
					WIFI_LOG(2, "Setting of TXREQ_SET.UNKNOWN1, for beacon?\n");
				}
				break;
			}

		case REG_WIFI_TXRESET: // 0x0B4
			{
				IOREG_W_TXBUF_RESET TXBUF_RESET;
				TXBUF_RESET.value = val;

				if(TXBUF_RESET.Loc1 != 0)
				{
					io.TXBUF_LOC1.TransferRequest = 0;
				}

				if(TXBUF_RESET.Cmd != 0)
				{
					io.TXBUF_CMD.TransferRequest = 0;
				}

				if(TXBUF_RESET.Loc2 != 0)
				{
					io.TXBUF_LOC2.TransferRequest = 0;
				}

				if(TXBUF_RESET.Loc3 != 0)
				{
					io.TXBUF_LOC3.TransferRequest = 0;
				}

				if(TXBUF_RESET.Reply2 != 0)
				{
					io.TXBUF_REPLY2.TransferRequest = 0;
				}

				if(TXBUF_RESET.Reply1 != 0)
				{
					io.TXBUF_REPLY1.TransferRequest = 0;
				}

				const bool reportUnknownWrite = (val != 0xFFFF);

				if(io.TXBUF_RESET.UNKNOWN1 != TXBUF_RESET.UNKNOWN1)
				{
					io.TXBUF_RESET.UNKNOWN1 = TXBUF_RESET.UNKNOWN1;

					if(reportUnknownWrite)
					{
						WIFI_LOG(2, "Writing value of %04Xh to TXBUF_RESET.UNKNOWN1\n", (int)io.TXBUF_RESET.UNKNOWN1);
					}
				}

				if(io.TXBUF_RESET.UNKNOWN2 != TXBUF_RESET.UNKNOWN2)
				{
					io.TXBUF_RESET.UNKNOWN2 = TXBUF_RESET.UNKNOWN2;

					if(reportUnknownWrite)
					{
						WIFI_LOG(2, "Writing value of %04Xh to TXBUF_RESET.UNKNOWN2\n", (int)io.TXBUF_RESET.UNKNOWN2);
					}
				}
				break;
			}

		case 0x0BA: // 0x0BA
			io.INTERNAL_0BA = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_0BA\n", (int)io.INTERNAL_0BA);
			break;

		case REG_WIFI_PREAMBLE: // 0x0BC
			io.PREAMBLE.value = val;
			break;

		case REG_WIFI_CMD_TOTALTIME: // 0x0C0
			io.CMD_TOTALTIME = val;
			break;

		case REG_WIFI_CMD_REPLYTIME: // 0x0C4
			io.CMD_REPLYTIME = val;
			break;

		case 0x0C8: // 0x0C8
			io.INTERNAL_0C8 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_0C8\n", (int)io.INTERNAL_0C8);
			break;

		case REG_WIFI_RXFILTER: // 0x0D0
			io.RXFILTER.value = val;
			break;

		case REG_WIFI_CONFIG_0D4: // 0x0D4
			io.CONFIG_0D4 = val;
			break;

		case REG_WIFI_CONFIG_0D8: // 0x0D8
			io.CONFIG_0D8 = val;
			break;

		case REG_WIFI_RX_LEN_CROP: // 0x0DA
			io.RX_LEN_CROP.value = val;
			break;

		case REG_WIFI_RXFILTER2: // 0x0E0
			io.RXFILTER2.value = val;
			break;

		case REG_WIFI_USCOUNTERCNT: // 0x0E8
			io.US_COUNTCNT.value = val & 0x0001;
			break;

		case REG_WIFI_USCOMPARECNT: // 0x0EA
			{
				IOREG_W_US_COMPARECNT US_COMPARECNT;
				US_COMPARECNT.value = val & 0x0003;

				io.US_COMPARECNT.EnableCompare = US_COMPARECNT.EnableCompare;

				if(US_COMPARECNT.ForceIRQ14 != 0)
				{
					WIFI_triggerIRQ(WifiIRQ14_TimeslotBeacon);
					io.US_COMPARECNT.ForceIRQ14 = 0;
				}
				break;
			}

		case REG_WIFI_CONFIG_0EC: // 0x0EC
			io.CONFIG_0EC = val;
			break;

		case REG_WIFI_EXTRACOUNTCNT: // 0x0EE
			io.CMD_COUNTCNT.value = val & 0x0001;
			break;

		case REG_WIFI_USCOMPARE0: // 0x0F0
			io.US_COMPARE0 = val;
			break;

		case REG_WIFI_USCOMPARE1: // 0x0F2
			io.US_COMPARE1 = val;
			break;

		case REG_WIFI_USCOMPARE2: // 0x0F4
			io.US_COMPARE2 = val;
			break;

		case REG_WIFI_USCOMPARE3: // 0x0F6
			io.US_COMPARE3 = val;
			break;

		case REG_WIFI_USCOUNTER0: // 0x0F8
			io.US_COUNT0 = val;
			break;

		case REG_WIFI_USCOUNTER1: // 0x0FA
			io.US_COUNT1 = val;
			break;

		case REG_WIFI_USCOUNTER2: // 0x0FC
			io.US_COUNT2 = val;
			break;

		case REG_WIFI_USCOUNTER3: // 0x0FE
			io.US_COUNT3 = val;
			break;

		case 0x100: // 0x100
			io.INTERNAL_100 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_100\n", (int)io.INTERNAL_100);
			break;

		case 0x102: // 0x102
			io.INTERNAL_102 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_102\n", (int)io.INTERNAL_102);
			break;

		case 0x104: // 0x104
			io.INTERNAL_104 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_104\n", (int)io.INTERNAL_104);
			break;

		case 0x106: // 0x106
			io.INTERNAL_106 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_106\n", (int)io.INTERNAL_106);
			break;

		case REG_WIFI_CONTENTFREE: // 0x10C
			io.CONTENTFREE = val;
			break;

		case REG_WIFI_PREBEACONCOUNT: // 0x110
			io.PRE_BEACON = val;
			break;

		case REG_WIFI_EXTRACOUNT: // 0x118
			io.CMD_COUNT = val;
			wifi.cmdCount_u32 = (u32)val * 10;
			break;

		case REG_WIFI_BEACONCOUNT1: // 0x11C
			io.BEACONCOUNT1 = val;
			break;

		case REG_WIFI_CONFIG_120: // 0x120
			io.CONFIG_120 = val;
			break;

		case REG_WIFI_CONFIG_122: // 0x122
			io.CONFIG_122 = val;
			break;

		case REG_WIFI_CONFIG_124: // 0x124
			io.CONFIG_124 = val;
			break;

		case 0x126: // 0x126
			io.INTERNAL_126 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_126\n", (int)io.INTERNAL_126);
			break;

		case REG_WIFI_CONFIG_128: // 0x128
			io.CONFIG_128 = val;
			break;

		case 0x12A: // 0x12A
			io.INTERNAL_12A = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_12A\n", (int)io.INTERNAL_12A);
			break;

		case REG_WIFI_CONFIG_130: // 0x130
			io.CONFIG_130 = val;
			break;

		case REG_WIFI_CONFIG_132: // 0x132
			io.CONFIG_132 = val;
			break;

		case REG_WIFI_BEACONCOUNT2: // 0x134
			io.BEACONCOUNT2 = val;
			break;

		case REG_WIFI_CONFIG_140: // 0x140
			io.CONFIG_140 = val;
			break;

		case REG_WIFI_CONFIG_142: // 0x142
			io.CONFIG_142 = val;
			break;

		case REG_WIFI_CONFIG_144: // 0x144
			io.CONFIG_144 = val;
			break;

		case REG_WIFI_CONFIG_146: // 0x146
			io.CONFIG_146 = val;
			break;

		case REG_WIFI_CONFIG_148: // 0x148
			io.CONFIG_148 = val;
			break;

		case REG_WIFI_CONFIG_14A: // 0x14A
			io.CONFIG_14A = val;
			break;

		case REG_WIFI_CONFIG_14C: // 0x14C
			io.CONFIG_14C = val;
			break;

		case REG_WIFI_CONFIG_150: // 0x150
			io.CONFIG_150 = val;
			break;

		case REG_WIFI_CONFIG_154: // 0x154
			io.CONFIG_154 = val;
			break;

		case REG_WIFI_BBCNT: // 0x158
			{
				bb_t& bb = wifiHandler->GetWifiData().bb;

				io.BB_CNT.value = val & 0xF0FF;

				if(io.BB_CNT.Direction == 5)  // Perform a write
				{
					io.BB_BUSY.Busy = 1;

					if(io.BB_CNT.Index > 0x68)
					{
						WIFI_LOG(2, "Writing value of %02Xh to BB_CNT.Index %02Xh, BB_CNT.Index should be 00h-68h\n", (int)io.BB_WRITE.Data, (int)io.BB_CNT.Index);
					}
					else if(BBIsDataWritable[io.BB_CNT.Index])
					{
						bb.data[io.BB_CNT.Index] = io.BB_WRITE.Data;
					}
				}
				else if(io.BB_CNT.Direction == 6) // Perform a read
				{
					io.BB_BUSY.Busy = 1;

					if(io.BB_CNT.Index > 0x68)
					{
						io.BB_READ.Data = 0x00;
					}
					else
					{
						io.BB_READ.Data = bb.data[io.BB_CNT.Index];
					}
				}

				// Normally, there should be some sort of transfer delay, but we're
				// not going to emulate the delay right now. Therefore, assume for
				// now that the transfer occurs instantaneously and clear the busy
				// flag accordingly.
				io.BB_BUSY.Busy = 0;
				break;
			}

		case REG_WIFI_BBWRITE: // 0x15A
			io.BB_WRITE.value = val & 0x00FF;
			break;

		case REG_WIFI_BBMODE: // 0x160
			io.BB_MODE.value = val & 0x0041;
			break;

		case REG_WIFI_BBPOWER: // 0x168
			io.BB_POWER.value = val & 0x800F;
			break;

		case 0x16A: // 0x16A
			io.INTERNAL_16A = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_16A\n", (int)io.INTERNAL_16A);
			break;

		case 0x170: // 0x170
			io.INTERNAL_170 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_170\n", (int)io.INTERNAL_170);
			break;

		case 0x172: // 0x172
			io.INTERNAL_172 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_172\n", (int)io.INTERNAL_172);
			break;

		case 0x174: // 0x174
			io.INTERNAL_174 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_174\n", (int)io.INTERNAL_174);
			break;

		case 0x176: // 0x176
			io.INTERNAL_176 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_176\n", (int)io.INTERNAL_176);
			break;

		case 0x178: // 0x178
			io.INTERNAL_178 = val;
			break;

		case REG_WIFI_RFDATA2: // 0x17C
			{
				RF2958_IOREG_MAP& rf = wifiHandler->GetWifiData().rf;

				io.RF_DATA2.value = val & 0x00FF;
				io.RF_BUSY.Busy = 1;

				const RegAddrRF2958 index = (RegAddrRF2958)io.RF_DATA2.Type2.Index;

				if(io.RF_DATA2.Type2.ReadCommand == 0) // Perform a write
				{
					switch(index)
					{
						case REG_RF2958_CFG1:
						case REG_RF2958_IPLL1:
						case REG_RF2958_IPLL2:
						case REG_RF2958_IPLL3:
						case REG_RF2958_RFPLL1:
						case REG_RF2958_RFPLL4:
						case REG_RF2958_CAL1:
						case REG_RF2958_TXRX1:
						case REG_RF2958_PCNT1:
						case REG_RF2958_PCNT2:
						case REG_RF2958_VCOT1:
							rf.reg[index].DataLSB = io.RF_DATA1.Type2.DataLSB;
							rf.reg[index].DataMSB = io.RF_DATA2.Type2.DataMSB;
							break;

						case REG_RF2958_RFPLL2:
						case REG_RF2958_RFPLL3:
							{
								u32 channelFreqN;

								rf.reg[index].DataLSB = io.RF_DATA1.Type2.DataLSB;
								rf.reg[index].DataMSB = io.RF_DATA2.Type2.DataMSB;

								// get the complete rfpll.n
								channelFreqN = (u32)rf.RFPLL3.NUM2 | ((u32)rf.RFPLL2.NUM2 << 18) | ((u32)rf.RFPLL2.N2 << 24);

								// frequency setting is out of range
								if(channelFreqN < 0x00A2E8BA)
								{
									break;
								}

								// substract base frequency (channel 1)
								channelFreqN -= 0x00A2E8BA;
								break;
							}

						case REG_RF2958_TEST:
							WIFI_LOG(2, "Writing value of %04Xh to RF test register\n", (int)rf.reg[index].Data);
							break;

						case REG_RF2958_RESET:
							WIFI_resetRF(rf);
							break;

							// Any unlisted register is assumed to be unused padding,
							// so just do nothing in this case.
						default:
							break;
					}
				}
				else // Perform a read
				{
					io.RF_DATA1.Type2.DataLSB = rf.reg[index].DataLSB;
					io.RF_DATA2.Type2.DataMSB = rf.reg[index].DataMSB;
				}

				// Normally, there should be some sort of transfer delay, but we're
				// not going to emulate the delay right now. Therefore, assume for
				// now that the transfer occurs instantaneously and clear the busy
				// flag accordingly.
				io.RF_BUSY.Busy = 0;
				break;
			}

		case REG_WIFI_RFDATA1: // 0x17E
			io.RF_DATA1.value = val;
			break;

		case REG_WIFI_RFCNT: // 0x184
			{
				if(io.RF_BUSY.Busy == 0)
				{
					IOREG_W_RF_CNT RF_CNT;
					RF_CNT.value = val;

					io.RF_CNT.TransferLen = RF_CNT.TransferLen;

					if(io.RF_CNT.UNKNOWN1 != RF_CNT.UNKNOWN1)
					{
						io.RF_CNT.UNKNOWN1 = RF_CNT.UNKNOWN1;
						WIFI_LOG(2, "Writing value of %d to RF_CNT.UNKNOWN1\n", (int)RF_CNT.UNKNOWN1);
					}

					if(io.RF_CNT.UNKNOWN2 != RF_CNT.UNKNOWN2)
					{
						io.RF_CNT.UNKNOWN2 = RF_CNT.UNKNOWN2;
						WIFI_LOG(2, "Writing value of %d to RF_CNT.UNKNOWN2\n", (int)RF_CNT.UNKNOWN2);
					}
				}
				break;
			}

		case 0x190: // 0x190
			io.INTERNAL_190 = val;
			break;

		case REG_WIFI_TX_HDR_CNT: // 0x194
			io.TX_HDR_CNT.value = val;
			break;

		case REG_WIFI_X_1A0: // 0x1A0
			io.X_1A0.value = val;
			WIFI_LOG(2, "Writing value of %04Xh to X_1A0\n", (int)io.X_1A0.value);
			break;

		case REG_WIFI_X_1A2: // 0x1A2
			io.X_1A2.value = val;
			WIFI_LOG(2, "Writing value of %04Xh to X_1A2\n", (int)io.X_1A2.value);
			break;

		case REG_WIFI_X_1A4: // 0x1A4
			io.X_1A4 = val;
			WIFI_LOG(2, "Writing value of %04Xh to X_1A4\n", (int)io.X_1A4);
			break;

		case REG_WIFI_RXSTAT_INC_IE: // 0x1AA
			io.RXSTAT_INC_IE.value = val;
			break;

		case REG_WIFI_RXSTAT_OVF_IE: // 0x1AE
			io.RXSTAT_OVF_IE.value = val;
			break;

		case REG_WIFI_RXSTAT0: // 0x1B0
			io.RXSTAT_COUNT.RXSTAT_1B0 = val;
			break;

		case REG_WIFI_RXSTAT1: // 0x1B2
			io.RXSTAT_COUNT.RXSTAT_1B2 = val;
			break;

		case REG_WIFI_RXSTAT2: // 0x1B4
			io.RXSTAT_COUNT.RXSTAT_1B4 = val;
			break;

		case REG_WIFI_RXSTAT3: // 0x1B6
			io.RXSTAT_COUNT.RXSTAT_1B6 = val;
			break;

		case REG_WIFI_RXSTAT4: // 0x1B8
			io.RXSTAT_COUNT.RXSTAT_1B8 = val;
			break;

		case REG_WIFI_RXSTAT5: // 0x1BA
			io.RXSTAT_COUNT.RXSTAT_1BA = val;
			break;

		case REG_WIFI_RXSTAT6: // 0x1BC
			io.RXSTAT_COUNT.RXSTAT_1BC = val;
			break;

		case REG_WIFI_RXSTAT7: // 0x1BE
			io.RXSTAT_COUNT.RXSTAT_1BE = val;
			break;

		case REG_WIFI_TXERR_COUNT: // 0x1C0
			io.TX_ERR_COUNT.Count++;
			break;

		case REG_WIFI_CMD_STAT0: // 0x1D0
			io.CMD_STAT_COUNT.CMD_STAT_1D0 = val;
			break;

		case REG_WIFI_CMD_STAT1: // 0x1D2
			io.CMD_STAT_COUNT.CMD_STAT_1D2 = val;
			break;

		case REG_WIFI_CMD_STAT2: // 0x1D4
			io.CMD_STAT_COUNT.CMD_STAT_1D4 = val;
			break;

		case REG_WIFI_CMD_STAT3: // 0x1D6
			io.CMD_STAT_COUNT.CMD_STAT_1D6 = val;
			break;

		case REG_WIFI_CMD_STAT4: // 0x1D8
			io.CMD_STAT_COUNT.CMD_STAT_1D8 = val;
			break;

		case REG_WIFI_CMD_STAT5: // 0x1DA
			io.CMD_STAT_COUNT.CMD_STAT_1DA = val;
			break;

		case REG_WIFI_CMD_STAT6: // 0x1DC
			io.CMD_STAT_COUNT.CMD_STAT_1DC = val;
			break;

		case REG_WIFI_CMD_STAT7: // 0x1DE
			io.CMD_STAT_COUNT.CMD_STAT_1DE = val;
			break;

		case 0x1F0: // 0x1F0
			io.INTERNAL_1F0 = val;
			break;

		case 0x204: // 0x204
			io.INTERNAL_204 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_204\n", (int)io.INTERNAL_204);
			break;

		case 0x208: // 0x208
			io.INTERNAL_208 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_208\n", (int)io.INTERNAL_208);
			break;

		case 0x20C: // 0x20C
			io.INTERNAL_20C = val;
			break;

		case REG_WIFI_IF_SET: // 0x21C
			WIFI_SetIRQ((WifiIRQ)(val & 0x000F));
			break;

		case 0x220: // 0x220
			io.INTERNAL_220 = val;
			break;

		case 0x224: // 0x224
			io.INTERNAL_224 = val;
			break;

		case REG_WIFI_X_228: // 0x228
			io.X_228 = val;
			break;

		case 0x230: // 0x230
			io.INTERNAL_230 = val;
			break;

		case 0x234: // 0x234
			io.INTERNAL_234 = val;
			break;

		case 0x238: // 0x238
			io.INTERNAL_238 = val;
			break;

		case 0x23C: // 0x23C
			io.INTERNAL_23C = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_23C\n", (int)io.INTERNAL_23C);
			break;

		case REG_WIFI_X_244: // 0x244
			io.X_244 = val;
			break;

		case 0x248: // 0x248
			io.INTERNAL_248 = val;
			break;

		case REG_WIFI_CONFIG_254: // 0x254
			io.CONFIG_254 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only CONFIG_254\n", (int)io.CONFIG_254);
			break;

		case 0x258: // 0x258
			io.INTERNAL_258 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_258\n", (int)io.INTERNAL_258);
			break;

		case 0x25C: // 0x25C
			io.INTERNAL_25C = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_25C\n", (int)io.INTERNAL_25C);
			break;

		case 0x260: // 0x260
			io.INTERNAL_260 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_260\n", (int)io.INTERNAL_260);
			break;

		case 0x274: // 0x274
			io.INTERNAL_274 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_274\n", (int)io.INTERNAL_274);
			break;

		case 0x278: // 0x278
			io.INTERNAL_278 = val;
			break;

		case 0x27C: // 0x27C
			io.INTERNAL_27C = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_27C\n", (int)io.INTERNAL_27C);
			break;

		case REG_WIFI_X_290: // 0x290
			io.X_290.value = val;
			break;

		case 0x298: // 0x298
			io.INTERNAL_298 = val;
			break;

		case 0x2A0: // 0x2A0
			io.INTERNAL_2A0 = val;
			break;

		case 0x2A8: // 0x2A8
			io.INTERNAL_2A8 = val;
			break;

		case 0x2AC: // 0x2AC
			io.INTERNAL_2AC = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_2AC\n", (int)io.INTERNAL_2AC);
			break;

		case 0x2B0: // 0x2B0
			io.INTERNAL_2B0 = val;
			break;

		case 0x2B4: // 0x2B4
			io.INTERNAL_2B4 = val;
			break;

		case 0x2B8: // 0x2B8
			io.INTERNAL_2B8 = val;
			WIFI_LOG(2, "Writing value of %04Xh to possible read-only INTERNAL_2B8\n", (int)io.INTERNAL_2B8);
			break;

		case 0x2C0: // 0x2C0
			io.INTERNAL_2C0 = val;
			break;

		case REG_WIFI_POWERACK: // 0x2D0
			{
				if(io.POWERSTATE.WillPowerOn != 0)
				{
					io.POWERACK = val;

					if(io.POWERACK == 0x0000)
					{
						io.POWERSTATE.WillPowerOn = 0;
						io.POWERSTATE.IsPowerOff = io.POWERFORCE.NewPowerOffState;

						if(io.POWERSTATE.IsPowerOff == 0)
						{
							io.POWERSTATE.RequestPowerOn = 0;
							WIFI_triggerIRQ(WifiIRQ11_RFWakeup);

							// This is supposedly the TX beacon transfer flag.
							// Since reading TXREQ_READ bit 4 should always return a 1 (unless
							// POWERFORCE clears it), we'll simply set this bit to 1 on power up
							// so that bit 4 gets its default value.
							io.TXREQ_READ.UNKNOWN1 = 1;
						}
						else
						{
							io.RF_STATUS.RFStatus = 0x9;
							io.RF_PINS.CarrierSense = 0;
							io.RF_PINS.TXMain = 1;
							io.RF_PINS.UNKNOWN1 = 1;
							io.RF_PINS.TX_On = 1;
							io.RF_PINS.RX_On = 0;

							io.INTERNAL_034 = 0x0002;
							io.TXREQ_READ.value &= 0x0010;
							io.POWERSTATE.WillPowerOn = 0;
							io.POWERSTATE.IsPowerOff = 1;
						}
					}
				}
				break;
			}

		case 0x2F0: // 0x2F0
			io.INTERNAL_2F0 = val;
			break;

		case 0x2F2: // 0x2F2
			io.INTERNAL_2F2 = val;
			break;

		case 0x2F4: // 0x2F4
			io.INTERNAL_2F4 = val;
			break;

		case 0x2F6: // 0x2F6
			io.INTERNAL_2F6 = val;
			break;

			// Report any writes to read-only registers because this shouldn't happen.
		case REG_WIFI_ID:						// 0x000
		case REG_WIFI_RANDOM:					// 0x044
		case REG_WIFI_RXHWWRITECSR:				// 0x054
		case REG_WIFI_CIRCBUFREAD:				// 0x060
		case REG_WIFI_TXBUF_REPLY2:				// 0x098
		case REG_WIFI_TXREQ_READ:				// 0x0B0
		case REG_WIFI_TXBUSY:					// 0x0B6
		case REG_WIFI_TXSTAT:					// 0x0B8
		case REG_WIFI_BBREAD:					// 0x15C
		case REG_WIFI_BBBUSY:					// 0x15E
		case REG_WIFI_RFBUSY:					// 0x180
		case REG_WIFI_RFPINS:					// 0x19C
		case REG_WIFI_RXSTAT_INC_IF:			// 0x1A8
		case REG_WIFI_RXSTAT_OVF_IF:			// 0x1AC
		case REG_WIFI_RX_COUNT:					// 0x1C4
		case REG_WIFI_TXSEQNO:					// 0x210
		case REG_WIFI_RFSTATUS:					// 0x214
		case 0x24C:								// 0x24C
		case 0x24E:								// 0x24E
		case 0x250:								// 0x250
		case 0x264:								// 0x264
		case REG_WIFI_RXTX_ADDR:				// 0x268
		case 0x270:								// 0x270
		case 0x2A2:								// 0x2A2
		case 0x2A4:								// 0x2A4
		case 0x2C4:								// 0x2C4
		case 0x2C8:								// 0x2C8
		case 0x2CC:								// 0x2CC
			WIFI_LOG(2, "Preventing writing value of %04Xh to read-only port 0x%03X\n", val, address);
			break;

			// Assume that any unlisted register is just padding. It's meaningless to
			// write to padding, so report that here.
		default:
			WIFI_LOG(2, "Preventing writing value of %04Xh to padding address 0x%03X\n", val, address);
			break;
	}
}

u16 WIFI_read16(u32 address)
{
	bool action = false;
	if(!nds.power2.wifi) return 0;

	WifiData& wifi = wifiHandler->GetWifiData();
	WIFI_IOREG_MAP& io = wifi.io;

	u32 page = address & 0x7000;

	// 0x2000 - 0x3FFF: unused
	if((page >= 0x2000) && (page < 0x4000))
		return 0xFFFF;

	WIFI_LOG(5, "Read at address %08X\n", address);

	// 0x4000 - 0x5FFF: wifi RAM
	if((page >= 0x4000) && (page < 0x6000))
	{
		return *(u16*)&wifi.RAM[address & 0x1FFE];
	}

	// anything else: I/O ports
	// only the first mirror causes a special action
	if(page == 0x0000) action = true;

	address &= 0x0FFF;
	switch(address)
	{
		case REG_WIFI_ID: // 0x000
			return WIFI_CHIPID;

		case REG_WIFI_MODE: // 0x004
			return io.MODE_RST.value;

		case REG_WIFI_WEP: // 0x006
			return io.MODE_WEP.value;

		case REG_WIFI_TXSTATCNT: // 0x008
			return io.TXSTATCNT.value;

		case REG_WIFI_X_00A: // 0x00A
			return io.X_00A;

		case REG_WIFI_IF: // 0x010
			return io.IF.value;

		case REG_WIFI_IE: // 0x012
			return io.IE.value;

		case REG_WIFI_MAC0: // 0x018
			return io.MACADDR0.value;

		case REG_WIFI_MAC1: // 0x01A
			return io.MACADDR1.value;

		case REG_WIFI_MAC2: // 0x01C
			return io.MACADDR2.value;

		case REG_WIFI_BSS0: // 0x020
			return io.BSSID0.value;

		case REG_WIFI_BSS1: // 0x022
			return io.BSSID1.value;

		case REG_WIFI_BSS2: // 0x024
			return io.BSSID2.value;

		case REG_WIFI_AID_LOW: // 0x028
			return io.AID_LOW.value;

		case REG_WIFI_AID_HIGH: // 0x02A
			return io.AID_FULL.value;

		case REG_WIFI_RETRYLIMIT: // 0x02C
			return io.TX_RETRYLIMIT.value;

		case 0x2E: // 0x02E
			return io.INTERNAL_02E;

		case REG_WIFI_RXCNT: // 0x030
			return io.RXCNT.value;

		case REG_WIFI_WEPCNT: // 0x032
			return io.WEP_CNT.value;

		case 0x034: // 0x034
			return io.INTERNAL_034;

		case REG_WIFI_POWER_US: // 0x036
			return io.POWER_US.value;

		case REG_WIFI_POWER_TX: // 0x038
			return io.POWER_TX.value;

		case REG_WIFI_POWERSTATE: // 0x03C
			if(io.POWERSTATE.IsPowerOff)
			{
				int zzz=9;
			}
			return io.POWERSTATE.value;

		case REG_WIFI_POWERFORCE: // 0x040
			return io.POWERFORCE.value;

		case REG_WIFI_RANDOM: // 0x044
			{
				u16 returnValue = io.RANDOM.Random;
				io.RANDOM.Random = (io.RANDOM.Random & 1) ^ (((io.RANDOM.Random << 1) & 0x7FE) | ((io.RANDOM.Random >> 10) & 0x1));
				return returnValue;
			}

		case REG_WIFI_POWER_UNK: // 0x048
			return io.POWER_UNKNOWN.value;

		case REG_WIFI_RXRANGEBEGIN: // 0x050
			return io.RXBUF_BEGIN;

		case REG_WIFI_RXRANGEEND: // 0x052
			return io.RXBUF_END;

		case REG_WIFI_RXHWWRITECSR: // 0x054
			return io.RXBUF_WRCSR.value;

		case REG_WIFI_WRITECSRLATCH: // 0x056
			return io.RXBUF_WR_ADDR.value;

		case REG_WIFI_CIRCBUFRADR: // 0x058
			return io.RXBUF_RD_ADDR.value;

		case REG_WIFI_RXREADCSR: // 0x05A
			return io.RXBUF_READCSR.value;

		case REG_WIFI_RXBUF_COUNT: // 0x05C
			return io.RXBUF_COUNT.value;

		case REG_WIFI_CIRCBUFREAD: // 0x060
		case REG_WIFI_CIRCBUFWRITE: // 0x070 - mirrored read of 0x060
			{
				if(action)
				{
					io.RXBUF_RD_ADDR.HalfwordAddress++;

					if(io.RXBUF_RD_ADDR.ByteAddress >= io.RXBUF_END)
					{
						io.RXBUF_RD_ADDR.ByteAddress = io.RXBUF_BEGIN;
					}
					else if(io.RXBUF_RD_ADDR.HalfwordAddress == io.RXBUF_GAP.HalfwordAddress)
					{
						io.RXBUF_RD_ADDR.HalfwordAddress += io.RXBUF_GAPDISP.HalfwordOffset;

						if(io.RXBUF_RD_ADDR.ByteAddress >= io.RXBUF_END)
						{
							io.RXBUF_RD_ADDR.ByteAddress = io.RXBUF_RD_ADDR.ByteAddress + io.RXBUF_BEGIN - io.RXBUF_END;
						}
					}

					io.RXBUF_RD_DATA = *(u16*)&wifi.RAM[io.RXBUF_RD_ADDR.HalfwordAddress << 1];

					if(io.RXBUF_COUNT.Count > 0)
					{
						io.RXBUF_COUNT.Count--;
						if(io.RXBUF_COUNT.Count == 0)
						{
							WIFI_triggerIRQ(WifiIRQ09_RXCountExpired);
						}
					}
				}

				return io.RXBUF_RD_DATA;
			}

		case REG_WIFI_CIRCBUFRD_END: // 0x062
			return io.RXBUF_GAP.value;

		case REG_WIFI_CIRCBUFRD_SKIP: // 0x064
			return io.RXBUF_GAPDISP.value;

		case REG_WIFI_CIRCBUFWADR: // 0x068
			return io.TXBUF_WR_ADDR.value;

		case REG_WIFI_TXBUFCOUNT: // 0x06C
			return io.TXBUF_COUNT.value;

		case REG_WIFI_CIRCBUFWR_END: // 0x074
			return io.TXBUF_GAP.value;

		case REG_WIFI_CIRCBUFWR_SKIP: // 0x076
			return io.TXBUF_GAPDISP.value;

		case 0x078: // 0x078 - mirrored read of 0x068
			return io.TXBUF_WR_ADDR.value;

		case REG_WIFI_TXLOCBEACON: // 0x080
			return io.TXBUF_BEACON.value;

		case REG_WIFI_TXBUF_TIM: // 0x084
			return io.TXBUF_TIM.value;

		case REG_WIFI_LISTENCOUNT: // 0x088
			return io.LISTENCOUNT.value;

		case REG_WIFI_BEACONPERIOD: // 0x08C
			return io.BEACONINT.value;

		case REG_WIFI_LISTENINT: // 0x08E
			return io.LISTENINT.value;

		case REG_WIFI_TXLOCEXTRA: // 0x090
			return io.TXBUF_CMD.value;

		case REG_WIFI_TXBUF_REPLY1: // 0x094
			return io.TXBUF_REPLY1.value;

		case REG_WIFI_TXBUF_REPLY2: // 0x098
			return io.TXBUF_REPLY2.value;

		case 0x09C: // 0x09C
			return io.INTERNAL_09C;

		case REG_WIFI_TXLOC1: // 0x0A0
			return io.TXBUF_LOC1.value;

		case REG_WIFI_TXLOC2: // 0x0A4
			return io.TXBUF_LOC2.value;

		case REG_WIFI_TXLOC3: // 0x0A8
			return io.TXBUF_LOC3.value;

		case REG_WIFI_TXREQ_RESET: // 0x0AC - mirrored read of 0x09C
		case REG_WIFI_TXREQ_SET: // 0x0AE - mirrored read of 0x09C
			return io.INTERNAL_09C;

		case REG_WIFI_TXREQ_READ: // 0x0B0
			return io.TXREQ_READ.value;

		case REG_WIFI_TXRESET: // 0x0B4 - mirrored read of 0x0B6
		case REG_WIFI_TXBUSY: // 0x0B6
			return io.TXBUSY.value;

		case REG_WIFI_TXSTAT: // 0x0B8
			return io.TXSTAT.value;

		case 0x0BA: // 0x0BA
			return io.INTERNAL_0BA;

		case REG_WIFI_PREAMBLE: // 0x0BC
			return io.PREAMBLE.value;

		case REG_WIFI_CMD_TOTALTIME: // 0x0C0
			return io.CMD_TOTALTIME;

		case REG_WIFI_CMD_REPLYTIME: // 0x0C4
			return io.CMD_REPLYTIME;

		case 0x0C8: // 0x0C8
			return io.INTERNAL_0C8;

		case REG_WIFI_RXFILTER: // 0x0D0
			return io.RXFILTER.value;

		case REG_WIFI_CONFIG_0D4: // 0x0D4
			return io.CONFIG_0D4;

		case REG_WIFI_CONFIG_0D8: // 0x0D8
			return io.CONFIG_0D8;

		case REG_WIFI_RX_LEN_CROP: // 0x0DA
			return io.RX_LEN_CROP.value;

		case REG_WIFI_RXFILTER2: // 0x0E0
			return io.RXFILTER2.value;

		case REG_WIFI_USCOUNTERCNT: // 0x0E8
			return io.US_COUNTCNT.value;

		case REG_WIFI_USCOMPARECNT: // 0x0EA
			return io.US_COMPARECNT.value;

		case REG_WIFI_CONFIG_0EC: // 0x0EC
			return io.CONFIG_0EC;

		case REG_WIFI_EXTRACOUNTCNT: // 0x0EE
			return io.CMD_COUNTCNT.value;

		case REG_WIFI_USCOMPARE0: // 0x0F0
			return io.US_COMPARE0;

		case REG_WIFI_USCOMPARE1: // 0x0F2
			return io.US_COMPARE1;

		case REG_WIFI_USCOMPARE2: // 0x0F4
			return io.US_COMPARE2;

		case REG_WIFI_USCOMPARE3: // 0x0F6
			return io.US_COMPARE3;

		case REG_WIFI_USCOUNTER0: // 0x0F8
			return io.US_COUNT0;

		case REG_WIFI_USCOUNTER1: // 0x0FA
			return io.US_COUNT1;

		case REG_WIFI_USCOUNTER2: // 0x0FC
			return io.US_COUNT2;

		case REG_WIFI_USCOUNTER3: // 0x0FE
			return io.US_COUNT3;

		case 0x100: // 0x100
			return io.INTERNAL_100;

		case 0x102: // 0x102
			return io.INTERNAL_102;

		case 0x104: // 0x104
			return io.INTERNAL_104;

		case 0x106: // 0x106
			return io.INTERNAL_106;

		case REG_WIFI_CONTENTFREE: // 0x10C
			return io.CONTENTFREE;

		case REG_WIFI_PREBEACONCOUNT: // 0x110
			return io.PRE_BEACON;

		case REG_WIFI_EXTRACOUNT: // 0x118
			return (u16)((wifi.cmdCount_u32 + 9) / 10);

		case REG_WIFI_BEACONCOUNT1: // 0x11C
			return io.BEACONCOUNT1;

		case REG_WIFI_CONFIG_120: // 0x120
			return io.CONFIG_120;

		case REG_WIFI_CONFIG_122: // 0x122
			return io.CONFIG_122;

		case REG_WIFI_CONFIG_124: // 0x124
			return io.CONFIG_124;

		case 0x126: // 0x126
			return io.INTERNAL_126;

		case REG_WIFI_CONFIG_128: // 0x128
			return io.CONFIG_128;

		case 0x12A: // 0x12A
			return io.INTERNAL_12A;

		case REG_WIFI_CONFIG_130: // 0x130
			return io.CONFIG_130;

		case REG_WIFI_CONFIG_132: // 0x132
			return io.CONFIG_132;

		case REG_WIFI_BEACONCOUNT2: // 0x134
			return io.BEACONCOUNT2;

		case REG_WIFI_CONFIG_140: // 0x140
			return io.CONFIG_140;

		case REG_WIFI_CONFIG_142: // 0x142
			return io.CONFIG_142;

		case REG_WIFI_CONFIG_144: // 0x144
			return io.CONFIG_144;

		case REG_WIFI_CONFIG_146: // 0x146
			return io.CONFIG_146;

		case REG_WIFI_CONFIG_148: // 0x148
			return io.CONFIG_148;

		case REG_WIFI_CONFIG_14A: // 0x14A
			return io.CONFIG_14A;

		case REG_WIFI_CONFIG_14C: // 0x14C
			return io.CONFIG_14C;

		case REG_WIFI_CONFIG_150: // 0x150
			return io.CONFIG_150;

		case REG_WIFI_CONFIG_154: // 0x154
			return io.CONFIG_154;

		case REG_WIFI_BBCNT: // 0x158 - mirrored read of 0x15C
			return io.BB_READ.value;

		case REG_WIFI_BBWRITE: // 0x15A
			return 0;

		case REG_WIFI_BBREAD: // 0x15C
			return io.BB_READ.value;

		case REG_WIFI_BBBUSY: // 0x15E
			return io.BB_BUSY.value;

		case REG_WIFI_BBMODE: // 0x160
			return io.BB_MODE.value;

		case REG_WIFI_BBPOWER: // 0x168
			return io.BB_POWER.value;

		case 0x16A: // 0x16A
			return io.INTERNAL_16A;

		case 0x170: // 0x170
			return io.INTERNAL_170;

		case 0x172: // 0x172
			return io.INTERNAL_172;

		case 0x174: // 0x174
			return io.INTERNAL_174;

		case 0x176: // 0x176
			return io.INTERNAL_176;

		case 0x178: // 0x178 - mirrored read of 0x17C
		case REG_WIFI_RFDATA2: // 0017C
			{
				if(io.RF_BUSY.Busy == 0)
				{
					return io.RF_DATA2.value;
				}
				else
				{
					return 0;
				}
			}

		case REG_WIFI_RFDATA1: // 0x17E
			{
				if(io.RF_BUSY.Busy == 0)
				{
					return io.RF_DATA1.value;
				}
				else
				{
					return 0;
				}
			}

		case REG_WIFI_RFBUSY: // 0x180
			return io.RF_BUSY.value;

		case REG_WIFI_RFCNT: // 0x184
			return io.RF_CNT.value;

		case 0x190: // 0x190
			return io.INTERNAL_190;

		case REG_WIFI_TX_HDR_CNT: // 0x194
			return io.TX_HDR_CNT.value;

		case 0x198: // 0x198
			return io.INTERNAL_198;

		case REG_WIFI_X_1A0: // 0x1A0
			return io.X_1A0.value;

		case REG_WIFI_X_1A2: // 0x1A2
			return io.X_1A2.value;

		case REG_WIFI_X_1A4: // 0x1A4
			return io.X_1A4;

		case REG_WIFI_RXSTAT_INC_IF: // 0x1A8
			return io.RXSTAT_INC_IF.value;

		case REG_WIFI_RXSTAT_INC_IE: // 0x1AA
			return io.RXSTAT_INC_IE.value;

		case REG_WIFI_RXSTAT_OVF_IF: // 0x1AC
			return io.RXSTAT_OVF_IF.value;

		case REG_WIFI_RXSTAT_OVF_IE: // 0x1AE
			return io.RXSTAT_OVF_IE.value;

		case REG_WIFI_RXSTAT0: // 0x1B0
			return io.RXSTAT_COUNT.RXSTAT_1B0;

		case REG_WIFI_RXSTAT1: // 0x1B2
			return io.RXSTAT_COUNT.RXSTAT_1B2;

		case REG_WIFI_RXSTAT2: // 0x1B4
			return io.RXSTAT_COUNT.RXSTAT_1B4;

		case REG_WIFI_RXSTAT3: // 0x1B6
			return io.RXSTAT_COUNT.RXSTAT_1B6;

		case REG_WIFI_RXSTAT4: // 0x1B8
			return io.RXSTAT_COUNT.RXSTAT_1B8;

		case REG_WIFI_RXSTAT5: // 0x1BA
			return io.RXSTAT_COUNT.RXSTAT_1BA;

		case REG_WIFI_RXSTAT6: // 0x1BC
			return io.RXSTAT_COUNT.RXSTAT_1BC;

		case REG_WIFI_RXSTAT7: // 0x1BE
			return io.RXSTAT_COUNT.RXSTAT_1BE;

		case REG_WIFI_TXERR_COUNT: // 0x1C0
			return io.TX_ERR_COUNT.value;

		case REG_WIFI_RX_COUNT: // 0x1C4
			return io.RX_COUNT.value;

		case REG_WIFI_CMD_STAT0: // 0x1D0
			return io.CMD_STAT_COUNT.CMD_STAT_1D0;

		case REG_WIFI_CMD_STAT1: // 0x1D2
			return io.CMD_STAT_COUNT.CMD_STAT_1D2;

		case REG_WIFI_CMD_STAT2: // 0x1D4
			return io.CMD_STAT_COUNT.CMD_STAT_1D4;

		case REG_WIFI_CMD_STAT3: // 0x1D6
			return io.CMD_STAT_COUNT.CMD_STAT_1D6;

		case REG_WIFI_CMD_STAT4: // 0x1D8
			return io.CMD_STAT_COUNT.CMD_STAT_1D8;

		case REG_WIFI_CMD_STAT5: // 0x1DA
			return io.CMD_STAT_COUNT.CMD_STAT_1DA;

		case REG_WIFI_CMD_STAT6: // 0x1DC
			return io.CMD_STAT_COUNT.CMD_STAT_1DC;

		case REG_WIFI_CMD_STAT7: // 0x1DE
			return io.CMD_STAT_COUNT.CMD_STAT_1DE;

		case 0x1F0: // 0x1F0
			return io.INTERNAL_1F0;

		case 0x204: // 0x204
			return io.INTERNAL_204;

		case 0x208: // 0x208
			return io.INTERNAL_208;

		case 0x20C: // 0x20C - mirrored read of 0x09C
			return io.INTERNAL_09C;

		case REG_WIFI_TXSEQNO: // 0x210
			return io.TX_SEQNO.value;

		case REG_WIFI_IF_SET: // 0x21C - mirrored read of 0x010
			return io.IF.value;

		case 0x220: // 0x220
			return io.INTERNAL_220;

		case 0x224: // 0x224
			return io.INTERNAL_224;

		case REG_WIFI_X_228: // 0x228
			return 0;

		case 0x230: // 0x230
			return io.INTERNAL_230;

		case 0x234: // 0x234
			return io.INTERNAL_234;

		case 0x238: // 0x238
			return io.INTERNAL_238;

		case 0x23C: // 0x23C
			return io.INTERNAL_23C;

		case REG_WIFI_X_244: // 0x244
			return io.X_244;

		case 0x248: // 0x248
			return io.INTERNAL_248;

		case 0x24C: // 0x24C
			return io.INTERNAL_24C;

		case 0x24E: // 0x24E
			return io.INTERNAL_24E;

		case 0x250: // 0x250
			return io.INTERNAL_250;

		case REG_WIFI_CONFIG_254: // 0x254
			return io.CONFIG_254;

		case 0x258: // 0x258
			return io.INTERNAL_258;

		case 0x25C: // 0x25C
			return io.INTERNAL_25C;

		case 0x260: // 0x260
			return io.INTERNAL_260;

		case 0x264: // 0x264
			return io.INTERNAL_264;

		case REG_WIFI_RXTX_ADDR: // 0x268
			return io.RXTX_ADDR.value;

		case 0x270: // 0x270
			return io.INTERNAL_270;

		case 0x274: // 0x274
			return io.INTERNAL_274;

		case 0x278: // 0x278
			return io.INTERNAL_278;

		case 0x27C: // 0x27C
			return io.INTERNAL_27C;

		case REG_WIFI_X_290: // 0x290
			return io.X_290.value;

		case 0x298: // 0x298 - mirrored read of 0x084
			return io.TXBUF_TIM.value;

		case 0x2A0: // 0x2A0
			return io.INTERNAL_2A0;

		case 0x2A2: // 0x2A2
			return io.INTERNAL_2A2;

		case 0x2A4: // 0x2A4
			return io.INTERNAL_2A4;

		case 0x2A8: // 0x2A8 - mirrored read of 0x238
			return io.INTERNAL_238;

		case 0x2AC: // 0x2AC
			return io.INTERNAL_2AC;

		case 0x2B0: // 0x2B0 - mirrored read of 0x084
			return io.TXBUF_TIM.value;

		case 0x2B4: // 0x2B4
			return io.INTERNAL_2B4;

		case 0x2B8: // 0x2B8
			return io.INTERNAL_2B8;

		case 0x2C0: // 0x2C0
			return io.INTERNAL_2C0;

		case 0x2C4: // 0x2C4
			return io.INTERNAL_2C4;

		case 0x2C8: // 0x2C8
			return io.INTERNAL_2C8;

		case 0x2CC: // 0x2CC
			return io.INTERNAL_2CC;

		case REG_WIFI_POWERACK: // 0x2D0
			{
				if(io.POWERSTATE.WillPowerOn != 0)
				{
					return io.POWERACK;
				}
				else
				{
					break;
				}
			}

		case 0x2F0: // 0x2F0
			return io.INTERNAL_2F0;

		case 0x2F2: // 0x2F2
			return io.INTERNAL_2F2;

		case 0x2F4: // 0x2F4
			return io.INTERNAL_2F4;

		case 0x2F6: // 0x2F6
			return io.INTERNAL_2F6;

			// RFSTATUS, RFPINS
			// TODO: figure out how to emulate those correctly
			// without breaking Nintendo's games
		case REG_WIFI_RFSTATUS: // 0x214
			return 0x0009;
		case REG_WIFI_RFPINS: // 0x19C
			return 0x00C6;

		default:
			//	printf("wifi: read unhandled reg %03X\n", address);
			break;
	}

	// TODO: We return the default value for the original NDS. However, the default value is different for NDS Lite.
	WIFI_LOG(2, "Reading value of %04Xh from unlabeled register 0x%03X\n", 0xFFFF, address);
	return 0xFFFF;
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

static const u8 SoftAP_MACAddr[6] = { SOFTAP_MACADDR };

static const u8 SoftAP_Beacon[] = {
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

static const u8 SoftAP_ProbeResponse[] = {
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

static const u8 SoftAP_AuthFrame[] = {
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

static const u8 SoftAP_AssocResponse[] = {
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
static const u8 SoftAP_DeauthFrame[] = {
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

static void SoftAP_RXPacketGet_Callback(u_char* userData, const pcap_pkthdr* pktHeader, const u_char* pktData)
{
	#if 0
	const WIFI_IOREG_MAP& io = wifiHandler->GetWifiData().io;

	if((userData == NULL) || (pktData == NULL) || (pktHeader == NULL))
	{
		return;
	}

	if(pktHeader->len == 0)
	{
		return; // No packet data was received.
	}

	// This is the smallest possible frame size there can be. As of this time:
	// 14 bytes - The IEEE 802.3 frame header size (EthernetFrameHeader)
	// 4 bytes - Size of the FCS at the end of every IEEE 802.3 frame. (not included because libpcap is not guaranteed to include this)
	if(pktHeader->len <= sizeof(EthernetFrameHeader))
	{
		return; // The packet is too small to be of any use.
	}

	// Check the 802.3 header and do a precheck on the relevant MAC addresses to see
	// if we can reject the packet early.
	const EthernetFrameHeader& IEEE8023Header = (EthernetFrameHeader&)pktData[0];
	if(!(WIFI_compareMAC(io.MACADDR, IEEE8023Header.destMAC) || (WIFI_isBroadcastMAC(IEEE8023Header.destMAC) && WIFI_compareMAC(io.BSSID, SoftAP_MACAddr))))
	{
		// Don't process packets that aren't for us.
		return;
	}
	else if(WIFI_compareMAC(io.MACADDR, IEEE8023Header.sendMAC))
	{
		// Don't process packets that we just sent.
		return;
	}

	RXRawPacketData* rawPacket = (RXRawPacketData*)userData;
	u8* targetPacket = &rawPacket->buffer[rawPacket->writeLocation];

	// Generate the emulator header.
	DesmumeFrameHeader& emulatorHeader = (DesmumeFrameHeader&)targetPacket[0];
	strncpy(emulatorHeader.frameID, DESMUME_EMULATOR_FRAME_ID, 8);
	emulatorHeader.version = DESMUME_EMULATOR_FRAME_CURRENT_VERSION;
	emulatorHeader.timeStamp = 0;
	emulatorHeader.emuPacketSize = (pktHeader->len + (sizeof(WifiDataFrameHeaderDS2STA) + sizeof(WifiLLCSNAPHeader) - sizeof(EthernetFrameHeader)) + 3) & 0xFFFC;

	emulatorHeader.packetAttributes.value = 0;
	emulatorHeader.packetAttributes.IsTXRate20 = 1;

	WifiHandler::ConvertDataFrame8023To80211((u8*)pktData, pktHeader->len, (u8*)userData + sizeof(DesmumeFrameHeader));

	rawPacket->writeLocation += emulatorHeader.emuPacketSize;
	rawPacket->count++;
	#endif
}

static void* Adhoc_RXPacketGetOnThread(void* arg)
{
	AdhocCommInterface* commInterface = (AdhocCommInterface*)arg;
	commInterface->RXPacketGet();

	return NULL;
}

static void* Infrastructure_RXPacketGetOnThread(void* arg)
{
	SoftAPCommInterface* commInterface = (SoftAPCommInterface*)arg;
	commInterface->RXPacketGet();

	return NULL;
}

void DummyPCapInterface::__CopyErrorString(char* errbuf)
{
	const char* errString = "libpcap is not available";
	strncpy(errbuf, errString, PCAP_ERRBUF_SIZE);
}

int DummyPCapInterface::findalldevs(void** alldevs, char* errbuf)
{
	this->__CopyErrorString(errbuf);
	return -1;
}

void DummyPCapInterface::freealldevs(void* alldevs)
{
	// Do nothing.
}

void* DummyPCapInterface::open(const char* source, int snaplen, int flags, int readtimeout, char* errbuf)
{
	this->__CopyErrorString(errbuf);
	return NULL;
}

void DummyPCapInterface::close(void* dev)
{
	// Do nothing.
}

int DummyPCapInterface::setnonblock(void* dev, int nonblock, char* errbuf)
{
	this->__CopyErrorString(errbuf);
	return -1;
}

int DummyPCapInterface::sendpacket(void* dev, const void* data, int len)
{
	return -1;
}

int DummyPCapInterface::dispatch(void* dev, int num, void* callback, void* userdata)
{
	return -1;
}

void DummyPCapInterface::breakloop(void* dev)
{
	// Do nothing.
}

#if 0

int POSIXPCapInterface::findalldevs(void** alldevs, char* errbuf)
{
	return pcap_findalldevs((pcap_if_t**)alldevs, errbuf);
}

void POSIXPCapInterface::freealldevs(void* alldevs)
{
	pcap_freealldevs((pcap_if_t*)alldevs);
}

void* POSIXPCapInterface::open(const char* source, int snaplen, int flags, int readtimeout, char* errbuf)
{
	return pcap_open_live(source, snaplen, flags, readtimeout, errbuf);
}

void POSIXPCapInterface::close(void* dev)
{
	pcap_close((pcap_t*)dev);
}

int POSIXPCapInterface::setnonblock(void* dev, int nonblock, char* errbuf)
{
	return pcap_setnonblock((pcap_t*)dev, nonblock, errbuf);
}

int POSIXPCapInterface::sendpacket(void* dev, const void* data, int len)
{
	return pcap_sendpacket((pcap_t*)dev, (u_char*)data, len);
}

int POSIXPCapInterface::dispatch(void* dev, int num, void* callback, void* userdata)
{
	if(callback == NULL)
	{
		return -1;
	}

	return pcap_dispatch((pcap_t*)dev, num, (pcap_handler)callback, (u_char*)userdata);
}

void POSIXPCapInterface::breakloop(void* dev)
{
	pcap_breakloop((pcap_t*)dev);
}

#endif

WifiCommInterface::WifiCommInterface()
{
	_rxTask = NULL;//new Task();
	_mutexRXThreadRunningFlag = slock_new();
	_isRXThreadRunning = false;
	_rawPacket = NULL;
	_wifiHandler = NULL;
}

WifiCommInterface::~WifiCommInterface()
{
	return;

	slock_lock(this->_mutexRXThreadRunningFlag);

	if(this->_isRXThreadRunning)
	{
		this->_isRXThreadRunning = false;
		slock_unlock(this->_mutexRXThreadRunningFlag);

		this->_rxTask->finish();
		delete this->_rxTask;
	}
	else
	{
		slock_unlock(this->_mutexRXThreadRunningFlag);
	}

	free(this->_rawPacket);
	this->_rawPacket = NULL;
	this->_wifiHandler = NULL;

	slock_free(this->_mutexRXThreadRunningFlag);
}

AdhocCommInterface::AdhocCommInterface()
{
	_commInterfaceID = WifiCommInterfaceID_AdHoc;

	_wifiSocket = (socket_t*)malloc(sizeof(socket_t));
	*((socket_t*)_wifiSocket) = INVALID_SOCKET;

	_sendAddr = (sockaddr_t*)malloc(sizeof(sockaddr_t));
}

AdhocCommInterface::~AdhocCommInterface()
{
	this->Stop();

	free(this->_wifiSocket);
	free(this->_sendAddr);
}

bool AdhocCommInterface::Start(WifiHandler* currentWifiHandler)
{
	return false;

	socket_t& thisSocket = *((socket_t*)this->_wifiSocket);
	int socketOptValueTrue = 1;
	int result = -1;


	// Create an UDP socket.
	thisSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if(thisSocket < 0)
	{
		thisSocket = INVALID_SOCKET;

		// Ad-hoc mode really needs a socket to work at all, so don't even bother
		// running this comm interface if we didn't get a working socket.
		WIFI_LOG(1, "Ad-hoc: Failed to create socket.\n");
		return false;
	}

	// Enable the socket to be bound to an address/port that is already in use
	// This enables us to communicate with another DeSmuME instance running on the same computer.
	result = setsockopt(thisSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&socketOptValueTrue, sizeof(int));
	if(result < 0)
	{
		closesocket(thisSocket);
		thisSocket = INVALID_SOCKET;

		WIFI_LOG(1, "Ad-hoc: Failed set socket option SO_REUSEADDR.\n");
		return false;
	}

	#ifdef SO_REUSEPORT
	// Some platforms also need to enable SO_REUSEPORT, so do so if its necessary.
	result = setsockopt(thisSocket, SOL_SOCKET, SO_REUSEPORT, (const char*)&socketOptValueTrue, sizeof(int));
	if(result < 0)
	{
		closesocket(thisSocket);
		thisSocket = INVALID_SOCKET;

		WIFI_LOG(1, "Ad-hoc: Failed set socket option SO_REUSEPORT.\n");
		return false;
	}
	#endif

	// Bind the socket to any address on port 7000.
	sockaddr_t saddr;
	saddr.sa_family = AF_INET;
	*(u32*)&saddr.sa_data[2] = htonl(INADDR_ANY);
	*(u16*)&saddr.sa_data[0] = htons(BASEPORT);

	result = bind(thisSocket, &saddr, sizeof(sockaddr_t));
	if(result < 0)
	{
		closesocket(thisSocket);
		thisSocket = INVALID_SOCKET;

		WIFI_LOG(1, "Ad-hoc: Failed to bind the socket.\n");
		return false;
	}

	// Enable broadcast mode
	// Not doing so results in failure when sendto'ing to broadcast address
	result = setsockopt(thisSocket, SOL_SOCKET, SO_BROADCAST, (const char*)&socketOptValueTrue, sizeof(int));
	if(result < 0)
	{
		closesocket(thisSocket);
		thisSocket = INVALID_SOCKET;

		WIFI_LOG(1, "Ad-hoc: Failed to enable broadcast mode.\n");
		return false;
	}

	// Prepare an address structure for sending packets.
	sockaddr_t& thisSendAddr = *((sockaddr_t*)this->_sendAddr);

	thisSendAddr.sa_family = AF_INET;
	*(u32*)&thisSendAddr.sa_data[2] = htonl(INADDR_BROADCAST);
	*(u16*)&thisSendAddr.sa_data[0] = htons(BASEPORT);

	// Start the RX packet thread.
	this->_wifiHandler = currentWifiHandler;
	this->_rawPacket = (RXRawPacketData*)calloc(1, sizeof(RXRawPacketData));

	#ifdef DESMUME_COCOA
	this->_rxTask->start(false, 43);
	#else
	this->_rxTask->start(false, 0, "wifi ad-hoc");
	#endif
	this->_isRXThreadRunning = true;
	this->_rxTask->execute(&Adhoc_RXPacketGetOnThread, this);

	WIFI_LOG(1, "Ad-hoc: Initialization successful.\n");
	return true;
}

void AdhocCommInterface::Stop()
{
	return;

	socket_t& thisSocket = *((socket_t*)this->_wifiSocket);

	if(thisSocket >= 0)
	{
		slock_lock(this->_mutexRXThreadRunningFlag);

		if(this->_isRXThreadRunning)
		{
			this->_isRXThreadRunning = false;
			slock_unlock(this->_mutexRXThreadRunningFlag);

			this->_rxTask->finish();
			this->_rxTask->shutdown();
		}
		else
		{
			slock_unlock(this->_mutexRXThreadRunningFlag);
		}

		closesocket(thisSocket);
		thisSocket = INVALID_SOCKET;
	}

	free(this->_rawPacket);
	this->_rawPacket = NULL;
	this->_wifiHandler = NULL;
}

size_t AdhocCommInterface::TXPacketSend(u8* txTargetBuffer, size_t txLength)
{
	return 0;

	size_t txPacketSize = 0;
	socket_t& thisSocket = *((socket_t*)this->_wifiSocket);
	sockaddr_t& thisSendAddr = *((sockaddr_t*)this->_sendAddr);

	if((thisSocket < 0) || (txTargetBuffer == NULL) || (txLength == 0))
	{
		return txPacketSize;
	}

	txPacketSize = sendto(thisSocket, (const char*)txTargetBuffer, txLength, 0, &thisSendAddr, sizeof(sockaddr_t));
	WIFI_LOG(4, "Ad-hoc: sent %i/%i bytes of packet, frame control: %04X\n", (int)txPacketSize, (int)txLength, *(u16*)(txTargetBuffer + sizeof(DesmumeFrameHeader)));

	return txPacketSize;
}

int AdhocCommInterface::_RXPacketGetFromSocket(RXRawPacketData& rawPacket)
{
	socket_t& thisSocket = *((socket_t*)this->_wifiSocket);
	int rxPacketSizeInt = 0;

	fd_set fd;
	struct timeval tv;

	FD_ZERO(&fd);
	FD_SET(thisSocket, &fd);
	tv.tv_sec = 0;
	tv.tv_usec = 250000;

	if(select(thisSocket + 1, &fd, NULL, NULL, &tv))
	{
		sockaddr_t fromAddr;
		socklen_t fromLen = sizeof(sockaddr_t);

		u8* targetPacket = &rawPacket.buffer[rawPacket.writeLocation];

		rxPacketSizeInt = recvfrom(thisSocket, (char*)targetPacket, WIFI_WORKING_PACKET_BUFFER_SIZE, 0, &fromAddr, &fromLen);
		if(rxPacketSizeInt <= 0)
		{
			return rxPacketSizeInt; // No packet data was received.
		}

		// This is the smallest possible frame size there can be. As of this time:
		// 16 bytes - The smallest emulator frame header size (DesmumeFrameHeader)
		// 10 bytes - The smallest possible IEEE 802.11 frame header size (WifiCtlFrameHeaderCTS/WifiCtlFrameHeaderACK)
		// 4 bytes - Size of the FCS at the end of every IEEE 802.11 frame
		if(rxPacketSizeInt <= (16 + 10 + 4))
		{
			rxPacketSizeInt = 0;
			return rxPacketSizeInt; // The packet is too small to be of any use.
		}

		// Set the packet size to a non-zero value.
		DesmumeFrameHeader& emulatorHeader = (DesmumeFrameHeader&)targetPacket[0];
		rawPacket.writeLocation += emulatorHeader.emuPacketSize;
		rawPacket.count++;
	}

	return rxPacketSizeInt;
}

void AdhocCommInterface::RXPacketGet()
{
	return;
	
	socket_t& thisSocket = *((socket_t*)this->_wifiSocket);

	if((thisSocket < 0) || (this->_rawPacket == NULL) || (this->_wifiHandler == NULL))
	{
		return;
	}

	slock_lock(this->_mutexRXThreadRunningFlag);

	while(this->_isRXThreadRunning)
	{
		slock_unlock(this->_mutexRXThreadRunningFlag);

		this->_rawPacket->writeLocation = 0;
		this->_rawPacket->count = 0;

		int result = this->_RXPacketGetFromSocket(*this->_rawPacket);
		if(result <= 0)
		{
			this->_rawPacket->count = 0;
		}
		else
		{
			this->_wifiHandler->RXPacketRawToQueue<false>(*this->_rawPacket);
		}

		slock_lock(this->_mutexRXThreadRunningFlag);
	}

	slock_unlock(this->_mutexRXThreadRunningFlag);
}

SoftAPCommInterface::SoftAPCommInterface()
{
	_commInterfaceID = WifiCommInterfaceID_Infrastructure;
	_pcap = &dummyPCapInterface;
	_bridgeDeviceIndex = 0;
	_bridgeDevice = NULL;
}

SoftAPCommInterface::~SoftAPCommInterface()
{
	this->Stop();
}

void* SoftAPCommInterface::_GetBridgeDeviceAtIndex(int deviceIndex, char* outErrorBuf)
{
	return NULL;
	/*
	void* deviceList = NULL;
	void* theDevice = NULL;
	int result = this->_pcap->findalldevs((void**)&deviceList, outErrorBuf);

	if((result == -1) || (deviceList == NULL))
	{
		WIFI_LOG(1, "SoftAP: Failed to find any network adapter: %s\n", outErrorBuf);
		return theDevice;
	}

	pcap_if_t* currentDevice = (pcap_if_t*)deviceList;

	for(int i = 0; i < deviceIndex; i++)
	{
		currentDevice = currentDevice->next;
	}

	theDevice = this->_pcap->open(currentDevice->name, PACKET_SIZE, PCAP_OPENFLAG_PROMISCUOUS, 1, outErrorBuf);
	if(theDevice == NULL)
	{
		WIFI_LOG(1, "SoftAP: Failed to open device %s: %s\n", currentDevice->PCAP_DEVICE_NAME, outErrorBuf);
	}
	else
	{
		WIFI_LOG(1, "SoftAP: Device %s successfully opened.\n", currentDevice->PCAP_DEVICE_NAME);
	}

	this->_pcap->freealldevs(deviceList);

	return theDevice;*/
}

bool SoftAPCommInterface::_IsDNSRequestToWFC(u16 ethertype, const u8* body)
{
	const IPv4Header& ipv4Header = (IPv4Header&)body[0];

	// Check the various headers...
	if(ntohs(ethertype) != 0x0800) return false;		// EtherType: IP
	if(ipv4Header.version != 4) return false;			// Version: 4
	if(ipv4Header.headerLen != 5) return false;		// Header Length: 5
	if(ipv4Header.protocol != 0x11) return false;		// Protocol: UDP
	if(ntohs(*(u16*)&body[22]) != 53) return false;	// Dest. port: 53 (DNS)
	if(htons(ntohs(*(u16*)&body[28 + 2])) & 0x8000) return false;	// must be a query

	// Analyze each question
	u16 numquestions = ntohs(*(u16*)&body[28 + 4]);
	u32 curoffset = 28 + 12;
	for(u16 curquestion = 0; curquestion < numquestions; curquestion++)
	{
		// Assemble the requested domain name
		u8 bitlength = 0; char domainname[256] = "";
		while((bitlength = body[curoffset++]) != 0)
		{
			strncat(domainname, (const char*)&body[curoffset], bitlength);

			curoffset += bitlength;
			if(body[curoffset] != 0)
			{
				strcat(domainname, ".");
			}
		}

		// if the domain name contains nintendowifi.net
		// it is most likely a WFC server
		// (note, conntest.nintendowifi.net just contains a dummy HTML page and
		// is used for connection tests, I think we can let this one slide)
		if((strstr(domainname, "nintendowifi.net") != NULL) &&
			(strcmp(domainname, "conntest.nintendowifi.net") != 0))
		{
			return true;
		}

		// Skip the type and class - we don't care about that
		curoffset += 4;
	}

	return false;
}

void SoftAPCommInterface::SetPCapInterface(ClientPCapInterface* pcapInterface)
{
	this->_pcap = (pcapInterface == NULL) ? &dummyPCapInterface : pcapInterface;
}

ClientPCapInterface* SoftAPCommInterface::GetPCapInterface()
{
	return this->_pcap;
}

int SoftAPCommInterface::GetBridgeDeviceIndex()
{
	return this->_bridgeDeviceIndex;
}

void SoftAPCommInterface::SetBridgeDeviceIndex(int deviceIndex)
{
	this->_bridgeDeviceIndex = deviceIndex;
}

bool SoftAPCommInterface::Start(WifiHandler* currentWifiHandler)
{
	return false;

	const bool isPCapSupported = (this->_pcap != &dummyPCapInterface);
	char errbuf[PCAP_ERRBUF_SIZE];

	if(isPCapSupported)
	{
		this->_bridgeDevice = this->_GetBridgeDeviceAtIndex(this->_bridgeDeviceIndex, errbuf);
	}
	else
	{
		this->_bridgeDevice = NULL;
		WIFI_LOG(1, "SoftAP: No libpcap interface has been set.\n");
	}

	const bool hasBridgeDevice = (this->_bridgeDevice != NULL);
	if(hasBridgeDevice)
	{
		// Start the RX packet thread.
		this->_wifiHandler = currentWifiHandler;
		this->_rawPacket = (RXRawPacketData*)calloc(1, sizeof(RXRawPacketData));

		#ifdef DESMUME_COCOA
		this->_rxTask->start(false, 43);
		#else
		this->_rxTask->start(false, 0, "wifi ap");
		#endif
		this->_isRXThreadRunning = true;
		this->_rxTask->execute(&Infrastructure_RXPacketGetOnThread, this);
	}

	return hasBridgeDevice;
}

void SoftAPCommInterface::Stop()
{
	return;

	if(this->_bridgeDevice != NULL)
	{
		slock_lock(this->_mutexRXThreadRunningFlag);

		if(this->_isRXThreadRunning)
		{
			this->_isRXThreadRunning = false;
			slock_unlock(this->_mutexRXThreadRunningFlag);

			this->_pcap->breakloop(this->_bridgeDevice);
			this->_rxTask->finish();
			this->_rxTask->shutdown();
		}
		else
		{
			slock_unlock(this->_mutexRXThreadRunningFlag);
		}

		this->_pcap->close(this->_bridgeDevice);
		this->_bridgeDevice = NULL;
	}

	free(this->_rawPacket);
	this->_rawPacket = NULL;
	this->_wifiHandler = NULL;
}

size_t SoftAPCommInterface::TXPacketSend(u8* txTargetBuffer, size_t txLength)
{
	return 0;

	size_t txPacketSize = 0;

	if((this->_bridgeDevice == NULL) || (txTargetBuffer == NULL) || (txLength == 0))
	{
		return txPacketSize;
	}

	int result = this->_pcap->sendpacket(this->_bridgeDevice, txTargetBuffer, txLength);
	if(result == 0)
	{
		txPacketSize = txLength;
	}

	return txPacketSize;
}

void SoftAPCommInterface::RXPacketGet()
{
	return;

	if((this->_bridgeDevice == NULL) || (this->_rawPacket == NULL) || (this->_wifiHandler == NULL))
	{
		return;
	}

	slock_lock(this->_mutexRXThreadRunningFlag);

	while(this->_isRXThreadRunning)
	{
		slock_unlock(this->_mutexRXThreadRunningFlag);

		this->_rawPacket->writeLocation = 0;
		this->_rawPacket->count = 0;

		int result = this->_pcap->dispatch(this->_bridgeDevice, 8, (void*)&SoftAP_RXPacketGet_Callback, (u_char*)this->_rawPacket);
		if(result <= 0)
		{
			this->_rawPacket->count = 0;
		}
		else
		{
			this->_wifiHandler->RXPacketRawToQueue<true>(*this->_rawPacket);
		}

		slock_lock(this->_mutexRXThreadRunningFlag);
	}

	slock_unlock(this->_mutexRXThreadRunningFlag);
}

WifiHandler::WifiHandler()
{
	_selectedEmulationLevel = WifiEmulationLevel_Off;
	_currentEmulationLevel = _selectedEmulationLevel;

	_adhocCommInterface = new AdhocCommInterface;
	_softAPCommInterface = new SoftAPCommInterface;

	_selectedBridgeDeviceIndex = 0;

	_workingTXBuffer = NULL;

	_mutexRXPacketQueue = slock_new();
	_rxPacketQueue.clear();
	_rxCurrentQueuedPacketPosition = 0;
	memset(&_rxCurrentPacket, 0, sizeof(RXQueuedPacket));

	_softAPStatus = APStatus_Disconnected;
	_softAPSequenceNumber = 0;

	_packetCaptureFile = NULL;

	#if 0
	_pcap = new POSIXPCapInterface;
	_isSocketsSupported = true;
	#else
	_pcap = &dummyPCapInterface;
	_isSocketsSupported = false;
	#endif

	WIFI_initCRC32Table();
	Reset();
}

WifiHandler::~WifiHandler()
{
	free(this->_workingTXBuffer);
	this->_workingTXBuffer = NULL;

	delete this->_adhocCommInterface;
	delete this->_softAPCommInterface;

	slock_free(this->_mutexRXPacketQueue);
}

void WifiHandler::_RXEmptyQueue()
{
	slock_lock(this->_mutexRXPacketQueue);
	this->_rxPacketQueue.clear();
	slock_unlock(this->_mutexRXPacketQueue);

	this->_rxCurrentQueuedPacketPosition = 0;
}

void WifiHandler::_RXWriteOneHalfword(u16 val)
{
	WifiData& wifi = this->_wifi;
	WIFI_IOREG_MAP& io = wifi.io;

	*(u16*)&wifi.RAM[io.RXBUF_WRCSR.HalfwordAddress << 1] = val;
	io.RXBUF_WRCSR.HalfwordAddress++;

	// wrap around
	if(io.RXBUF_WRCSR.HalfwordAddress >= ((io.RXBUF_END & 0x1FFE) >> 1))
	{
		io.RXBUF_WRCSR.HalfwordAddress = ((io.RXBUF_BEGIN & 0x1FFE) >> 1);
	}

	io.RXTX_ADDR.HalfwordAddress = io.RXBUF_WRCSR.HalfwordAddress;
}

const u8* WifiHandler::_RXPacketFilter(const u8* rxBuffer, const size_t rxBytes, RXPacketHeader& outRXHeader)
{
	WifiData& wifi = this->_wifi;
	WIFI_IOREG_MAP& io = wifi.io;

	const u8* packetIEEE80211HeaderPtr = NULL;
	if(rxBuffer == NULL)
	{
		return packetIEEE80211HeaderPtr;
	}

	memset(&outRXHeader, 0, sizeof(RXPacketHeader));

	const DesmumeFrameHeader* desmumeFrameHeader = (DesmumeFrameHeader*)rxBuffer;
	size_t rxPacketSize = 0;
	bool isTXRate20 = true;

	if(strncmp(desmumeFrameHeader->frameID, DESMUME_EMULATOR_FRAME_ID, sizeof(desmumeFrameHeader->frameID)) == 0)
	{
		switch(desmumeFrameHeader->version)
		{
			case 0x10:
				{
					packetIEEE80211HeaderPtr = rxBuffer + sizeof(DesmumeFrameHeader);
					isTXRate20 = (desmumeFrameHeader->packetAttributes.IsTXRate20 != 0);

					if(desmumeFrameHeader->emuPacketSize > (rxBytes - sizeof(DesmumeFrameHeader)))
					{
						rxPacketSize = rxBytes - sizeof(DesmumeFrameHeader);
					}
					else
					{
						rxPacketSize = desmumeFrameHeader->emuPacketSize;
					}
					break;
				}

			default:
				// No valid version found -- do nothing.
				return packetIEEE80211HeaderPtr;
		}
	}
	else
	{
		return packetIEEE80211HeaderPtr; // No valid frame ID string found.
	}

	const WifiFrameControl& fc = (WifiFrameControl&)packetIEEE80211HeaderPtr[0];

	size_t destMACOffset = 0xFFFFFFFF;
	size_t sendMACOffset = 0xFFFFFFFF;
	size_t BSSIDOffset = 0xFFFFFFFF;
	bool willAcceptPacket = false;

	switch((WifiFrameType)fc.Type)
	{
		case WifiFrameType_Management:
			{
				destMACOffset = offsetof(WifiMgmtFrameHeader, destMAC[0]);
				sendMACOffset = offsetof(WifiMgmtFrameHeader, sendMAC[0]);
				BSSIDOffset = offsetof(WifiMgmtFrameHeader, BSSID[0]);

				switch((WifiFrameManagementSubtype)fc.Subtype)
				{
					case WifiFrameManagementSubtype_Beacon:
						willAcceptPacket = true;
						break;

					default:
						willAcceptPacket = WIFI_compareMAC(io.MACADDR, &packetIEEE80211HeaderPtr[destMACOffset]) || (WIFI_isBroadcastMAC(&packetIEEE80211HeaderPtr[destMACOffset]) && WIFI_compareMAC(io.BSSID, &packetIEEE80211HeaderPtr[BSSIDOffset]));
						break;
				}
				break;
			}

		case WifiFrameType_Control:
			{
				switch((WifiFrameControlSubtype)fc.Subtype)
				{
					case WifiFrameControlSubtype_PSPoll:
						BSSIDOffset = offsetof(WifiCtlFrameHeaderPSPoll, BSSID[0]);
						sendMACOffset = offsetof(WifiCtlFrameHeaderPSPoll, txMAC[0]);
						willAcceptPacket = WIFI_compareMAC(io.BSSID, &packetIEEE80211HeaderPtr[BSSIDOffset]);
						break;

					case WifiFrameControlSubtype_RTS:
						destMACOffset = offsetof(WifiCtlFrameHeaderRTS, rxMAC[0]);
						sendMACOffset = offsetof(WifiCtlFrameHeaderRTS, txMAC[0]);
						willAcceptPacket = WIFI_compareMAC(io.MACADDR, &packetIEEE80211HeaderPtr[destMACOffset]);
						break;

					case WifiFrameControlSubtype_CTS:
						destMACOffset = offsetof(WifiCtlFrameHeaderCTS, rxMAC[0]);
						willAcceptPacket = WIFI_compareMAC(io.MACADDR, &packetIEEE80211HeaderPtr[destMACOffset]);
						break;

					case WifiFrameControlSubtype_ACK:
						destMACOffset = offsetof(WifiCtlFrameHeaderACK, rxMAC[0]);
						willAcceptPacket = WIFI_compareMAC(io.MACADDR, &packetIEEE80211HeaderPtr[destMACOffset]);
						break;

					case WifiFrameControlSubtype_End:
						destMACOffset = offsetof(WifiCtlFrameHeaderEnd, rxMAC[0]);
						BSSIDOffset = offsetof(WifiCtlFrameHeaderEnd, BSSID[0]);
						willAcceptPacket = WIFI_compareMAC(io.MACADDR, &packetIEEE80211HeaderPtr[destMACOffset]) || (WIFI_isBroadcastMAC(&packetIEEE80211HeaderPtr[destMACOffset]) && WIFI_compareMAC(io.BSSID, &packetIEEE80211HeaderPtr[BSSIDOffset]));
						break;

					case WifiFrameControlSubtype_EndAck:
						destMACOffset = offsetof(WifiCtlFrameHeaderEndAck, rxMAC[0]);
						BSSIDOffset = offsetof(WifiCtlFrameHeaderEndAck, BSSID[0]);
						willAcceptPacket = WIFI_compareMAC(io.MACADDR, &packetIEEE80211HeaderPtr[destMACOffset]) || (WIFI_isBroadcastMAC(&packetIEEE80211HeaderPtr[destMACOffset]) && WIFI_compareMAC(io.BSSID, &packetIEEE80211HeaderPtr[BSSIDOffset]));
						break;

					default:
						break;
				}
				break;
			}

		case WifiFrameType_Data:
			{
				switch((WifiFCFromToState)fc.FromToState)
				{
					case WifiFCFromToState_STA2STA:
						{
							destMACOffset = offsetof(WifiDataFrameHeaderSTA2STA, destMAC[0]);
							sendMACOffset = offsetof(WifiDataFrameHeaderSTA2STA, sendMAC[0]);
							BSSIDOffset = offsetof(WifiDataFrameHeaderSTA2STA, BSSID[0]);
							willAcceptPacket = WIFI_compareMAC(io.MACADDR, &packetIEEE80211HeaderPtr[destMACOffset]) || (WIFI_isBroadcastMAC(&packetIEEE80211HeaderPtr[destMACOffset]) && WIFI_compareMAC(io.BSSID, &packetIEEE80211HeaderPtr[BSSIDOffset]));
							break;
						}

					case WifiFCFromToState_STA2DS:
						WIFI_LOG(1, "Rejecting data packet with frame control STA-to-DS.\n");
						packetIEEE80211HeaderPtr = NULL;
						return packetIEEE80211HeaderPtr;

					case WifiFCFromToState_DS2STA:
						destMACOffset = offsetof(WifiDataFrameHeaderDS2STA, destMAC[0]);
						sendMACOffset = offsetof(WifiDataFrameHeaderDS2STA, sendMAC[0]);
						BSSIDOffset = offsetof(WifiDataFrameHeaderDS2STA, BSSID[0]);
						willAcceptPacket = WIFI_compareMAC(io.MACADDR, &packetIEEE80211HeaderPtr[destMACOffset]) || (WIFI_isBroadcastMAC(&packetIEEE80211HeaderPtr[destMACOffset]) && WIFI_compareMAC(io.BSSID, &packetIEEE80211HeaderPtr[BSSIDOffset]));
						break;

					case WifiFCFromToState_DS2DS:
						WIFI_LOG(1, "Rejecting data packet with frame control DS-to-DS.\n");
						packetIEEE80211HeaderPtr = NULL;
						return packetIEEE80211HeaderPtr;
				}
				break;
			}
	}

	// Don't process packets that aren't for us.
	if(!willAcceptPacket)
	{
		packetIEEE80211HeaderPtr = NULL;
		return packetIEEE80211HeaderPtr;
	}

	// Don't process packets that we just sent.
	const bool isReceiverSameAsSender = (sendMACOffset != 0xFFFFFFFF) && WIFI_compareMAC(io.MACADDR, &packetIEEE80211HeaderPtr[sendMACOffset]);
	if(isReceiverSameAsSender)
	{
		packetIEEE80211HeaderPtr = NULL;
		return packetIEEE80211HeaderPtr;
	}

	// Save ethernet packet into the PCAP file.
	// Filter broadcast because of privacy. They aren't needed to study the protocol with the nintendo server
	// and can include PC Discovery protocols
	#if WIFI_SAVE_PCAP_TO_FILE
	if(!WIFI_isBroadcastMAC(&packetIEEE80211HeaderPtr[destMACOffset]))
	{
		this->_PacketCaptureFileWrite(packetIEEE80211HeaderPtr, rxPacketSize, true, wifi.usecCounter);
	}
	#endif

	// Generate the RX header and return the IEEE 802.11 header.
	outRXHeader = WIFI_GenerateRXHeader(packetIEEE80211HeaderPtr, 1, isTXRate20, rxPacketSize);
	return packetIEEE80211HeaderPtr;
}

// Create and open a PCAP file to store different Ethernet packets
// of the current connection.
void WifiHandler::_PacketCaptureFileOpen()
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
	this->_packetCaptureFile = fopen(file_name, "wb");
	if(this->_packetCaptureFile == NULL)
	{
		printf("Can't create capture log file: %s\n", file_name);
	}
	else
	{
		// Create global header of LCAP packet
		// More info here: http://www.kroosec.com/2012/10/a-look-at-pcap-file-format.html
		const u32 magic_header = 0xa1b2c3d4;
		const u16 major_version = 0x02;
		const u16 minor_version = 0x04;
		const u32 gmt_time = 0x00000000; // usually not used
		const u32 pre_time = 0x00000000; // usually not used
		const u32 snapshot_len = 0x0000ffff; // Maximum length of each packet
		const u32 ll_header_typ = 0x00000001; // For Ethernet

		fwrite(&magic_header, sizeof(char), 4, this->_packetCaptureFile);
		fwrite(&major_version, sizeof(char), 2, this->_packetCaptureFile);
		fwrite(&minor_version, sizeof(char), 2, this->_packetCaptureFile);
		fwrite(&gmt_time, sizeof(char), 4, this->_packetCaptureFile);
		fwrite(&pre_time, sizeof(char), 4, this->_packetCaptureFile);
		fwrite(&snapshot_len, sizeof(char), 4, this->_packetCaptureFile);
		fwrite(&ll_header_typ, sizeof(char), 4, this->_packetCaptureFile);

		fflush(this->_packetCaptureFile);
	}
}

void WifiHandler::_PacketCaptureFileClose()
{
	if(this->_packetCaptureFile != NULL)
	{
		fclose(this->_packetCaptureFile);
		this->_packetCaptureFile = NULL;
	}
}

// Save an Ethernet packet into the PCAP file of the current connection.
void WifiHandler::_PacketCaptureFileWrite(const u8* packet, u32 len, bool isReceived, u64 timeStamp)
{
	if(this->_packetCaptureFile == NULL)
	{
		printf("Can't save packet... %d\n", isReceived);
		return;
	}

	const u32 seconds = timeStamp / 1000000;
	const u32 millis = timeStamp % 1000000;

	// Add the packet
	// more info: http://www.kroosec.com/2012/10/a-look-at-pcap-file-format.html
	printf("WIFI: Saving packet of %04x bytes | %d\n", len, isReceived);

	// First create the header
	fwrite(&seconds, sizeof(char), 4, this->_packetCaptureFile); // This should be seconds since Unix Epoch, but it works :D
	fwrite(&millis, sizeof(char), 4, this->_packetCaptureFile);
	fwrite(&len, sizeof(char), 4, this->_packetCaptureFile);
	fwrite(&len, sizeof(char), 4, this->_packetCaptureFile);

	// Then write the packet
	fwrite(packet, sizeof(char), len, this->_packetCaptureFile);

	// Flush the file
	fflush(this->_packetCaptureFile);
}

RXQueuedPacket WifiHandler::_GenerateSoftAPDeauthenticationFrame(u16 sequenceNumber)
{
	RXQueuedPacket newRXPacket;

	u8* IEEE80211FrameHeaderPtr = newRXPacket.rxData;
	WifiMgmtFrameHeader& mgmtFrameHeader = (WifiMgmtFrameHeader&)IEEE80211FrameHeaderPtr[0];

	memcpy(IEEE80211FrameHeaderPtr, SoftAP_DeauthFrame, sizeof(SoftAP_DeauthFrame));

	mgmtFrameHeader.destMAC[0] = FW_Mac[0];
	mgmtFrameHeader.destMAC[1] = FW_Mac[1];
	mgmtFrameHeader.destMAC[2] = FW_Mac[2];
	mgmtFrameHeader.destMAC[3] = FW_Mac[3];
	mgmtFrameHeader.destMAC[4] = FW_Mac[4];
	mgmtFrameHeader.destMAC[5] = FW_Mac[5];

	mgmtFrameHeader.seqCtl.SequenceNumber = sequenceNumber;

	newRXPacket.rxHeader = WIFI_GenerateRXHeader(newRXPacket.rxData, 1, true, sizeof(SoftAP_DeauthFrame));

	return newRXPacket;
}

RXQueuedPacket WifiHandler::_GenerateSoftAPBeaconFrame(u16 sequenceNumber, u64 timeStamp)
{
	RXQueuedPacket newRXPacket;

	u8* IEEE80211FrameHeaderPtr = newRXPacket.rxData;
	u8* mgmtFrameBody = IEEE80211FrameHeaderPtr + sizeof(WifiMgmtFrameHeader);
	WifiMgmtFrameHeader& mgmtFrameHeader = (WifiMgmtFrameHeader&)IEEE80211FrameHeaderPtr[0];

	memcpy(IEEE80211FrameHeaderPtr, SoftAP_Beacon, sizeof(SoftAP_Beacon));
	mgmtFrameHeader.seqCtl.SequenceNumber = sequenceNumber;

	*(u64*)mgmtFrameBody = timeStamp;

	newRXPacket.rxHeader = WIFI_GenerateRXHeader(IEEE80211FrameHeaderPtr, 1, true, sizeof(SoftAP_Beacon));

	return newRXPacket;
}

RXQueuedPacket WifiHandler::_GenerateSoftAPMgmtResponseFrame(WifiFrameManagementSubtype mgmtFrameSubtype, u16 sequenceNumber, u64 timeStamp)
{
	RXQueuedPacket newRXPacket;

	size_t packetLen = 0;
	u8* IEEE80211FrameHeaderPtr = newRXPacket.rxData;
	u8* mgmtFrameBody = IEEE80211FrameHeaderPtr + sizeof(WifiMgmtFrameHeader);
	WifiMgmtFrameHeader& mgmtFrameHeader = (WifiMgmtFrameHeader&)IEEE80211FrameHeaderPtr[0];

	switch(mgmtFrameSubtype)
	{
		case WifiFrameManagementSubtype_ProbeRequest:
			{
				packetLen = sizeof(SoftAP_ProbeResponse);
				memcpy(IEEE80211FrameHeaderPtr, SoftAP_ProbeResponse, packetLen);

				*(u64*)mgmtFrameBody = timeStamp;
				break;
			}

		case WifiFrameManagementSubtype_Authentication:
			{
				packetLen = sizeof(SoftAP_AuthFrame);
				memcpy(IEEE80211FrameHeaderPtr, SoftAP_AuthFrame, packetLen);
				this->_softAPStatus = APStatus_Authenticated;
				break;
			}

		case WifiFrameManagementSubtype_AssociationRequest:
			{
				if(this->_softAPStatus != APStatus_Authenticated)
				{
					memset(&newRXPacket.rxHeader, 0, sizeof(RXPacketHeader));
					return newRXPacket;
				}

				packetLen = sizeof(SoftAP_AssocResponse);
				memcpy(IEEE80211FrameHeaderPtr, SoftAP_AssocResponse, packetLen);

				this->_softAPStatus = APStatus_Associated;
				WIFI_LOG(1, "SoftAP connected!\n");
				#if WIFI_SAVE_PCAP_TO_FILE
				this->_PacketCaptureFileOpen();
				#endif
				break;
			}

		case WifiFrameManagementSubtype_Disassociation:
			{
				this->_softAPStatus = APStatus_Authenticated;

				const u16 reasonCode = *(u16*)mgmtFrameBody;
				if(reasonCode != 0)
				{
					WIFI_LOG(1, "SoftAP disassocation error. ReasonCode=%d\n", (int)reasonCode);
				}
				break;
			}

		case WifiFrameManagementSubtype_Deauthentication:
			{
				const u16 reasonCode = *(u16*)mgmtFrameBody;

				this->_softAPStatus = APStatus_Disconnected;
				WIFI_LOG(1, "SoftAP disconnected. ReasonCode=%d\n", (int)reasonCode);
				this->_PacketCaptureFileClose();
				break;
			}

		default:
			WIFI_LOG(2, "SoftAP: unknown management frame type %04X\n", mgmtFrameSubtype);
			break;
	}

	mgmtFrameHeader.destMAC[0] = FW_Mac[0];
	mgmtFrameHeader.destMAC[1] = FW_Mac[1];
	mgmtFrameHeader.destMAC[2] = FW_Mac[2];
	mgmtFrameHeader.destMAC[3] = FW_Mac[3];
	mgmtFrameHeader.destMAC[4] = FW_Mac[4];
	mgmtFrameHeader.destMAC[5] = FW_Mac[5];

	mgmtFrameHeader.seqCtl.SequenceNumber = sequenceNumber;

	newRXPacket.rxHeader = WIFI_GenerateRXHeader(IEEE80211FrameHeaderPtr, 1, true, packetLen);

	return newRXPacket;
}

RXQueuedPacket WifiHandler::_GenerateSoftAPCtlACKFrame(const WifiDataFrameHeaderSTA2DS& inIEEE80211FrameHeader, const size_t sendPacketLength)
{
	RXQueuedPacket newRXPacket;

	u8* outIEEE80211FrameHeaderPtr = newRXPacket.rxData;
	WifiCtlFrameHeaderACK& outCtlFrameHeader = (WifiCtlFrameHeaderACK&)outIEEE80211FrameHeaderPtr[0];
	u32& fcs = (u32&)outIEEE80211FrameHeaderPtr[sizeof(WifiCtlFrameHeaderACK)];

	outCtlFrameHeader.fc.value = 0;
	outCtlFrameHeader.fc.Type = WifiFrameType_Control;
	outCtlFrameHeader.fc.Subtype = WifiFrameControlSubtype_ACK;
	outCtlFrameHeader.fc.ToDS = 0;
	outCtlFrameHeader.fc.FromDS = 0;

	outCtlFrameHeader.duration = (inIEEE80211FrameHeader.fc.MoreFragments == 0) ? 0 : sendPacketLength * 4;

	outCtlFrameHeader.rxMAC[0] = inIEEE80211FrameHeader.sendMAC[0];
	outCtlFrameHeader.rxMAC[1] = inIEEE80211FrameHeader.sendMAC[1];
	outCtlFrameHeader.rxMAC[2] = inIEEE80211FrameHeader.sendMAC[2];
	outCtlFrameHeader.rxMAC[3] = inIEEE80211FrameHeader.sendMAC[3];
	outCtlFrameHeader.rxMAC[4] = inIEEE80211FrameHeader.sendMAC[4];
	outCtlFrameHeader.rxMAC[5] = inIEEE80211FrameHeader.sendMAC[5];

	fcs = WIFI_calcCRC32(outIEEE80211FrameHeaderPtr, sizeof(WifiCtlFrameHeaderACK));

	newRXPacket.rxHeader = WIFI_GenerateRXHeader(outIEEE80211FrameHeaderPtr, 1, true, sizeof(WifiCtlFrameHeaderACK));

	return newRXPacket;
}

bool WifiHandler::_SoftAPTrySendPacket(const TXPacketHeader& txHeader, const u8* IEEE80211PacketData)
{
	bool isPacketHandled = false;
	const WifiFrameControl& fc = (WifiFrameControl&)IEEE80211PacketData[0];

	switch((WifiFrameType)fc.Type)
	{
		case WifiFrameType_Management:
			{
				const WifiMgmtFrameHeader& mgmtFrameHeader = (WifiMgmtFrameHeader&)IEEE80211PacketData[0];

				if(WIFI_compareMAC(mgmtFrameHeader.BSSID, SoftAP_MACAddr) ||
					(WIFI_isBroadcastMAC(mgmtFrameHeader.BSSID) && (fc.Subtype == WifiFrameManagementSubtype_ProbeRequest)))
				{
					slock_lock(this->_mutexRXPacketQueue);

					RXQueuedPacket newRXPacket = this->_GenerateSoftAPMgmtResponseFrame((WifiFrameManagementSubtype)fc.Subtype, this->_softAPSequenceNumber, this->_wifi.usecCounter);
					if(newRXPacket.rxHeader.length > 0)
					{
						newRXPacket.latencyCount = 0;
						this->_rxPacketQueue.push_back(newRXPacket);
						this->_softAPSequenceNumber++;
					}

					slock_unlock(this->_mutexRXPacketQueue);
					isPacketHandled = true;
				}
				break;
			}

		case WifiFrameType_Control:
			{
				// For control frames, SoftAP isn't going to send any replies. Instead, just
				// assume that the packet is always handled as long as it is destined for SoftAP.

				switch((WifiFrameControlSubtype)fc.Subtype)
				{
					case WifiFrameControlSubtype_PSPoll:
						{
							const WifiCtlFrameHeaderPSPoll& ctlFrameHeader = (WifiCtlFrameHeaderPSPoll&)IEEE80211PacketData[0];
							isPacketHandled = WIFI_compareMAC(ctlFrameHeader.BSSID, SoftAP_MACAddr);
							break;
						}

					case WifiFrameControlSubtype_RTS:
						{
							const WifiCtlFrameHeaderRTS& ctlFrameHeader = (WifiCtlFrameHeaderRTS&)IEEE80211PacketData[0];
							isPacketHandled = WIFI_compareMAC(ctlFrameHeader.rxMAC, SoftAP_MACAddr);
							break;
						}

					case WifiFrameControlSubtype_CTS:
						{
							const WifiCtlFrameHeaderCTS& ctlFrameHeader = (WifiCtlFrameHeaderCTS&)IEEE80211PacketData[0];
							isPacketHandled = WIFI_compareMAC(ctlFrameHeader.rxMAC, SoftAP_MACAddr);
							break;
						}

					case WifiFrameControlSubtype_ACK:
						{
							const WifiCtlFrameHeaderACK& ctlFrameHeader = (WifiCtlFrameHeaderACK&)IEEE80211PacketData[0];
							isPacketHandled = WIFI_compareMAC(ctlFrameHeader.rxMAC, SoftAP_MACAddr);
							break;
						}

					case WifiFrameControlSubtype_End:
						{
							const WifiCtlFrameHeaderEnd& ctlFrameHeader = (WifiCtlFrameHeaderEnd&)IEEE80211PacketData[0];
							isPacketHandled = WIFI_compareMAC(ctlFrameHeader.rxMAC, SoftAP_MACAddr);
							break;
						}

					case WifiFrameControlSubtype_EndAck:
						{
							const WifiCtlFrameHeaderEndAck& ctlFrameHeader = (WifiCtlFrameHeaderEndAck&)IEEE80211PacketData[0];
							isPacketHandled = WIFI_compareMAC(ctlFrameHeader.rxMAC, SoftAP_MACAddr);
							break;
						}

					default:
						break;
				}
				break;
			}

		case WifiFrameType_Data:
			{
				if(fc.FromToState == WifiFCFromToState_STA2DS)
				{
					const WifiDataFrameHeaderSTA2DS& IEEE80211FrameHeader = (WifiDataFrameHeaderSTA2DS&)IEEE80211PacketData[0];

					if(WIFI_compareMAC(IEEE80211FrameHeader.BSSID, SoftAP_MACAddr) && (this->_softAPStatus == APStatus_Associated))
					{
						size_t sendPacketSize = WifiHandler::ConvertDataFrame80211To8023(IEEE80211PacketData, txHeader.length, this->_workingTXBuffer);
						if(sendPacketSize > 0)
						{
							sendPacketSize = this->_softAPCommInterface->TXPacketSend(this->_workingTXBuffer, sendPacketSize);
							if(sendPacketSize > 0)
							{
								RXQueuedPacket newRXPacket = this->_GenerateSoftAPCtlACKFrame(IEEE80211FrameHeader, sendPacketSize);
								newRXPacket.latencyCount = 0;

								slock_lock(this->_mutexRXPacketQueue);
								this->_rxPacketQueue.push_back(newRXPacket);
								this->_softAPSequenceNumber++;
								slock_unlock(this->_mutexRXPacketQueue);

								#if WIFI_SAVE_PCAP_TO_FILE
								// Store the packet in the PCAP file.
								this->_PacketCaptureFileWrite(this->_workingTXBuffer, sendPacketSize, false, this->_wifi.usecCounter);
								#endif
							}
						}

						isPacketHandled = true;
					}
				}
				break;
			}
	}

	return isPacketHandled;
}

bool WifiHandler::_AdhocTrySendPacket(const TXPacketHeader& txHeader, const u8* IEEE80211PacketData)
{
	const size_t emulatorHeaderSize = sizeof(DesmumeFrameHeader);
	const size_t emulatorPacketSize = emulatorHeaderSize + txHeader.length;

	DesmumeFrameHeader& emulatorHeader = (DesmumeFrameHeader&)this->_workingTXBuffer[0];
	strncpy(emulatorHeader.frameID, DESMUME_EMULATOR_FRAME_ID, 8);
	emulatorHeader.version = DESMUME_EMULATOR_FRAME_CURRENT_VERSION;
	emulatorHeader.timeStamp = 0;
	emulatorHeader.emuPacketSize = txHeader.length;

	emulatorHeader.packetAttributes.value = 0;
	emulatorHeader.packetAttributes.IsTXRate20 = (txHeader.txRate == 20) ? 1 : 0;

	memcpy(this->_workingTXBuffer + emulatorHeaderSize, IEEE80211PacketData, txHeader.length);

	this->_adhocCommInterface->TXPacketSend(this->_workingTXBuffer, emulatorPacketSize);

	return true;
}

void WifiHandler::Reset()
{
	memset(&legacyWifiSF, 0, sizeof(LegacyWifiSFormat));
	memset(&_wifi, 0, sizeof(WifiData));
	memset(_wifi.txPacketInfo, 0, sizeof(TXPacketInfo) * 6);

	WIFI_resetRF(this->_wifi.rf);
	memcpy(_wifi.bb.data, BBDefaultData, sizeof(BBDefaultData));

	_wifi.io.POWER_US.Disable = 1;
	_wifi.io.POWERSTATE.IsPowerOff = 1;
	_wifi.io.TXREQ_READ.UNKNOWN1 = 1;
	_wifi.io.BB_POWER.Disable = 0x0D;
	_wifi.io.BB_POWER.DisableAllPorts = 1;
	_wifi.io.RF_PINS.UNKNOWN1 = 1;

	_wifi.io.MACADDR[0] = 0xFF;
	_wifi.io.MACADDR[1] = 0xFF;
	_wifi.io.MACADDR[2] = 0xFF;
	_wifi.io.MACADDR[3] = 0xFF;
	_wifi.io.MACADDR[4] = 0xFF;
	_wifi.io.MACADDR[5] = 0xFF;

	_wifi.io.BSSID[0] = 0xFF;
	_wifi.io.BSSID[1] = 0xFF;
	_wifi.io.BSSID[2] = 0xFF;
	_wifi.io.BSSID[3] = 0xFF;
	_wifi.io.BSSID[4] = 0xFF;
	_wifi.io.BSSID[5] = 0xFF;

	_wifi.txPacketInfo[0].txLocation = &_wifi.io.TXBUF_LOC1;
	_wifi.txPacketInfo[1].txLocation = &_wifi.io.TXBUF_CMD;
	_wifi.txPacketInfo[2].txLocation = &_wifi.io.TXBUF_LOC2;
	_wifi.txPacketInfo[3].txLocation = &_wifi.io.TXBUF_LOC3;
	_wifi.txPacketInfo[4].txLocation = &_wifi.io.TXBUF_BEACON;
	_wifi.txPacketInfo[5].txLocation = &_wifi.io.TXBUF_REPLY2;

	this->_didWarnWFCUser = false;
}

WifiData& WifiHandler::GetWifiData()
{
	return this->_wifi;
}

TXPacketInfo& WifiHandler::GetPacketInfoAtSlot(size_t txSlot)
{
	if(txSlot > 4)
	{
		return this->_wifi.txPacketInfo[0];
	}

	return this->_wifi.txPacketInfo[txSlot];
}

WifiEmulationLevel WifiHandler::GetSelectedEmulationLevel()
{
	return this->_selectedEmulationLevel;
}

WifiEmulationLevel WifiHandler::GetCurrentEmulationLevel()
{
	return this->_currentEmulationLevel;
}

void WifiHandler::SetEmulationLevel(WifiEmulationLevel emulationLevel)
{
	#ifdef EXPERIMENTAL_WIFI_COMM
	this->_selectedEmulationLevel = emulationLevel;
	#else
	this->_selectedEmulationLevel = WifiEmulationLevel_Off;
	#endif
}

int WifiHandler::GetBridgeDeviceList(std::vector<std::string>* deviceStringList)
{
	int result = -1;

	return result;

#if 0

	if(deviceStringList == NULL)
	{
		return result;
	}

	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_if_t* deviceList;

	result = this->GetPCapInterface()->findalldevs((void**)&deviceList, errbuf);
	if((result == -1) || (deviceList == NULL))
	{
		return result;
	}

	pcap_if_t* currentDevice = deviceList;
	for(size_t i = 0; currentDevice != NULL; i++, currentDevice = currentDevice->next)
	{
		if((currentDevice->description == NULL) || (currentDevice->description[0] == '\0'))
		{
			deviceStringList->push_back(currentDevice->name);
		}
		else
		{
			deviceStringList->push_back(currentDevice->description);
		}
	}

	return deviceStringList->size();
	#endif
}

int WifiHandler::GetSelectedBridgeDeviceIndex()
{
	return this->_selectedBridgeDeviceIndex;
}

int WifiHandler::GetCurrentBridgeDeviceIndex()
{
	return this->_softAPCommInterface->GetBridgeDeviceIndex();
}

void WifiHandler::SetBridgeDeviceIndex(int deviceIndex)
{
	this->_selectedBridgeDeviceIndex = deviceIndex;
}

bool WifiHandler::CommStart()
{
	// Stop the current comm interfaces.
	this->_adhocCommInterface->Stop();
	this->_softAPCommInterface->Stop();

	// Reset internal values.
	this->_wifi.usecCounter = 0;
	this->_RXEmptyQueue();

	FW_Mac[0] = MMU.fw.data.wifiInfo.MACAddr[0];
	FW_Mac[1] = MMU.fw.data.wifiInfo.MACAddr[1];
	FW_Mac[2] = MMU.fw.data.wifiInfo.MACAddr[2];
	FW_Mac[3] = MMU.fw.data.wifiInfo.MACAddr[3];
	FW_Mac[4] = MMU.fw.data.wifiInfo.MACAddr[4];
	FW_Mac[5] = MMU.fw.data.wifiInfo.MACAddr[5];

	WIFI_LOG(1, "MAC Address = %02X:%02X:%02X:%02X:%02X:%02X\n",
		FW_Mac[0], FW_Mac[1], FW_Mac[2], FW_Mac[3], FW_Mac[4], FW_Mac[5]);

	// Allocate 16KB worth of memory per buffer for sending packets. Hopefully, this should be plenty.
	this->_workingTXBuffer = (u8*)malloc(WIFI_WORKING_PACKET_BUFFER_SIZE);
	this->_softAPStatus = APStatus_Disconnected;
	this->_softAPSequenceNumber = 0;

	// Assign the pcap interface to SoftAP if pcap is available.
	this->_softAPCommInterface->SetPCapInterface(this->_pcap);
	this->_softAPCommInterface->SetBridgeDeviceIndex(this->_selectedBridgeDeviceIndex);

	if(this->_selectedEmulationLevel == WifiEmulationLevel_Off)
	{
		WIFI_LOG(1, "Emulation level is OFF.\n");
	}
	else
	{
		// Start the new comm interfaces.
		if(this->_isSocketsSupported)
		{
			this->_adhocCommInterface->Start(this);
		}
		else
		{
			WIFI_LOG(1, "Ad-hoc mode requires sockets, but sockets are not supported on this system.\n");
		}

		if(this->IsPCapSupported())
		{
			this->_softAPCommInterface->Start(this);
		}
		else
		{
			WIFI_LOG(1, "Infrastructure mode requires libpcap for full functionality,\n      but libpcap is not available on this system. Network functions\n      will be disabled for this session.\n");
		}
	}

	this->_currentEmulationLevel = this->_selectedEmulationLevel;

	return true;
}

void WifiHandler::CommStop()
{
	this->_PacketCaptureFileClose();

	this->_adhocCommInterface->Stop();
	this->_softAPCommInterface->Stop();

	this->_RXEmptyQueue();

	free(this->_workingTXBuffer);
	this->_workingTXBuffer = NULL;
}

void WifiHandler::CommSendPacket(const TXPacketHeader& txHeader, const u8* IEEE80211PacketData)
{
	bool isFrameSent = false;

	// First, give SoftAP a chance at sending the frame.
	isFrameSent = this->_SoftAPTrySendPacket(txHeader, IEEE80211PacketData);

	// If the frame wasn't sent, try sending it via an ad-hoc connection.
	if(!isFrameSent)
	{
		this->_AdhocTrySendPacket(txHeader, IEEE80211PacketData);
	}
}

void WifiHandler::CommTrigger()
{
	WifiData& wifi = this->_wifi;
	WIFI_IOREG_MAP& io = wifi.io;

	if(io.POWER_US.Disable != 0)
	{
		return; // Don't do anything if WiFi isn't powered up.
	}

	wifi.usecCounter++;

	// a usec has passed
	if(io.US_COUNTCNT.EnableCounter != 0)
	{
		io.US_COUNT++;
	}

	// Note: the extra counter is decremented every 10 microseconds.
	// To avoid a modulo every microsecond, we multiply the counter
	// value by 10 and decrement it every microsecond :)
	if(io.CMD_COUNTCNT.EnableCounter != 0)
	{
		if(wifi.cmdCount_u32 > 0)
		{
			wifi.cmdCount_u32--;

			if(wifi.cmdCount_u32 == 0)
			{
				WIFI_TXStart(WifiTXLocIndex_CMD, io.TXBUF_CMD);
			}
		}
	}

	// The beacon counters are in milliseconds
	// GBATek says they're decremented every 1024 usecs
	if((io.US_COUNT & 1023) == 0)
	{
		io.BEACONCOUNT1--;
		if(io.BEACONCOUNT1 == (io.PRE_BEACON >> 10))
		{
			WIFI_triggerIRQ(WifiIRQ15_TimeslotPreBeacon);
		}
		else if(io.BEACONCOUNT1 == 0)
		{
			WIFI_triggerIRQ(WifiIRQ14_TimeslotBeacon);
		}

		if(io.BEACONCOUNT2 > 0)
		{
			io.BEACONCOUNT2--;
			if(io.BEACONCOUNT2 == 0)
			{
				WIFI_triggerIRQ(WifiIRQ13_TimeslotPostBeacon);
			}
		}
	}

	if((io.US_COMPARECNT.EnableCompare != 0) && (io.US_COMPARE == io.US_COUNT))
	{
		//printf("ucmp irq14\n");
		WIFI_triggerIRQ(WifiIRQ14_TimeslotBeacon);
	}

	if(io.CONTENTFREE > 0)
	{
		io.CONTENTFREE--;
	}

	if((io.US_COUNT & 3) == 0)
	{
		const WifiTXLocIndex txSlotIndex = wifi.txCurrentSlot;
		bool isTXSlotBusy = false;

		switch(txSlotIndex)
		{
			case WifiTXLocIndex_LOC1: isTXSlotBusy = (io.TXBUSY.Loc1 != 0); break;
				//case WifiTXLocIndex_CMD: isTXSlotBusy = (io.TXBUSY.Cmd != 0); break;
			case WifiTXLocIndex_LOC2: isTXSlotBusy = (io.TXBUSY.Loc2 != 0); break;
			case WifiTXLocIndex_LOC3: isTXSlotBusy = (io.TXBUSY.Loc3 != 0); break;
				//case WifiTXLocIndex_BEACON: isTXSlotBusy = (io.TXBUSY.Beacon != 0); break;

			default:
				break;
		}

		if(isTXSlotBusy)
		{
			IOREG_W_TXBUF_LOCATION* txBufLocation = NULL;
			TXPacketInfo& txPacketInfo = this->GetPacketInfoAtSlot(txSlotIndex);

			switch(txSlotIndex)
			{
				case WifiTXLocIndex_LOC1: txBufLocation = &io.TXBUF_LOC1; break;
					//case WifiTXLocIndex_CMD: txBufLocation = &io.TXBUF_CMD; break;
				case WifiTXLocIndex_LOC2: txBufLocation = &io.TXBUF_LOC2; break;
				case WifiTXLocIndex_LOC3: txBufLocation = &io.TXBUF_LOC3; break;
					//case WifiTXLocIndex_BEACON: txBufLocation = &io.TXBUF_BEACON; break;

				default:
					break;
			}

			txPacketInfo.remainingBytes--;
			io.RXTX_ADDR.HalfwordAddress++;

			if(txPacketInfo.remainingBytes == 0)
			{
				isTXSlotBusy = false;

				switch(txSlotIndex)
				{
					case WifiTXLocIndex_LOC1: io.TXBUSY.Loc1 = 0; break;
						//case WifiTXLocIndex_CMD: io.TXBUSY.Cmd = 0; break;
					case WifiTXLocIndex_LOC2: io.TXBUSY.Loc2 = 0; break;
					case WifiTXLocIndex_LOC3: io.TXBUSY.Loc3 = 0; break;
						//case WifiTXLocIndex_BEACON: io.TXBUSY.Beacon = 0; break;

					default:
						break;
				}

				txBufLocation->TransferRequest = 0;

				TXPacketHeader& txHeader = (TXPacketHeader&)wifi.RAM[txBufLocation->HalfwordAddress << 1];
				this->CommSendPacket(txHeader, &wifi.RAM[(txBufLocation->HalfwordAddress << 1) + sizeof(TXPacketHeader)]);
				txHeader.txStatus = 0x0001;
				txHeader.UNKNOWN3 = 0;

				switch(txSlotIndex)
				{
					//case WifiTXLocIndex_CMD:
					//case WifiTXLocIndex_BEACON:
					case WifiTXLocIndex_LOC1: io.TXSTAT.PacketUpdate = 0; break;
					case WifiTXLocIndex_LOC2: io.TXSTAT.PacketUpdate = 1; break;
					case WifiTXLocIndex_LOC3: io.TXSTAT.PacketUpdate = 2; break;

					default:
						break;
				}

				io.TXSTAT.PacketCompleted = 1;

				WIFI_triggerIRQ(WifiIRQ01_TXComplete);

				//io.RF_STATUS.RFStatus = 0x01;
				//io.RF_PINS.RX_On = 1;

				io.RF_STATUS.RFStatus = 0x09;
				io.RF_PINS.CarrierSense = 0;
				io.RF_PINS.TXMain = 0;
				io.RF_PINS.UNKNOWN1 = 1;
				io.RF_PINS.TX_On = 0;
				io.RF_PINS.RX_On = 0;

				// Switch to the next TX slot.
				while(!isTXSlotBusy && (wifi.txCurrentSlot != WifiTXLocIndex_LOC1))
				{
					/*if (wifi.txCurrentSlot == WifiTXLocIndex_BEACON)
					 {
					 wifi.txCurrentSlot = WifiTXLocIndex_CMD;
					 isTXSlotBusy = (io.TXBUSY.Cmd != 0);
					 }
					 else if (wifi.txCurrentSlot == WifiTXLocIndex_CMD)
					 {
					 wifi.txCurrentSlot = WifiTXLocIndex_LOC3;
					 isTXSlotBusy = (io.TXBUSY.Loc3 != 0);
					 }
					 else */if(wifi.txCurrentSlot == WifiTXLocIndex_LOC3)
					 {
						 wifi.txCurrentSlot = WifiTXLocIndex_LOC2;
						 isTXSlotBusy = (io.TXBUSY.Loc2 != 0);
					 }
					 else if(wifi.txCurrentSlot == WifiTXLocIndex_LOC2)
					 {
						 wifi.txCurrentSlot = WifiTXLocIndex_LOC1;
						 isTXSlotBusy = (io.TXBUSY.Loc1 != 0);
					 }
				}

				WIFI_LOG(3, "TX slot %i finished sending its packet. Next is slot %i. TXStat = %04X\n",
					(int)txSlotIndex, (int)wifi.txCurrentSlot, io.TXSTAT.value);
			}
		}
	}

	if(io.RXCNT.EnableRXFIFOQueuing != 0)
	{
		this->_AddPeriodicPacketsToRXQueue(wifi.usecCounter);
		this->_CopyFromRXQueue();
	}
}

void WifiHandler::_AddPeriodicPacketsToRXQueue(const u64 usecCounter)
{
	if((usecCounter & 131071) == 0)
	{
		slock_lock(this->_mutexRXPacketQueue);

		//zero sez: every 1/10 second? does it have to be precise? this is so costly..
		// Okay for 128 ms then
		RXQueuedPacket newRXPacket = this->_GenerateSoftAPBeaconFrame(this->_softAPSequenceNumber, this->_wifi.usecCounter);
		newRXPacket.latencyCount = 0;

		this->_rxPacketQueue.push_back(newRXPacket);
		this->_softAPSequenceNumber++;

		slock_unlock(this->_mutexRXPacketQueue);
	}
}

void WifiHandler::_CopyFromRXQueue()
{
	WIFI_IOREG_MAP& io = this->_wifi.io;

	// Retrieve a packet from the RX packet queue if we're not already working on one.
	if(this->_rxCurrentQueuedPacketPosition == 0)
	{
		slock_lock(this->_mutexRXPacketQueue);

		if(this->_rxPacketQueue.empty())
		{
			// However, if the queue is empty, then there is no packet to retrieve.
			slock_unlock(this->_mutexRXPacketQueue);
			return;
		}

		this->_rxCurrentPacket = this->_rxPacketQueue.front();
		this->_rxPacketQueue.pop_front();

		slock_unlock(this->_mutexRXPacketQueue);

		WIFI_triggerIRQ(WifiIRQ06_RXStart);
	}

	const size_t totalPacketLength = (this->_rxCurrentPacket.rxHeader.length > MAX_PACKET_SIZE_80211) ? sizeof(RXPacketHeader) + MAX_PACKET_SIZE_80211 : sizeof(RXPacketHeader) + this->_rxCurrentPacket.rxHeader.length;
	this->_rxCurrentPacket.latencyCount++;

	// If the user selects compatibility mode, then we will emulate the transfer delays
	// involved with copying RX packet data into WiFi RAM. Otherwise, just copy all of
	// the RX packet data into WiFi RAM immediately.
	if(this->_currentEmulationLevel == WifiEmulationLevel_Compatibility)
	{
		// Copy the RX packet data into WiFi RAM over time.
		if((this->_rxCurrentQueuedPacketPosition == 0) || (this->_rxCurrentPacket.latencyCount >= RX_LATENCY_LIMIT))
		{
			this->_RXWriteOneHalfword(*(u16*)&this->_rxCurrentPacket.rawFrameData[this->_rxCurrentQueuedPacketPosition]);
			this->_rxCurrentQueuedPacketPosition += 2;
			this->_rxCurrentPacket.latencyCount = 0;
		}
	}
	else
	{
		// Copy the entire RX packet data into WiFi RAM immediately.
		while(this->_rxCurrentQueuedPacketPosition < totalPacketLength)
		{
			this->_RXWriteOneHalfword(*(u16*)&this->_rxCurrentPacket.rawFrameData[this->_rxCurrentQueuedPacketPosition]);
			this->_rxCurrentQueuedPacketPosition += 2;
		}
	}

	if(this->_rxCurrentQueuedPacketPosition >= totalPacketLength)
	{
		this->_rxCurrentQueuedPacketPosition = 0;

		// Adjust the RX cursor address so that it is 4-byte aligned.
		io.RXBUF_WRCSR.HalfwordAddress = ((io.RXBUF_WRCSR.HalfwordAddress + 1) & 0x0FFE);
		if(io.RXBUF_WRCSR.HalfwordAddress >= ((io.RXBUF_END & 0x1FFE) >> 1))
		{
			io.RXBUF_WRCSR.HalfwordAddress = ((io.RXBUF_BEGIN & 0x1FFE) >> 1);
		}

		io.RX_COUNT.OkayCount++;
		WIFI_triggerIRQ(WifiIRQ00_RXComplete);
		io.RF_STATUS.RFStatus = WifiRFStatus1_TXComplete;
		io.RF_PINS.value = RFPinsLUT[WifiRFStatus1_TXComplete];
	}
}

void WifiHandler::CommEmptyRXQueue()
{
	this->_RXEmptyQueue();
}

template <bool WILLADVANCESEQNO>
void WifiHandler::RXPacketRawToQueue(const RXRawPacketData& rawPacket)
{
	RXQueuedPacket newRXPacket;

	slock_lock(this->_mutexRXPacketQueue);

	for(size_t i = 0, readLocation = 0; i < rawPacket.count; i++)
	{
		const u8* currentPacket = &rawPacket.buffer[readLocation];
		const DesmumeFrameHeader& emulatorHeader = (DesmumeFrameHeader&)*currentPacket;
		readLocation += sizeof(DesmumeFrameHeader) + emulatorHeader.emuPacketSize;

		const u8* packetIEEE80211HeaderPtr = this->_RXPacketFilter(currentPacket, sizeof(DesmumeFrameHeader) + emulatorHeader.emuPacketSize, newRXPacket.rxHeader);
		if(packetIEEE80211HeaderPtr != NULL)
		{
			memset(newRXPacket.rxData, 0, sizeof(newRXPacket.rxData));
			memcpy(newRXPacket.rxData, packetIEEE80211HeaderPtr, newRXPacket.rxHeader.length);
			newRXPacket.latencyCount = 0;

			if(WILLADVANCESEQNO)
			{
				// Update the sequence number.
				WifiDataFrameHeaderDS2STA& IEEE80211Header = (WifiDataFrameHeaderDS2STA&)newRXPacket.rxData[0];
				IEEE80211Header.seqCtl.SequenceNumber = this->_softAPSequenceNumber;
				this->_softAPSequenceNumber++;

				// Write the FCS at the end of the frame.
				u32& fcs = (u32&)newRXPacket.rxData[newRXPacket.rxHeader.length];
				fcs = WIFI_calcCRC32(newRXPacket.rxData, newRXPacket.rxHeader.length);
				newRXPacket.rxHeader.length += sizeof(u32);
			}

			// Add the packet to the RX queue.
			this->_rxPacketQueue.push_back(newRXPacket);
		}
	}

	slock_unlock(this->_mutexRXPacketQueue);
}

bool WifiHandler::IsPCapSupported()
{
	return ((this->_pcap != NULL) && (this->_pcap != &dummyPCapInterface));
}

bool WifiHandler::IsSocketsSupported()
{
	return this->_isSocketsSupported;
}

void WifiHandler::SetSocketsSupported(bool isSupported)
{
	this->_isSocketsSupported = isSupported;
}

ClientPCapInterface* WifiHandler::GetPCapInterface()
{
	return this->_pcap;
}

void WifiHandler::SetPCapInterface(ClientPCapInterface* pcapInterface)
{
	this->_pcap = (pcapInterface == NULL) ? &dummyPCapInterface : pcapInterface;
}

void WifiHandler::PrepareSaveStateWrite()
{
	WIFI_IOREG_MAP& io = this->_wifi.io;
	RF2958_IOREG_MAP& rf = this->_wifi.rf;

	legacyWifiSF.powerOn = (u32)(io.POWERSTATE.IsPowerOff == 0);
	legacyWifiSF.powerOnPending = (u32)(io.POWERSTATE.WillPowerOn != 0);

	legacyWifiSF.rfStatus = io.RF_STATUS.value;
	legacyWifiSF.rfPins = io.RF_PINS.value;

	legacyWifiSF.IE = io.IE.value;
	legacyWifiSF.IF = io.IF.value;

	legacyWifiSF.macMode = io.MODE_RST.value;
	legacyWifiSF.wepMode = io.MODE_WEP.value;
	legacyWifiSF.WEP_enable = io.WEP_CNT.Enable;

	legacyWifiSF.TXCnt = io.TXREQ_READ.value;
	legacyWifiSF.TXStat = io.TXSTAT.value;

	legacyWifiSF.RXCnt = io.RXCNT.value;
	legacyWifiSF.RXCheckCounter = 0;

	legacyWifiSF.macAddr[0] = io.MACADDR[0];
	legacyWifiSF.macAddr[1] = io.MACADDR[1];
	legacyWifiSF.macAddr[2] = io.MACADDR[2];
	legacyWifiSF.macAddr[3] = io.MACADDR[3];
	legacyWifiSF.macAddr[4] = io.MACADDR[4];
	legacyWifiSF.macAddr[5] = io.MACADDR[5];
	legacyWifiSF.bssid[0] = io.BSSID[0];
	legacyWifiSF.bssid[1] = io.BSSID[1];
	legacyWifiSF.bssid[2] = io.BSSID[2];
	legacyWifiSF.bssid[3] = io.BSSID[3];
	legacyWifiSF.bssid[4] = io.BSSID[4];
	legacyWifiSF.bssid[5] = io.BSSID[5];

	legacyWifiSF.aid = io.AID_FULL.AssociationID;
	legacyWifiSF.pid = io.AID_LOW.PlayerID;
	legacyWifiSF.retryLimit = io.TX_RETRYLIMIT.value;

	legacyWifiSF.crystalEnabled = (u32)(io.POWER_US.Disable == 0);
	legacyWifiSF.usec = io.US_COUNT;
	legacyWifiSF.usecEnable = io.US_COUNTCNT.EnableCounter;
	legacyWifiSF.ucmp = io.US_COMPARE;
	legacyWifiSF.ucmpEnable = io.US_COMPARECNT.EnableCompare;
	legacyWifiSF.eCount = this->_wifi.cmdCount_u32;
	legacyWifiSF.eCountEnable = io.CMD_COUNTCNT.EnableCounter;

	legacyWifiSF.rf_cfg1 = rf.CFG1.value;
	legacyWifiSF.rf_ifpll1 = rf.IFPLL1.value;
	legacyWifiSF.rf_ifpll2 = rf.IFPLL2.value;
	legacyWifiSF.rf_ifpll3 = rf.IFPLL3.value;
	legacyWifiSF.rf_rfpll1 = rf.RFPLL1.value;
	legacyWifiSF.rf_rfpll2 = rf.RFPLL2.value;
	legacyWifiSF.rf_rfpll3 = rf.RFPLL3.value;
	legacyWifiSF.rf_rfpll4 = rf.RFPLL4.value;
	legacyWifiSF.rf_cal1 = rf.CAL1.value;
	legacyWifiSF.rf_txrx1 = rf.TXRX1.value;
	legacyWifiSF.rf_pcnt1 = rf.PCNT1.value;
	legacyWifiSF.rf_pcnt2 = rf.PCNT2.value;
	legacyWifiSF.rf_vcot1 = rf.VCOT1.value;

	legacyWifiSF.rfIOCnt = io.RF_CNT.value;
	legacyWifiSF.rfIOStatus = io.RF_BUSY.Busy;
	legacyWifiSF.rfIOData = (io.RF_DATA2.value << 16) | io.RF_DATA1.value;
	legacyWifiSF.bbIOCnt = io.BB_CNT.value;

	legacyWifiSF.rxRangeBegin = io.RXBUF_BEGIN;
	legacyWifiSF.rxRangeEnd = io.RXBUF_END;
	legacyWifiSF.rxWriteCursor = io.RXBUF_WRCSR.HalfwordAddress;
	legacyWifiSF.rxReadCursor = io.RXBUF_READCSR.HalfwordAddress;
	legacyWifiSF.rxUnits = 0;
	legacyWifiSF.rxBufCount = io.RXBUF_COUNT.Count;
	legacyWifiSF.circBufReadAddress = io.RXBUF_RD_ADDR.ByteAddress;
	legacyWifiSF.circBufWriteAddress = io.TXBUF_WR_ADDR.ByteAddress;
	legacyWifiSF.circBufReadEnd = io.RXBUF_GAP.ByteAddress;
	legacyWifiSF.circBufReadSkip = io.RXBUF_GAPDISP.HalfwordOffset;
	legacyWifiSF.circBufWriteEnd = io.TXBUF_GAP.ByteAddress;
	legacyWifiSF.circBufWriteSkip = io.TXBUF_GAPDISP.HalfwordOffset;

	legacyWifiSF.randomSeed = io.RANDOM.Random;

	memcpy(legacyWifiSF.wifiIOPorts, &this->_wifi.io, sizeof(WIFI_IOREG_MAP));
	memcpy(legacyWifiSF.bb_data, this->_wifi.bb.data, sizeof(this->_wifi.bb.data));
	memcpy(legacyWifiSF.wifiRAM, this->_wifi.RAM, sizeof(this->_wifi.RAM));
}

bool WifiHandler::LoadState(EMUFILE& is, int size)
{
	int version;
	if(is.read_32LE(version) != 1) return false;
	is.fread(&_wifi, sizeof(_wifi));
	return true;
}

void WifiHandler::SaveState(EMUFILE& f)
{
	f.write_32LE(1); //version
	f.fwrite(&_wifi, sizeof(_wifi));
}

void WifiHandler::ParseSaveStateRead()
{
	RF2958_IOREG_MAP& rf = this->_wifi.rf;
	rf.CFG1.value = legacyWifiSF.rf_cfg1;
	rf.IFPLL1.value = legacyWifiSF.rf_ifpll1;
	rf.IFPLL2.value = legacyWifiSF.rf_ifpll2;
	rf.IFPLL3.value = legacyWifiSF.rf_ifpll3;
	rf.RFPLL1.value = legacyWifiSF.rf_rfpll1;
	rf.RFPLL2.value = legacyWifiSF.rf_rfpll2;
	rf.RFPLL3.value = legacyWifiSF.rf_rfpll3;
	rf.RFPLL4.value = legacyWifiSF.rf_rfpll4;
	rf.CAL1.value = legacyWifiSF.rf_cal1;
	rf.TXRX1.value = legacyWifiSF.rf_txrx1;
	rf.PCNT1.value = legacyWifiSF.rf_pcnt1;
	rf.PCNT2.value = legacyWifiSF.rf_pcnt2;
	rf.VCOT1.value = legacyWifiSF.rf_vcot1;

	memcpy(&this->_wifi.io, legacyWifiSF.wifiIOPorts, sizeof(WIFI_IOREG_MAP));
	memcpy(this->_wifi.bb.data, legacyWifiSF.bb_data, sizeof(this->_wifi.bb.data));
	memcpy(this->_wifi.RAM, legacyWifiSF.wifiRAM, sizeof(this->_wifi.RAM));
}

size_t WifiHandler::ConvertDataFrame80211To8023(const u8* inIEEE80211Frame, const size_t txLength, u8* outIEEE8023Frame)
{
	size_t sendPacketSize = 0;

	const WifiFrameControl& fc = (WifiFrameControl&)inIEEE80211Frame[0];

	// Ensure that the incoming 802.11 frame is a STA-to-DS data frame with a LLC/SNAP header.
	if((fc.Type != WifiFrameType_Data) ||
		(fc.FromToState != WifiFCFromToState_STA2DS) ||
		!WIFI_IsLLCSNAPHeader(inIEEE80211Frame + sizeof(WifiDataFrameHeaderSTA2DS)))
	{
		return sendPacketSize;
	}

	const WifiDataFrameHeaderSTA2DS& IEEE80211FrameHeader = (WifiDataFrameHeaderSTA2DS&)inIEEE80211Frame[0];
	const WifiLLCSNAPHeader& snapHeader = (WifiLLCSNAPHeader&)inIEEE80211Frame[sizeof(WifiDataFrameHeaderSTA2DS)];
	const u8* inFrameBody = inIEEE80211Frame + sizeof(WifiDataFrameHeaderSTA2DS) + sizeof(WifiLLCSNAPHeader);

	EthernetFrameHeader& IEEE8023FrameHeader = (EthernetFrameHeader&)outIEEE8023Frame[0];
	u8* outFrameBody = outIEEE8023Frame + sizeof(EthernetFrameHeader);

	IEEE8023FrameHeader.destMAC[0] = IEEE80211FrameHeader.destMAC[0];
	IEEE8023FrameHeader.destMAC[1] = IEEE80211FrameHeader.destMAC[1];
	IEEE8023FrameHeader.destMAC[2] = IEEE80211FrameHeader.destMAC[2];
	IEEE8023FrameHeader.destMAC[3] = IEEE80211FrameHeader.destMAC[3];
	IEEE8023FrameHeader.destMAC[4] = IEEE80211FrameHeader.destMAC[4];
	IEEE8023FrameHeader.destMAC[5] = IEEE80211FrameHeader.destMAC[5];

	IEEE8023FrameHeader.sendMAC[0] = IEEE80211FrameHeader.sendMAC[0];
	IEEE8023FrameHeader.sendMAC[1] = IEEE80211FrameHeader.sendMAC[1];
	IEEE8023FrameHeader.sendMAC[2] = IEEE80211FrameHeader.sendMAC[2];
	IEEE8023FrameHeader.sendMAC[3] = IEEE80211FrameHeader.sendMAC[3];
	IEEE8023FrameHeader.sendMAC[4] = IEEE80211FrameHeader.sendMAC[4];
	IEEE8023FrameHeader.sendMAC[5] = IEEE80211FrameHeader.sendMAC[5];

	IEEE8023FrameHeader.ethertype = snapHeader.ethertype;

	sendPacketSize = txLength - sizeof(WifiDataFrameHeaderSTA2DS) - sizeof(WifiLLCSNAPHeader) - sizeof(u32) + sizeof(EthernetFrameHeader);
	memcpy(outFrameBody, inFrameBody, sendPacketSize - sizeof(EthernetFrameHeader));

	return sendPacketSize;
}

size_t WifiHandler::ConvertDataFrame8023To80211(const u8* inIEEE8023Frame, const size_t rxLength, u8* outIEEE80211Frame)
{
	size_t sendPacketSize = 0;

	// Convert the libpcap 802.3 header into an NDS-compatible 802.11 header.
	const EthernetFrameHeader& IEEE8023Header = (EthernetFrameHeader&)inIEEE8023Frame[0];
	WifiDataFrameHeaderDS2STA& IEEE80211Header = (WifiDataFrameHeaderDS2STA&)outIEEE80211Frame[0];
	IEEE80211Header.fc.value = 0;
	IEEE80211Header.fc.Type = WifiFrameType_Data;
	IEEE80211Header.fc.Subtype = WifiFrameDataSubtype_Data;
	IEEE80211Header.fc.ToDS = 0;
	IEEE80211Header.fc.FromDS = 1;

	IEEE80211Header.duration = 0;
	IEEE80211Header.destMAC[0] = IEEE8023Header.destMAC[0];
	IEEE80211Header.destMAC[1] = IEEE8023Header.destMAC[1];
	IEEE80211Header.destMAC[2] = IEEE8023Header.destMAC[2];
	IEEE80211Header.destMAC[3] = IEEE8023Header.destMAC[3];
	IEEE80211Header.destMAC[4] = IEEE8023Header.destMAC[4];
	IEEE80211Header.destMAC[5] = IEEE8023Header.destMAC[5];
	IEEE80211Header.BSSID[0] = SoftAP_MACAddr[0];
	IEEE80211Header.BSSID[1] = SoftAP_MACAddr[1];
	IEEE80211Header.BSSID[2] = SoftAP_MACAddr[2];
	IEEE80211Header.BSSID[3] = SoftAP_MACAddr[3];
	IEEE80211Header.BSSID[4] = SoftAP_MACAddr[4];
	IEEE80211Header.BSSID[5] = SoftAP_MACAddr[5];
	IEEE80211Header.sendMAC[0] = IEEE8023Header.sendMAC[0];
	IEEE80211Header.sendMAC[1] = IEEE8023Header.sendMAC[1];
	IEEE80211Header.sendMAC[2] = IEEE8023Header.sendMAC[2];
	IEEE80211Header.sendMAC[3] = IEEE8023Header.sendMAC[3];
	IEEE80211Header.sendMAC[4] = IEEE8023Header.sendMAC[4];
	IEEE80211Header.sendMAC[5] = IEEE8023Header.sendMAC[5];
	IEEE80211Header.seqCtl.value = 0; // This is 0 for now, but will need to be set later.

	// 802.3 to 802.11 LLC/SNAP header
	WifiLLCSNAPHeader& snapHeader = (WifiLLCSNAPHeader&)outIEEE80211Frame[sizeof(WifiDataFrameHeaderDS2STA)];
	snapHeader = DefaultSNAPHeader;
	snapHeader.ethertype = IEEE8023Header.ethertype;

	// Packet body
	u8* packetBody = outIEEE80211Frame + sizeof(WifiDataFrameHeaderDS2STA) + sizeof(WifiLLCSNAPHeader);
	memcpy(packetBody, (u8*)inIEEE8023Frame + sizeof(EthernetFrameHeader), rxLength - sizeof(EthernetFrameHeader));

	sendPacketSize = rxLength + sizeof(WifiDataFrameHeaderDS2STA) + sizeof(WifiLLCSNAPHeader) - sizeof(EthernetFrameHeader);
	return sendPacketSize;
}
