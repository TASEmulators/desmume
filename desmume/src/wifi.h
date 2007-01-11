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

/* Referenced as RF_ in dswifi: rffilter_t */
/* based on the documentation for the RF2958 chip of RF Micro Devices */
/* using the register names as in docs */
/* even tho every register only has 18 bits we are using u32 */
typedef struct rffilter_t
{
	union
	{
		struct
		{
/* 0*/		unsigned IF_VGA_REG_EN:1;
/* 1*/		unsigned IF_VCO_REG_EN:1;
/* 2*/		unsigned RF_VCO_REG_EN:1;
/* 3*/		unsigned HYBERNATE:1;
/* 4*/      unsigned :10;
/*14*/      unsigned REF_SEL:2;
/*16*/      unsigned :2 ;
		} bits ;
		u32 val ;
	} CFG1 ;
	union
	{
		struct
		{
/* 0*/		unsigned DAC:4;
/* 4*/		unsigned :5;
/* 9*/      unsigned P1:1;
/*10*/		unsigned LD_EN1:1;
/*11*/      unsigned AUTOCAL_EN1:1;
/*12*/      unsigned PDP1:1;
/*13*/      unsigned CPL1:1;
/*14*/      unsigned LPF1:1;
/*15*/      unsigned VTC_EN1:1;
/*16*/      unsigned KV_EN1:1;
/*17*/      unsigned PLL_EN1:1;
		} bits ;
		u32 val ;
	} IFPLL1
	union
	{
		struct
		{
/* 0*/		unsigned IF_N:16;
/*16*/      unsigned :2;
		} bits ;
		u32 val ;
	} IFPLL2
	union
	{
		struct
		{
/* 0*/      unsigned KV_DEF:4;
/* 4*/      unsigned CT_DEF:4;
/* 8*/      unsigned DN1:8;
/*16*/      unsigned :2;
		} bits ;
		u32 val ;
	} IFPLL3
} rffilter_t ;

#ifdef __cplusplus
}
#endif

#endif
