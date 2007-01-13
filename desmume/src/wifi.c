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

#include "wifi.h"

/*******************************************************************************

	RF-Chip

 *******************************************************************************/

void WIFI_resetRF(rffilter_t *rf) {
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
	if (!wifi->rfIOStatus.bits.busy)
		wifi->rfIOCnt.val = val ;
}

void WIFI_setRF_DATA(wifimac_t *wifi, u16 val, u8 part)
{
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
					/* special purpose register: TEST1, on write, the RF chip resets */
					if (wifi->rfIOData.bits.address == 13)
					{
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
	return wifi->rfIOStatus.val ;
}

/*******************************************************************************

	BB-Chip

 *******************************************************************************/

void WIFI_setBB_CNT(wifimac_t *wifi,u16 val)
{
	wifi->bbIOCnt.val = val ;
}

u8 WIFI_getBB_DATA(wifimac_t *wifi)
{
	if ((wifi->bbIOCnt.bits.mode != 2) || !(wifi->bbIOCnt.bits.enable)) return 0 ; /* not for read or disabled */
	return wifi->BB.data[wifi->bbIOCnt.bits.address] ;
}

void WIFI_setBB_DATA(wifimac_t *wifi, u8 val)
{
	if ((wifi->bbIOCnt.bits.mode != 1) || !(wifi->bbIOCnt.bits.enable)) return ; /* not for write or disabled */
    wifi->BB.data[wifi->bbIOCnt.bits.address] = val ;
}

/*******************************************************************************

	wifimac IO: a lot of the wifi regs are action registers, that are mirrored
				without action, so the default IO via MMU.c does not seem to
				be very suitable

				all registers are 16 bit

 *******************************************************************************/

void WIFI_write16(wifimac_t *wifi,u32 address, u16 val)
{
	BOOL action = FALSE ;
	if ((address & 0xFF800000) != 0x04800000) return ;      /* error: the address does not describe a wiifi register */

	/* the first 0x1000 bytes are mirrored at +0x1000,+0x2000,+0x3000,+06000,+0x7000 */
	/* only the first mirror causes an special action */
	/* the gap at +0x4000 is filled with the circular bufferspace */
	/* the so created 0x8000 byte block is then mirrored till 0x04FFFFFF */
	/* see: http://www.akkit.org/info/dswifi.htm#Wifihwcap */
	if (((address & 0x00007000) >= 0x00004000) && ((address & 0x00007000) < 0x00006000))
	{
		/* access to the circular buffer */
        wifi->circularBuffer[(address & 0x1FFF) >> 1] = val ;
		return ;
	}
	if (!(address & 0x00007000)) action = TRUE ;
	/* mirrors => register address */
	address &= 0x00000FFF ;
	switch (address)
	{
		case REG_WIFI_MODE:
			wifi->macMode = val ;
			break ;
		case REG_WIFI_WEP:
			wifi->wepMode = val ;
			break ;
		case REG_WIFI_IE:
			wifi->IE.val = val ;
			break ;
		case REG_WIFI_IF:
			wifi->IF.val = val ;
			break ;
		case REG_WIFI_MAC0:
		case REG_WIFI_MAC1:
		case REG_WIFI_MAC2:
			wifi->mac[(address - REG_WIFI_MAC0) >> 1] = val ;
			break ;
		case REG_WIFI_BSS0:
		case REG_WIFI_BSS1:
		case REG_WIFI_BSS2:
			wifi->bss[(address - REG_WIFI_BSS0) >> 1] = val ;
			break ;
		case REG_WIFI_AID:
			wifi->aid = val ;
			break ;
		case REG_WIFI_RETRYLIMIT:
			wifi->retryLimit = val ;
			break ;
		case REG_WIFI_RXRANGEBEGIN:
			wifi->RXRangeBegin = val ;
			break ;
		case REG_WIFI_RXRANGEEND:
			wifi->RXRangeEnd = val ;
			break ;
		case REG_WIFI_WRITECSRLATCH:
			if ((action) && (wifi->RXCnt & 1))        /* only when action register and CSR change enabled */
			{
				wifi->RXHWWriteCursor = val ;
			}
			break ;
		case REG_WIFI_CIRCBUFRADR:
			wifi->CircBufReadAddress = val ;
			break ;
		case REG_WIFI_RXREADCSR:
			wifi->RXReadCursor = val ;
			break ;
		case REG_WIFI_CIRCBUFWADR:
			wifi->CircBufWriteAddress = val ;
			break ;
		case REG_WIFI_CIRCBUFWRITE:
			/* set value into the circ buffer, and move cursor to the next hword on action */
			wifi->circularBuffer[wifi->CircBufWriteAddress >> 1] = val ;
			if (action)
			{
				/* move to next hword */
                wifi->CircBufWriteAddress+=2 ;
				if (wifi->CircBufWriteAddress == wifi->CircBufEnd)
				{
					/* on end of buffer, add skip hwords to it */
					wifi->CircBufEnd += wifi->CircBufSkip * 2 ;
				}
			}
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
	}
}

u16 WIFI_read16(wifimac_t *wifi,u32 address)
{
	BOOL action = FALSE ;
	if ((address & 0xFF800000) != 0x04800000) return 0 ;      /* error: the address does not describe a wiifi register */

	/* the first 0x1000 bytes are mirrored at +0x1000,+0x2000,+0x3000,+06000,+0x7000 */
	/* only the first mirror causes an special action */
	/* the gap at +0x4000 is filled with the circular bufferspace */
	/* the so created 0x8000 byte block is then mirrored till 0x04FFFFFF */
	/* see: http://www.akkit.org/info/dswifi.htm#Wifihwcap */
	if (((address & 0x00007000) >= 0x00004000) && ((address & 0x00007000) < 0x00006000))
	{
		/* access to the circular buffer */
        return wifi->circularBuffer[(address & 0x1FFF) >> 1] ;
	}
	if (!(address & 0x00007000)) action = TRUE ;
	/* mirrors => register address */
	address &= 0x00000FFF ;
	switch (address)
	{
		case REG_WIFI_IE:
			return wifi->IE.val ;
		case REG_WIFI_IF:
			return wifi->IF.val ;
		case REG_WIFI_RFIODATA1:
			return WIFI_getRF_DATA(wifi,0) ;
		case REG_WIFI_RFIODATA2:
			return WIFI_getRF_DATA(wifi,1) ;

	}
}

wifimac_t wifiMac ;
