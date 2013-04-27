/*
	Copyright (C) 2007 Tim Seidel
	Copyright (C) 2008-2011 DeSmuME team

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

#ifndef WIFI_H
#define WIFI_H

#include <stdio.h>
#include "types.h"

#include <queue>

#ifdef EXPERIMENTAL_WIFI_COMM

#define HAVE_REMOTE
#define WPCAP
#define PACKET_SIZE 65535
#define _INC_STDIO

#endif


#define		REG_WIFI_ID					0x000
#define     REG_WIFI_MODE       		0x004
#define     REG_WIFI_WEP        		0x006
#define		REG_WIFI_TXSTATCNT			0x008
#define		REG_WIFI_0A					0x00A
#define     REG_WIFI_IF     			0x010
#define     REG_WIFI_IE     			0x012
#define     REG_WIFI_MAC0       		0x018
#define     REG_WIFI_MAC1       		0x01A
#define     REG_WIFI_MAC2       		0x01C
#define     REG_WIFI_BSS0       		0x020
#define     REG_WIFI_BSS1       		0x022
#define     REG_WIFI_BSS2       		0x024
#define     REG_WIFI_AID_LOW       		0x028
#define     REG_WIFI_AID_HIGH      		0x02A
#define     REG_WIFI_RETRYLIMIT 		0x02C
// 02E - internal
#define		REG_WIFI_RXCNT				0x030
#define		REG_WIFI_WEPCNT				0x032
// 034 - internal

#define		REG_WIFI_POWER_US			0x036
#define		REG_WIFI_POWER_TX			0x038
#define     REG_WIFI_POWERSTATE 		0x03C
#define     REG_WIFI_POWERFORCE    		0x040
#define     REG_WIFI_RANDOM     		0x044
#define		REG_WIFI_POWER_UNK			0x048

#define     REG_WIFI_RXRANGEBEGIN       0x050
#define     REG_WIFI_RXRANGEEND         0x052
#define     REG_WIFI_RXHWWRITECSR       0x054
#define     REG_WIFI_WRITECSRLATCH      0x056
#define     REG_WIFI_CIRCBUFRADR        0x058
#define     REG_WIFI_RXREADCSR          0x05A
#define     REG_WIFI_RXBUF_COUNT		0x05C
#define     REG_WIFI_CIRCBUFREAD        0x060
#define     REG_WIFI_CIRCBUFRD_END      0x062
#define     REG_WIFI_CIRCBUFRD_SKIP     0x064
#define     REG_WIFI_CIRCBUFWADR        0x068
#define		REG_WIFI_TXBUFCOUNT			0x06C
#define     REG_WIFI_CIRCBUFWRITE       0x070
#define     REG_WIFI_CIRCBUFWR_END      0x074
#define     REG_WIFI_CIRCBUFWR_SKIP     0x076

// 078 - internal
#define     REG_WIFI_TXBUF_BEACON       0x080
#define     REG_WIFI_LISTENCOUNT        0x088
#define     REG_WIFI_BEACONPERIOD       0x08C
#define     REG_WIFI_LISTENINT          0x08E
#define 	REG_WIFI_TXBUF_CMD			0x090
#define		REG_WIFI_TXBUF_REPLY1		0x094
#define		REG_WIFI_TXBUF_REPLY2		0x098
// 09C - internal
#define     REG_WIFI_TXBUF_LOC1         0x0A0
#define     REG_WIFI_TXBUF_LOC2         0x0A4
#define     REG_WIFI_TXBUF_LOC3         0x0A8
#define     REG_WIFI_TXREQ_RESET        0x0AC
#define     REG_WIFI_TXREQ_SET          0x0AE
#define		REG_WIFI_TXREQ_READ			0x0B0
#define		REG_WIFI_TXRESET			0x0B4
#define		REG_WIFI_TXBUSY				0x0B6
#define     REG_WIFI_TXSTAT             0x0B8
// 0BA - internal
#define		REG_WIFI_PREAMBLE			0x0BC
// 0C0 - ?
// 0C4 - ?
// 0C8 - internal
#define     REG_WIFI_RXFILTER           0x0D0
// 0D4 - config
// 0D8 - config
// 0DA - config
#define		REG_WIFI_RXFILTER2			0x0E0

#define     REG_WIFI_USCOUNTERCNT       0x0E8
#define     REG_WIFI_USCOMPARECNT       0x0EA
// 0EC - config
#define		REG_WIFI_EXTRACOUNTCNT		0x0EE
#define     REG_WIFI_USCOMPARE0         0x0F0
#define     REG_WIFI_USCOMPARE1         0x0F2
#define     REG_WIFI_USCOMPARE2         0x0F4
#define     REG_WIFI_USCOMPARE3         0x0F6
#define     REG_WIFI_USCOUNTER0         0x0F8
#define     REG_WIFI_USCOUNTER1         0x0FA
#define     REG_WIFI_USCOUNTER2         0x0FC
#define     REG_WIFI_USCOUNTER3         0x0FE
// 100 - internal
// 102 - internal
// 104 - internal
// 106 - internal
#define		REG_WIFI_CONTENTFREE		0x10C
#define		REG_WIFI_PREBEACONCOUNT		0x110
#define		REG_WIFI_EXTRACOUNT			0x118
#define 	REG_WIFI_BEACONCOUNT1		0x11C

// 120 to 132 - config/internal ports
#define 	REG_WIFI_BEACONCOUNT2		0x134
// 140 to 154 - config ports

#define     REG_WIFI_BBCNT				0x158
#define     REG_WIFI_BBWRITE			0x15A
#define     REG_WIFI_BBREAD				0x15C
#define     REG_WIFI_BBBUSY				0x15E
#define		REG_WIFI_BBMODE				0x160
#define		REG_WIFI_BBPOWER			0x168

// 16A to 178 - internal

#define     REG_WIFI_RFDATA2  			0x17C
#define     REG_WIFI_RFDATA1  			0x17E
#define     REG_WIFI_RFBUSY				0x180
#define     REG_WIFI_RFCNT		  		0x184

// 190 - internal
// 194 - ?
// 198 - internal
#define		REG_WIFI_RFPINS				0x19C
// 1A0 - internal
// 1A2 - internal
#define		REG_WIFI_MAYBE_RATE			0x1A4

#define		REG_WIFI_RXSTAT_INC_IF		0x1A8
#define		REG_WIFI_RXSTAT_INC_IE		0x1AA
#define		REG_WIFI_RXSTAT_OVF_IF		0x1AC
#define		REG_WIFI_RXSTAT_OVF_IE		0x1AE
#define		REG_WIFI_TXERR_COUNT		0x1C0
#define		REG_WIFI_RX_COUNT			0x1C4

// 1D0 to 1DE - related to multiplayer. argh.

#define		REG_WIFI_RFSTATUS			0x214
#define		REG_WIFI_IF_SET				0x21C
#define 	REG_WIFI_TXSEQNO			0x210
#define		REG_WIFI_RXTXADDR			0x268
#define		REG_WIFI_POWERACK			0x2D0


#define		WIFI_IOREG(reg)				wifiMac.IOPorts[(reg) >> 1]

/* WIFI misc constants */
#define		WIFI_CHIPID					0x1440		/* emulates "old" wifi chip, new is 0xC340 */

/* Referenced as RF_ in dswifi: rffilter_t */
/* based on the documentation for the RF2958 chip of RF Micro Devices */
/* using the register names as in docs ( http://www.rfmd.com/pdfs/2958.pdf )*/
/* even tho every register only has 18 bits we are using u32 */
typedef struct rffilter_t
{
	union CFG1
	{
		struct 
		{
/* 0*/		unsigned IF_VGA_REG_EN:1;
/* 1*/		unsigned IF_VCO_REG_EN:1;
/* 2*/		unsigned RF_VCO_REG_EN:1;
/* 3*/		unsigned HYBERNATE:1;
/* 4*/		unsigned :10;
/*14*/		unsigned REF_SEL:2;
/*16*/		unsigned :2;
		} bits;
		u32 val;
	} CFG1;
	union IFPLL1
	{
		struct 
		{
/* 0*/		unsigned DAC:4;
/* 4*/		unsigned :5;
/* 9*/		unsigned P1:1;
/*10*/		unsigned LD_EN1:1;
/*11*/		unsigned AUTOCAL_EN1:1;
/*12*/		unsigned PDP1:1;
/*13*/		unsigned CPL1:1;
/*14*/		unsigned LPF1:1;
/*15*/		unsigned VTC_EN1:1;
/*16*/		unsigned KV_EN1:1;
/*17*/		unsigned PLL_EN1:1;
		} bits;
		u32 val;
	} IFPLL1;
	union IFPLL2
	{
		struct 
		{
/* 0*/		unsigned IF_N:16;
/*16*/		unsigned :2;
		} bits;
		u32 val;
	} IFPLL2;
	union IFPLL3
	{
		struct 
		{
/* 0*/		unsigned KV_DEF1:4;
/* 4*/		unsigned CT_DEF1:4;
/* 8*/		unsigned DN1:9;
/*17*/		unsigned :1;
		} bits;
		u32 val;
	} IFPLL3;
	union RFPLL1
	{
		struct 
		{
/* 0*/      unsigned DAC:4;
/* 4*/      unsigned :5;
/* 9*/      unsigned P:1;
/*10*/      unsigned LD_EN:1;
/*11*/      unsigned AUTOCAL_EN:1;
/*12*/      unsigned PDP:1;
/*13*/      unsigned CPL:1;
/*14*/      unsigned LPF:1;
/*15*/      unsigned VTC_EN:1;
/*16*/      unsigned KV_EN:1;
/*17*/      unsigned PLL_EN:1;
		} bits;
		u32 val;
	} RFPLL1;
	union RFPLL2
	{
		struct 
		{
/* 0*/      unsigned NUM2:6;
/* 6*/      unsigned N2:12;
		} bits;
		u32 val;
	} RFPLL2;
	union RFPLL3
	{
		struct 
		{
/* 0*/		unsigned NUM2:18;
		} bits;
		u32 val;
	} RFPLL3;
	union RFPLL4
	{
		struct 
		{
/* 0*/		unsigned KV_DEF:4;
/* 4*/      unsigned CT_DEF:4;
/* 8*/      unsigned DN:9;
/*17*/      unsigned :1;
		} bits;
		u32 val;
	} RFPLL4;
	union CAL1
	{
		struct 
		{
/* 0*/      unsigned LD_WINDOW:3;
/* 3*/      unsigned M_CT_VALUE:5;
/* 8*/      unsigned TLOCK:5;
/*13*/      unsigned TVCO:5;
		} bits;
		u32 val;
	} CAL1;
	union TXRX1
	{
		struct 
		{
/* 0*/      unsigned TXBYPASS:1;
/* 1*/      unsigned INTBIASEN:1;
/* 2*/      unsigned TXENMODE:1;
/* 3*/      unsigned TXDIFFMODE:1;
/* 4*/      unsigned TXLPFBW:3;
/* 7*/      unsigned RXLPFBW:3;
/*10*/      unsigned TXVGC:5;
/*15*/      unsigned PCONTROL:2;
/*17*/      unsigned RXDCFBBYPS:1;
		} bits;
		u32 val;
	} TXRX1;
	union PCNT1
	{
		struct 
		{
/* 0*/      unsigned TX_DELAY:3;
/* 3*/      unsigned PC_OFFSET:6;
/* 9*/      unsigned P_DESIRED:6;
/*15*/      unsigned MID_BIAS:3;
		} bits;
		u32 val;
	} PCNT1;
	union PCNT2
	{
		struct 
		{
/* 0*/      unsigned MIN_POWER:6;
/* 6*/      unsigned MID_POWER:6;
/*12*/      unsigned MAX_POWER:6;
		} bits;
		u32 val;
	} PCNT2;
	union VCOT1
	{
		struct 
		{
/* 0*/      unsigned :16;
/*16*/      unsigned AUX1:1;
/*17*/      unsigned AUX:1;
		} bits;
		u32 val;
	} VCOT1;
} rffilter_t;

/* baseband chip refered as BB_, dataformat is unknown yet */
/* it has at least 105 bytes of functional data */
typedef struct
{
	u8    data[105];
} bb_t;

/* communication interface between RF,BB and the mac */
typedef union
{
	struct {
/* 0*/  unsigned wordsize:5;
/* 5*/  unsigned :2;
/* 7*/  unsigned readOperation:1;
/* 8*/  unsigned :8;
	} bits;
	u16 val;
} rfIOCnt_t;

typedef union
{
	struct {
/* 0*/		unsigned busy:1;
/* 1*/      unsigned :15;
	} bits;
	u16 val;
} rfIOStat_t;

typedef union
{
	struct {
/* 0*/  unsigned content:18;
/*18*/  unsigned address:5;
/*23*/  unsigned :9;
	} bits;
	struct {
/* 0*/  unsigned low:16;
/*16*/  unsigned high:16;
	} val16;
	u16 array16[2];
	u32 val;
} rfIOData_t;

typedef union
{
	struct {
/* 0*/  unsigned address:7;
/* 7*/  unsigned :5;
/*12*/  unsigned mode:2;
/*14*/  unsigned enable:1;
/*15*/  unsigned :1;
	} bits;
	u16 val;
} bbIOCnt_t;

#define WIFI_IRQ_RXEND					0
#define WIFI_IRQ_TXEND					1
#define WIFI_IRQ_RXINC					2
#define WIFI_IRQ_TXERROR	            3
#define WIFI_IRQ_RXOVF					4
#define WIFI_IRQ_TXERROVF				5
#define WIFI_IRQ_RXSTART				6
#define WIFI_IRQ_TXSTART				7
#define WIFI_IRQ_TXCOUNTEXP				8
#define WIFI_IRQ_RXCOUNTEXP				9
#define WIFI_IRQ_RFWAKEUP               11
#define WIFI_IRQ_UNK					12
#define WIFI_IRQ_TIMEPOSTBEACON			13
#define WIFI_IRQ_TIMEBEACON             14
#define WIFI_IRQ_TIMEPREBEACON          15

struct Wifi_TXSlot
{
	u16 RegVal;

	u16 CurAddr;
	int RemPreamble; // preamble time in µs
	int RemHWords;
	u32 TimeMask; // 3 = 2mbps, 7 = 1mbps
	bool NotStarted;
};

#define WIFI_TXSLOT_LOC1		0
#define WIFI_TXSLOT_MPCMD		1
#define WIFI_TXSLOT_LOC2		2
#define WIFI_TXSLOT_LOC3		3
#define WIFI_TXSLOT_BEACON		4
#define WIFI_TXSLOT_MPREPLY		5
#define WIFI_TXSLOT_NUM			6

struct Wifi_RXPacket
{
	u8* Data;
	int CurOffset;
	int RemHWords;
	bool NotStarted;
};

typedef std::queue<Wifi_RXPacket> Wifi_RXPacketQueue;

enum EAPStatus
{
	APStatus_Disconnected = 0,
	APStatus_Authenticated,
	APStatus_Associated
};

/* wifimac_t: the buildin mac (arm7 addressrange: 0x04800000-0x04FFFFFF )*/
/* http://www.akkit.org/info/dswifi.htm#WifiIOMap */

typedef struct 
{
	/* power */
	BOOL powerOn;
	BOOL powerOnPending;

	/* status */
	u16 rfStatus;
	u16 rfPins;

	/* wifi interrupt handling */
    u16   IE;
    u16   IF;

	/* modes */
	u16 macMode;
	u16 wepMode;
	BOOL WEP_enable;

	/* sending */
	u16 TXStatCnt;
	u16 TXPower;
	u16 TXCnt;
	u16 TXStat;
	u16 TXSeqNo;
	Wifi_TXSlot TXSlots[WIFI_TXSLOT_NUM];
	int TXCurSlot;
	u16 TXBusy;

	/* receiving */
	u16 RXCnt;
	u16 RXCheckCounter;
	u8 RXNum;
	Wifi_RXPacketQueue RXPacketQueue;

	u16 RXStatIncIF, RXStatIncIE;
	u16 RXStatOvfIF, RXStatOvfIE;
	u8 RXStat[16];
	u16 RXTXAddr;

	/* addressing/handshaking */
	union
	{
		//TODO - is this endian safe? don't think so
		u16  words[3];
		u8	 bytes[6];
	} mac;
	union
	{
		u16  words[3];
		u8	 bytes[6];
	} bss;
	u16 aid;
	u16 pid; /* player ID or aid_low */
	u16 retryLimit;

	/* timing */
	u64 GlobalUsecTimer;
	BOOL crystalEnabled;
	u64 usec;
	BOOL usecEnable;
	u64 ucmp;
	BOOL ucmpEnable;
	u32 eCount;
	BOOL eCountEnable;
	u16 BeaconInterval;
	u16 BeaconCount1;
	u16 BeaconCount2;
	u16 ListenInterval;
	u16 ListenCount;

	/* subchips */
    rffilter_t 	RF;
	bb_t        BB;

	/* subchip communications */
    rfIOCnt_t   rfIOCnt;
	rfIOStat_t  rfIOStatus;
	rfIOData_t  rfIOData;
    bbIOCnt_t   bbIOCnt;

	/* buffers */
	u16         RAM[0x1000];
	u16         RXRangeBegin;
	u16         RXRangeEnd;
	u16			RXWriteCursor;
	u16         RXReadCursor;
	u16         RXUnits;
	u16			RXBufCount;
	u16         CircBufReadAddress;
	u16         CircBufWriteAddress;
	u16         CircBufRdEnd;
	u16         CircBufRdSkip;
	u16         CircBufWrEnd;
	u16         CircBufWrSkip;

	/* I/O ports */
	u16			IOPorts[0x800];

	/* others */
	u16			randomSeed;

} wifimac_t;


typedef struct
{
	EAPStatus status;
	u16 seqNum;

} SoftAP_t;

// desmume host communication
#ifdef EXPERIMENTAL_WIFI_COMM
typedef struct pcap pcap_t;
extern pcap_t *wifi_bridge;
#endif

extern wifimac_t wifiMac;
extern SoftAP_t SoftAP;

bool WIFI_Init();
void WIFI_DeInit();
void WIFI_Reset();

/* subchip communication IO functions */
void WIFI_setRF_CNT(u16 val);
void WIFI_setRF_DATA(u16 val, u8 part);
u16  WIFI_getRF_DATA(u8 part);
u16  WIFI_getRF_STATUS();

void WIFI_setBB_CNT(u16 val);
u8   WIFI_getBB_DATA();
void WIFI_setBB_DATA(u8 val);

/* wifimac io */
void WIFI_write16(u32 address, u16 val);
u16  WIFI_read16(u32 address);

/* wifimac timing */
void WIFI_usTrigger();


/* DS WFC profile data documented here : */
/* http://dsdev.bigredpimp.com/2006/07/31/aoss-wfc-profile-data/ */
/* Note : we use bytes to avoid endianness issues */
typedef struct _FW_WFCProfile
{
	char SSID[32];
	char SSID_WEP64[32];
	char WEPKEY_PART1[16];
	char WEPKEY_PART2[16];
	char WEPKEY_PART3[16];
	char WEPKEY_PART4[16];
	u8 IP_ADDRESS[4];
	u8 GATEWAY[4];
	u8 PRIM_DNS[4];
	u8 SEC_DNS[4];
	u8 SUBNET_MASK;
	u8 WEP64_KEY_AOSS[20];
	u8 UNK1;
	u8 WEP_MODE;
	u8 STATUS;
	u8 UNK2[7];
	u8 UNK3;
	u8 UNK4[14];
	u8 CRC16[2];

} FW_WFCProfile;

/* wifi data to be stored in firmware, when no firmware image was loaded */
extern u8 FW_Mac[6];
extern const u8 FW_WIFIInit[32];
extern const u8 FW_BBInit[105];
extern const u8 FW_RFInit[36];
extern const u8 FW_RFChannel[6*14];
extern const u8 FW_BBChannel[14];
extern FW_WFCProfile FW_WFCProfile1;
extern FW_WFCProfile FW_WFCProfile2;
extern FW_WFCProfile FW_WFCProfile3;

#endif
