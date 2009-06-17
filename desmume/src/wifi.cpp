/*
    Copyright (C) 2007 Tim Seidel

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <assert.h>
#include "wifi.h"
#include "armcpu.h"
#include "NDSSystem.h"
#include "debug.h"

#ifdef EXPERIMENTAL_WIFI

wifimac_t wifiMac ;

#endif

/*******************************************************************************

	Firmware info needed for wifi, if no firmware image is available
	see: http://www.akkit.org/info/dswifi.htm#WifiInit

	written in bytes, to avoid endianess issues

 *******************************************************************************/

u8 FW_Mac[6] 			= { 0x00, 0x09, 0xBF, 0x12, 0x34, 0x56 } ;
const u8 FW_WIFIInit[32] 		= { 0x02,0x00,  0x17,0x00,  0x26,0x00,  0x18,0x18,
							0x48,0x00,  0x40,0x48,  0x58,0x00,  0x42,0x00,
							0x40,0x01,  0x64,0x80,  0xE0,0xE0,  0x43,0x24,
							0x0E,0x00,  0x32,0x00,  0xF4,0x01,  0x01,0x01 } ;
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
						  } ;
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
						  } ;
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
						  } ;
const u8 FW_BBChannel[14]		= { 0xb3, 0xb3, 0xb3, 0xb3, 0xb3,	/* channel  1- 6 */
							0xb4, 0xb4, 0xb4, 0xb4, 0xb4,	/* channel  7-10 */ 
							0xb5, 0xb5,						/* channel 11-12 */
							0xb6, 0xb6						/* channel 13-14 */
						  } ;

/* Note : the values are inspired from what I found in a firmware image from my DS */

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
								{0xBA, 0xA0, 0x35, 0xE7, 0x01, 0xD0, 0x05, 0xAD, 0x39, 0x0F, 0x40, 0x1C, 0x2B, 0x2C},
								{0, 0}
							   } ;

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
							   } ;

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
							   } ;

#ifdef EXPERIMENTAL_WIFI

/*******************************************************************************

	Logging

 *******************************************************************************/

// 0: disable logging
// 1: lowest logging, shows most important messages such as errors
// 2: low logging, shows things such as warnings
// 3: medium logging, for debugging, shows lots of stuff
// 4: high logging, for debugging, shows almost everything, may slow down
// 5: highest logging, for debugging, shows everything, may slow down a lot
#define WIFI_LOGGING_LEVEL 3

#if (WIFI_LOGGING_LEVEL >= 1)
#define WIFI_LOG(level, ...) if(level <= WIFI_LOGGING_LEVEL) LOGC(8, "WIFI: "__VA_ARGS__);
#else
#define WIFI_LOG(level, ...)
#endif

/*******************************************************************************

	Helpers

 *******************************************************************************/

INLINE u32 WIFI_alignedLen(u32 len)
{
	return ((len + 3) & ~3);
}

#ifdef WIN32
static pcap_t *desmume_pcap_open(const char *source, int snaplen, int flags,
		int read_timeout, char *errbuf)
{
	return PCAP::pcap_open(source, snaplen, flags, read_timeout, NULL, errbuf);
}

static int desmume_pcap_findalldevs(pcap_if_t **alldevs, char *errbuf)
{
	return PCAP::pcap_findalldevs_ex(PCAP_SRC_IF_STRING, NULL, alldevs, errbuf);
}

static void desmume_pcap_freealldevs(pcap_if_t *alldevs)
{
	return PCAP::pcap_freealldevs(alldevs);
}

static void desmume_pcap_close(pcap_t *p)
{
	return PCAP::pcap_close(p);
}

static int desmume_pcap_sendpacket(pcap_t *p, u_char *buf, int size)
{
	return PCAP::pcap_sendpacket(p, buf, size);
}

#else
static pcap_t *desmume_pcap_open(const char *device, int snaplen, int promisc,
		int to_ms, char *errbuf)
{
	return pcap_open_live(device, snaplen, promisc, to_ms, errbuf);
}

static int desmume_pcap_findalldevs(pcap_if_t **alldevs, char *errbuf)
{
	return pcap_findalldevs(alldevs, errbuf);
}

static void desmume_pcap_freealldevs(pcap_if_t *alldevs)
{
	return pcap_freealldevs(alldevs);
}

static void desmume_pcap_close(pcap_t *p)
{
	return pcap_close(p);
}

static int desmume_pcap_sendpacket(pcap_t *p, u_char *buf, int size)
{
	return pcap_sendpacket(p, buf, size);
}
#endif

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

static u32 WIFI_getCRC32(u8 *data, int len)
{
	u32 crc = 0xFFFFFFFF;

	while(len--)
        crc = (crc >> 8) ^ WIFI_CRC32Table[(crc & 0xFF) ^ *data++];

	return (crc ^ 0xFFFFFFFF);
}

static void WIFI_initCRC32Table()
{
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

static void WIFI_resetRF(rffilter_t *rf) {
	/* reinitialize RF chip with the default values refer RF2958 docs */
	/* CFG1 */
	rf->CFG1.bits.IF_VGA_REG_EN = 1 ;
	rf->CFG1.bits.IF_VCO_REG_EN = 1 ;
	rf->CFG1.bits.RF_VCO_REG_EN = 1 ;
	rf->CFG1.bits.HYBERNATE = 0 ;
	rf->CFG1.bits.REF_SEL = 0 ;
	/* IFPLL1 */
	rf->IFPLL1.bits.DAC = 3 ;
	rf->IFPLL1.bits.P1 = 0 ;
	rf->IFPLL1.bits.LD_EN1 = 0 ;
	rf->IFPLL1.bits.AUTOCAL_EN1 = 0 ;
	rf->IFPLL1.bits.PDP1 = 1 ;
	rf->IFPLL1.bits.CPL1 = 0 ;
	rf->IFPLL1.bits.LPF1 = 0 ;
	rf->IFPLL1.bits.VTC_EN1 = 1 ;
	rf->IFPLL1.bits.KV_EN1 = 0 ;
	rf->IFPLL1.bits.PLL_EN1 = 0 ;
	/* IFPLL2 */
	rf->IFPLL2.bits.IF_N = 0x22 ;
	/* IFPLL3 */
	rf->IFPLL3.bits.KV_DEF1 = 8 ;
	rf->IFPLL3.bits.CT_DEF1 = 7 ;
	rf->IFPLL3.bits.DN1 = 0x1FF ;
	/* RFPLL1 */
	rf->RFPLL1.bits.DAC = 3 ;
	rf->RFPLL1.bits.P = 0 ;
	rf->RFPLL1.bits.LD_EN = 0 ;
	rf->RFPLL1.bits.AUTOCAL_EN = 0 ;
	rf->RFPLL1.bits.PDP = 1 ;
	rf->RFPLL1.bits.CPL = 0 ;
	rf->RFPLL1.bits.LPF = 0 ;
	rf->RFPLL1.bits.VTC_EN = 0 ;
	rf->RFPLL1.bits.KV_EN = 0 ;
	rf->RFPLL1.bits.PLL_EN = 0 ;
	/* RFPLL2 */
	rf->RFPLL2.bits.NUM2 = 0 ;
	rf->RFPLL2.bits.N2 = 0x5E ;
	/* RFPLL3 */
	rf->RFPLL3.bits.NUM2 = 0 ;
	/* RFPLL4 */
	rf->RFPLL4.bits.KV_DEF = 8 ;
	rf->RFPLL4.bits.CT_DEF = 7 ;
	rf->RFPLL4.bits.DN = 0x145 ;
	/* CAL1 */
	rf->CAL1.bits.LD_WINDOW = 2 ;
	rf->CAL1.bits.M_CT_VALUE = 8 ;
	rf->CAL1.bits.TLOCK = 7 ;
	rf->CAL1.bits.TVCO = 0x0F ;
	/* TXRX1 */
	rf->TXRX1.bits.TXBYPASS = 0 ;
	rf->TXRX1.bits.INTBIASEN = 0 ;
	rf->TXRX1.bits.TXENMODE = 0 ;
	rf->TXRX1.bits.TXDIFFMODE = 0 ;
	rf->TXRX1.bits.TXLPFBW = 2 ;
	rf->TXRX1.bits.RXLPFBW = 2 ;
	rf->TXRX1.bits.TXVGC = 0 ;
	rf->TXRX1.bits.PCONTROL = 0 ;
	rf->TXRX1.bits.RXDCFBBYPS = 0 ;
	/* PCNT1 */
	rf->PCNT1.bits.TX_DELAY = 0 ;
	rf->PCNT1.bits.PC_OFFSET = 0 ;
	rf->PCNT1.bits.P_DESIRED = 0 ;
	rf->PCNT1.bits.MID_BIAS = 0 ;
	/* PCNT2 */
	rf->PCNT2.bits.MIN_POWER = 0 ;
	rf->PCNT2.bits.MID_POWER = 0 ;
	rf->PCNT2.bits.MAX_POWER = 0 ;
	/* VCOT1 */
	rf->VCOT1.bits.AUX1 = 0 ;
	rf->VCOT1.bits.AUX = 0 ;
} ;


void WIFI_setRF_CNT(wifimac_t *wifi, u16 val)
{
//	printf("write rf cnt %04X\n", val);
	if (!wifi->rfIOStatus.bits.busy)
		wifi->rfIOCnt.val = val ;
}

void WIFI_setRF_DATA(wifimac_t *wifi, u16 val, u8 part)
{
//	printf("write rf data %04X %s part\n", val, (part?"high":"low"));
	if (!wifi->rfIOStatus.bits.busy)
	{
        rfIOData_t *rfreg = (rfIOData_t *)&wifi->RF;
		switch (wifi->rfIOCnt.bits.readOperation)
		{
			case 1: /* read from RF chip */
				/* low part of data is ignored on reads */
				/* on high part, the address is read, and the data at this is written back */
				if (part==1)
				{
					wifi->rfIOData.array16[part] = val ;
					if (wifi->rfIOData.bits.address > (sizeof(wifi->RF) / 4)) return ; /* out of bound */
					/* get content of the addressed register */
					wifi->rfIOData.bits.content = rfreg[wifi->rfIOData.bits.address].bits.content ;
				}
				break ;
			case 0: /* write to RF chip */
				wifi->rfIOData.array16[part] = val ;
				if (wifi->rfIOData.bits.address > (sizeof(wifi->RF) / 4)) return ; /* out of bound */
				/* the actual transfer is done on high part write */
				if (part==1)
				{
					switch (wifi->rfIOData.bits.address)
					{
						case 5:		/* write to upper part of the frequency filter */
						case 6:		/* write to lower part of the frequency filter */
							{
								u32 channelFreqN ;
								rfreg[wifi->rfIOData.bits.address].bits.content = wifi->rfIOData.bits.content ;
								/* get the complete rfpll.n */
								channelFreqN = (u32)wifi->RF.RFPLL3.bits.NUM2 + ((u32)wifi->RF.RFPLL2.bits.NUM2 << 18) + ((u32)wifi->RF.RFPLL2.bits.N2 << 24) ;
								/* frequency setting is out of range */
								if (channelFreqN<0x00A2E8BA) return ;
								/* substract base frequency (channel 1) */
								channelFreqN -= 0x00A2E8BA ;
							}
							return ;
						case 13:
							/* special purpose register: TEST1, on write, the RF chip resets */
							WIFI_resetRF(&wifi->RF) ;
							return ;
					}
					/* set content of the addressed register */
					rfreg[wifi->rfIOData.bits.address].bits.content = wifi->rfIOData.bits.content ;
				}
				break ;
		}
	}
}

u16 WIFI_getRF_DATA(wifimac_t *wifi, u8 part)
{
//	printf("read rf data %s part\n", (part?"high":"low"));
	if (!wifi->rfIOStatus.bits.busy)
	{
		return wifi->rfIOData.array16[part] ;
	} else
	{   /* data is not (yet) available */
		return 0 ;
	}
 }

u16 WIFI_getRF_STATUS(wifimac_t *wifi)
{
//	printf("read rf status\n");
	return wifi->rfIOStatus.val ;
}

/*******************************************************************************

	BB-Chip

 *******************************************************************************/

void WIFI_setBB_CNT(wifimac_t *wifi,u16 val)
{
	wifi->bbIOCnt.val = val;

	if(wifi->bbIOCnt.bits.mode == 1)
		wifi->BB.data[wifi->bbIOCnt.bits.address] = wifi->bbDataToWrite;
}

u8 WIFI_getBB_DATA(wifimac_t *wifi)
{
	if((!wifi->bbIOCnt.bits.enable) || (wifi->bbIOCnt.bits.mode != 2))
		return 0;

	return wifi->BB.data[wifi->bbIOCnt.bits.address];
//	if ((wifi->bbIOCnt.bits.mode != 2) || !(wifi->bbIOCnt.bits.enable)) return 0 ; /* not for read or disabled */
//	return wifi->BB.data[wifi->bbIOCnt.bits.address] ;
}

void WIFI_setBB_DATA(wifimac_t *wifi, u8 val)
{
	wifi->bbDataToWrite = (val & 0xFF);
//	if ((wifi->bbIOCnt.bits.mode != 1) || !(wifi->bbIOCnt.bits.enable)) return ; /* not for write or disabled */
//    wifi->BB.data[wifi->bbIOCnt.bits.address] = val ;
}

/*******************************************************************************

	wifimac IO: a lot of the wifi regs are action registers, that are mirrored
				without action, so the default IO via MMU.c does not seem to
				be very suitable

				all registers are 16 bit

 *******************************************************************************/

static void WIFI_triggerIRQMask(wifimac_t *wifi, u16 mask)
{
	u16 oResult,nResult ;
	oResult = wifi->IE.val & wifi->IF.val ;
	wifi->IF.val = wifi->IF.val | (mask & ~0x0400) ;
	nResult = wifi->IE.val & wifi->IF.val ;
	//printf("wifi: trigger irq: ie=%04X, if=%04X, mask=%08X, oldresult=%04X, newresult=%04X\n", 
	//	wifi->IE.val, wifi->IF.val, mask, oResult, nResult);
	if (!oResult && nResult)
	{
		NDS_makeARM7Int(24) ;   /* cascade it via arm7 wifi irq */
	}
}

static void WIFI_triggerIRQ(wifimac_t *wifi, u8 irq)
{
	WIFI_triggerIRQMask(wifi,1<<irq) ;
}


void WIFI_Init(wifimac_t *wifi)
{
	WIFI_initCRC32Table();

	WIFI_resetRF(&wifi->RF) ;
	wifi->netEnabled = false;
	if(driver->WIFI_Host_InitSystem())
	{
		wifi->netEnabled = true;
	}
	wifi->powerOn = FALSE;
	wifi->powerOnPending = FALSE;
	
	wifi->rfStatus = 0x0000;
	wifi->rfPins = 0x0004;
}

static void WIFI_RXPutWord(wifimac_t *wifi,u16 val)
{
//	printf("wifi: rx circbuf write attempt: rxcnt=%04X, rxread=%04X, rxwrite=%04X\n", 
//		wifi->RXCnt, wifi->RXReadCursor, wifi->RXHWWriteCursor);
	/* abort when RX data queuing is not enabled */
	if (!(wifi->RXCnt & 0x8000)) return ;
	/* abort when ringbuffer is full */
	//if (wifi->RXReadCursor == wifi->RXHWWriteCursor) return ;
	/*if(wifi->RXHWWriteCursor >= wifi->RXReadCursor) 
	{
		printf("WIFI: write cursor (%04X) above READCSR (%04X). Cannot write received packet.\n", 
			wifi->RXHWWriteCursor, wifi->RXReadCursor);
		return;
	}*/
	/* write the data to cursor position */
	wifi->circularBuffer[wifi->RXHWWriteCursor & 0xFFF] = val;
//	printf("wifi: written word %04X to circbuf addr %04X\n", val, (wifi->RXHWWriteCursor << 1));
	/* move cursor by one */
	//printf("written one word to %04X (start %04X, end %04X), ", wifi->RXHWWriteCursor, wifi->RXRangeBegin, wifi->RXRangeEnd);
	wifi->RXHWWriteCursor++ ;
	/* wrap around */
//	wifi->RXHWWriteCursor %= (wifi->RXRangeEnd - wifi->RXRangeBegin) >> 1 ;
//	printf("new addr=%04X\n", wifi->RXHWWriteCursor);
	if(wifi->RXHWWriteCursor >= ((wifi->RXRangeEnd & 0x1FFE) >> 1))
		wifi->RXHWWriteCursor = ((wifi->RXRangeBegin & 0x1FFE) >> 1);
}

static void WIFI_TXStart(wifimac_t *wifi,u8 slot)
{
	WIFI_LOG(3, "TX slot %i trying to send a packet: TXCnt = %04X, TXBufLoc = %04X\n", 
		slot, wifi->TXCnt, wifi->TXSlot[slot]);

	if (wifi->TXSlot[slot] & 0x8000)	/* is slot enabled? */
	{
		u16 txLen;
		/* the address has to be somewhere in the circular buffer, so drop the other bits */
		u16 address = (wifi->TXSlot[slot] & 0x0FFF);
		/* is there even enough space for the header (6 hwords) in the tx buffer? */
		if (address > 0x1000-6)
		{
			WIFI_LOG(1, "TX slot %i trying to send a packet overflowing from the TX buffer (address %04X). Attempt ignored.\n", 
				slot, (address << 1));
			return;
		}

		/* 12 byte header TX Header: http://www.akkit.org/info/dswifi.htm#FmtTx */
		txLen = wifi->circularBuffer[address+5];
		/* zero length */
		if (txLen == 0)
		{
			WIFI_LOG(1, "TX slot %i trying to send a packet with length field set to zero. Attempt ignored.\n", 
				slot);
			return;
		}
		/* unsupported txRate */
		switch (wifi->circularBuffer[address+4] & 0xFF)
		{
			case 10: /* 1 mbit */
			case 20: /* 2 mbit */
				break;
			default: /* other rates */
				WIFI_LOG(1, "TX slot %i trying to send a packet with transfer rate field set to an invalid value of %i. Attempt ignored.\n", 
					slot, wifi->circularBuffer[address+4] & 0xFF);
				return;
		}

		/* FIXME: calculate FCS */

		WIFI_triggerIRQ(wifi, WIFI_IRQ_SENDSTART) ;

		if(slot > wifi->txCurSlot)
			wifi->txCurSlot = slot;

		wifi->txSlotBusy[slot] = 1;
		wifi->txSlotAddr[slot] = address;
		wifi->txSlotLen[slot] = txLen;
		wifi->txSlotRemainingBytes[slot] = (txLen + 12);

		wifi->rfStatus = 0x0003;
		wifi->rfPins = 0x0046;

#if 0
		WIFI_SoftAP_RecvPacketFromDS(wifi, (u8*)&wifi->circularBuffer[address+6], txLen);
		WIFI_triggerIRQ(wifi, WIFI_IRQ_SENDCOMPLETE) ;

		wifi->circularBuffer[address] = 0x0001;
		wifi->circularBuffer[address+4] &= 0x00FF;
#endif
	}
} 

void WIFI_write16(wifimac_t *wifi,u32 address, u16 val)
{
	BOOL action = FALSE ;
	if (!(MMU_read32(ARMCPU_ARM7,REG_PWRCNT) & 0x0002)) return ;		/* access to wifi hardware was disabled */
	if ((address & 0xFF800000) != 0x04800000) return ;      /* error: the address does not describe a wifi register */

	WIFI_LOG(5, "Write at address %08X, %04X\n", address, val);

	/* the first 0x1000 bytes are mirrored at +0x1000,+0x2000,+0x3000,+06000,+0x7000 */
	/* only the first mirror causes an special action */
	/* the gap at +0x4000 is filled with the circular bufferspace */
	/* the so created 0x8000 byte block is then mirrored till 0x04FFFFFF */
	/* see: http://www.akkit.org/info/dswifi.htm#Wifihwcap */
	if (((address & 0x00007000) >= 0x00004000) && ((address & 0x00007000) < 0x00006000))
	{
		/* access to the circular buffer */
		address &= 0x1FFF ;
	//	printf("wifi: circbuf write at %04X, %04X, readcsr=%04X, wrcsr=%04X(%04X)\n", 
	//		address, val, wifi->RXReadCursor << 1, wifi->RXHWWriteCursorReg << 1, wifi->RXHWWriteCursor << 1);
	/*	if((address == 0x0BFE) && (val == 0x061E))
		{
			extern void emu_halt();
			emu_halt();
		}*/
        wifi->circularBuffer[address >> 1] = val ;
		return ;
	}
	if (!(address & 0x00007000)) action = TRUE ;
	/* mirrors => register address */
	address &= 0x00000FFF ;
	//if((address < 0x158) || ((address > 0x168) && (address < 0x17C)) || (address > 0x184))
	//	printf("wifi write at %08X, %04X\n", address, val);
/*	FILE *f = fopen("wifidbg.txt", "a");
	fprintf(f, "wifi write at %08X, %04X\n", address, val);
	fclose(f);*/
	switch (address)
	{
		case REG_WIFI_ID:
			break ;
		case REG_WIFI_MODE:
			if (val & 0x4000) 
			{
				/* does some resets */
				wifi->RXRangeBegin = 0x4000 ;
				/* this bit does not save */
				val &= ~0x4000 ;
			}

			if(val & 0x0001)
			{
				wifi->rfStatus = 0x0009;
				wifi->rfPins = 0x0004;
			}

			wifi->macMode = val ;
			break ;
		case REG_WIFI_WEP:
			wifi->wepMode = val ;
			break ;
		case REG_WIFI_IE:
			wifi->IE.val = val ;
			//printf("wifi ie write %04X\n", val);
			break ;
		case REG_WIFI_IF:
			wifi->IF.val &= ~val ;		/* clear flagging bits */
			break ;
		case REG_WIFI_MAC0:
		case REG_WIFI_MAC1:
		case REG_WIFI_MAC2:
			wifi->mac.words[(address - REG_WIFI_MAC0) >> 1] = val ;
			break ;
		case REG_WIFI_BSS0:
		case REG_WIFI_BSS1:
		case REG_WIFI_BSS2:
			wifi->bss.words[(address - REG_WIFI_BSS0) >> 1] = val ;
			break ;
		case REG_WIFI_RETRYLIMIT:
			wifi->retryLimit = val ;
			break ;
		case REG_WIFI_WEPCNT:
			wifi->WEP_enable = (val & 0x8000) != 0 ;
			break ;
		case REG_WIFI_POWERSTATE:
			wifi->powerOn = ((val & 0x0002)?TRUE:FALSE);
			if(wifi->powerOn) WIFI_triggerIRQ(wifi, 11);
			break;
		case REG_WIFI_FORCEPS:
			if((val & 0x8000) && (!wifi->powerOnPending))
			{
			/*	BOOL newPower = ((val & 0x0001)?FALSE:TRUE);
				if(newPower != wifi->powerOn)
				{
					if(!newPower)
						wifi->powerOn = FALSE;
					else
						wifi->powerOnPending = TRUE;
				}*/
				wifi->powerOn = ((val & 0x0001) ? FALSE : TRUE);
			}
			break;
		case REG_WIFI_POWERACK:
			if((val == 0x0000) && wifi->powerOnPending)
			{
				wifi->powerOn = TRUE;
				wifi->powerOnPending = FALSE;
			}
			break;
		case REG_WIFI_RXCNT:
			wifi->RXCnt = val;
			if(wifi->RXCnt & 0x0001)
			{
				wifi->RXHWWriteCursor = wifi->RXHWWriteCursorReg = wifi->RXHWWriteCursorLatched;
				//printf("wifi: write wrcsr %04X\n", wifi->RXHWWriteCursorReg);
			}
			break;
		case REG_WIFI_RXRANGEBEGIN:
			wifi->RXRangeBegin = val ;
			if(wifi->RXHWWriteCursor < ((val & 0x1FFE) >> 1))
				wifi->RXHWWriteCursor = ((val & 0x1FFE) >> 1);
			//printf("wifi: rx range begin=%04X\n", val);
			break ;
		case REG_WIFI_RXRANGEEND:
			wifi->RXRangeEnd = val ;
			if(wifi->RXHWWriteCursor >= ((val & 0x1FFE) >> 1))
				wifi->RXHWWriteCursor = ((wifi->RXRangeBegin & 0x1FFE) >> 1);
			//printf("wifi: rx range end=%04X\n", val);
			break ;
		case REG_WIFI_WRITECSRLATCH:
			if (action)        /* only when action register and CSR change enabled */
			{
				wifi->RXHWWriteCursorLatched = val ;
			}
			break ;
		case REG_WIFI_CIRCBUFRADR:
			wifi->CircBufReadAddress = (val & 0x1FFE);
			break ;
		case REG_WIFI_RXREADCSR:
			//printf("wifi: write readcsr: %04X\n", val << 1);
			wifi->RXReadCursor = val ;
			break ;
		case REG_WIFI_CIRCBUFWADR:
			wifi->CircBufWriteAddress = val ;
			break ;
		case REG_WIFI_CIRCBUFWRITE:
			/* set value into the circ buffer, and move cursor to the next hword on action */
			//printf("wifi: circbuf fifo write at %04X, %04X (action=%i)\n", (wifi->CircBufWriteAddress & 0x1FFF), val, action);
			wifi->circularBuffer[(wifi->CircBufWriteAddress >> 1) & 0xFFF] = val ;
			if (action)
			{
				/* move to next hword */
                wifi->CircBufWriteAddress+=2 ;
				if (wifi->CircBufWriteAddress == wifi->CircBufWrEnd)
				{
					/* on end of buffer, add skip hwords to it */
					wifi->CircBufWrEnd += wifi->CircBufWrSkip * 2 ;
				}
			}
			break ;
		case REG_WIFI_CIRCBUFWR_SKIP:
			wifi->CircBufWrSkip = val ;
			break ;
		case REG_WIFI_BEACONTRANS:
			wifi->BEACONSlot = val & 0x7FFF ;
			wifi->BEACON_enable = (val & 0x8000) != 0 ;
			//printf("wifi beacon: enable=%s, addr=%04X\n", (wifi->BEACON_enable?"yes":"no"), (wifi->BEACONSlot<<1));
			break ;
		case REG_WIFI_TXLOC1:
		case REG_WIFI_TXLOC2:
		case REG_WIFI_TXLOC3:
			wifi->TXSlot[(address - REG_WIFI_TXLOC1) >> 2] = val ;
			break ;
		case REG_WIFI_TXOPT:
#if 0
			if (val == 0xFFFF)
			{
				/* reset TX logic */
				/* CHECKME */
            //    wifi->TXSlot[0] = 0 ; wifi->TXSlot[1] = 0 ; wifi->TXSlot[2] = 0 ;
                wifi->TXOpt = 0 ;
                wifi->TXCnt = 0 ;
			} else
			{
				wifi->TXOpt = val ;
			}
#endif
			wifi->TXCnt &= ~val;
			break ;
		case REG_WIFI_TXCNT:
			wifi->TXCnt |= val ;
			if (val & 0x01)	WIFI_TXStart(wifi,0) ;
			if (val & 0x04)	WIFI_TXStart(wifi,1) ;
			if (val & 0x08)	WIFI_TXStart(wifi,2) ;
		/*	if(val&0x04)
			{
				extern void emu_halt();
				emu_halt();
			}*/
			break ;
		case REG_WIFI_RFIOCNT:
			WIFI_setRF_CNT(wifi,val) ;
			break ;
		case REG_WIFI_RFIOBSY:
			/* CHECKME: read only? */
			break ;
		case REG_WIFI_RFIODATA1:
			WIFI_setRF_DATA(wifi,val,0) ;
			break ;
		case REG_WIFI_RFIODATA2:
			WIFI_setRF_DATA(wifi,val,1) ;
			break ;
		case REG_WIFI_USCOUNTERCNT:
			wifi->usecEnable = (val & 1)==1 ;
			break ;
		case REG_WIFI_USCOMPARECNT:
			wifi->ucmpEnable = (val & 1)==1 ;
			break ;
		case REG_WIFI_BBSIOCNT:
            WIFI_setBB_CNT(wifi,val) ;
			break ;
		case REG_WIFI_BBSIOWRITE:
            WIFI_setBB_DATA(wifi,val) ;
			break ;
		case REG_WIFI_RXBUF_COUNT:
			wifi->RXBufCount = val & 0x0FFF ;
			break ;
		case REG_WIFI_EXTRACOUNTCNT:
			wifi->eCountEnable = (val & 0x0001) ;
			break ;
		case REG_WIFI_EXTRACOUNT:
			wifi->eCount = val ;
			break ;
		case REG_WIFI_POWER_US:
			wifi->crystalEnabled = !(val & 0x0001) ;
			break ;
		case REG_WIFI_IF_SET:
			WIFI_triggerIRQMask(wifi,val) ;
			break ;
		case REG_WIFI_CIRCBUFRD_END:
			wifi->CircBufRdEnd = (val & 0x1FFE) ;
			break ;
		case REG_WIFI_CIRCBUFRD_SKIP:
			wifi->CircBufRdSkip = val & 0xFFF ;
			break ;
		case REG_WIFI_AID_LOW:
			wifi->pid = val & 0x0F ;
			break ;
		case REG_WIFI_AID_HIGH:
			wifi->aid = val & 0x07FF ;
			break ;
		//case 0x36:
		//case 0x38:
		//case 0x3C:
		//case 0x40:
		//	printf("wifi power reg %03X write %04X\n", address, val);
		//	break;
		case 0xD0:
		//	printf("wifi: rxfilter=%04X\n", val);
			break;
		default:
		//	printf("wifi: write unhandled reg %03X, %04X\n", address, val);
		//	val = 0 ;       /* not handled yet */
			break ;
	}

	wifi->ioMem[address >> 1] = val;
}
//int packet_arrived=0;
u16 WIFI_read16(wifimac_t *wifi,u32 address)
{
	BOOL action = FALSE ;
	u16 temp ;
	if (!(MMU_read32(ARMCPU_ARM7,REG_PWRCNT) & 0x0002)) return 0 ;		/* access to wifi hardware was disabled */
	if ((address & 0xFF800000) != 0x04800000) return 0 ;      /* error: the address does not describe a wifi register */

	WIFI_LOG(5, "Read at address %08X\n", address);

	/* the first 0x1000 bytes are mirrored at +0x1000,+0x2000,+0x3000,+06000,+0x7000 */
	/* only the first mirror causes an special action */
	/* the gap at +0x4000 is filled with the circular bufferspace */
	/* the so created 0x8000 byte block is then mirrored till 0x04FFFFFF */
	/* see: http://www.akkit.org/info/dswifi.htm#Wifihwcap */
	if (((address & 0x00007000) >= 0x00004000) && ((address & 0x00007000) < 0x00006000))
	{
		/* access to the circular buffer */
	//	printf("wifi: circbuf read at %04X, %04X, readcsr=%04X, wrcsr=%04X(%04X)\n", 
		//	address, wifi->circularBuffer[(address & 0x1FFF) >> 1], wifi->RXReadCursor << 1, wifi->RXHWWriteCursorReg << 1, wifi->RXHWWriteCursor << 1);
		/*if(((address == 0x04804C38) || 
			(address == 0x04804C04)) && 
			(wifi->circularBuffer[(address & 0x1FFF) >> 1] != 0x0000) && 
			(wifi->circularBuffer[(address & 0x1FFF) >> 1] != 0x5A5A) &&
			(wifi->circularBuffer[(address & 0x1FFF) >> 1] != 0xA5A5))
		{
			extern void emu_halt();
			emu_halt();
		}*/

		// hax
	/*	if(packet_arrived)
		{
			if(address == 0x04804C38)
				return 0x0010;
			if(address == 0x04804C3E)
				return 0x0014;

			if(address == 0x048047D0)
				return 0x0010;
			if(address == 0x048047D6)
				return 0x0014;
		}*/

        return wifi->circularBuffer[(address & 0x1FFF) >> 1] ;
	}
	if (!(address & 0x00007000)) action = TRUE ;
	//if((address != 0x04808214) && (address != 0x0480803C) && (address != 0x048080F8) && (address != 0x048080FA) && (address != 0x0480819C))
	/* mirrors => register address */
	address &= 0x00000FFF ;
//	if((address < 0x158) || ((address > 0x168) && (address < 0x17C)) || (address > 0x184))
//		printf("wifi read at %08X\n", address);
/*	FILE *f = fopen("wifidbg.txt", "a");
	fprintf(f, "wifi read at %08X\n", address);
	fclose(f);*/
	switch (address)
	{
		case REG_WIFI_ID:
			return WIFI_CHIPID ;
		case REG_WIFI_MODE:
			return wifi->macMode ;
		case REG_WIFI_WEP:
			return wifi->wepMode ;
		case REG_WIFI_IE:
			//printf("wifi: read ie (%04X)\n", wifi->IE.val);
			return wifi->IE.val ;
		case REG_WIFI_IF:
			//printf("wifi: read if (%04X)\n", wifi->IF.val);
			return wifi->IF.val ;
		case REG_WIFI_POWERSTATE:
			return ((wifi->powerOn ? 0x0000 : 0x0200) | (wifi->powerOnPending ? 0x0102 : 0x0000));
		case REG_WIFI_RFIODATA1:
			return WIFI_getRF_DATA(wifi,0) ;
		case REG_WIFI_RFIODATA2:
			return WIFI_getRF_DATA(wifi,1) ;
		case REG_WIFI_RFIOBSY:
		case REG_WIFI_BBSIOBUSY:
			return 0 ;	/* we are never busy :p */
		case REG_WIFI_BBSIOREAD:
			return WIFI_getBB_DATA(wifi) ;
		case REG_WIFI_RANDOM:
			/* FIXME: random generator */
			return (rand() & 0x7FF);
			return 0 ;
		case REG_WIFI_MAC0:
		case REG_WIFI_MAC1:
		case REG_WIFI_MAC2:
			//printf("read mac addr: word %i = %02X\n", (address - REG_WIFI_MAC0) >> 1, wifi->mac.words[(address - REG_WIFI_MAC0) >> 1]);
			return wifi->mac.words[(address - REG_WIFI_MAC0) >> 1] ;
		case REG_WIFI_BSS0:
		case REG_WIFI_BSS1:
		case REG_WIFI_BSS2:
			//printf("read bssid addr: word %i = %02X\n", (address - REG_WIFI_BSS0) >> 1, wifi->bss.words[(address - REG_WIFI_BSS0) >> 1]);
			return wifi->bss.words[(address - REG_WIFI_BSS0) >> 1] ;
		case REG_WIFI_RXCNT:
			return wifi->RXCnt;
		case REG_WIFI_RXRANGEBEGIN:
			return wifi->RXRangeBegin ;
		case REG_WIFI_CIRCBUFREAD:
			temp = wifi->circularBuffer[((wifi->RXRangeBegin + wifi->CircBufReadAddress) >> 1) & 0x0FFF] ;
			//printf("wifi: circbuf fifo read at %04X, action=%i\n", (wifi->RXRangeBegin + wifi->CircBufReadAddress), action);
			if (action)
			{
				wifi->CircBufReadAddress += 2 ;
				wifi->CircBufReadAddress &= 0x1FFE ;
				if (wifi->CircBufReadAddress + wifi->RXRangeBegin == wifi->RXRangeEnd) 
				{ 
					wifi->CircBufReadAddress = 0 ;
				} else
				{
					/* skip does not fire after a reset */
					if (wifi->CircBufReadAddress == wifi->CircBufRdEnd)
					{
						wifi->CircBufReadAddress += wifi->CircBufRdSkip * 2 ;
						wifi->CircBufReadAddress &= 0x1FFE ;
						if (wifi->CircBufReadAddress + wifi->RXRangeBegin == wifi->RXRangeEnd) wifi->CircBufReadAddress = 0 ;
					}
				}
				if (wifi->RXBufCount > 0)
				{
					if (wifi->RXBufCount == 1)
					{
						WIFI_triggerIRQ(wifi,9) ;
					}
					wifi->RXBufCount-- ;
				}				
			}
			return temp;
		case REG_WIFI_CIRCBUFRADR:
			return wifi->CircBufReadAddress ;
		case REG_WIFI_RXHWWRITECSR:
			//printf("wifi: read writecsr (%04X)\n", wifi->RXHWWriteCursorReg);
			return wifi->RXHWWriteCursorReg;
			//return wifi->RXReadCursor;
		case REG_WIFI_RXREADCSR:
			//printf("wifi: read readcsr (%04X)\n", wifi->RXReadCursor);
			return wifi->RXReadCursor;
			//return wifi->RXHWWriteCursorReg;
		case REG_WIFI_RXBUF_COUNT:
			return wifi->RXBufCount ;
		case REG_WIFI_TXREQ_READ:
			return wifi->TXCnt;
		case REG_WIFI_TXBUSY:
			return ((wifi->txSlotBusy[0] ? 0x01 : 0x00) | (wifi->txSlotBusy[1] ? 0x04 : 0x00) | (wifi->txSlotBusy[2] ? 0x08 : 0x00));
		case REG_WIFI_TXSTAT:
			return wifi->TXStat;
		case REG_WIFI_EXTRACOUNTCNT:
			return wifi->eCountEnable?1:0 ;
		case REG_WIFI_EXTRACOUNT:
			return wifi->eCount ;
		case REG_WIFI_USCOMPARE0:
			return (wifi->usec & 0xFFFF);
		case REG_WIFI_USCOMPARE1:
			return ((wifi->usec >> 16) & 0xFFFF);
		case REG_WIFI_USCOMPARE2:
			return ((wifi->usec >> 32) & 0xFFFF);
		case REG_WIFI_USCOMPARE3:
			return ((wifi->usec >> 48) & 0xFFFF);
		case REG_WIFI_POWER_US:
			return wifi->crystalEnabled?0:1 ;
		case REG_WIFI_CIRCBUFRD_END:
			return wifi->CircBufRdEnd ;
		case REG_WIFI_CIRCBUFRD_SKIP:
			return wifi->CircBufRdSkip ;
		case REG_WIFI_AID_LOW:
			return wifi->pid ;
		case REG_WIFI_AID_HIGH:
			return wifi->aid ;
		case 0x214:
			//printf("wifi: read rfstatus (%04X)\n", wifi->rfStatus);
			return 0x0009;
			//return wifi->rfStatus;
		case 0x19C:
			//assert(false); //luigi, please pick something to return from here
			//printf("wifi: read rfpins (%04X)\n", wifi->rfPins);
			//return wifi->rfPins;
			return 0x0004;
		default:
		//	printf("wifi: read unhandled reg %03X\n", address);
			return wifi->ioMem[address >> 1];
	}
}


void WIFI_usTrigger(wifimac_t *wifi)
{
	u8 dataBuffer[0x2000] ; 
	u16 rcvSize ;
	if (wifi->crystalEnabled)
	{
		/* a usec (=3F03 cycles) has passed */
		if (wifi->usecEnable)
			wifi->usec++ ;
		if (wifi->eCountEnable)
		{
			if (wifi->eCount > 0)
			{
				wifi->eCount-- ;
			}
		}
	}
	if ((wifi->ucmpEnable) && (wifi->ucmp == wifi->usec))
	{
			WIFI_triggerIRQ(wifi, WIFI_IRQ_TIMEBEACON) ;
	}

	if((wifi->usec & 3) == 0)
	{
		int slot = wifi->txCurSlot;

		if(wifi->txSlotBusy[slot])
		{
			wifi->txSlotRemainingBytes[slot]--;
			if(wifi->txSlotRemainingBytes[slot] == 0)
			{
				wifi->txSlotBusy[slot] = 0;

				WIFI_SoftAP_RecvPacketFromDS(wifi, (u8*)&wifi->circularBuffer[wifi->txSlotAddr[slot]], wifi->txSlotLen[slot]);

				while((wifi->txSlotBusy[wifi->txCurSlot] == 0) && (wifi->txCurSlot > 0))
					wifi->txCurSlot--;

				wifi->circularBuffer[wifi->txSlotAddr[slot]] = 0x0001;
				wifi->circularBuffer[wifi->txSlotAddr[slot]+4] &= 0x00FF;

				wifi->TXStat = (0x0001 | (slot << 12));

				WIFI_triggerIRQ(wifi, WIFI_IRQ_SENDCOMPLETE);
				
				//wifi->rfStatus = 0x0001;
				//wifi->rfPins = 0x0084;
				wifi->rfStatus = 0x0009;
				wifi->rfPins = 0x0004;

				WIFI_LOG(3, "TX slot %i finished sending its packet. Next is slot %i. TXStat = %04X\n", 
					slot, wifi->txCurSlot, wifi->TXStat);
			}
		}
	}
}

/*******************************************************************************

	SoftAP (fake wifi access point)

 *******************************************************************************/

u8 SoftAP_MACAddr[6] = {0x00, 0xF0, 0x1A, 0x2B, 0x3C, 0x4D};

u8 SoftAP_Beacon[] = {
	/* 802.11 header */
	0x80, 0x00,											// Frame control
	0x00, 0x00,											// Duration ID
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,					// Receiver
	0x00, 0xF0, 0x1A, 0x2B, 0x3C, 0x4D,					// Sender
	0x00, 0xF0, 0x1A, 0x2B, 0x3C, 0x4D,					// BSSID
	0x00, 0x00,											// Sequence control

	/* Frame body */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		// Timestamp
	0x64, 0x00,											// Beacon interval
	0x0F, 0x00,											// Capablilty information
	0x00, 0x06, 'S', 'o', 'f', 't', 'A', 'P',			// SSID
	0x01, 0x02, 0x82, 0x84,								// Supported rates
	0x05, 0x04, 0x00, 0x00, 0x00, 0x00,					// TIM
	
	/* CRC32 */
	0x00, 0x00, 0x00, 0x00
};

u8 SoftAP_ProbeResponse[] = {
	/* 802.11 header */
	0x50, 0x00,											// Frame control
	0x00, 0x00,											// Duration ID
	0x00, 0x09, 0xBF, 0x12, 0x34, 0x56,					// Receiver
	0x00, 0xF0, 0x1A, 0x2B, 0x3C, 0x4D,					// Sender
	0x00, 0xF0, 0x1A, 0x2B, 0x3C, 0x4D,					// BSSID
	0x00, 0x00,											// Sequence control

	/* Frame body */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		// Timestamp
	0x64, 0x00,											// Beacon interval
	0x01, 0x00,											// Capablilty information
	0x00, 0x06, 'S', 'o', 'f', 't', 'A', 'P',			// SSID
	0x01, 0x02, 0x82, 0x84,								// Supported rates
	
	/* CRC32 */
	0x00, 0x00, 0x00, 0x00
};

u8 SoftAP_AuthFrame[] = {
	/* 802.11 header */
	0xB0, 0x00,											// Frame control
	0x00, 0x00,											// Duration ID
	0x00, 0x09, 0xBF, 0x12, 0x34, 0x56,					// Receiver
	0x00, 0xF0, 0x1A, 0x2B, 0x3C, 0x4D,					// Sender
	0x00, 0xF0, 0x1A, 0x2B, 0x3C, 0x4D,					// BSSID
	0x00, 0x00,											// Sequence control

	/* Frame body */
	0x00, 0x00,											// Authentication algorithm
	0x02, 0x00,											// Authentication sequence
	0x00, 0x00,											// Status
	
	/* CRC32 */
	0x00, 0x00, 0x00, 0x00
};

u8 SoftAP_AssocResponse[] = {
	/* 802.11 header */
	0x10, 0x00,											// Frame control
	0x00, 0x00,											// Duration ID
	0x00, 0x09, 0xBF, 0x12, 0x34, 0x56,					// Receiver
	0x00, 0xF0, 0x1A, 0x2B, 0x3C, 0x4D,					// Sender
	0x00, 0xF0, 0x1A, 0x2B, 0x3C, 0x4D,					// BSSID
	0x00, 0x00,											// Sequence control

	/* Frame body */
	0x0F, 0x00,											// Capability information
	0x00, 0x00,											// Status
	0x01, 0xC0,											// Assocation ID
	0x01, 0x02, 0x82, 0x84,								// Supported rates
	
	/* CRC32 */
	0x00, 0x00, 0x00, 0x00
};

//todo - make a class to wrap this
//todo - zeromus - inspect memory leak safety of all this
static pcap_if_t * WIFI_index_device(pcap_if_t *alldevs, int index) {
	pcap_if_t *curr = alldevs;
	for(int i=0;i<index;i++)
		curr = curr->next;
	return curr;
}

int WIFI_SoftAP_Init(wifimac_t *wifi)
{
	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_if_t *alldevs;

	wifi->SoftAP.usecCounter = 0;

	wifi->SoftAP.curPacket = NULL;
	wifi->SoftAP.curPacketSize = 0;
	wifi->SoftAP.curPacketPos = 0;
	wifi->SoftAP.curPacketSending = FALSE;
	
	if(wifiMac.netEnabled)
	{
		if(desmume_pcap_findalldevs(&alldevs, errbuf) == -1)
		{
			printf("SoftAP: PCAP error with pcap_findalldevs_ex(): %s\n", errbuf);
			return 0;
		}

		wifi->SoftAP.bridge = desmume_pcap_open(WIFI_index_device(alldevs,CommonSettings.wifiBridgeAdapterNum)->name, PACKET_SIZE, 0, 1, errbuf);
		if(wifi->SoftAP.bridge == NULL)
		{
			printf("SoftAP: PCAP error with pcap_open(): %s\n", errbuf);
			return 0;
		}

		desmume_pcap_freealldevs(alldevs);
	}

	return 1;
}

void WIFI_SoftAP_Shutdown(wifimac_t *wifi)
{
	if(wifiMac.netEnabled)
	{
	if(wifi->SoftAP.bridge != NULL)
		desmume_pcap_close(wifi->SoftAP.bridge);
	}

	if(wifi->SoftAP.curPacket)
		delete wifi->SoftAP.curPacket;
}

static void WIFI_SoftAP_MakeRXHeader(wifimac_t *wifi, u16 flags, u16 xferRate, u16 len, u8 maxRSSI, u8 minRSSI)
{
	*(u16*)&wifi->SoftAP.curPacket[0] = flags;

	// Unknown (usually 0x0400)
	wifi->SoftAP.curPacket[2] = 0x40;
	wifi->SoftAP.curPacket[3] = 0x00;

	// Time since last packet??? Random??? Left unchanged???
	wifi->SoftAP.curPacket[4] = 0x01;
	wifi->SoftAP.curPacket[5] = 0x00;

	*(u16*)&wifi->SoftAP.curPacket[6] = xferRate;

	*(u16*)&wifi->SoftAP.curPacket[8] = WIFI_alignedLen(len);

	wifi->SoftAP.curPacket[10] = maxRSSI;
	wifi->SoftAP.curPacket[11] = minRSSI;
}

void WIFI_SoftAP_RecvPacketFromDS(wifimac_t *wifi, u8 *packet, int len)
{
	int alignedLen = WIFI_alignedLen(len);
	u16 frameCtl = *(u16*)&packet[12];

	WIFI_LOG(3, "SoftAP: Received a packet of length %i bytes (%i aligned). Frame control = %04X\n",
		len, alignedLen, frameCtl);

	switch((frameCtl >> 2) & 0x3)
	{
	case 0x0:				// Management
		{
			switch((frameCtl >> 4) & 0xF)
			{
			case 0x4:		// Probe request (Nintendo WFC config util)
				{
					int packetLen = sizeof(SoftAP_ProbeResponse);
					int totalLen = (packetLen + 12);

					wifi->SoftAP.curPacket = new u8[totalLen];

					// Make the RX header
					// About the packet length: 
					// GBATek says the length entry of the RX header is the length of the IEEE
					// header and frame body, without including the FCS. But the Nintendo WFC
					// config util expects this length to be the length of the IEEE header and
					// the frame body AND the FCS. Actually, it expects WRCSR to be equal to
					// (READCSR + 12 + packet_length).
					WIFI_SoftAP_MakeRXHeader(wifi, 0x0010, 20, packetLen, 0, 0);

					// Copy the probe response template
					memcpy(&wifi->SoftAP.curPacket[12], SoftAP_ProbeResponse, packetLen);

					// Add the MAC address
					memcpy(&wifi->SoftAP.curPacket[12 + 4], FW_Mac, 6);

					// The timestamp
					u64 timestamp = (wifi->SoftAP.usecCounter / 1000);		// FIXME: is it correct?
					*(u64*)&wifi->SoftAP.curPacket[12 + 24] = timestamp;

					// And the CRC32 (FCS)
					u32 crc32 = WIFI_getCRC32(&wifi->SoftAP.curPacket[12], (packetLen - 4));
					*(u32*)&wifi->SoftAP.curPacket[12 + packetLen - 4] = crc32;

					// Let's prepare to send
					wifi->SoftAP.curPacketSize = totalLen;
					wifi->SoftAP.curPacketPos = 0;
					wifi->SoftAP.curPacketSending = TRUE;
				}
				break;

			case 0xB:		// Authentication
				{
					int packetLen = sizeof(SoftAP_AuthFrame);
					int totalLen = (packetLen + 12);

					wifi->SoftAP.curPacket = new u8[totalLen];

					// Make the RX header
					WIFI_SoftAP_MakeRXHeader(wifi, 0x0010, 20, packetLen, 0, 0);

					// Copy the authentication frame template
					memcpy(&wifi->SoftAP.curPacket[12], SoftAP_AuthFrame, packetLen);

					// Add the MAC address
					memcpy(&wifi->SoftAP.curPacket[12 + 4], FW_Mac, 6);

					// And the CRC32 (FCS)
					u32 crc32 = WIFI_getCRC32(&wifi->SoftAP.curPacket[12], (packetLen - 4));
					*(u32*)&wifi->SoftAP.curPacket[12 + packetLen - 4] = crc32;

					// Let's prepare to send
					wifi->SoftAP.curPacketSize = totalLen;
					wifi->SoftAP.curPacketPos = 0;
					wifi->SoftAP.curPacketSending = TRUE;
				}
				break;

			case 0x0:		// Association request
				{
					int packetLen = sizeof(SoftAP_AssocResponse);
					int totalLen = (packetLen + 12);

					wifi->SoftAP.curPacket = new u8[totalLen];

					// Make the RX header
					WIFI_SoftAP_MakeRXHeader(wifi, 0x0010, 20, packetLen, 0, 0);

					// Copy the association response template
					memcpy(&wifi->SoftAP.curPacket[12], SoftAP_AssocResponse, packetLen);

					// Add the MAC address
					memcpy(&wifi->SoftAP.curPacket[12 + 4], FW_Mac, 6);

					// And the CRC32 (FCS)
					u32 crc32 = WIFI_getCRC32(&wifi->SoftAP.curPacket[12], (packetLen - 4));
					*(u32*)&wifi->SoftAP.curPacket[12 + packetLen - 4] = crc32;

					// Let's prepare to send
					wifi->SoftAP.curPacketSize = totalLen;
					wifi->SoftAP.curPacketPos = 0;
					wifi->SoftAP.curPacketSending = TRUE;
				}
				break;
			}
		}
		break;

	case 0x2:				// Data
		{
			// We convert the packet into an Ethernet packet

			int eflen = (alignedLen - 4 - 30 + 14);
			u8 *ethernetframe = new u8[eflen];

			// Destination address
			ethernetframe[0] = packet[28];
			ethernetframe[1] = packet[29];
			ethernetframe[2] = packet[30];
			ethernetframe[3] = packet[31];
			ethernetframe[4] = packet[32];
			ethernetframe[5] = packet[33];

			// Source address
			ethernetframe[6] = packet[22];
			ethernetframe[7] = packet[23];
			ethernetframe[8] = packet[24];
			ethernetframe[9] = packet[25];
			ethernetframe[10] = packet[26];
			ethernetframe[11] = packet[27];

			// EtherType
			ethernetframe[12] = packet[42];
			ethernetframe[13] = packet[43];

			// Frame body
			memcpy((ethernetframe + 14), (packet + 44), (alignedLen - 30 - 4));

			// Checksum 
			// TODO ?

			if(wifi->netEnabled) //dont try to pcap out the packet unless network is enabled
				desmume_pcap_sendpacket(wifi->SoftAP.bridge, ethernetframe, eflen);

			delete ethernetframe;
		}
		break;
	}
}

static void WIFI_SoftAP_SendBeacon(wifimac_t *wifi)
{
	int packetLen = sizeof(SoftAP_Beacon);
	int totalLen = (packetLen + 12);

	if(wifi->SoftAP.curPacket) delete wifi->SoftAP.curPacket;
	wifi->SoftAP.curPacket = new u8[totalLen];

	// Make the RX header
	WIFI_SoftAP_MakeRXHeader(wifi, 0x0011, 20, packetLen, 0, 0);

	// Copy the beacon template
	memcpy(&wifi->SoftAP.curPacket[12], SoftAP_Beacon, packetLen);

	// Add the timestamp
	u64 timestamp = (wifi->SoftAP.usecCounter / 1000);		// FIXME: is it correct?
	*(u64*)&wifi->SoftAP.curPacket[12 + 24] = timestamp;

	// And the CRC32 (FCS)
	u32 crc32 = WIFI_getCRC32(&wifi->SoftAP.curPacket[12], (packetLen - 4));
	*(u32*)&wifi->SoftAP.curPacket[12 + packetLen - 4] = crc32;

	// Let's prepare to send
	wifi->SoftAP.curPacketSize = totalLen;
	wifi->SoftAP.curPacketPos = 0;
	wifi->SoftAP.curPacketSending = TRUE;
}

void WIFI_SoftAP_usTrigger(wifimac_t *wifi)
{
	wifi->SoftAP.usecCounter++;

	if(!wifi->SoftAP.curPacketSending)
	{
		//if(wifi->ioMem[0xD0 >> 1] & 0x0400)
		{
			if((wifi->SoftAP.usecCounter % 100000) == 0)
			{
				WIFI_SoftAP_SendBeacon(wifi);
			}
		}
	}

	/* Given a connection of 2 megabits per second, */
	/* we take ~4 microseconds to transfer a byte, */
	/* ie ~8 microseconds to transfer a word. */
	if((wifi->SoftAP.curPacketSending) && !(wifi->SoftAP.usecCounter & 7))
	{
		if(wifi->SoftAP.curPacketPos >= 0)
		{
			if(wifi->SoftAP.curPacketPos == 0)
			{
				WIFI_triggerIRQ(wifi, WIFI_IRQ_RECVSTART);

				wifi->rfStatus = 0x0009;
				wifi->rfPins = 0x0004;

			//	printf("wifi: softap: started sending packet at %04X\n", wifi->RXHWWriteCursor << 1);
			//	wifi->SoftAP.curPacketPos += 2;
			}
			else
			{
				wifi->rfStatus = 0x0001;
				wifi->rfPins = 0x0084;
			}

			u16 word = (wifi->SoftAP.curPacket[wifi->SoftAP.curPacketPos] | (wifi->SoftAP.curPacket[wifi->SoftAP.curPacketPos+1] << 8));
			WIFI_RXPutWord(wifi, word);
		}

		wifi->SoftAP.curPacketPos += 2;
		if(wifi->SoftAP.curPacketPos >= wifi->SoftAP.curPacketSize)
		{
			wifi->SoftAP.curPacketSize = 0;
			wifi->SoftAP.curPacketPos = 0;
			wifi->SoftAP.curPacketSending = FALSE;

			delete wifi->SoftAP.curPacket;
			wifi->SoftAP.curPacket = NULL;

			wifi->RXHWWriteCursorReg = ((wifi->RXHWWriteCursor + 1) & (~1));

			WIFI_triggerIRQ(wifi, WIFI_IRQ_RECVCOMPLETE);

			// hax
			//packet_arrived=1;

			//printf("wifi: softap: finished sending packet. end at %04X (aligned=%04X)\n", wifi->RXHWWriteCursor << 1, wifi->RXHWWriteCursorReg << 1);
		}
	}
}

#endif
