/*
    Copyright (C) 2007 Tim Seidel
    Copyright (C) 2014 pleonex
    Copyright (C) 2008-2018 DeSmuME team

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

#ifndef WIFI_H
#define WIFI_H

#include <stdio.h>
#include "types.h"

#include <deque>
#include <string>
#include <vector>

#define		REG_WIFI_ID					0x000
#define     REG_WIFI_MODE       		0x004
#define     REG_WIFI_WEP        		0x006
#define		REG_WIFI_TXSTATCNT			0x008
#define		REG_WIFI_X_00A				0x00A
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
#define     REG_WIFI_TXLOCBEACON        0x080
#define		REG_WIFI_TXBUF_TIM			0x084
#define     REG_WIFI_LISTENCOUNT        0x088
#define     REG_WIFI_BEACONPERIOD       0x08C
#define     REG_WIFI_LISTENINT          0x08E
#define 	REG_WIFI_TXLOCEXTRA			0x090
#define		REG_WIFI_TXBUF_REPLY1		0x094
#define		REG_WIFI_TXBUF_REPLY2		0x098
// 09C - internal
#define     REG_WIFI_TXLOC1             0x0A0
#define     REG_WIFI_TXLOC2             0x0A4
#define     REG_WIFI_TXLOC3             0x0A8
#define     REG_WIFI_TXREQ_RESET        0x0AC
#define     REG_WIFI_TXREQ_SET          0x0AE
#define		REG_WIFI_TXREQ_READ			0x0B0
#define		REG_WIFI_TXRESET			0x0B4
#define		REG_WIFI_TXBUSY				0x0B6
#define     REG_WIFI_TXSTAT             0x0B8
// 0BA - internal
#define		REG_WIFI_PREAMBLE			0x0BC
#define		REG_WIFI_CMD_TOTALTIME		0x0C0
#define		REG_WIFI_CMD_REPLYTIME		0x0C4
// 0C8 - internal
#define     REG_WIFI_RXFILTER           0x0D0
#define		REG_WIFI_CONFIG_0D4			0x0D4
#define		REG_WIFI_CONFIG_0D8			0x0D8
#define		REG_WIFI_RX_LEN_CROP		0x0DA
#define		REG_WIFI_RXFILTER2			0x0E0

#define     REG_WIFI_USCOUNTERCNT       0x0E8
#define     REG_WIFI_USCOMPARECNT       0x0EA
#define		REG_WIFI_CONFIG_0EC			0x0EC
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

#define		REG_WIFI_CONFIG_120			0x120
#define		REG_WIFI_CONFIG_122			0x122
#define		REG_WIFI_CONFIG_124			0x124
#define		REG_WIFI_CONFIG_128			0x128
#define		REG_WIFI_CONFIG_130			0x130
#define		REG_WIFI_CONFIG_132			0x132
#define 	REG_WIFI_BEACONCOUNT2		0x134
#define		REG_WIFI_CONFIG_140			0x140
#define		REG_WIFI_CONFIG_142			0x142
#define		REG_WIFI_CONFIG_144			0x144
#define		REG_WIFI_CONFIG_146			0x146
#define		REG_WIFI_CONFIG_148			0x148
#define		REG_WIFI_CONFIG_14A			0x14A
#define		REG_WIFI_CONFIG_14C			0x14C
#define		REG_WIFI_CONFIG_150			0x150
#define		REG_WIFI_CONFIG_154			0x154

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
#define		REG_WIFI_TX_HDR_CNT			0x194
// 198 - internal
#define		REG_WIFI_RFPINS				0x19C
#define		REG_WIFI_X_1A0				0x1A0
#define		REG_WIFI_X_1A2				0x1A2
#define		REG_WIFI_X_1A4				0x1A4

#define		REG_WIFI_RXSTAT_INC_IF		0x1A8
#define		REG_WIFI_RXSTAT_INC_IE		0x1AA
#define		REG_WIFI_RXSTAT_OVF_IF		0x1AC
#define		REG_WIFI_RXSTAT_OVF_IE		0x1AE
#define		REG_WIFI_RXSTAT0			0x1B0
#define		REG_WIFI_RXSTAT1			0x1B2
#define		REG_WIFI_RXSTAT2			0x1B4
#define		REG_WIFI_RXSTAT3			0x1B6
#define		REG_WIFI_RXSTAT4			0x1B8
#define		REG_WIFI_RXSTAT5			0x1BA
#define		REG_WIFI_RXSTAT6			0x1BC
#define		REG_WIFI_RXSTAT7			0x1BE
#define		REG_WIFI_TXERR_COUNT		0x1C0
#define		REG_WIFI_RX_COUNT			0x1C4

#define		REG_WIFI_CMD_STAT0			0x1D0
#define		REG_WIFI_CMD_STAT1			0x1D2
#define		REG_WIFI_CMD_STAT2			0x1D4
#define		REG_WIFI_CMD_STAT3			0x1D6
#define		REG_WIFI_CMD_STAT4			0x1D8
#define		REG_WIFI_CMD_STAT5			0x1DA
#define		REG_WIFI_CMD_STAT6			0x1DC
#define		REG_WIFI_CMD_STAT7			0x1DE

#define		REG_WIFI_RFSTATUS			0x214
#define		REG_WIFI_IF_SET				0x21C
#define 	REG_WIFI_TXSEQNO			0x210
#define		REG_WIFI_X_228				0x228
#define		REG_WIFI_X_244				0x244
#define		REG_WIFI_CONFIG_254			0x254
#define		REG_WIFI_RXTX_ADDR			0x268
#define		REG_WIFI_X_290				0x290
#define		REG_WIFI_POWERACK			0x2D0

/* WIFI misc constants */
#define		WIFI_CHIPID					0x1440		/* emulates "old" wifi chip, new is 0xC340 */

#define		WIFI_WORKING_PACKET_BUFFER_SIZE (16 * 1024 * sizeof(u8)) // 16 KB size

class WifiHandler;
class Task;
struct slock;
typedef slock slock_t;

// RF2958 Register Addresses
enum RegAddrRF2958
{
	REG_RF2958_CFG1			= 0,
	REG_RF2958_IPLL1		= 1,
	REG_RF2958_IPLL2		= 2,
	REG_RF2958_IPLL3		= 3,
	REG_RF2958_RFPLL1		= 4,
	REG_RF2958_RFPLL2		= 5,
	REG_RF2958_RFPLL3		= 6,
	REG_RF2958_RFPLL4		= 7,
	REG_RF2958_CAL1			= 8,
	REG_RF2958_TXRX1		= 9,
	REG_RF2958_PCNT1		= 10,
	REG_RF2958_PCNT2		= 11,
	REG_RF2958_VCOT1		= 12,
	REG_RF2958_TEST			= 27,
	REG_RF2958_RESET		= 31
};

typedef struct
{
	u32 powerOn;
	u32 powerOnPending;
	
	u16 rfStatus;
	u16 rfPins;
	
	u16 IE;
	u16 IF;
	
	u16 macMode;
	u16 wepMode;
	u32 WEP_enable;
	
	u16 TXCnt;
	u16 TXStat;
	
	u16 RXCnt;
	u16 RXCheckCounter;
	
	u8 macAddr[6];
	u8 bssid[6];
	
	u16 aid;
	u16 pid;
	u16 retryLimit;
	
	u32 crystalEnabled;
	u64 usec;
	u32 usecEnable;
	u64 ucmp;
	u32 ucmpEnable;
	u16 eCount;
	u32 eCountEnable;
	
	u32 rf_cfg1;
	u32 rf_ifpll1;
	u32 rf_ifpll2;
	u32 rf_ifpll3;
	u32 rf_rfpll1;
	u32 rf_rfpll2;
	u32 rf_rfpll3;
	u32 rf_rfpll4;
	u32 rf_cal1;
	u32 rf_txrx1;
	u32 rf_pcnt1;
	u32 rf_pcnt2;
	u32 rf_vcot1;
	
	u8 bb_data[105];
	
	u16 rfIOCnt;
	u16 rfIOStatus;
	u32 rfIOData;
	u16 bbIOCnt;
	
	u8 wifiRAM[0x2000];
	u16 rxRangeBegin;
	u16 rxRangeEnd;
	u16 rxWriteCursor;
	u16 rxReadCursor;
	u16 rxUnits;
	u16 rxBufCount;
	u16 circBufReadAddress;
	u16 circBufWriteAddress;
	u16 circBufReadEnd;
	u16 circBufReadSkip;
	u16 circBufWriteEnd;
	u16 circBufWriteSkip;
	
	u16 wifiIOPorts[0x800];
	u16 randomSeed;
} LegacyWifiSFormat;

typedef union
{
	u32 value;
	
	struct
	{
		u32 Data:18;
		u32 :14;
	};
	
	struct
	{
#ifndef MSB_FIRST
		u16 DataLSB:16;
		
		u16 DataMSB:2;
		u16 :6;
		
		u16 :8;
#else
		u16 DataLSB:16;
		
		u16 :6;
		u16 DataMSB:2;
		
		u16 :8;
#endif
	};
} RFREG_RF2958;

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u8 IF_VGA_REG_EN:1;
		u8 IF_VCO_REG_EN:1;
		u8 RF_VCO_REG_EN:1;
		u8 HYBERNATE:1;
		u8 :4;
		
		u8 :6;
		u8 REF_SEL:2;
		
		u8 reserved:2;
		u8 :6;
		
		u8 :8;
#else
		u8 :4;
		u8 HYBERNATE:1;
		u8 RF_VCO_REG_EN:1;
		u8 IF_VCO_REG_EN:1;
		u8 IF_VGA_REG_EN:1;
		
		u8 REF_SEL:2;
		u8 :6;
		
		u8 :6;
		u8 reserved:2;
		
		u8 :8;
#endif
	};
} RFREG_CFG1;

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u8 DAC:4;
		u8 reserved1:4;
		
		u8 reserved2:1;
		u8 P1:1;
		u8 LD_EN1:1;
		u8 AUTOCAL_EN1:1;
		u8 PDP1:1;
		u8 CPL1:1;
		u8 LPF1:1;
		u8 VTC_EN1:1;
		
		u8 KV_EN1:1;
		u8 PLL_EN1:1;
		u8 :6;
		
		u8 :8;
#else
		u8 reserved1:4;
		u8 DAC:4;
		
		u8 VTC_EN1:1;
		u8 LPF1:1;
		u8 CPL1:1;
		u8 PDP1:1;
		u8 AUTOCAL_EN1:1;
		u8 LD_EN1:1;
		u8 P1:1;
		u8 reserved2:1;
		
		u8 :6;
		u8 PLL_EN1:1;
		u8 KV_EN1:1;
		
		u8 :8;
#endif
	};
} RFREG_IFPLL1;

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 IF_N:16;
		
		u8 reserved:2;
		u8 :6;
		
		u8 :8;
#else
		u16 IF_N:16;
		
		u8 :6;
		u8 reserved:2;
		
		u8 :8;
#endif
	};
} RFREG_IFPLL2;

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u8 KV_DEF1:4;
		u8 CT_DEF1:4;
		
		u16 DN1:9;
		u16 reserved:1;
		u16 :6;
		
		u8 :8;
#else
		u8 CT_DEF1:4;
		u8 KV_DEF1:4;
		
		u16 :6;
		u16 reserved:1;
		u16 DN1:9;
		
		u8 :8;
#endif
	};
} RFREG_IFPLL3;

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u8 DAC:4;
		u8 reserved1:4;
		
		u8 reserved2:1;
		u8 P:1;
		u8 LD_EN:1;
		u8 AUTOCAL_EN:1;
		u8 PDP:1;
		u8 CPL:1;
		u8 LPF:1;
		u8 VTC_EN:1;
		
		u8 KV_EN:1;
		u8 PLL_EN:1;
		u8 :6;
		
		u8 :8;
#else
		u8 reserved1:4;
		u8 DAC:4;
		
		u8 VTC_EN:1;
		u8 LPF:1;
		u8 CPL:1;
		u8 PDP:1;
		u8 AUTOCAL_EN:1;
		u8 LD_EN:1;
		u8 P:1;
		u8 reserved2:1;
		
		u8 :6;
		u8 PLL_EN:1;
		u8 KV_EN:1;
		
		u8 :8;
#endif
	};
} RFREG_RFPLL1;

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 NUM2:6;
		u16 N2:12;
		u16 :6;
		
		u8 :8;
#else
		u16 :6;
		u16 N2:12;
		u16 NUM2:6;
		
		u8 :8;
#endif
	};
} RFREG_RFPLL2;

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u32 NUM2:18;
		u32 :6;
		
		u8 :8;
#else
		u32 :6;
		u32 NUM2:18;
		
		u8 :8;
#endif
	};
} RFREG_RFPLL3;

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u8 KV_DEF:4;
		u8 CT_DEF:4;
		
		u16 DN:9;
		u16 reserved:1;
		u16 :6;
		
		u8 :8;
#else
		u8 CT_DEF:4;
		u8 KV_DEF:4;
		
		u16 :6;
		u16 reserved:1;
		u16 DN:9;
		
		u8 :8;
#endif
	};
} RFREG_RFPLL4;

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u8 LD_WINDOW:3;
		u8 M_CT_VALUE:5;
		
		u16 TLOCK:5;
		u16 TVCO:5;
		u16 :6;
		
		u8 :8;
#else
		u8 M_CT_VALUE:5;
		u8 LD_WINDOW:3;
		
		u16 :6;
		u16 TVCO:5;
		u16 TLOCK:5;
		
		u8 :8;
#endif
	};
} RFREG_CAL1;

typedef union
{
	u32 value;
	
	struct
	{
		u32 TXBYPASS:1;
		u32 INTBIASEN:1;
		u32 TXENMODE:1;
		u32 TXDIFFMODE:1;
		u32 TXLPFBW:3;
		u32 RXLPFBW:3;
		u32 TXVGC:5;
		u32 PCONTROL:2;
		u32 RXDCFBBYPS:1;
		u32 :6;
		
		u8 :8;
	};
} RFREG_TXRX1;

typedef union
{
	u32 value;
	
	struct
	{
		u32 TX_DELAY:3;
		u32 PC_OFFSET:6;
		u32 P_DESIRED:6;
		u32 MID_BIAS:3;
		u32 :6;
		
		u8 :8;
	};
} RFREG_PCNT1;

typedef union
{
	u32 value;
	
	struct
	{
		u32 MIN_POWER:6;
		u32 MID_POWER:6;
		u32 MAX_POWER:6;
		u32 :6;
		
		u8 :8;
	};
} RFREG_PCNT2;

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 reserved:16;
		
		u8 AUX1:1;
		u8 AUX:1;
		u8 :6;
		
		u8 :8;
#else
		u16 reserved:16;
		
		u8 :6;
		u8 AUX:1;
		u8 AUX1:1;
		
		u8 :8;
#endif
	};
} RFREG_VCOT1;

// This struct is based on the RF2958 chip from RF Micro Devices.
// All register names match the names that are on the RF2958 Datasheet:
// https://pdf1.alldatasheet.com/datasheet-pdf/view/116332/RFMD/RF2958/+0154WWVOELEbTptEC.+/datasheet.pdf
//
// Note that all the registers are 18 bits in length. However, for the sake of compatibility,
// we pad out each registers to 32 bits.

#include "PACKED.h"
typedef union
{
	RFREG_RF2958 reg[32];
	
	struct
	{
		RFREG_CFG1		CFG1;			// 0
		RFREG_IFPLL1	IFPLL1;			// 1
		RFREG_IFPLL2	IFPLL2;			// 2
		RFREG_IFPLL3	IFPLL3;			// 3
		RFREG_RFPLL1	RFPLL1;			// 4
		RFREG_RFPLL2	RFPLL2;			// 5
		RFREG_RFPLL3	RFPLL3;			// 6
		RFREG_RFPLL4	RFPLL4;			// 7
		RFREG_CAL1		CAL1;			// 8
		RFREG_TXRX1		TXRX1;			// 9
		RFREG_PCNT1		PCNT1;			// 10
		RFREG_PCNT2		PCNT2;			// 11
		RFREG_VCOT1		VCOT1;			// 12
		
		RFREG_RF2958	PADDING_13;		// 13
		RFREG_RF2958	PADDING_14;		// 14
		RFREG_RF2958	PADDING_15;		// 15
		RFREG_RF2958	PADDING_16;		// 16
		RFREG_RF2958	PADDING_17;		// 17
		RFREG_RF2958	PADDING_18;		// 18
		RFREG_RF2958	PADDING_19;		// 19
		RFREG_RF2958	PADDING_20;		// 20
		RFREG_RF2958	PADDING_21;		// 21
		RFREG_RF2958	PADDING_22;		// 22
		RFREG_RF2958	PADDING_23;		// 23
		RFREG_RF2958	PADDING_24;		// 24
		RFREG_RF2958	PADDING_25;		// 25
		RFREG_RF2958	PADDING_26;		// 26
		
		RFREG_RF2958	TEST;			// 27
		
		RFREG_RF2958	PADDING_28;		// 28
		RFREG_RF2958	PADDING_29;		// 29
		RFREG_RF2958	PADDING_30;		// 30
		
		RFREG_RF2958	RESET;			// 31
	};
} RF2958_IOREG_MAP;
#include "PACKED_END.h"

/* baseband chip refered as BB_, dataformat is unknown yet */
/* it has at least 105 bytes of functional data */
typedef struct
{
	u8    data[105];
} bb_t;

enum WifiRFStatus
{
	WifiRFStatus0_Initial			= 0,
	WifiRFStatus1_TXComplete		= 1,
	WifiRFStatus2_TransitionRXTX	= 2,
	WifiRFStatus3_TXEnabled			= 3,
	WifiRFStatus4_TransitionTXRX	= 4,
	WifiRFStatus5_MPHostReplyWait	= 5,
	WifiRFStatus6_RXEnabled			= 6,
	WifiRFStatus7_UNKNOWN7			= 7,
	WifiRFStatus8_MPHostAcknowledge	= 8,
	WifiRFStatus9_Idle				= 9
};

enum WifiIRQ
{
	WifiIRQ00_RXComplete			= 0,
	WifiIRQ01_TXComplete			= 1,
	WifiIRQ02_RXEventIncrement		= 2,
	WifiIRQ03_TXErrorIncrement		= 3,
	WifiIRQ04_RXEventOverflow		= 4,
	WifiIRQ05_TXErrorOverflow		= 5,
	WifiIRQ06_RXStart				= 6,
	WifiIRQ07_TXStart				= 7,
	WifiIRQ08_TXCountExpired		= 8,
	WifiIRQ09_RXCountExpired		= 9,
	WifiIRQ10_UNUSED				= 10,
	WifiIRQ11_RFWakeup				= 11,
	WifiIRQ12_UNKNOWN				= 12,
	WifiIRQ13_TimeslotPostBeacon	= 13,
	WifiIRQ14_TimeslotBeacon		= 14,
	WifiIRQ15_TimeslotPreBeacon		= 15
};

enum EAPStatus
{
	APStatus_Disconnected = 0,
	APStatus_Authenticated,
	APStatus_Associated
};

enum WifiTXLocIndex
{
	WifiTXLocIndex_LOC1		= 0,
	WifiTXLocIndex_CMD		= 1,
	WifiTXLocIndex_LOC2		= 2,
	WifiTXLocIndex_LOC3		= 3,
	WifiTXLocIndex_BEACON	= 4,
	WifiTXLocIndex_CMDREPLY	= 5
};

enum WifiStageID
{
	WifiStageID_PreambleDone			= 0,
	WifiStageID_TransmitDone			= 1,
	WifiStageID_CmdHostTransferDone		= 2,
	WifiStageID_CmdHostAck				= 3,
	WifiStageID_CmdReplyTransferDone	= 4
};

enum WifiEmulationLevel
{
	WifiEmulationLevel_Off = 0,
	WifiEmulationLevel_Normal = 10000,
	WifiEmulationLevel_Compatibility = 65535
};

enum WifiCommInterfaceID
{
	WifiCommInterfaceID_AdHoc = 0,
	WifiCommInterfaceID_Infrastructure = 1
};

typedef u16 IOREG_W_PADDING;

typedef u16 IOREG_W_INTERNAL;
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_02E;		// 0x480802E
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_034;		// 0x4808034
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_078;		// 0x4808078
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_09C;		// 0x480809C
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_0BA;		// 0x48080BA
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_0C8;		// 0x48080C8
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_100;		// 0x4808100
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_102;		// 0x4808102
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_104;		// 0x4808104
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_106;		// 0x4808106
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_126;		// 0x4808126
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_12A;		// 0x480812A
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_16A;		// 0x480816A
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_170;		// 0x4808170
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_172;		// 0x4808172
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_174;		// 0x4808174
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_176;		// 0x4808176
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_178;		// 0x4808178
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_190;		// 0x4808190
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_198;		// 0x4808198
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_1F0;		// 0x48081F0
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_204;		// 0x4808204
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_208;		// 0x4808208
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_20C;		// 0x480820C
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_220;		// 0x4808220
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_224;		// 0x4808224
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_230;		// 0x4808230
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_234;		// 0x4808234
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_238;		// 0x4808238
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_23C;		// 0x480823C
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_248;		// 0x4808248
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_24C;		// 0x480824C
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_24E;		// 0x480824E
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_250;		// 0x4808250
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_258;		// 0x4808258
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_25C;		// 0x480825C
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_260;		// 0x4808260
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_264;		// 0x4808264
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_270;		// 0x4808270
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_274;		// 0x4808274
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_278;		// 0x4808278
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_27C;		// 0x480827C
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_298;		// 0x4808298
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2A0;		// 0x48082A0
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2A2;		// 0x48082A2
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2A4;		// 0x48082A4
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2A8;		// 0x48082A8
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2AC;		// 0x48082AC
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2B0;		// 0x48082B0
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2B4;		// 0x48082B4
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2B8;		// 0x48082B8
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2C0;		// 0x48082C0
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2C4;		// 0x48082C4
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2C8;		// 0x48082C8
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2CC;		// 0x48082CC
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2F0;		// 0x48082F0
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2F2;		// 0x48082F2
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2F4;		// 0x48082F4
typedef IOREG_W_INTERNAL IOREG_W_INTERNAL_2F6;		// 0x48082F6

typedef u16 IOREG_W_CONFIG;
typedef IOREG_W_CONFIG IOREG_W_CONFIG_0D4;			// 0x48080D4
typedef IOREG_W_CONFIG IOREG_W_CONFIG_0D8;			// 0x48080D8
typedef IOREG_W_CONFIG IOREG_W_CONFIG_0EC;			// 0x48080EC
typedef IOREG_W_CONFIG IOREG_W_CONFIG_120;			// 0x4808120
typedef IOREG_W_CONFIG IOREG_W_CONFIG_122;			// 0x4808122
typedef IOREG_W_CONFIG IOREG_W_CONFIG_124;			// 0x4808124
typedef IOREG_W_CONFIG IOREG_W_CONFIG_128;			// 0x4808128
typedef IOREG_W_CONFIG IOREG_W_CONFIG_130;			// 0x4808130
typedef IOREG_W_CONFIG IOREG_W_CONFIG_132;			// 0x4808132
typedef IOREG_W_CONFIG IOREG_W_CONFIG_140;			// 0x4808140
typedef IOREG_W_CONFIG IOREG_W_CONFIG_142;			// 0x4808142
typedef IOREG_W_CONFIG IOREG_W_CONFIG_144;			// 0x4808144
typedef IOREG_W_CONFIG IOREG_W_CONFIG_146;			// 0x4808146
typedef IOREG_W_CONFIG IOREG_W_CONFIG_148;			// 0x4808148
typedef IOREG_W_CONFIG IOREG_W_CONFIG_14A;			// 0x480814A
typedef IOREG_W_CONFIG IOREG_W_CONFIG_14C;			// 0x480814C
typedef IOREG_W_CONFIG IOREG_W_CONFIG_150;			// 0x4808150
typedef IOREG_W_CONFIG IOREG_W_CONFIG_154;			// 0x4808154
typedef IOREG_W_CONFIG IOREG_W_CONFIG_254;			// 0x4808254

typedef u16 IOREG_W_ID;								// 0x4808000

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 TXMasterEnable:1;
		u16 UNKNOWN1:7;
		
		u16 UNKNOWN2:5;
		u16 ResetPortSet1:1;
		u16 ResetPortSet2:1;
		u16 UNKNOWN3:1;
#else
		u16 UNKNOWN1:7;
		u16 TXMasterEnable:1;
		
		u16 UNKNOWN3:1;
		u16 ResetPortSet2:1;
		u16 ResetPortSet1:1;
		u16 UNKNOWN2:5;
#endif
	};
} IOREG_W_MODE_RST;									// 0x4808004

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 UNKNOWN1:3;
		u16 WEPEncryptionKeySize:3;
		u16 UNKNOWN2:2;
		
		u16 :8;
#else
		u16 UNKNOWN2:2;
		u16 WEPEncryptionKeySize:3;
		u16 UNKNOWN1:3;
		
		u16 :8;
#endif
	};
} IOREG_W_MODE_WEP;									// 0x4808006

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 UNKNOWN1:8;
		
		u16 UNKNOWN2:4;
		u16 UpdateTXStat_0401:1;
		u16 UpdateTXStat_0B01:1;
		u16 UpdateTXStat_0800:1;
		u16 UpdateTXStatBeacon:1;
#else
		u16 UNKNOWN1:8;
		
		u16 UpdateTXStatBeacon:1;
		u16 UpdateTXStat_0800:1;
		u16 UpdateTXStat_0B01:1;
		u16 UNKNOWN2:5;
#endif
	};
} IOREG_W_TXSTATCNT;								// 0x4808008

typedef u16 IOREG_W_X_00A;							// 0x480800A

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 RXComplete:1;
		u16 TXComplete:1;
		u16 RXEventIncrement:1;
		u16 TXErrorIncrement:1;
		u16 RXEventOverflow:1;
		u16 TXErrorOverflow:1;
		u16 RXStart:1;
		u16 TXStart:1;
		
		u16 TXBufCountExpired:1;
		u16 RXBufCountExpired:1;
		u16 :1;
		u16 RFWakeup:1;
		u16 Multiplayer:1;
		u16 PostBeaconTimeslot:1;
		u16 BeaconTimeslot:1;
		u16 PreBeaconTimeslot:1;
#else
		u16 TXStart:1;
		u16 RXStart:1;
		u16 TXErrorOverflow:1;
		u16 RXEventOverflow:1;
		u16 TXErrorIncrement:1;
		u16 RXEventIncrement:1;
		u16 TXComplete:1;
		u16 RXComplete:1;
		
		u16 PreBeaconTimeslot:1;
		u16 BeaconTimeslot:1;
		u16 PostBeaconTimeslot:1;
		u16 Multiplayer:1;
		u16 RFWakeup:1;
		u16 :1;
		u16 RXBufCountExpired:1;
		u16 TXBufCountExpired:1;
#endif
	};
} IOREG_W_IF;										// 0x4808010

typedef IOREG_W_IF IOREG_W_IE;						// 0x4808012
typedef IOREG_W_IF IOREG_W_IF_SET;					// 0x480821C

typedef union
{
	u16 value;
	u8 component[2];
} IOREG_W_MACADDRn;

typedef IOREG_W_MACADDRn IOREG_W_MACADDR0;			// 0x4808018
typedef IOREG_W_MACADDRn IOREG_W_MACADDR1;			// 0x480801A
typedef IOREG_W_MACADDRn IOREG_W_MACADDR2;			// 0x480801C
typedef IOREG_W_MACADDRn IOREG_W_BSSID0;			// 0x4808020
typedef IOREG_W_MACADDRn IOREG_W_BSSID1;			// 0x4808022
typedef IOREG_W_MACADDRn IOREG_W_BSSID2;			// 0x4808024

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 PlayerID:4;
		u16 :4;
		
		u16 :8;
#else
		u16 :4;
		u16 PlayerID:4;
		
		u16 :8;
#endif
	};
} IOREG_W_AID_LOW;									// 0x4808028

typedef union
{
	u16 value;
	
	struct
	{
		u16 AssociationID:11;
		u16 :5;
	};
} IOREG_W_AID_FULL;									// 0x480802A

typedef union
{
	u16 value;
	
	struct
	{
		u16 Count:8;
		u16 UNKNOWN1:8;
	};
} IOREG_W_TX_RETRYLIMIT;							// 0x480802C

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 CopyAddrToWRCSR:1;
		u16 UNKNOWN1:3;
		u16 :3;
		u16 CopyTXBufReply1To2:1;
		
		u16 UNKNOWN2:7;
		u16 EnableRXFIFOQueuing:1;
#else
		u16 CopyTXBufReply1To2:1;
		u16 :3;
		u16 UNKNOWN1:3;
		u16 CopyAddrToWRCSR:1;
		
		u16 EnableRXFIFOQueuing:1;
		u16 UNKNOWN2:7;
#endif
	};
} IOREG_W_RXCNT;									// 0x4808030

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 :8;
		
		u16 :7;
		u16 Enable:1;
#else
		u16 :8;
		
		u16 Enable:1;
		u16 :7;
#endif
	};
} IOREG_W_WEP_CNT;									// 0x4808032

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 Disable:1;
		u16 UNKNOWN1:1;
		u16 :6;
		
		u16 :8;
#else
		u16 :6;
		u16 UNKNOWN1:1;
		u16 Disable:1;
		
		u16 :8;
#endif
	};
} IOREG_W_POWER_US;									// 0x4808036

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 AutoWakeup:1;
		u16 AutoSleep:1;
		u16 UNKNOWN1:1;
		u16 UNKNOWN2:1;
		u16 :4;
		
		u16 :8;
#else
		u16 :4;
		u16 UNKNOWN2:1;
		u16 UNKNOWN1:1;
		u16 AutoSleep:1;
		u16 AutoWakeup:1;
		
		u16 :8;
#endif
	};
} IOREG_W_POWER_TX;									// 0x4808038

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 UNKNOWN1:1;
		u16 RequestPowerOn:1;
		u16 :6;
		
		u16 WillPowerOn:1;
		u16 IsPowerOff:1;
		u16 :6;
#else
		u16 :6;
		u16 RequestPowerOn:1;
		u16 UNKNOWN1:1;
		
		u16 :6;
		u16 IsPowerOff:1;
		u16 WillPowerOn:1;
#endif
	};
} IOREG_W_POWERSTATE;								// 0x480803C

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 NewPowerOffState:1;
		u16 :7;
		
		u16 :7;
		u16 ApplyNewPowerOffState:1;
#else
		u16 :7;
		u16 NewPowerOffState:1;
		
		u16 ApplyNewPowerOffState:1;
		u16 :7;
#endif
	};
} IOREG_W_POWERFORCE;								// 0x4808040

typedef union
{
	u16 value;
	
	struct
	{
		u16 Random:11;
		u16 :5;
	};
} IOREG_W_RANDOM;									// 0x4808044

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 UNKNOWN1:1;
		u16 UNKNOWN2:1;
		u16 :6;
		
		u16 :8;
#else
		u16 :6;
		u16 UNKNOWN2:1;
		u16 UNKNOWN1:1;
		
		u16 :8;
#endif
	};
} IOREG_W_POWER_UNKNOWN;							// 0x4808048

typedef u16 IOREG_W_RXBUF_BEGIN;					// 0x4808050
typedef u16 IOREG_W_RXBUF_END;						// 0x4808052

typedef union
{
	u16 value;
	
	struct
	{
		u16 HalfwordAddress:12;
		u16 :4;
	};
} IOREG_W_RXBUF_WRCSR;								// 0x4808054

typedef union
{
	u16 value;
	
	struct
	{
		u16 HalfwordAddress:12;
		u16 :4;
	};
} IOREG_W_RXBUF_WR_ADDR;							// 0x4808056

typedef union
{
	u16 value;
	
	struct
	{
		u16 ByteAddress:13;
		u16 :3;
	};
	
	struct
	{
		u16 :1;
		u16 HalfwordAddress:12;
		u16 :3;
	};
} IOREG_W_RXBUF_RD_ADDR;							// 0x4808058

typedef union
{
	u16 value;
	
	struct
	{
		u16 HalfwordAddress:12;
		u16 :4;
	};
} IOREG_W_RXBUF_READCSR;							// 0x480805A

typedef union
{
	u16 value;
	
	struct
	{
		u16 Count:12;
		u16 :4;
	};
} IOREG_W_RXBUF_COUNT;								// 0x480805C

typedef u16 IOREG_W_RXBUF_RD_DATA;					// 0x4808060

typedef union
{
	u16 value;
	
	struct
	{
		u16 ByteAddress:13;
		u16 :3;
	};
	
	struct
	{
		u16 :1;
		u16 HalfwordAddress:12;
		u16 :3;
	};
} IOREG_W_RXBUF_GAP;								// 0x4808062

typedef union
{
	u16 value;
	
	struct
	{
		u16 HalfwordOffset:12;
		u16 :4;
	};
} IOREG_W_RXBUF_GAPDISP;							// 0x4808064

typedef union
{
	u16 value;
	
	struct
	{
		u16 ByteAddress:13;
		u16 :3;
	};
	
	struct
	{
		u16 :1;
		u16 HalfwordAddress:12;
		u16 :3;
	};
} IOREG_W_TXBUF_WR_ADDR;							// 0x4808068

typedef union
{
	u16 value;
	
	struct
	{
		u16 Count:12;
		u16 :4;
	};
} IOREG_W_TXBUF_COUNT;								// 0x480806C

typedef u16 IOREG_W_TXBUF_WR_DATA;					// 0x4808070

typedef union
{
	u16 value;
	
	struct
	{
		u16 ByteAddress:13;
		u16 :3;
	};
	
	struct
	{
		u16 :1;
		u16 HalfwordAddress:12;
		u16 :3;
	};
} IOREG_W_TXBUF_GAP;								// 0x4808074

typedef union
{
	u16 value;
	
	struct
	{
		u16 HalfwordOffset:12;
		u16 :4;
	};
} IOREG_W_TXBUF_GAPDISP;							// 0x4808076

typedef union
{
	u16 value;
	
	struct
	{
		u16 HalfwordAddress:12;
		u16 Bit12:1;
		u16 IEEESeqCtrl:1;
		u16 UNKNOWN1:1;
		u16 TransferRequest:1;
	};
} IOREG_W_TXBUF_LOCATION;							// 0x4808080, 0x4808090, 0x4808094, 0x4808098, 0x48080A0, 0x48080A4, 0x48080A8

typedef union
{
	u16 value;
	
	struct
	{
		u16 Location:8;
		u16 :8;
	};
} IOREG_W_TXBUF_TIM;								// 0x4808084

typedef union
{
	u16 value;
	
	struct
	{
		u16 Count:8;
		u16 :8;
	};
} IOREG_W_LISTENCOUNT;								// 0x4808088

typedef union
{
	u16 value;
	
	struct
	{
		u16 Interval:10;
		u16 :6;
	};
} IOREG_W_BEACONINT;								// 0x480808C

typedef union
{
	u16 value;
	
	struct
	{
		u16 Interval:8;
		u16 :8;
	};
} IOREG_W_LISTENINT;								// 0x480808E

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 Loc1:1;
		u16 Cmd:1;
		u16 Loc2:1;
		u16 Loc3:1;
		u16 UNKNOWN1:1;
		u16 :3;
		
		u16 :8;
#else
		u16 :3;
		u16 UNKNOWN1:1;
		u16 Loc3:1;
		u16 Loc2:1;
		u16 Cmd:1;
		u16 Loc1:1;
		
		u16 :8;
#endif
	};
} IOREG_W_TXREQ_RESET;								// 0x48080AC

typedef IOREG_W_TXREQ_RESET IOREG_W_TXREQ_SET;		// 0x48080AE

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 Loc1:1;
		u16 Cmd:1;
		u16 Loc2:1;
		u16 Loc3:1;
		u16 UNKNOWN1:1;
		u16 :3;
		
		u16 :8;
#else
		u16 :3;
		u16 UNKNOWN1:1;
		u16 Loc3:1;
		u16 Loc2:1;
		u16 Cmd:1;
		u16 Loc1:1;
		
		u16 :8;
#endif
	};
} IOREG_W_TXREQ_READ;								// 0x48080B0

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 Loc1:1;
		u16 Cmd:1;
		u16 Loc2:1;
		u16 Loc3:1;
		u16 UNKNOWN1:2;
		u16 Reply2:1;
		u16 Reply1:1;
		
		u16 UNKNOWN2:8;
#else
		u16 Reply1:1;
		u16 Reply2:1;
		u16 UNKNOWN1:2;
		u16 Loc3:1;
		u16 Loc2:1;
		u16 Cmd:1;
		u16 Loc1:1;
		
		u16 UNKNOWN2:8;
#endif
	};
} IOREG_W_TXBUF_RESET;								// 0x48080B4

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 Loc1:1;
		u16 Cmd:1;
		u16 Loc2:1;
		u16 Loc3:1;
		u16 Beacon:1;
		u16 UNKNOWN1:2;
		u16 CmdReply:1;
		
		u16 UNKNOWN2:8;
#else
		u16 CmdReply:1;
		u16 UNKNOWN1:2;
		u16 Beacon:1;
		u16 Loc3:1;
		u16 Loc2:1;
		u16 Cmd:1;
		u16 Loc1:1;
		
		u16 UNKNOWN2:8;
#endif
	};
} IOREG_W_TXBUSY;									// 0x48080B6

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 PacketCompleted:1;
		u16 PacketFailed:1;
		u16 UNKNOWN1:6;
		
		u16 UNKNOWN2:4;
		u16 PacketUpdate:2;
		u16 UNKNOWN3:2;
#else
		u16 UNKNOWN1:6;
		u16 PacketFailed:1;
		u16 PacketCompleted:1;
		
		u16 UNKNOWN3:2;
		u16 PacketUpdate:2;
		u16 UNKNOWN2:4;
#endif
	};
} IOREG_W_TXSTAT;									// 0x48080B8

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 UNKNOWN1:1;
		u16 PreambleWithoutTX:1;
		u16 PreambleWithTX:1;
		u16 :5;
		
		u16 :8;
#else
		u16 :5;
		u16 PreambleWithTX:1;
		u16 PreambleWithoutTX:1;
		u16 UNKNOWN1:1;
		
		u16 :8;
#endif
	};
} IOREG_W_PREAMBLE;									// 0x48080BC

typedef u16 IOREG_W_CMD_TOTALTIME;					// 0x48080C0
typedef u16 IOREG_W_CMD_REPLYTIME;					// 0x48080C4

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 DisableNeedBSSID:1;
		u16 UNKNOWN1:6;
		u16 UNKNOWN2:1;
		
		u16 UNKNOWN3:1;
		u16 UNKNOWN4:1;
		u16 UNKNOWN5:1;
		u16 UNKNOWN6:1;
		u16 UNKNOWN7:1;
		u16 :3;
#else
		u16 UNKNOWN2:1;
		u16 UNKNOWN1:6;
		u16 DisableNeedBSSID:1;
		
		u16 :3;
		u16 UNKNOWN7:1;
		u16 UNKNOWN6:1;
		u16 UNKNOWN5:1;
		u16 UNKNOWN4:1;
		u16 UNKNOWN3:1;
#endif
	};
} IOREG_W_RXFILTER;									// 0x48080D0

typedef union
{
	u16 value;
	
	struct
	{
		u16 ReduceLenForNonWEP:8;
		u16 ReduceLenForWEP:8;
	};
} IOREG_W_RX_LEN_CROP;								// 0x48080DA

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 UNKNOWN1:1;
		u16 UNKNOWN2:1;
		u16 UNKNOWN3:1;
		u16 UNKNOWN4:1;
		u16 :4;
		
		u16 :8;
#else
		u16 :8;
		
		u16 :4;
		u16 UNKNOWN4:1;
		u16 UNKNOWN3:1;
		u16 UNKNOWN2:1;
		u16 UNKNOWN1:1;
#endif
	};
} IOREG_W_RXFILTER2;								// 0x48080E0

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 EnableCounter:1;
		u16 :7;
		
		u16 :8;
#else
		u16 :8;
		
		u16 :7;
		u16 EnableCounter:1;
#endif
	};
} IOREG_W_US_COUNTCNT;								// 0x48080E8

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 EnableCompare:1;
		u16 ForceIRQ14:1;
		u16 :6;
		
		u16 :8;
#else
		u16 :8;
		
		u16 :6;
		u16 ForceIRQ14:1;
		u16 EnableCompare:1;
#endif
	};
} IOREG_W_US_COMPARECNT;							// 0x48080EA

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 EnableCounter:1;
		u16 :7;
		
		u16 :8;
#else
		u16 :8;
		
		u16 :7;
		u16 EnableCounter:1;
#endif
	};
} IOREG_W_CMD_COUNTCNT;								// 0x48080EE

typedef u16 IOREG_W_CONTENTFREE;					// 0x480810C

typedef u16 IOREG_W_PRE_BEACON;						// 0x4808110

typedef u16 IOREG_W_CMD_COUNT;						// 0x4808118

typedef u16 IOREG_W_BEACONCOUNT1;					// 0x480811C

typedef u16 IOREG_W_BEACONCOUNT2;					// 0x4808134

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 Index:8;
		
		u16 :4;
		u16 Direction:4;
#else
		u16 Direction:4;
		u16 :4;
		
		u16 Index:8;
#endif
	};
} IOREG_W_BB_CNT;									// 0x4808158

typedef union
{
	u16 value;
	
	struct
	{
		u8 Data:8;
		u8 :8;
	};
} IOREG_W_BB_WRITE;									// 0x480815A

typedef union
{
	u16 value;
	
	struct
	{
		u8 Data:8;
		u8 :8;
	};
} IOREG_W_BB_READ;									// 0x480815C

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 Busy:1;
		u16 :7;
		
		u16 :8;
#else
		u16 :8;
		
		u16 :7;
		u16 Busy:1;
#endif
	};
} IOREG_W_BB_BUSY;									// 0x480815E

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 :8;
		
		u16 UNKNOWN1:1;
		u16 :5;
		u16 UNKNOWN2:1;
		u16 :1;
#else
		u16 :8;
		
		u16 :1;
		u16 UNKNOWN2:1;
		u16 :5;
		u16 UNKNOWN1:1;
#endif
	};
} IOREG_W_BB_MODE;									// 0x4808160

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 Disable:4;
		u16 :4;
		
		u16 :7;
		u16 DisableAllPorts:1;
#else
		u16 :4;
		u16 Disable:4;
		
		u16 DisableAllPorts:1;
		u16 :7;
#endif
	};
} IOREG_W_BB_POWER;									// 0x4808168

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 DataMSB:2;
		u16 Index:5;
		u16 ReadCommand:1;
		
		u16 :8;
#else
		u16 ReadCommand:1;
		u16 Index:5;
		u16 DataMSB:2;
		
		u16 :8;
#endif
	} Type2;
	
	struct
	{
#ifndef MSB_FIRST
		u16 Command:4;
		u16 :4;
		
		u16 :8;
#else
		u16 :4;
		u16 Command:4;
		
		u16 :8;
#endif
	} Type3;
} IOREG_W_RF_DATA2;									// 0x480817C

typedef union
{
	u16 value;
	
	struct
	{
		u16 DataLSB;
	} Type2;
	
	struct
	{
		u8 Data:8;
		u8 Index:8;
	} Type3;
} IOREG_W_RF_DATA1;									// 0x480817E

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 Busy:1;
		u16 :7;
		
		u16 :8;
#else
		u16 :7;
		u16 Busy:1;
		
		u16 :8;
#endif
	};
} IOREG_W_RF_BUSY;									// 0x4808180

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 TransferLen:6;
		u16 :2;
		
		u16 UNKNOWN1:1;
		u16 :5;
		u16 UNKNOWN2:1;
		u16 :1;
#else
		u16 :2;
		u16 TransferLen:6;
		
		u16 :1;
		u16 UNKNOWN2:1;
		u16 :5;
		u16 UNKNOWN1:1;
#endif
	};
} IOREG_W_RF_CNT;									// 0x4808184

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 Duration:1;
		u16 FrameCheckSeq:1;
		u16 SeqControl:1;
		u16 :5;
		
		u16 :8;
#else
		u16 :8;
		
		u16 :5;
		u16 SeqControl:1;
		u16 FrameCheckSeq:1;
		u16 Duration:1;
#endif
	};
} IOREG_W_TX_HDR_CNT;								// 0x4808194

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 CarrierSense:1;
		u16 TXMain:1;
		u16 UNKNOWN1:1;
		u16 :3;
		u16 TX_On:1;
		u16 RX_On:1;
		
		u16 :8;
#else
		u16 :8;
		
		u16 RX_On:1;
		u16 TX_On:1;
		u16 :3;
		u16 UNKNOWN1:1;
		u16 TXMain:1;
		u16 CarrierSense:1;
#endif
	};
} IOREG_W_RF_PINS;									// 0x480819C

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 UNKNOWN1:2;
		u16 :2;
		u16 UNKNOWN2:2;
		u16 :2;
		
		u16 UNKNOWN3:1;
		u16 :2;
		u16 UNKNOWN4:1;
		u16 :4;
#else
		u16 :4;
		u16 UNKNOWN4:1;
		u16 :2;
		u16 UNKNOWN3:1;
		
		u16 :2;
		u16 UNKNOWN2:2;
		u16 :2;
		u16 UNKNOWN1:2;
#endif
	};
} IOREG_W_X_1A0;									// 0x48081A0

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 UNKNOWN1:2;
		u16 :6;
		
		u16 :8;
#else
		u16 :8;
		
		u16 :6;
		u16 UNKNOWN1:2;
#endif
	};
} IOREG_W_X_1A2;									// 0x48081A2

typedef u16 IOREG_W_X_1A4;							// 0x48081A4

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 RX0L:1; // Bit 0
		u16 RX1L:1; // Bit 1
		u16 RX1H:1; // Bit 2
		u16 RX2L:1; // Bit 3
		u16 RX2H:1; // Bit 4
		u16 RX3L:1; // Bit 5
		u16 RX3H:1; // Bit 6
		u16 RX4L:1; // Bit 7
		
		u16 RX5L:1; // Bit 8
		u16 RX6L:1; // Bit 9
		u16 RX6H:1; // Bit 10
		u16 RX7L:1; // Bit 11
		u16 RX7H:1; // Bit 12
		u16 :3;
#else
		u16 :3;
		u16 RX7H:1; // Bit 12
		u16 RX7L:1; // Bit 11
		u16 RX6H:1; // Bit 10
		u16 RX6L:1; // Bit 9
		u16 RX5L:1; // Bit 8
		
		u16 RX4L:1; // Bit 7
		u16 RX3H:1; // Bit 6
		u16 RX3L:1; // Bit 5
		u16 RX2H:1; // Bit 4
		u16 RX2L:1; // Bit 3
		u16 RX1H:1; // Bit 2
		u16 RX1L:1; // Bit 1
		u16 RX0L:1; // Bit 0
#endif
	};
} IOREG_W_RXSTAT_INC_IF;							// 0x48081A8

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 RX0L:1; // Bit 0
		u16 RX1L:1; // Bit 1
		u16 RX1H:1; // Bit 2
		u16 RX2L:1; // Bit 3
		u16 RX2H:1; // Bit 4
		u16 RX3L:1; // Bit 5
		u16 RX3H:1; // Bit 6
		u16 RX4L:1; // Bit 7
		
		u16 RX5L:1; // Bit 8
		u16 RX6L:1; // Bit 9
		u16 RX6H:1; // Bit 10
		u16 RX7L:1; // Bit 11
		u16 RX7H:1; // Bit 12
		u16 UNKNOWN1:3;
#else
		u16 UNKNOWN1:3;
		u16 RX7H:1; // Bit 12
		u16 RX7L:1; // Bit 11
		u16 RX6H:1; // Bit 10
		u16 RX6L:1; // Bit 9
		u16 RX5L:1; // Bit 8
		
		u16 RX4L:1; // Bit 7
		u16 RX3H:1; // Bit 6
		u16 RX3L:1; // Bit 5
		u16 RX2H:1; // Bit 4
		u16 RX2L:1; // Bit 3
		u16 RX1H:1; // Bit 2
		u16 RX1L:1; // Bit 1
		u16 RX0L:1; // Bit 0
#endif
	};
} IOREG_W_RXSTAT_INC_IE;							// 0x48081AA

typedef IOREG_W_RXSTAT_INC_IF IOREG_W_RXSTAT_OVF_IF;	// 0x48081AC
typedef IOREG_W_RXSTAT_INC_IE IOREG_W_RXSTAT_OVF_IE;	// 0x48081AE

typedef union
{
	u8 count[16];
	
	struct
	{
		u16 RXSTAT_1B0;								// 0x48081B0
		u16 RXSTAT_1B2;								// 0x48081B2
		u16 RXSTAT_1B4;								// 0x48081B4
		u16 RXSTAT_1B6;								// 0x48081B6
		
		u16 RXSTAT_1B8;								// 0x48081B8
		u16 RXSTAT_1BA;								// 0x48081BA
		u16 RXSTAT_1BC;								// 0x48081BC
		u16 RXSTAT_1BE;								// 0x48081BE
	};
	
	struct
	{
		u8 UNKNOWN1;
		u8 _unused0;
		u8 RateError;
		u8 LengthError;
		u8 BufferFullError;
		u8 UNKNOWN2;
		u8 WrongFCSError;
		u8 PacketHeaderError;
		
		u8 UNKNOWN3;
		u8 _unused1;
		u8 UNKNOWN4;
		u8 _unused2;
		u8 WEPError;
		u8 UNKNOWN5;
		u8 SeqControl;
		u8 UNKNOWN6;
	};
} IOREG_W_RXSTAT_COUNT;								// 0x48081B0 - 0x48081BF

typedef union
{
	u16 value;
	
	struct
	{
		u16 Count:8;
		u16 :8;
	};
} IOREG_W_TX_ERR_COUNT;								// 0x48081C0

typedef union
{
	u16 value;
	
	struct
	{
		u16 OkayCount:8;
		u16 ErrorCount:8;
	};
} IOREG_W_RX_COUNT;									// 0x48081C4

typedef union
{
	u8 count[16];
	
	struct
	{
		u16 CMD_STAT_1D0;							// 0x48081D0
		u16 CMD_STAT_1D2;							// 0x48081D2
		u16 CMD_STAT_1D4;							// 0x48081D4
		u16 CMD_STAT_1D6;							// 0x48081D6
		u16 CMD_STAT_1D8;							// 0x48081D8
		u16 CMD_STAT_1DA;							// 0x48081DA
		u16 CMD_STAT_1DC;							// 0x48081DC
		u16 CMD_STAT_1DE;							// 0x48081DE
	};
} IOREG_W_CMD_STAT_COUNT;							// 0x48081D0 - 0x48081DF

typedef union
{
	u16 value;
	
	struct
	{
		u16 Number:12;
		u16 :4;
	};
} IOREG_W_TX_SEQNO;									// 0x4808210

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 RFStatus:4;
		u16 :4;
		
		u16 :8;
#else
		u16 :8;
		
		u16 :4;
		u16 RFStatus:4;
#endif
	};
} IOREG_W_RF_STATUS;								// 0x4808214

typedef u16 IOREG_W_X_228;							// 0x4808228
typedef u16 IOREG_W_X_244;							// 0x4808244

typedef union
{
	u16 value;
	
	struct
	{
		u16 HalfwordAddress:12;
		u16 :4;
	};
} IOREG_W_RXTX_ADDR;								// 0x4808268

typedef union
{
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 UNKNOWN1:1;
		u16 :7;
		
		u16 :8;
#else
		u16 :8;
		
		u16 :7;
		u16 UNKNOWN1:1;
#endif
	};
} IOREG_W_X_290;									// 0x4808290

typedef u16 IOREG_W_POWERACK;						// 0x48082D0

// WIFI_IOREG_MAP: the buildin mac (arm7 addressrange: 0x04800000-0x04FFFFFF )
// http://www.akkit.org/info/dswifi.htm#WifiIOMap
#include "PACKED.h"
typedef struct
{
	IOREG_W_ID					ID;					// 0x4808000
	IOREG_W_PADDING				pad_002;			// 0x4808002
	IOREG_W_MODE_RST			MODE_RST;			// 0x4808004
	IOREG_W_MODE_WEP			MODE_WEP;			// 0x4808006
	IOREG_W_TXSTATCNT			TXSTATCNT;			// 0x4808008
	IOREG_W_X_00A				X_00A;				// 0x480800A
	IOREG_W_PADDING				pad_00C;			// 0x480800C
	IOREG_W_PADDING				pad_00E;			// 0x480800E
	IOREG_W_IF					IF;					// 0x4808010
	IOREG_W_IE					IE;					// 0x4808012
	IOREG_W_PADDING				pad_014;			// 0x4808014
	IOREG_W_PADDING				pad_016;			// 0x4808016
	
	union
	{
		u8						MACADDR[6];			// 48-bit MAC address of the console. Should be initialized from firmware.
		
		struct
		{
			IOREG_W_MACADDR0	MACADDR0;			// 0x4808018
			IOREG_W_MACADDR1	MACADDR1;			// 0x480801A
			IOREG_W_MACADDR2	MACADDR2;			// 0x480801C
		};
	};
	
	IOREG_W_PADDING				pad_01E;			// 0x480801E
	
	union
	{
		u8						BSSID[6];			// 48-bit MAC address of the console. Should be initialized from firmware.
		
		struct
		{
			IOREG_W_BSSID0		BSSID0;				// 0x4808020
			IOREG_W_BSSID1		BSSID1;				// 0x4808022
			IOREG_W_BSSID2		BSSID2;				// 0x4808024
		};
	};
	
	IOREG_W_PADDING				pad_026;			// 0x4808026
	
	IOREG_W_AID_LOW				AID_LOW;			// 0x4808028
	IOREG_W_AID_FULL			AID_FULL;			// 0x480802A
	IOREG_W_TX_RETRYLIMIT		TX_RETRYLIMIT;		// 0x480802C
	IOREG_W_INTERNAL_02E		INTERNAL_02E;		// 0x480802E
	IOREG_W_RXCNT				RXCNT;				// 0x4808030
	IOREG_W_WEP_CNT				WEP_CNT;			// 0x4808032
	IOREG_W_INTERNAL_034		INTERNAL_034;		// 0x4808034
	
	IOREG_W_POWER_US			POWER_US;			// 0x4808036
	IOREG_W_POWER_TX			POWER_TX;			// 0x4808038
	IOREG_W_PADDING				pad_03A;			// 0x480803A
	IOREG_W_POWERSTATE			POWERSTATE;			// 0x480803C
	IOREG_W_PADDING				pad_03E;			// 0x480803E
	IOREG_W_POWERFORCE			POWERFORCE;			// 0x4808040
	IOREG_W_PADDING				pad_042;			// 0x4808042
	IOREG_W_RANDOM				RANDOM;				// 0x4808044
	IOREG_W_PADDING				pad_046;			// 0x4808046
	IOREG_W_POWER_UNKNOWN		POWER_UNKNOWN;		// 0x4808048
	
	IOREG_W_PADDING				pad_04A;			// 0x480804A
	IOREG_W_PADDING				pad_04C;			// 0x480804C
	IOREG_W_PADDING				pad_04E;			// 0x480804E
	
	IOREG_W_RXBUF_BEGIN			RXBUF_BEGIN;		// 0x4808050
	IOREG_W_RXBUF_END			RXBUF_END;			// 0x4808052
	IOREG_W_RXBUF_WRCSR			RXBUF_WRCSR;		// 0x4808054
	IOREG_W_RXBUF_WR_ADDR		RXBUF_WR_ADDR;		// 0x4808056
	IOREG_W_RXBUF_RD_ADDR		RXBUF_RD_ADDR;		// 0x4808058
	IOREG_W_RXBUF_READCSR		RXBUF_READCSR;		// 0x480805A
	IOREG_W_RXBUF_COUNT			RXBUF_COUNT;		// 0x480805C
	IOREG_W_PADDING				pad_05E;			// 0x480805E
	IOREG_W_RXBUF_RD_DATA		RXBUF_RD_DATA;		// 0x4808060
	IOREG_W_RXBUF_GAP			RXBUF_GAP;			// 0x4808062
	IOREG_W_RXBUF_GAPDISP		RXBUF_GAPDISP;		// 0x4808064
	IOREG_W_PADDING				pad_066;			// 0x4808066
	IOREG_W_TXBUF_WR_ADDR		TXBUF_WR_ADDR;		// 0x4808068
	IOREG_W_PADDING				pad_06A;			// 0x480806A
	IOREG_W_TXBUF_COUNT			TXBUF_COUNT;		// 0x480806C
	IOREG_W_PADDING				pad_06E;			// 0x480806E
	IOREG_W_TXBUF_WR_DATA		TXBUF_WR_DATA;		// 0x4808070
	IOREG_W_PADDING				pad_072;			// 0x4808072
	IOREG_W_TXBUF_GAP			TXBUF_GAP;			// 0x4808074
	IOREG_W_TXBUF_GAPDISP		TXBUF_GAPDISP;		// 0x4808076
	
	IOREG_W_INTERNAL_078		INTERNAL_078;		// 0x4808078
	IOREG_W_PADDING				pad_07A;			// 0x480807A
	IOREG_W_PADDING				pad_07C;			// 0x480807C
	IOREG_W_PADDING				pad_07E;			// 0x480807E
	IOREG_W_TXBUF_LOCATION		TXBUF_BEACON;		// 0x4808080
	IOREG_W_PADDING				pad_082;			// 0x4808082
	IOREG_W_TXBUF_TIM			TXBUF_TIM;			// 0x4808084
	IOREG_W_PADDING				pad_086;			// 0x4808086
	IOREG_W_LISTENCOUNT			LISTENCOUNT;		// 0x4808088
	IOREG_W_PADDING				pad_08A;			// 0x480808A
	IOREG_W_BEACONINT			BEACONINT;			// 0x480808C
	IOREG_W_LISTENINT			LISTENINT;			// 0x480808E
	IOREG_W_TXBUF_LOCATION		TXBUF_CMD;			// 0x4808090
	IOREG_W_PADDING				pad_092;			// 0x4808092
	IOREG_W_TXBUF_LOCATION		TXBUF_REPLY1;		// 0x4808094
	IOREG_W_PADDING				pad_096;			// 0x4808096
	IOREG_W_TXBUF_LOCATION		TXBUF_REPLY2;		// 0x4808098
	IOREG_W_PADDING				pad_09A;			// 0x480809A
	IOREG_W_INTERNAL_09C		INTERNAL_09C;		// 0x480809C
	IOREG_W_PADDING				pad_09E;			// 0x480809E
	IOREG_W_TXBUF_LOCATION		TXBUF_LOC1;			// 0x48080A0
	IOREG_W_PADDING				pad_0A2;			// 0x48080A2
	IOREG_W_TXBUF_LOCATION		TXBUF_LOC2;			// 0x48080A4
	IOREG_W_PADDING				pad_0A6;			// 0x48080A6
	IOREG_W_TXBUF_LOCATION		TXBUF_LOC3;			// 0x48080A8
	IOREG_W_PADDING				pad_0AA;			// 0x48080AA
	IOREG_W_TXREQ_RESET			TXREQ_RESET;		// 0x48080AC
	IOREG_W_TXREQ_SET			TXREQ_SET;			// 0x48080AE
	IOREG_W_TXREQ_READ			TXREQ_READ;			// 0x48080B0
	IOREG_W_PADDING				pad_0B2;			// 0x48080B2
	IOREG_W_TXBUF_RESET			TXBUF_RESET;		// 0x48080B4
	IOREG_W_TXBUSY				TXBUSY;				// 0x48080B6
	IOREG_W_TXSTAT				TXSTAT;				// 0x48080B8
	IOREG_W_INTERNAL_0BA		INTERNAL_0BA;		// 0x48080BA
	IOREG_W_PREAMBLE			PREAMBLE;			// 0x48080BC
	IOREG_W_PADDING				pad_0BE;			// 0x48080BE
	IOREG_W_CMD_TOTALTIME		CMD_TOTALTIME;		// 0x48080C0
	IOREG_W_PADDING				pad_0C2;			// 0x48080C2
	IOREG_W_CMD_REPLYTIME		CMD_REPLYTIME;		// 0x48080C4
	IOREG_W_PADDING				pad_0C6;			// 0x48080C6
	IOREG_W_INTERNAL_0C8		INTERNAL_0C8;		// 0x48080C8
	IOREG_W_PADDING				pad_0CA;			// 0x48080CA
	IOREG_W_PADDING				pad_0CC;			// 0x48080CC
	IOREG_W_PADDING				pad_0CE;			// 0x48080CE
	IOREG_W_RXFILTER			RXFILTER;			// 0x48080D0
	IOREG_W_PADDING				pad_0D2;			// 0x48080D2
	IOREG_W_CONFIG_0D4			CONFIG_0D4;			// 0x48080D4
	IOREG_W_PADDING				pad_0D6;			// 0x48080D6
	IOREG_W_CONFIG_0D8			CONFIG_0D8;			// 0x48080D8
	IOREG_W_RX_LEN_CROP			RX_LEN_CROP;		// 0x48080DA
	IOREG_W_PADDING				pad_0DC;			// 0x48080DC
	IOREG_W_PADDING				pad_0DE;			// 0x48080DE
	IOREG_W_RXFILTER2			RXFILTER2;			// 0x48080E0
	IOREG_W_PADDING				pad_0E2;			// 0x48080E2
	IOREG_W_PADDING				pad_0E4;			// 0x48080E4
	IOREG_W_PADDING				pad_0E6;			// 0x48080E6
	
	IOREG_W_US_COUNTCNT			US_COUNTCNT;		// 0x48080E8
	IOREG_W_US_COMPARECNT		US_COMPARECNT;		// 0x48080EA
	IOREG_W_CONFIG_0EC			CONFIG_0EC;			// 0x48080EC
	IOREG_W_CMD_COUNTCNT		CMD_COUNTCNT;		// 0x48080EE
	
	union
	{
		u64 US_COMPARE;								// 0x48080F0 - 0x48080F7
		
		struct
		{
			u16	US_COMPARE0;						// 0x48080F0
			u16	US_COMPARE1;						// 0x48080F2
			u16	US_COMPARE2;						// 0x48080F4
			u16	US_COMPARE3;						// 0x48080F6
		};
		
		struct
		{
			u64 UNKNOWN1:1;
			u64 :9;
			u64 MS_Compare:54;
		};
	};
	
	union
	{
		u64 US_COUNT;								// 0x48080F8 - 0x48080FF
		
		struct
		{
			u16	US_COUNT0;							// 0x48080F8
			u16	US_COUNT1;							// 0x48080FA
			u16	US_COUNT2;							// 0x48080FC
			u16	US_COUNT3;							// 0x48080FE
		};
	};
	
	IOREG_W_INTERNAL_100		INTERNAL_100;		// 0x4808100
	IOREG_W_INTERNAL_102		INTERNAL_102;		// 0x4808102
	IOREG_W_INTERNAL_104		INTERNAL_104;		// 0x4808104
	IOREG_W_INTERNAL_106		INTERNAL_106;		// 0x4808106
	IOREG_W_PADDING				pad_108;			// 0x4808108
	IOREG_W_PADDING				pad_10A;			// 0x480810A
	IOREG_W_CONTENTFREE			CONTENTFREE;		// 0x480810C
	IOREG_W_PADDING				pad_10E;			// 0x480810E
	IOREG_W_PRE_BEACON			PRE_BEACON;			// 0x4808110
	IOREG_W_PADDING				pad_112;			// 0x4808112
	IOREG_W_PADDING				pad_114;			// 0x4808114
	IOREG_W_PADDING				pad_116;			// 0x4808116
	IOREG_W_CMD_COUNT			CMD_COUNT;			// 0x4808118
	IOREG_W_PADDING				pad_11A;			// 0x480811A
	IOREG_W_BEACONCOUNT1		BEACONCOUNT1;		// 0x480811C
	IOREG_W_PADDING				pad_11E;			// 0x480811E
	
	IOREG_W_CONFIG_120			CONFIG_120;			// 0x4808120
	IOREG_W_CONFIG_122			CONFIG_122;			// 0x4808122
	IOREG_W_CONFIG_124			CONFIG_124;			// 0x4808124
	IOREG_W_INTERNAL_126		INTERNAL_126;		// 0x4808126
	IOREG_W_CONFIG_128			CONFIG_128;			// 0x4808128
	IOREG_W_INTERNAL_12A		INTERNAL_12A;		// 0x480812A
	IOREG_W_PADDING				pad_12C;			// 0x480812C
	IOREG_W_PADDING				pad_12E;			// 0x480812E
	IOREG_W_CONFIG_130			CONFIG_130;			// 0x4808130
	IOREG_W_CONFIG_132			CONFIG_132;			// 0x4808132
	IOREG_W_BEACONCOUNT2		BEACONCOUNT2;		// 0x4808134
	IOREG_W_PADDING				pad_136;			// 0x4808136
	IOREG_W_PADDING				pad_138;			// 0x4808138
	IOREG_W_PADDING				pad_13A;			// 0x480813A
	IOREG_W_PADDING				pad_13C;			// 0x480813C
	IOREG_W_PADDING				pad_13E;			// 0x480813E
	IOREG_W_CONFIG_140			CONFIG_140;			// 0x4808140
	IOREG_W_CONFIG_142			CONFIG_142;			// 0x4808142
	IOREG_W_CONFIG_144			CONFIG_144;			// 0x4808144
	IOREG_W_CONFIG_146			CONFIG_146;			// 0x4808146
	IOREG_W_CONFIG_148			CONFIG_148;			// 0x4808148
	IOREG_W_CONFIG_14A			CONFIG_14A;			// 0x480814A
	IOREG_W_CONFIG_14C			CONFIG_14C;			// 0x404814C
	IOREG_W_PADDING				pad_14E;			// 0x480814E
	IOREG_W_CONFIG_150			CONFIG_150;			// 0x4808150
	IOREG_W_PADDING				pad_152;			// 0x4808152
	IOREG_W_CONFIG_154			CONFIG_154;			// 0x4808154
	IOREG_W_PADDING				pad_156;			// 0x4808156
	
	IOREG_W_BB_CNT				BB_CNT;				// 0x4808158
	IOREG_W_BB_WRITE			BB_WRITE;			// 0x480815A
	IOREG_W_BB_READ				BB_READ;			// 0x480815C
	IOREG_W_BB_BUSY				BB_BUSY;			// 0x480815E
	IOREG_W_BB_MODE				BB_MODE;			// 0x4808160
	IOREG_W_PADDING				pad_162;			// 0x4808162
	IOREG_W_PADDING				pad_164;			// 0x4808164
	IOREG_W_PADDING				pad_166;			// 0x4808166
	IOREG_W_BB_POWER			BB_POWER;			// 0x4808168
	
	IOREG_W_INTERNAL_16A		INTERNAL_16A;		// 0x480816A
	IOREG_W_PADDING				pad_16C;			// 0x480816C
	IOREG_W_PADDING				pad_16E;			// 0x480816E
	IOREG_W_INTERNAL_170		INTERNAL_170;		// 0x4808170
	IOREG_W_INTERNAL_172		INTERNAL_172;		// 0x4808172
	IOREG_W_INTERNAL_174		INTERNAL_174;		// 0x4808174
	IOREG_W_INTERNAL_176		INTERNAL_176;		// 0x4808176
	IOREG_W_INTERNAL_178		INTERNAL_178;		// 0x4808178
	IOREG_W_PADDING				pad_17A;			// 0x480817A
	
	IOREG_W_RF_DATA2			RF_DATA2;			// 0x480817C
	IOREG_W_RF_DATA1			RF_DATA1;			// 0x480817E
	IOREG_W_RF_BUSY				RF_BUSY;			// 0x4808180
	IOREG_W_PADDING				pad_182;			// 0x4808182
	IOREG_W_RF_CNT				RF_CNT;				// 0x4808184
	IOREG_W_PADDING				pad_186;			// 0x4808186
	IOREG_W_PADDING				pad_188;			// 0x4808188
	IOREG_W_PADDING				pad_18A;			// 0x480818A
	IOREG_W_PADDING				pad_18C;			// 0x480818C
	IOREG_W_PADDING				pad_18E;			// 0x480818E
	
	IOREG_W_INTERNAL_190		INTERNAL_190;		// 0x4808190
	IOREG_W_PADDING				pad_192;			// 0x4808192
	IOREG_W_TX_HDR_CNT			TX_HDR_CNT;			// 0x4808194
	IOREG_W_PADDING				pad_196;			// 0x4808196
	IOREG_W_INTERNAL_198		INTERNAL_198;		// 0x4808198
	IOREG_W_PADDING				pad_19A;			// 0x480819A
	IOREG_W_RF_PINS				RF_PINS;			// 0x480819C
	IOREG_W_PADDING				pad_19E;			// 0x480819E
	IOREG_W_X_1A0				X_1A0;				// 0x48081A0
	IOREG_W_X_1A2				X_1A2;				// 0x48081A2
	IOREG_W_X_1A4				X_1A4;				// 0x48081A4
	IOREG_W_PADDING				pad_1A6;			// 0x48081A6
	
	IOREG_W_RXSTAT_INC_IF		RXSTAT_INC_IF;		// 0x48081A8
	IOREG_W_RXSTAT_INC_IE		RXSTAT_INC_IE;		// 0x48081AA
	IOREG_W_RXSTAT_OVF_IF		RXSTAT_OVF_IF;		// 0x48081AC
	IOREG_W_RXSTAT_OVF_IE		RXSTAT_OVF_IE;		// 0x48081AE
	
	IOREG_W_RXSTAT_COUNT		RXSTAT_COUNT;		// 0x48081B0 - 0x48081BF
	
	IOREG_W_TX_ERR_COUNT		TX_ERR_COUNT;		// 0x48081C0
	IOREG_W_PADDING				pad_1C2;			// 0x48081C2
	IOREG_W_RX_COUNT			RX_COUNT;			// 0x48081C4
	IOREG_W_PADDING				pad_1C6;			// 0x48081C6
	IOREG_W_PADDING				pad_1C8;			// 0x48081C8
	IOREG_W_PADDING				pad_1CA;			// 0x48081CA
	IOREG_W_PADDING				pad_1CC;			// 0x48081CC
	IOREG_W_PADDING				pad_1CE;			// 0x48081CE
	
	IOREG_W_CMD_STAT_COUNT		CMD_STAT_COUNT;		// 0x48081D0 - 0x48081DF
	
	IOREG_W_PADDING				pad_1E0;			// 0x48081E0
	IOREG_W_PADDING				pad_1E2;			// 0x48081E2
	IOREG_W_PADDING				pad_1E4;			// 0x48081E4
	IOREG_W_PADDING				pad_1E6;			// 0x48081E6
	IOREG_W_PADDING				pad_1E8;			// 0x48081E8
	IOREG_W_PADDING				pad_1EA;			// 0x48081EA
	IOREG_W_PADDING				pad_1EC;			// 0x48081EC
	IOREG_W_PADDING				pad_1EE;			// 0x48081EE
	IOREG_W_INTERNAL_1F0		INTERNAL_1F0;		// 0x48081F0
	IOREG_W_PADDING				pad_1F2;			// 0x48081F2
	IOREG_W_PADDING				pad_1F4;			// 0x48081F4
	IOREG_W_PADDING				pad_1F6;			// 0x48081F6
	IOREG_W_PADDING				pad_1F8;			// 0x48081F8
	IOREG_W_PADDING				pad_1FA;			// 0x48081FA
	IOREG_W_PADDING				pad_1FC;			// 0x48081FC
	IOREG_W_PADDING				pad_1FE;			// 0x48081FE
	IOREG_W_PADDING				pad_200;			// 0x4808200
	IOREG_W_PADDING				pad_202;			// 0x4808202
	IOREG_W_INTERNAL_204		INTERNAL_204;		// 0x4808204
	IOREG_W_PADDING				pad_206;			// 0x4808206
	IOREG_W_INTERNAL_208		INTERNAL_208;		// 0x4808208
	IOREG_W_PADDING				pad_20A;			// 0x480820A
	IOREG_W_INTERNAL_20C		INTERNAL_20C;		// 0x480820C
	IOREG_W_PADDING				pad_20E;			// 0x480820E
	IOREG_W_TX_SEQNO			TX_SEQNO;			// 0x4808210
	IOREG_W_PADDING				pad_212;			// 0x4808212
	IOREG_W_RF_STATUS			RF_STATUS;			// 0x4808214
	IOREG_W_PADDING				pad_216;			// 0x4808216
	IOREG_W_PADDING				pad_218;			// 0x4808218
	IOREG_W_PADDING				pad_21A;			// 0x480821A
	IOREG_W_IF_SET				IF_SET;				// 0x480821C
	IOREG_W_PADDING				pad_21E;			// 0x480821E
	IOREG_W_INTERNAL_220		INTERNAL_220;		// 0x4808220
	IOREG_W_PADDING				pad_222;			// 0x4808222
	IOREG_W_INTERNAL_224		INTERNAL_224;		// 0x4808224
	IOREG_W_PADDING				pad_226;			// 0x4808226
	IOREG_W_X_228				X_228;				// 0x4808228
	IOREG_W_PADDING				pad_22A;			// 0x480822A
	IOREG_W_PADDING				pad_22C;			// 0x480822C
	IOREG_W_PADDING				pad_22E;			// 0x480822E
	IOREG_W_INTERNAL_230		INTERNAL_230;		// 0x4808230
	IOREG_W_PADDING				pad_232;			// 0x4808232
	IOREG_W_INTERNAL_234		INTERNAL_234;		// 0x4808234
	IOREG_W_PADDING				pad_236;			// 0x4808236
	IOREG_W_INTERNAL_238		INTERNAL_238;		// 0x4808238
	IOREG_W_PADDING				pad_23A;			// 0x480823A
	IOREG_W_INTERNAL_23C		INTERNAL_23C;		// 0x480823C
	IOREG_W_PADDING				pad_23E;			// 0x480823E
	IOREG_W_PADDING				pad_240;			// 0x4808240
	IOREG_W_PADDING				pad_242;			// 0x4808242
	IOREG_W_X_244				X_244;				// 0x4808244
	IOREG_W_PADDING				pad_246;			// 0x4808246
	IOREG_W_INTERNAL_248		INTERNAL_248;		// 0x4808248
	IOREG_W_PADDING				pad_24A;			// 0x480824A
	IOREG_W_INTERNAL_24C		INTERNAL_24C;		// 0x480824C
	IOREG_W_INTERNAL_24E		INTERNAL_24E;		// 0x480824E
	IOREG_W_INTERNAL_250		INTERNAL_250;		// 0x4808250
	IOREG_W_PADDING				pad_252;			// 0x4808252
	IOREG_W_CONFIG_254			CONFIG_254;			// 0x4808254
	IOREG_W_PADDING				pad_256;			// 0x4808256
	IOREG_W_INTERNAL_258		INTERNAL_258;		// 0x4808258
	IOREG_W_PADDING				pad_25A;			// 0x480825A
	IOREG_W_INTERNAL_25C		INTERNAL_25C;		// 0x480825C
	IOREG_W_PADDING				pad_25E;			// 0x480825E
	IOREG_W_INTERNAL_260		INTERNAL_260;		// 0x4808260
	IOREG_W_PADDING				pad_262;			// 0x4808262
	IOREG_W_INTERNAL_264		INTERNAL_264;		// 0x4808264
	IOREG_W_PADDING				pad_266;			// 0x4808266
	IOREG_W_RXTX_ADDR			RXTX_ADDR;			// 0x4808268
	IOREG_W_PADDING				pad_26A;			// 0x480826A
	IOREG_W_PADDING				pad_26C;			// 0x480826C
	IOREG_W_PADDING				pad_26E;			// 0x480826E
	IOREG_W_INTERNAL_270		INTERNAL_270;		// 0x4808270
	IOREG_W_PADDING				pad_272;			// 0x4808272
	IOREG_W_INTERNAL_274		INTERNAL_274;		// 0x4808274
	IOREG_W_PADDING				pad_276;			// 0x4808276
	IOREG_W_INTERNAL_278		INTERNAL_278;		// 0x4808278
	IOREG_W_PADDING				pad_27A;			// 0x480827A
	IOREG_W_INTERNAL_27C		INTERNAL_27C;		// 0x480827C
	IOREG_W_PADDING				pad_27E;			// 0x480827E
	IOREG_W_PADDING				pad_280;			// 0x4808280
	IOREG_W_PADDING				pad_282;			// 0x4808282
	IOREG_W_PADDING				pad_284;			// 0x4808284
	IOREG_W_PADDING				pad_286;			// 0x4808286
	IOREG_W_PADDING				pad_288;			// 0x4808288
	IOREG_W_PADDING				pad_28A;			// 0x480828A
	IOREG_W_PADDING				pad_28C;			// 0x480828C
	IOREG_W_PADDING				pad_28E;			// 0x480828E
	IOREG_W_X_290				X_290;				// 0x4808290
	IOREG_W_PADDING				pad_292;			// 0x4808292
	IOREG_W_PADDING				pad_294;			// 0x4808294
	IOREG_W_PADDING				pad_296;			// 0x4808296
	IOREG_W_INTERNAL_298		INTERNAL_298;		// 0x4808298
	IOREG_W_PADDING				pad_29A;			// 0x480829A
	IOREG_W_PADDING				pad_29C;			// 0x480829C
	IOREG_W_PADDING				pad_29E;			// 0x480829E
	IOREG_W_INTERNAL_2A0		INTERNAL_2A0;		// 0x48082A0
	IOREG_W_INTERNAL_2A2		INTERNAL_2A2;		// 0x48082A2
	IOREG_W_INTERNAL_2A4		INTERNAL_2A4;		// 0x48082A4
	IOREG_W_PADDING				pad_2A6;			// 0x48082A6
	IOREG_W_INTERNAL_2A8		INTERNAL_2A8;		// 0x48082A8
	IOREG_W_PADDING				pad_2AA;			// 0x48082AA
	IOREG_W_INTERNAL_2AC		INTERNAL_2AC;		// 0x48082AC
	IOREG_W_PADDING				pad_2AE;			// 0x48082AE
	IOREG_W_INTERNAL_2B0		INTERNAL_2B0;		// 0x48082B0
	IOREG_W_PADDING				pad_2B2;			// 0x48082B2
	IOREG_W_INTERNAL_2B4		INTERNAL_2B4;		// 0x48082B4
	IOREG_W_PADDING				pad_2B6;			// 0x48082B6
	IOREG_W_INTERNAL_2B8		INTERNAL_2B8;		// 0x48082B8
	IOREG_W_PADDING				pad_2BA;			// 0x48082BA
	IOREG_W_PADDING				pad_2BC;			// 0x48082BC
	IOREG_W_PADDING				pad_2BE;			// 0x48082BE
	IOREG_W_INTERNAL_2C0		INTERNAL_2C0;		// 0x48082C0
	IOREG_W_PADDING				pad_2C2;			// 0x48082C2
	IOREG_W_INTERNAL_2C4		INTERNAL_2C4;		// 0x48082C4
	IOREG_W_PADDING				pad_2C6;			// 0x48082C6
	IOREG_W_INTERNAL_2C8		INTERNAL_2C8;		// 0x48082C8
	IOREG_W_PADDING				pad_2CA;			// 0x48082CA
	IOREG_W_INTERNAL_2CC		INTERNAL_2CC;		// 0x48082CC
	IOREG_W_PADDING				pad_2CE;			// 0x48082CE
	IOREG_W_POWERACK			POWERACK;			// 0x48082D0
	IOREG_W_PADDING				pad_2D2;			// 0x48082D2
	IOREG_W_PADDING				pad_2D4;			// 0x48082D4
	IOREG_W_PADDING				pad_2D6;			// 0x48082D6
	IOREG_W_PADDING				pad_2D8;			// 0x48082D8
	IOREG_W_PADDING				pad_2DA;			// 0x48082DA
	IOREG_W_PADDING				pad_2DC;			// 0x48082DC
	IOREG_W_PADDING				pad_2DE;			// 0x48082DE
	IOREG_W_PADDING				pad_2E0;			// 0x48082E0
	IOREG_W_PADDING				pad_2E2;			// 0x48082E2
	IOREG_W_PADDING				pad_2E4;			// 0x48082E4
	IOREG_W_PADDING				pad_2E6;			// 0x48082E6
	IOREG_W_PADDING				pad_2E8;			// 0x48082E8
	IOREG_W_PADDING				pad_2EA;			// 0x48082EA
	IOREG_W_PADDING				pad_2EC;			// 0x48082EC
	IOREG_W_PADDING				pad_2EE;			// 0x48082EE
	IOREG_W_INTERNAL_2F0		INTERNAL_2F0;		// 0x48082F0
	IOREG_W_INTERNAL_2F2		INTERNAL_2F2;		// 0x48082F2
	IOREG_W_INTERNAL_2F4		INTERNAL_2F4;		// 0x48082F4
	IOREG_W_INTERNAL_2F6		INTERNAL_2F6;		// 0x48082F6
} WIFI_IOREG_MAP;
#include "PACKED_END.h"

typedef struct
{
	IOREG_W_TXBUF_LOCATION *txLocation;
	size_t emuPacketLength;
	size_t remainingBytes;
} TXPacketInfo;

typedef struct
{
	WIFI_IOREG_MAP io;
	RF2958_IOREG_MAP rf;
	bb_t bb;
	u8 RAM[0x2000];
	
	WifiTXLocIndex txCurrentSlot;
	TXPacketInfo txPacketInfo[6];
	u32 cmdCount_u32;
	u64 usecCounter;
} WifiData;

// Emulator frame headers
#define DESMUME_EMULATOR_FRAME_ID "DESMUME\0"
#define DESMUME_EMULATOR_FRAME_CURRENT_VERSION 0x10

typedef union
{
	u8 value;
	
	struct
	{
		u8 :7;
		u8 IsTXRate20:1;
	};
} DesmumeFrameHeaderAttributes;

typedef struct
{
	char frameID[8];				// "DESMUME\0" (null terminated string)
	u8 version;						// Ad-hoc protocol version (for example 0x52 = v5.2)
	DesmumeFrameHeaderAttributes packetAttributes; // Attribute bits for describing the packet
	
	u16 timeStamp;					// Timestamp for when the packet was sent.
	u16 emuPacketSize;				// Size of the entire emulated packet in bytes.
	
	u16 reserved;					// Padding to make the header exactly 16 bytes.
	
} DesmumeFrameHeader;				// Should total 16 bytes

// IEEE 802.11 Frame Information
enum WifiFrameType
{
	WifiFrameType_Management = 0,
	WifiFrameType_Control = 1,
	WifiFrameType_Data = 2
};

enum WifiFrameManagementSubtype
{
	WifiFrameManagementSubtype_AssociationRequest		= 0x00,
	WifiFrameManagementSubtype_AssociationResponse		= 0x01,
	WifiFrameManagementSubtype_ReassociationRequest		= 0x02,
	WifiFrameManagementSubtype_RessociationResponse		= 0x03,
	WifiFrameManagementSubtype_ProbeRequest				= 0x04,
	WifiFrameManagementSubtype_ProbeResponse			= 0x05,
	WifiFrameManagementSubtype_RESERVED06				= 0x06,
	WifiFrameManagementSubtype_RESERVED07				= 0x07,
	WifiFrameManagementSubtype_Beacon					= 0x08,
	WifiFrameManagementSubtype_ATIM						= 0x09,
	WifiFrameManagementSubtype_Disassociation			= 0x0A,
	WifiFrameManagementSubtype_Authentication			= 0x0B,
	WifiFrameManagementSubtype_Deauthentication			= 0x0C,
	WifiFrameManagementSubtype_RESERVED0D				= 0x0D,
	WifiFrameManagementSubtype_RESERVED0E				= 0x0E,
	WifiFrameManagementSubtype_RESERVED0F				= 0x0F
};

enum WifiFrameControlSubtype
{
	WifiFrameControlSubtype_RESERVED00		= 0x00,
	WifiFrameControlSubtype_RESERVED01		= 0x01,
	WifiFrameControlSubtype_RESERVED02		= 0x02,
	WifiFrameControlSubtype_RESERVED03		= 0x03,
	WifiFrameControlSubtype_RESERVED04		= 0x04,
	WifiFrameControlSubtype_RESERVED05		= 0x05,
	WifiFrameControlSubtype_RESERVED06		= 0x06,
	WifiFrameControlSubtype_RESERVED07		= 0x07,
	WifiFrameControlSubtype_RESERVED08		= 0x08,
	WifiFrameControlSubtype_RESERVED09		= 0x09,
	WifiFrameControlSubtype_PSPoll			= 0x0A,
	WifiFrameControlSubtype_RTS				= 0x0B,
	WifiFrameControlSubtype_CTS				= 0x0C,
	WifiFrameControlSubtype_ACK				= 0x0D,
	WifiFrameControlSubtype_End				= 0x0E,
	WifiFrameControlSubtype_EndAck			= 0x0F
};

enum WifiFrameDataSubtype
{
	WifiFrameDataSubtype_Data			= 0x00,
	WifiFrameDataSubtype_DataAck		= 0x01,
	WifiFrameDataSubtype_DataPoll		= 0x02,
	WifiFrameDataSubtype_DataAckPoll	= 0x03,
	WifiFrameDataSubtype_Null			= 0x04,
	WifiFrameDataSubtype_Ack			= 0x05,
	WifiFrameDataSubtype_Poll			= 0x06,
	WifiFrameDataSubtype_AckPoll		= 0x07,
	WifiFrameDataSubtype_RESERVED08		= 0x08,
	WifiFrameDataSubtype_RESERVED09		= 0x09,
	WifiFrameDataSubtype_RESERVED0A		= 0x0A,
	WifiFrameDataSubtype_RESERVED0B		= 0x0B,
	WifiFrameDataSubtype_RESERVED0C		= 0x0C,
	WifiFrameDataSubtype_RESERVED0D		= 0x0D,
	WifiFrameDataSubtype_RESERVED0E		= 0x0E,
	WifiFrameDataSubtype_RESERVED0F		= 0x0F
};

enum WifiFCFromToState
{
	WifiFCFromToState_STA2STA			= 0x0,
	WifiFCFromToState_STA2DS			= 0x1,
	WifiFCFromToState_DS2STA			= 0x2,
	WifiFCFromToState_DS2DS				= 0x3
};

typedef union
{
	u16 value;
	
	struct
	{
		u16 Version:2;
		u16 Type:2;
		u16 Subtype:4;
		
		u16 ToDS:1;
		u16 FromDS:1;
		u16 MoreFragments:1;
		u16 Retry:1;
		u16 PowerManagement:1;
		u16 MoreData:1;
		u16 WEPEncryption:1;
		u16 Order:1;
	};
	
	struct
	{
		u16 :8;
		
		u16 FromToState:2;
		u16 :6;
	};
} WifiFrameControl;

typedef union
{
	u16 value;
	
	struct
	{
		u16 FragmentNumber:4;
		u16 SequenceNumber:12;
	};
} WifiSequenceControl;

typedef struct
{
	WifiFrameControl fc;
	u16 duration;
	u8 destMAC[6];
	u8 sendMAC[6];
	u8 BSSID[6];
	WifiSequenceControl seqCtl;
} WifiMgmtFrameHeader;

typedef struct
{
	WifiFrameControl fc;
	u16 aid;
	u8 BSSID[6];
	u8 txMAC[6];
} WifiCtlFrameHeaderPSPoll;

typedef struct
{
	WifiFrameControl fc;
	u16 duration;
	u8 rxMAC[6];
	u8 txMAC[6];
} WifiCtlFrameHeaderRTS;

typedef struct
{
	WifiFrameControl fc;
	u16 duration;
	u8 rxMAC[6];
} WifiCtlFrameHeaderCTS;

typedef struct
{
	WifiFrameControl fc;
	u16 duration;
	u8 rxMAC[6];
} WifiCtlFrameHeaderACK;

typedef struct
{
	WifiFrameControl fc;
	u16 duration;
	u8 rxMAC[6];
	u8 BSSID[6];
} WifiCtlFrameHeaderEnd;

typedef struct
{
	WifiFrameControl fc;
	u16 duration;
	u8 rxMAC[6];
	u8 BSSID[6];
} WifiCtlFrameHeaderEndAck;

typedef struct
{
	WifiFrameControl fc;
	u16 duration;
	u8 destMAC[6];
	u8 sendMAC[6];
	u8 BSSID[6];
	WifiSequenceControl seqCtl;
} WifiDataFrameHeaderSTA2STA;

typedef struct
{
	WifiFrameControl fc;
	u16 duration;
	u8 BSSID[6];
	u8 sendMAC[6];
	u8 destMAC[6];
	WifiSequenceControl seqCtl;
} WifiDataFrameHeaderSTA2DS;

typedef struct
{
	WifiFrameControl fc;
	u16 duration;
	u8 destMAC[6];
	u8 BSSID[6];
	u8 sendMAC[6];
	WifiSequenceControl seqCtl;
} WifiDataFrameHeaderDS2STA;

typedef struct
{
	WifiFrameControl fc;
	u16 duration;
	u8 rxMAC[6];
	u8 txMAC[6];
	u8 destMAC[6];
	WifiSequenceControl seqCtl;
	u8 sendMAC[6];
} WifiDataFrameHeaderDS2DS;

// IEEE 802.11 Frame Header Information
typedef struct
{
	u8 destMAC[6];
	u8 sendMAC[6];
	
	union
	{
		u16 length;
		u16 ethertype;
	};
} EthernetFrameHeader;

typedef struct
{
	u8 dsap;
	u8 ssap;
	u8 control;
	u8 encapsulation[3];
	u16 ethertype;
} WifiLLCSNAPHeader;

typedef struct
{
	union
	{
		u8 :8;
		
		struct
		{
			u8 headerLen:4;
			u8 version:4;
		};
	};
	
	u8 typeOfService;
	u16 packetLen;
	u16 fragmentID;
	
	union
	{
		u16 :16;
		
		struct
		{
			u16 fragmentOffset:13;
			u16 flags:3;
		};
	};
	
	u8 timeToLive;
	u8 protocol;
	u16 checksum;
	
	union
	{
		u32 ipSrcValue;
		u8 ipSrc[4];
	};
	
	union
	{
		u32 ipDstValue;
		u8 ipDst[4];
	};
} IPv4Header;



// The maximum possible size of any 802.11 frame is 2346 bytes:
// - Max MTU is 2304 bytes
// - Max 802.11 header size is 30 bytes
// - WEP Encryption Header size is 8 bytes
// - FCS size is 4 bytes
#define MAX_PACKET_SIZE_80211 2346

// Given a connection of 2 megabits per second, we take ~4 microseconds to transfer a byte.
// This works out to needing ~8 microseconds to transfer a halfword.
#define TX_LATENCY_LIMIT 8
#define RX_LATENCY_LIMIT 8

// NDS Frame Header Information
typedef struct
{
	u16 txStatus;
	u16 mpSlaves;
	u8 seqNumberControl;
	u8 UNKNOWN1;
	u16 UNKNOWN2;
	u8 txRate;
	u8 UNKNOWN3;
	u16 length; // Total length of header+body+checksum, in bytes.
} TXPacketHeader;

typedef union
{
	u8 rawFrameData[sizeof(TXPacketHeader) + MAX_PACKET_SIZE_80211 + sizeof(u16) + sizeof(u16)];
	
	struct
	{
		TXPacketHeader txHeader;
		u8 txData[MAX_PACKET_SIZE_80211];
		u16 remainingBytes;
		u16 latencyCount;
	};
} TXBufferedPacket;

typedef union
{
	u16 value;
	
	struct
	{
		u16 FrameType:4;
		u16 UNKNOWN1:1;
		u16 UNKNOWN2:3;
		
		u16 MoreFragments:1;
		u16 UNKNOWN3:1;
		u16 :5;
		u16 MatchingBSSID:1;
	};
} RXPacketHeaderFlags;

typedef struct
{
	RXPacketHeaderFlags rxFlags;
	u16 UNKNOWN1;
	u16 timeStamp;
	u16 txRate;
	u16 length; // Total length of header+body, in bytes (excluding the FCS checksum).
	u8 rssiMax;
	u8 rssiMin;
} RXPacketHeader;

typedef union
{
	u8 rawFrameData[sizeof(RXPacketHeader) + MAX_PACKET_SIZE_80211 + sizeof(u16)];
	
	struct
	{
		RXPacketHeader rxHeader;
		u8 rxData[MAX_PACKET_SIZE_80211];
		u16 latencyCount;
	};
} RXQueuedPacket;

typedef struct
{
	u8 buffer[(sizeof(DesmumeFrameHeader) + MAX_PACKET_SIZE_80211) * 16];
	size_t writeLocation;
	size_t count;
} RXRawPacketData;

extern LegacyWifiSFormat legacyWifiSF;

/* wifimac io */
void WIFI_write16(u32 address, u16 val);
u16  WIFI_read16(u32 address);

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

class ClientPCapInterface
{
public:
	virtual int findalldevs(void **alldevs, char *errbuf) = 0;
	virtual void freealldevs(void *alldevs) = 0;
	virtual void* open(const char *source, int snaplen, int flags, int readtimeout, char *errbuf) = 0;
	virtual void close(void *dev) = 0;
	virtual int setnonblock(void *dev, int nonblock, char *errbuf) = 0;
	virtual int sendpacket(void *dev, const void *data, int len) = 0;
	virtual int dispatch(void *dev, int num, void *callback, void *userdata) = 0;
	virtual void breakloop(void *dev) = 0;
};

class DummyPCapInterface : public ClientPCapInterface
{
private:
	void __CopyErrorString(char *errbuf);
	
public:
	virtual int findalldevs(void **alldevs, char *errbuf);
	virtual void freealldevs(void *alldevs);
	virtual void* open(const char *source, int snaplen, int flags, int readtimeout, char *errbuf);
	virtual void close(void *dev);
	virtual int setnonblock(void *dev, int nonblock, char *errbuf);
	virtual int sendpacket(void *dev, const void *data, int len);
	virtual int dispatch(void *dev, int num, void *callback, void *userdata);
	virtual void breakloop(void *dev);
};

#ifndef HOST_WINDOWS

class POSIXPCapInterface : public ClientPCapInterface
{
public:
	virtual int findalldevs(void **alldevs, char *errbuf);
	virtual void freealldevs(void *alldevs);
	virtual void* open(const char *source, int snaplen, int flags, int readtimeout, char *errbuf);
	virtual void close(void *dev);
	virtual int setnonblock(void *dev, int nonblock, char *errbuf);
	virtual int sendpacket(void *dev, const void *data, int len);
	virtual int dispatch(void *dev, int num, void *callback, void *userdata);
	virtual void breakloop(void *dev);
};

#endif

class WifiCommInterface
{
protected:
	WifiCommInterfaceID _commInterfaceID;
	WifiHandler *_wifiHandler;
	
	Task *_rxTask;
	slock_t *_mutexRXThreadRunningFlag;
	volatile bool _isRXThreadRunning;
	RXRawPacketData *_rawPacket;
	
public:
	WifiCommInterface();
	~WifiCommInterface();
	
	virtual bool Start(WifiHandler *currentWifiHandler) = 0;
	virtual void Stop() = 0;
	virtual size_t TXPacketSend(u8 *txTargetBuffer, size_t txLength) = 0;
	virtual void RXPacketGet() = 0;
};

class AdhocCommInterface : public WifiCommInterface
{
protected:
	void *_wifiSocket;
	void *_sendAddr;
	
public:
	AdhocCommInterface();
	~AdhocCommInterface();
	
	int _RXPacketGetFromSocket(RXRawPacketData &rawPacket);
	
	virtual bool Start(WifiHandler *currentWifiHandler);
	virtual void Stop();
	virtual size_t TXPacketSend(u8 *txTargetBuffer, size_t txLength);
	virtual void RXPacketGet();
};

class SoftAPCommInterface : public WifiCommInterface
{
protected:
	ClientPCapInterface *_pcap;
	int _bridgeDeviceIndex;
	void *_bridgeDevice;
	
	void* _GetBridgeDeviceAtIndex(int deviceIndex, char *outErrorBuf);
	bool _IsDNSRequestToWFC(u16 ethertype, const u8 *body);
	
public:
	SoftAPCommInterface();
	virtual ~SoftAPCommInterface();
	
	void SetPCapInterface(ClientPCapInterface *pcapInterface);
	ClientPCapInterface* GetPCapInterface();
	
	int GetBridgeDeviceIndex();
	void SetBridgeDeviceIndex(int deviceIndex);
	
	virtual bool Start(WifiHandler *currentWifiHandler);
	virtual void Stop();
	virtual size_t TXPacketSend(u8 *txTargetBuffer, size_t txLength);
	virtual void RXPacketGet();
};

class WifiHandler
{
protected:
	WifiData _wifi;
	
	AdhocCommInterface *_adhocCommInterface;
	SoftAPCommInterface *_softAPCommInterface;
	
	WifiEmulationLevel _selectedEmulationLevel;
	WifiEmulationLevel _currentEmulationLevel;
	
	int _selectedBridgeDeviceIndex;
	
	ClientPCapInterface *_pcap;
	bool _isSocketsSupported;
	bool _didWarnWFCUser;
	
	u8 *_workingTXBuffer;
	
	slock_t *_mutexRXPacketQueue;
	std::deque<RXQueuedPacket> _rxPacketQueue;
	RXQueuedPacket _rxCurrentPacket;
	size_t _rxCurrentQueuedPacketPosition;
	
	EAPStatus _softAPStatus;
	u16 _softAPSequenceNumber;
	
	FILE *_packetCaptureFile; // PCAP file to store the Ethernet packets.
	
	void _AddPeriodicPacketsToRXQueue(const u64 usecCounter);
	void _CopyFromRXQueue();
	
	void _RXEmptyQueue();
	void _RXWriteOneHalfword(u16 val);
	const u8* _RXPacketFilter(const u8 *rxBuffer, const size_t rxBytes, RXPacketHeader &outRXHeader);
	
	void _PacketCaptureFileOpen();
	void _PacketCaptureFileClose();
	void _PacketCaptureFileWrite(const u8 *packet, u32 len, bool isReceived, u64 timeStamp);
	
	RXQueuedPacket _GenerateSoftAPDeauthenticationFrame(u16 sequenceNumber);
	RXQueuedPacket _GenerateSoftAPBeaconFrame(u16 sequenceNumber, u64 timeStamp);
	RXQueuedPacket _GenerateSoftAPMgmtResponseFrame(WifiFrameManagementSubtype mgmtFrameSubtype, u16 sequenceNumber, u64 timeStamp);
	RXQueuedPacket _GenerateSoftAPCtlACKFrame(const WifiDataFrameHeaderSTA2DS &inIEEE80211FrameHeader, const size_t sendPacketLength);
	
	bool _SoftAPTrySendPacket(const TXPacketHeader &txHeader, const u8 *IEEE80211PacketData);
	bool _AdhocTrySendPacket(const TXPacketHeader &txHeader, const u8 *IEEE80211PacketData);
	
public:
	WifiHandler();
	~WifiHandler();
	
	void Reset();
	
	WifiData& GetWifiData();
	
	TXPacketInfo& GetPacketInfoAtSlot(size_t txSlot);
	
	WifiEmulationLevel GetSelectedEmulationLevel();
	WifiEmulationLevel GetCurrentEmulationLevel();
	void SetEmulationLevel(WifiEmulationLevel emulationLevel);
	
	int GetBridgeDeviceList(std::vector<std::string> *deviceStringList);

	int GetSelectedBridgeDeviceIndex();
	int GetCurrentBridgeDeviceIndex();
	void SetBridgeDeviceIndex(int deviceIndex);
	
	bool CommStart();
	void CommStop();
	void CommSendPacket(const TXPacketHeader &txHeader, const u8 *packetData);
	void CommTrigger();
	void CommEmptyRXQueue();
	
	template<bool WILLADVANCESEQNO> void RXPacketRawToQueue(const RXRawPacketData &rawPacket);
	
	bool IsSocketsSupported();
	void SetSocketsSupported(bool isSupported);
	
	bool IsPCapSupported();
	ClientPCapInterface* GetPCapInterface();
	void SetPCapInterface(ClientPCapInterface *pcapInterface);
	
	void PrepareSaveStateWrite();
	void ParseSaveStateRead();
	
	static size_t ConvertDataFrame80211To8023(const u8 *inIEEE80211Frame, const size_t txLength, u8 *outIEEE8023Frame);
	static size_t ConvertDataFrame8023To80211(const u8 *inIEEE8023Frame, const size_t txLength, u8 *outIEEE80211Frame);
};

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
extern DummyPCapInterface dummyPCapInterface;
extern WifiHandler *wifiHandler;

#endif
