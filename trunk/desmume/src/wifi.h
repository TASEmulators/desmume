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

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

/* Referenced as RF_ in dswifi: rffilter_t */
/* based on the documentation for the RF2958 chip of RF Micro Devices */
/* using the register names as in docs ( http://www.rfmd.com/pdfs/2958.pdf )*/
/* even tho every register only has 18 bits we are using u32 */
typedef struct rffilter_t
{
	union CFG1
	{
		struct bits
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
		struct bits
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
		struct bits
		{
/* 0*/		unsigned IF_N:16;
/*16*/		unsigned :2;
		} bits ;
		u32 val ;
	} IFPLL2 ;
	union IFPLL3
	{
		struct bits
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
		struct bits
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
		struct bits
		{
/* 0*/      unsigned NUM2:6;
/* 6*/      unsigned N2:12;
		} bits ;
		u32 val ;
	} RFPLL2 ;
	union RFPLL3
	{
		struct bits
		{
/* 0*/		unsigned NUM2:18;
		} bits ;
		u32 val ;
	} RFPLL3 ;
	union RFPLL4
	{
		struct bits
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
		struct bits
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
		struct bits
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
		struct bits
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
		struct bits
		{
/* 0*/      unsigned MIN_POWER:6;
/* 6*/      unsigned MID_POWER:6;
/*12*/      unsigned MAX_POWER:6;
		} bits ;
	} PCNT2 ;
	union VCOT1
	{
		struct bits
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
/* 0*/  unsigned address:7;
/* 7*/  unsigned :5;
/*12*/  unsigned mode:2;
/*14*/  unsigned enable:1;
/*15*/  unsigned :1;
	} bits ;
	u16 val ;
} bbIOCnt_t ;

/* definition of the irq bitfields for wifi irq's (cascaded at arm7 irq #24) */
typedef union
{
	struct
	{
/* 0*/  unsigned recv_complete:1;
/* 1*/  unsigned send_complete:1;
/* 2*/  unsigned recv_countup:1;
/* 3*/  unsigned send_error:1;
/* 4*/  unsigned stat_coutup:1;
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

	/* wifi interrupt handling */
    wifiirq_t   IE ;
    wifiirq_t   IF ;

	/* modes */
	u16 macMode ;
	u16 wepMode ;

	/* addressing/handshaking */
	u8  mac[6] ;
	u8  bss[6] ;

	/* subchips */
    rffilter_t 	RF ;
	bb_t        BB ;

	/* subchip communications */
    rfIOCnt_t   rfIOCnt ;
    bbIOCnt_t   bbIOCnt ;

} wifimac_t ;

void WIFI_setRF_CNT(wifimac_t *wifi, u16 val) ;


#ifdef __cplusplus
}
#endif

#endif
