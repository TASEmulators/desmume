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
#include "armcpu.h"

#ifdef EXPERIMENTAL_WIFI

wifimac_t wifiMac ;

socket_t 	WIFI_Host_OpenChannel(u8 num) ;
void 		WIFI_Host_CloseChannel(socket_t sock) ;
void 		WIFI_Host_SendData(socket_t sock, u8 channel, u8 *data, u16 length) ;
u16			WIFI_Host_RecvData(socket_t sock, u8 *data, u16 maxLength) ;
BOOL 		WIFI_Host_InitSystem(void) ;
void 		WIFI_Host_ShutdownSystem(void) ;

#endif

/*******************************************************************************

	Firmware info needed for wifi, if no firmware image is available
	see: http://www.akkit.org/info/dswifi.htm#WifiInit

	written in bytes, to avoid endianess issues

 *******************************************************************************/

u8 FW_Mac[6] 			= { 0x00, 0x09, 0xBF, 0x12, 0x34, 0x56 } ;
u8 FW_WIFIInit[32] 		= { 0x02,0x00,  0x17,0x00,  0x26,0x00,  0x18,0x18,
							0x48,0x00,  0x40,0x48,  0x58,0x00,  0x42,0x00,
							0x40,0x01,  0x64,0x80,  0xE0,0xE0,  0x43,0x24,
							0x0E,0x00,  0x32,0x00,  0xF4,0x01,  0x01,0x01 } ;
u8 FW_BBInit[105] 		= { 0x6D, 0x9E, 0x40, 0x05,
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
u8 FW_RFInit[36] 		= { 0x07, 0xC0, 0x00,
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
u8 FW_RFChannel[6*14]	= { 0x28, 0x17, 0x14,		/* Channel 1 */
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
u8 FW_BBChannel[14]		= { 0xb3, 0xb3, 0xb3, 0xb3, 0xb3,	/* channel  1- 6 */
							0xb4, 0xb4, 0xb4, 0xb4, 0xb4,	/* channel  7-10 */ 
							0xb5, 0xb5,						/* channel 11-12 */
							0xb6, 0xb6						/* channel 13-14 */
						  } ;

u8 FW_WFCProfile[0xC0]  = { 'D','e','S','m','u','m','E',' ','S','o','f','t','A','P',
							' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ', /* ssid */
							'D','e','S','m','u','m','E',' ','S','o','f','t','A','P',
							' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ', /* ssid */
							'W','E','P','K','E','Y',' ','P','A','R','T',' ','1',' ',' ',' ',
							'W','E','P','K','E','Y',' ','P','A','R','T',' ','2',' ',' ',' ',
							'W','E','P','K','E','Y',' ','P','A','R','T',' ','3',' ',' ',' ',
							'W','E','P','K','E','Y',' ','P','A','R','T',' ','4',' ',' ',' ',
							127,0,0,1,                          /* IP address */
							127,0,0,1,                          /* Gateway */
							127,0,0,1,                          /* DNS 1 */
							127,0,0,1,                          /* DNS 2 */
							24,                                 /* subnet/node seperating bit (n*'1' | (32-n)*'0' = subnet mask) */
							0,0,0,0,0,0,0,
							0,0,0,0,0,0,0,0,
							0,0,0,0,0,0,
							0,                                  /* WEP: disabled */
							0,                                  /* This entry is: normal (1= AOSS, FF = deleted)*/
							0,0,0,0,0,0,0,0,
							'W','F','C',' ','U','S','E','R',' ','I','D',' ',' ',' ',                   /* user id */
							0,0                                 /* CRC */
						  } ;

#ifdef EXPERIMENTAL_WIFI

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
					switch (wifi->rfIOData.bits.address)
					{
						case 5:		/* write to upper part of the frequency filter */
						case 6:		/* write to lower part of the frequency filter */
							{
								u32 channelFreqN ;
								rfreg[wifi->rfIOData.bits.address].bits.content = wifi->rfIOData.bits.content ;
								/* get the complete rfpll.n */
								channelFreqN = (u32)wifi->RF.RFPLL3.bits.NUM2 + ((u32)wifi->RF.RFPLL2.bits.NUM2) << 18 + ((u32)wifi->RF.RFPLL2.bits.N2) << 24 ;
								/* frequency setting is out of range */
								if (channelFreqN<0x00A2E8BA) return ;
								/* substract base frequency (channel 1) */
								channelFreqN -= 0x00A2E8BA ;
								/* every channel is now ~3813000 steps further */
								WIFI_Host_CloseChannel(wifi->udpSocket) ;
								wifi->channel = (wifi->udpSocket,channelFreqN / 3813000)+1 ;
								WIFI_Host_OpenChannel(wifi->channel) ;
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

void WIFI_triggerIRQ(wifimac_t *wifi, u8 irq)
{
	/* trigger an irq */
	u16 irqBit = 1 << irq ;
	if (wifi->IE.val & irqBit)
	{
		wifi->IF.val |= irqBit ;
        NDS_makeARM7Int(24) ;   /* cascade it via arm7 wifi irq */
	}
}

void WIFI_Init(wifimac_t *wifi)
{
	WIFI_resetRF(&wifi->RF) ;
	WIFI_Host_InitSystem() ;
	wifi->udpSocket = WIFI_Host_OpenChannel(1) ;
}

void WIFI_RXPutWord(wifimac_t *wifi,u16 val)
{
	/* abort when RX data queuing is not enabled */
	if (!(wifi->RXCnt & 0x8000)) return ;
	/* abort when ringbuffer is full */
	if (wifi->RXReadCursor == wifi->RXHWWriteCursor) return ;
	/* write the data to cursor position */
	wifi->circularBuffer[wifi->RXHWWriteCursor & 0xFFF] ;
	/* move cursor by one */
	wifi->RXHWWriteCursor++ ;
	/* wrap around */
	wifi->RXHWWriteCursor %= (wifi->RXRangeEnd - wifi->RXRangeBegin) >> 1 ;
}

void WIFI_TXStart(wifimac_t *wifi,u8 slot)
{
	if (wifi->TXSlot[slot] & 0x8000)	/* is slot enabled? */
	{
		u16 txLen ;
		/* the address has to be somewhere in the circular buffer, so drop the other bits */
		u16 address = (wifi->TXSlot[slot] & 0x7FFF) ;
		/* is there even enough space for the header (6 hwords) in the tx buffer? */
		if (address > 0x1000-6) return ; 

		/* 12 byte header TX Header: http://www.akkit.org/info/dswifi.htm#FmtTx */
		txLen = ntohs(wifi->circularBuffer[address+5]) ;
		/* zero length */
		if (txLen==0) return ;
		/* unsupported txRate */
		switch (ntohs(wifi->circularBuffer[address+4]))
		{
			case 10: /* 1 mbit */
			case 20: /* 2 mbit */
				break ;
			default: /* other rates */
				return ;
		}

		/* FIXME: calculate FCS */

		WIFI_triggerIRQ(wifi,WIFI_IRQ_SENDSTART) ;
		WIFI_Host_SendData(wifi->udpSocket,wifi->channel,(u8 *)&wifi->circularBuffer[address],txLen) ;
		WIFI_triggerIRQ(wifi,WIFI_IRQ_SENDCOMPLETE) ;
	}
} 

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
		address &= 0x1FFFF ;
        wifi->circularBuffer[address >> 1] = val ;
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
		case REG_WIFI_AID:			/* CHECKME: are those two really the same? */
		case REG_WIFI_AIDCPY:
			wifi->aid = val ;
			break ;
		case REG_WIFI_RETRYLIMIT:
			wifi->retryLimit = val ;
			break ;
		case REG_WIFI_WEPCNT:
			wifi->WEP_enable = (val & 0x8000) != 0 ;
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
			wifi->circularBuffer[(wifi->CircBufWriteAddress >> 1) & 0xFFF] = val ;
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
		case REG_WIFI_CIRCBUFWR_SKIP:
			wifi->CircBufSkip = val ;
			break ;
		case REG_WIFI_BEACONTRANS:
			wifi->BEACONSlot = val & 0x7FFF ;
			wifi->BEACON_enable = (val & 0x8000) != 0 ;
			break ;
		case REG_WIFI_TXLOC1:
		case REG_WIFI_TXLOC2:
		case REG_WIFI_TXLOC3:
			wifi->TXSlot[(address - REG_WIFI_TXLOC1) >> 2] = val ;
			break ;
		case REG_WIFI_TXOPT:
			if (val == 0xFFFF)
			{
				/* reset TX logic */
				/* CHECKME */
                wifi->TXSlot[0] = 0 ; wifi->TXSlot[1] = 0 ; wifi->TXSlot[2] = 0 ;
                wifi->TXOpt = 0 ;
                wifi->TXCnt = 0 ;
			} else
			{
				wifi->TXOpt = val ;
			}
			break ;
		case REG_WIFI_TXCNT:
			wifi->TXCnt = val ;
			if (val & 0x01)	WIFI_TXStart(wifi,0) ;
			if (val & 0x04)	WIFI_TXStart(wifi,1) ;
			if (val & 0x08)	WIFI_TXStart(wifi,2) ;
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
		default:
			val = 0 ;       /* not handled yet */
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
		case REG_WIFI_MODE:
			return wifi->macMode ;
		case REG_WIFI_WEP:
			return wifi->wepMode ;
		case REG_WIFI_IE:
			return wifi->IE.val ;
		case REG_WIFI_IF:
			return wifi->IF.val ;
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
			return 0 ;
		case REG_WIFI_MAC0:
		case REG_WIFI_MAC1:
		case REG_WIFI_MAC2:
			return wifi->mac.words[(address - REG_WIFI_MAC0) >> 1] ;
		case REG_WIFI_BSS0:
		case REG_WIFI_BSS1:
		case REG_WIFI_BSS2:
			return wifi->bss.words[(address - REG_WIFI_BSS0) >> 1] ;
		default:
			return 0 ;
	}
}


void WIFI_usTrigger(wifimac_t *wifi)
{
	u8 dataBuffer[0x2000] ; 
	u16 rcvSize ;
	/* a usec (=3F03 cycles) has passed */
	if (wifi->usecEnable)
		wifi->usec++ ;
	if ((wifi->ucmpEnable) && (wifi->ucmp == wifi->usec))
	{
			WIFI_triggerIRQ(wifi,WIFI_IRQ_TIMEBEACON) ;
	}
	/* receive check, given a 2 mbit connection, 2 bits per usec can be transfered. */
	/* for a packet of 32 Bytes, at least 128 usec passed, we will use the 32 byte accuracy to reduce load */
	if (!(wifi->RXCheckCounter++ & 0x7F))
	{
		/* check if data arrived in the meantime */
		rcvSize = WIFI_Host_RecvData(wifi->udpSocket,dataBuffer,0x2000) ;
		if (rcvSize)
		{
			u16 i ;
			/* process data, put it into mac memory */
			WIFI_triggerIRQ(wifi,WIFI_IRQ_RECVSTART) ;
			for (i=0;i<(rcvSize+1) << 1;i++)
			{
				WIFI_RXPutWord(wifi,((u16 *)dataBuffer)[i]) ;
			}
			WIFI_triggerIRQ(wifi,WIFI_IRQ_RECVCOMPLETE) ;
		}
	}
}

/*******************************************************************************

	Host system operations

 *******************************************************************************/

socket_t WIFI_Host_OpenChannel(u8 num)
{
	sockaddr_t address ;
	/* create a new socket */
	socket_t channelSocket = socket(AF_INET,SOCK_DGRAM,0) ;
	/* set the sockets address */
	memset(&address,0,sizeof(sockaddr_t)) ;
	address.sa_family = AF_INET ;
	*(u32 *)&address.sa_data[2] = htonl(0) ; 				/* IP: any */
	*(u16 *)&address.sa_data[0] = htons(BASEPORT + num-1) ; /* Port */
	bind(channelSocket,&address,sizeof(sockaddr_t)) ;
	return channelSocket ;
}

void WIFI_Host_CloseChannel(socket_t sock)
{
	if (sock!=INVALID_SOCKET)
	{
#ifdef WIN32
		closesocket(sock) ;
#else
		close(sock) ;
#endif
	}
}

void WIFI_Host_SendData(socket_t sock, u8 channel, u8 *data, u16 length)
{
	sockaddr_t address ;
	/* create the frame to validate data: "DSWIFI" string and a u16 constant of the payload length */
	u8 *frame = (u8 *)malloc(length + 8) ;
	sprintf((char *)frame,"DSWIFI") ;
	*(u16 *)(frame+6) = htons(length) ;
	memcpy(frame+8,data,length) ;
	/* target address */
	memset(&address,0,sizeof(sockaddr_t)) ;
	address.sa_family = AF_INET ;
	*(u32 *)&address.sa_data[2] = htonl(0xFFFFFFFF) ; 			/* IP: broadcast */
	*(u16 *)&address.sa_data[0] = htons(BASEPORT + channel-1) ; /* Port */
	/* send the data now */
	sendto(sock,(const char *)frame,length+8,0,&address,sizeof(sockaddr_t)) ;
	/* clean up frame buffer */
	free(frame) ;
}

u16 WIFI_Host_RecvData(socket_t sock, u8 *data, u16 maxLength)
{
	fd_set dataCheck ;
	struct timeval tv ;
	FD_ZERO(&dataCheck) ;
	FD_SET(sock,&dataCheck) ;
	tv.tv_sec = 0 ;
	tv.tv_usec = 0 ;
	/* check if there is data, without blocking */
	if (select(1,&dataCheck,0,0,&tv))
	{
		/* there is data available */
		u32 receivedBytes = recv(sock,(char*)data,maxLength,0) ;
		if (receivedBytes < 8) return 0 ;			/* this cant be data for us, the header alone is 8 bytes */
		if (memcmp(data,"DSWIFI",6)!=0) return 0 ;	/* this isnt data for us, as the tag is not our */
		if (ntohs(*(u16 *)(data+6))+8 != receivedBytes) return 0 ; /* the datalen+headerlen is not what we received */
		memmove(data,data+8,receivedBytes-8) ;
		return receivedBytes-8 ;
	}
	return 0 ;
}

BOOL WIFI_Host_InitSystem(void)
{
	#ifdef WIN32
	WSADATA wsaData ;
	WORD version = MAKEWORD(1,1) ;
	if (WSAStartup(version,&wsaData))
	{
		return FALSE ;
	}
	#endif
	return TRUE ;
}

void WIFI_Host_ShutdownSystem(void)
{
	#ifdef WIN32
	WSACleanup() ;
	#endif
}

#endif
