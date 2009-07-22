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
#ifndef WIFI_H
#define WIFI_H

#include <stdio.h>
#include "types.h"

#ifdef EXPERIMENTAL_WIFI

#ifdef WIN32
#include "windriver.h"
#else
#include "pcap/pcap.h"
#endif

#define HAVE_REMOTE
#define WPCAP
#define PACKET_SIZE 65535
#define _INC_STDIO


#define		REG_WIFI_ID					0x000
#define     REG_WIFI_MODE       		0x004
#define     REG_WIFI_WEP        		0x006
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
#define		REG_WIFI_RXCNT				0x030
#define		REG_WIFI_WEPCNT				0x032
#define		REG_WIFI_POWER_US			0x036
#define     REG_WIFI_POWERSTATE 		0x03C
#define     REG_WIFI_FORCEPS    		0x040
#define     REG_WIFI_RANDOM     		0x044
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
#define     REG_WIFI_CIRCBUFWRITE       0x070
#define     REG_WIFI_CIRCBUFWR_END      0x074
#define     REG_WIFI_CIRCBUFWR_SKIP     0x076
#define     REG_WIFI_BEACONTRANS        0x080
#define     REG_WIFI_LISTENCOUNT        0x088
#define     REG_WIFI_BEACONPERIOD       0x08C
#define     REG_WIFI_LISTENINT          0x08E
#define     REG_WIFI_TXLOC1             0x0A0
#define     REG_WIFI_TXLOC2             0x0A4
#define     REG_WIFI_TXLOC3             0x0A8
#define     REG_WIFI_TXOPT              0x0AC
#define     REG_WIFI_TXCNT              0x0AE
#define		REG_WIFI_TXREQ_READ			0x0B0
#define		REG_WIFI_TXBUSY				0x0B6
#define     REG_WIFI_TXSTAT             0x0B8
#define     REG_WIFI_RXFILTER           0x0D0
#define     REG_WIFI_USCOUNTERCNT       0x0E8
#define     REG_WIFI_USCOMPARECNT       0x0EA
#define		REG_WIFI_EXTRACOUNTCNT		0x0EE
#define     REG_WIFI_USCOMPARE0         0x0F0
#define     REG_WIFI_USCOMPARE1         0x0F2
#define     REG_WIFI_USCOMPARE2         0x0F4
#define     REG_WIFI_USCOMPARE3         0x0F6
#define     REG_WIFI_USCOUNTER0         0x0F8
#define     REG_WIFI_USCOUNTER1         0x0FA
#define     REG_WIFI_USCOUNTER2         0x0FC
#define     REG_WIFI_USCOUNTER3         0x0FE
#define		REG_WIFI_EXTRACOUNT			0x118
#define     REG_WIFI_BBSIOCNT           0x158
#define     REG_WIFI_BBSIOWRITE         0x15A
#define     REG_WIFI_BBSIOREAD          0x15C
#define     REG_WIFI_BBSIOBUSY          0x15E
#define     REG_WIFI_RFIODATA2  		0x17C
#define     REG_WIFI_RFIODATA1  		0x17E
#define     REG_WIFI_RFIOBSY    		0x180
#define     REG_WIFI_RFIOCNT    		0x184
#define		REG_WIFI_IF_SET				0x21C
#define		REG_WIFI_POWERACK			0x2D0

/* WIFI misc constants */
#define		WIFI_CHIPID					0x1440		/* emulates "old" wifi chip, new is 0xC340 */
#define		REG_PWRCNT					0x04000304	

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
/*16*/		unsigned :2 ;
		} bits ;
		u32 val ;
	} CFG1 ;
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
		} bits ;
		u32 val ;
	} IFPLL1 ;
	union IFPLL2
	{
		struct 
		{
/* 0*/		unsigned IF_N:16;
/*16*/		unsigned :2;
		} bits ;
		u32 val ;
	} IFPLL2 ;
	union IFPLL3
	{
		struct 
		{
/* 0*/		unsigned KV_DEF1:4;
/* 4*/		unsigned CT_DEF1:4;
/* 8*/		unsigned DN1:9;
/*17*/		unsigned :1;
		} bits ;
		u32 val ;
	} IFPLL3 ;
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
		} bits ;
		u32 val ;
	} RFPLL1 ;
	union RFPLL2
	{
		struct 
		{
/* 0*/      unsigned NUM2:6;
/* 6*/      unsigned N2:12;
		} bits ;
		u32 val ;
	} RFPLL2 ;
	union RFPLL3
	{
		struct 
		{
/* 0*/		unsigned NUM2:18;
		} bits ;
		u32 val ;
	} RFPLL3 ;
	union RFPLL4
	{
		struct 
		{
/* 0*/		unsigned KV_DEF:4;
/* 4*/      unsigned CT_DEF:4;
/* 8*/      unsigned DN:9;
/*17*/      unsigned :1;
		} bits ;
		u32 val ;
	} RFPLL4 ;
	union CAL1
	{
		struct 
		{
/* 0*/      unsigned LD_WINDOW:3;
/* 3*/      unsigned M_CT_VALUE:5;
/* 8*/      unsigned TLOCK:5;
/*13*/      unsigned TVCO:5;
		} bits ;
		u32 val ;
	} CAL1 ;
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
		} bits ;
		u32 val ;
	} TXRX1 ;
	union PCNT1
	{
		struct 
		{
/* 0*/      unsigned TX_DELAY:3;
/* 3*/      unsigned PC_OFFSET:6;
/* 9*/      unsigned P_DESIRED:6;
/*15*/      unsigned MID_BIAS:3;
		} bits ;
		u32 val ;
	} PCNT1 ;
	union PCNT2
	{
		struct 
		{
/* 0*/      unsigned MIN_POWER:6;
/* 6*/      unsigned MID_POWER:6;
/*12*/      unsigned MAX_POWER:6;
		} bits ;
	} PCNT2 ;
	union VCOT1
	{
		struct 
		{
/* 0*/      unsigned :16;
/*16*/      unsigned AUX1:1;
/*17*/      unsigned AUX:1;
		} bits ;
		u32 val ;
	} VCOT1 ;
} rffilter_t ;

/* baseband chip refered as BB_, dataformat is unknown yet */
/* it has at least 105 bytes of functional data */
typedef struct
{
	u8    data[105] ;
} bb_t ;

/* communication interface between RF,BB and the mac */
typedef union
{
	struct {
/* 0*/  unsigned wordsize:5;
/* 5*/  unsigned :2;
/* 7*/  unsigned readOperation:1;
/* 8*/  unsigned :8;
	} bits ;
	u16 val ;
} rfIOCnt_t ;

typedef union
{
	struct {
/* 0*/		unsigned busy:1;
/* 1*/      unsigned :15;
	} bits ;
	u16 val ;
} rfIOStat_t ;

typedef union
{
	struct {
/* 0*/  unsigned content:18 ;
/*18*/  unsigned address:5;
/*23*/  unsigned :9;
	} bits ;
	struct {
/* 0*/  unsigned low:16 ;
/*16*/  unsigned high:16 ;
	} val16 ;
	u16 array16[2] ;
	u32 val ;
} rfIOData_t ;

typedef union
{
	struct {
/* 0*/  unsigned address:7;
/* 7*/  unsigned :5;
/*12*/  unsigned mode:2;
/*14*/  unsigned enable:1;
/*15*/  unsigned :1;
	} bits ;
	u16 val ;
} bbIOCnt_t ;

#define WIFI_IRQ_RECVCOMPLETE       	0
#define WIFI_IRQ_SENDCOMPLETE           1
#define WIFI_IRQ_COUNTUP                2
#define WIFI_IRQ_SENDERROR              3
#define WIFI_IRQ_STATCOUNTUP            4
#define WIFI_IRQ_STATACKUP              5
#define WIFI_IRQ_RECVSTART              6
#define WIFI_IRQ_SENDSTART              7
#define WIFI_IRQ_RFWAKEUP               11
#define WIFI_IRQ_TIMEBEACON             14
#define WIFI_IRQ_TIMEPREBEACON          15

/* definition of the irq bitfields for wifi irq's (cascaded at arm7 irq #24) */
typedef union
{
	struct
	{
/* 0*/  unsigned recv_complete:1;
/* 1*/  unsigned send_complete:1;
/* 2*/  unsigned recv_countup:1;
/* 3*/  unsigned send_error:1;
/* 4*/  unsigned stat_countup:1;
/* 5*/  unsigned stat_ackup:1;
/* 6*/  unsigned recv_start:1;
/* 7*/  unsigned send_start:1;
/* 8*/  unsigned :3;
/*11*/  unsigned rf_wakeup:1;
/*12*/  unsigned :2;
/*14*/  unsigned time_beacon:1;
/*15*/  unsigned time_prebeacon:1;
	} bits ;
	u16 val ;
} wifiirq_t ;

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
    wifiirq_t   IE ;
    wifiirq_t   IF ;

	/* modes */
	u16 macMode ;
	u16 wepMode ;
	BOOL WEP_enable ;

	/* sending */
	u16 TXSlot[3] ;
	u16 TXCnt ;
	u16 TXOpt ;
	u16 TXStat;
	u16 BEACONSlot ;
	BOOL BEACON_enable ;
	u8 txCurSlot;
	u8 txSlotBusy[3];
	u32 txSlotAddr[3];
	u32 txSlotLen[3];
	u32 txSlotRemainingBytes[3];

	/* receiving */
	u16 RXCnt ;
	u16 RXCheckCounter ;

	/* addressing/handshaking */
	union
	{
		u16  words[3] ;
		u8	 bytes[6] ;
	} mac ;
	union
	{
		u16  words[3] ;
		u8	 bytes[6] ;
	} bss ;
	u16 aid ;
	u16 pid ; /* player ID or aid_low */
	u16 retryLimit ;

	/* timing */
	BOOL crystalEnabled ;
	u64 usec ;
	BOOL usecEnable ;
	u64 ucmp ;
	BOOL ucmpEnable ;
	u16 eCount ;
	BOOL eCountEnable ;

	/* subchips */
    rffilter_t 	RF ;
	bb_t        BB ;

	/* subchip communications */
    rfIOCnt_t   rfIOCnt ;
	rfIOStat_t  rfIOStatus ;
	rfIOData_t  rfIOData ;
    bbIOCnt_t   bbIOCnt ;

	u8 bbDataToWrite;

	/* buffers */
	u16         circularBuffer[0x1000] ;
	u16         RXRangeBegin ;
	u16         RXRangeEnd ;
	u16         RXHWWriteCursor ;
	u16			RXHWWriteCursorReg;
	u16			RXHWWriteCursorLatched;
	u16         RXReadCursor ;
	u16         RXUnits ;
	u16			RXBufCount ;
	u16         CircBufReadAddress ;
	u16         CircBufWriteAddress ;
	u16         CircBufRdEnd ;
	u16         CircBufRdSkip ;
	u16         CircBufWrEnd ;
	u16         CircBufWrSkip ;

	/* tx packets */
	u8 *curPacket[3];
	int curPacketSize[3];
	int curPacketPos[3];
	BOOL curPacketSending[3];

	/* i/o mem */
	u16			ioMem[0x800];

	/* others */
	u16			randomSeed ;

	/* SoftAP */
	struct _SoftAP
	{
		pcap_t *bridge;

		u64 usecCounter;

		u8 *curPacket;
		int curPacketSize;
		int curPacketPos;
		BOOL curPacketSending;

	} SoftAP;

	/* desmume host communication */
	bool		netEnabled;

} wifimac_t ;

extern wifimac_t wifiMac ;

void WIFI_Init(wifimac_t *wifi);

/* subchip communication IO functions */
void WIFI_setRF_CNT(wifimac_t *wifi, u16 val) ;
void WIFI_setRF_DATA(wifimac_t *wifi, u16 val, u8 part) ;
u16  WIFI_getRF_DATA(wifimac_t *wifi, u8 part) ;
u16  WIFI_getRF_STATUS(wifimac_t *wifi) ;

void WIFI_setBB_CNT(wifimac_t *wifi,u16 val) ;
u8   WIFI_getBB_DATA(wifimac_t *wifi) ;
void WIFI_setBB_DATA(wifimac_t *wifi, u8 val) ;

/* wifimac io */
void WIFI_write16(wifimac_t *wifi,u32 address, u16 val) ;
u16  WIFI_read16(wifimac_t *wifi,u32 address) ;

/* wifimac timing */
void WIFI_usTrigger(wifimac_t *wifi) ;

/* SoftAP */

typedef struct _WIFI_FrameHeader
{
	u8 FrameControl[2];
	u8 DurationID[2];
	u8 Receiver[6];
	u8 Sender[6];
	u8 BSSID[6];
	u8 SeqCtl[2];

} WIFI_FrameHeader;

int WIFI_SoftAP_Init(wifimac_t *wifi);
void WIFI_SoftAP_Shutdown(wifimac_t *wifi);
void WIFI_SoftAP_RecvPacketFromDS(wifimac_t *wifi, u8 *packet, int len);
void WIFI_SoftAP_usTrigger(wifimac_t *wifi);

#endif

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
extern const u8 FW_WIFIInit[32] ;
extern const u8 FW_BBInit[105] ;
extern const u8 FW_RFInit[36] ;
extern const u8 FW_RFChannel[6*14] ;
extern const u8 FW_BBChannel[14] ;
extern FW_WFCProfile FW_WFCProfile1;
extern FW_WFCProfile FW_WFCProfile2;
extern FW_WFCProfile FW_WFCProfile3;

#endif
