/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright 2008 CrazyMax
	Copyright 2008-2009 DeSmuME team

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

// TODO: interrupt handler

#include "rtc.h"
#include "common.h"
#include "debug.h"
#include "armcpu.h"
#include <time.h>
#include <string.h>

typedef struct
{
	// RTC registers
	u8	regStatus1;
	u8	regStatus2;
	u8	regAdjustment;
	u8	regFree;

	// BUS
	u8	_prevSCK;
	u8	_prevCS;
	u8	_prevSIO;
	u8	_SCK;
	u8	_CS;
	u8	_SIO;
	u8	_DD;
	u16	_REG;

	// command & data
	u8	cmd;
	u8	cmdStat;
	u8	bitsCount;
	u8	data[8];
} _RTC;

_RTC	rtc;

u8 cmdBitsSize[8] = {8, 8, 56, 24, 0, 24, 8, 8};

#define toBCD(x) ((x / 10) << 4) | (x % 10);

static void rtcRecv()
{
	//INFO("RTC Read command 0x%02X\n", rtc.cmd);
	memset(rtc.data, 0, sizeof(rtc.data));
	switch (rtc.cmd)
	{
		case 0:				// status register 1
			//INFO("RTC: read regstatus1 (0x%02X)\n", rtc.regStatus1);
			rtc.data[0] = rtc.regStatus1;
			rtc.regStatus1 &= 0x7F;
			break;
		case 1:				// status register 2
			//INFO("RTC: read regstatus2 (0x%02X)\n", rtc.regStatus1);
			rtc.data[0] = rtc.regStatus2;
			break;
		case 2:				// date & time
			{
				//INFO("RTC: read date & time\n");
				time_t	tm;
				time(&tm);
				struct tm *tm_local= localtime(&tm);
				tm_local->tm_year %= 100;
				tm_local->tm_mon++;
				rtc.data[0] = toBCD(tm_local->tm_year);
				rtc.data[1] = toBCD(tm_local->tm_mon);
				rtc.data[2] = toBCD(tm_local->tm_mday);
				rtc.data[3] =  (tm_local->tm_wday + 6) & 7;
				if (!(rtc.regStatus1 & 0x02)) tm_local->tm_hour %= 12;
				rtc.data[4] = ((tm_local->tm_hour < 12) ? 0x00 : 0x40) | toBCD(tm_local->tm_hour);
				rtc.data[5] =  toBCD(tm_local->tm_min);
				rtc.data[6] =  toBCD(tm_local->tm_sec);
				break;
			}
		case 3:				// time
			{
				//INFO("RTC: read time\n");
				time_t	tm;
				time(&tm);
				struct tm *tm_local= localtime(&tm);
				if (!(rtc.regStatus1 & 0x02)) tm_local->tm_hour %= 12;
				rtc.data[0] = ((tm_local->tm_hour < 12) ? 0x00 : 0x40) | toBCD(tm_local->tm_hour);
				rtc.data[1] =  toBCD(tm_local->tm_min);
				rtc.data[2] =  toBCD(tm_local->tm_sec);
				break;
			}
		case 4:				// freq/alarm 1
			/*if (cmdBitsSize[0x04] == 8)
				INFO("RTC: read INT1 freq\n");
			else
				INFO("RTC: read INT1 alarm1\n");*/
			//NDS_makeARM7Int(7);
			break;
		case 5:				// alarm 2
			//INFO("RTC: read alarm 2\n");
			break;
		case 6:				// clock adjust
			//INFO("RTC: read clock adjust\n");
			rtc.data[0] = rtc.regAdjustment;
			break;
		case 7:				// free register
			//INFO("RTC: read free register\n");
			rtc.data[0] = rtc.regFree;
			break;
	}
}

static void rtcSend()
{
	//INFO("RTC write 0x%02X\n", rtc.cmd);
	switch (rtc.cmd)
	{
		case 0:				// status register 1
			//INFO("RTC: write regstatus1 0x%02X\n", rtc.data[0]);
			rtc.regStatus1 &= 0xF1;
			rtc.regStatus1 |= (rtc.data[0] | 0x0E);
			break;
		case 1:				// status register 2
			//INFO("RTC: write regstatus2 0x%02X\n", rtc.data[0]);
			rtc.regStatus2 = rtc.data[0];
			break;
		case 2:				// date & time
			//INFO("RTC: write date & time\n");
		break;
		case 3:				// time
			//INFO("RTC: write time\n");
		break;
		case 4:				// freq/alarm 1
			/*if (cmdBitsSize[0x04] == 8)
				INFO("RTC: write INT1 freq 0x%02X\n", rtc.data[0]);
			else
				INFO("RTC: write INT1 alarm1 0x%02X\n", rtc.data[0]);*/
		break;
		case 5:				// alarm 2
			//INFO("RTC: write alarm 2\n");
		break;
		case 6:				// clock adjust
			//INFO("RTC: write clock adjust\n");
			rtc.regAdjustment = rtc.data[0];
			break;
		case 7:				// free register
			//INFO("RTC: write free register\n");
			rtc.regFree = rtc.data[0];
		break;
	}
}

void rtcInit() 
{
	memset(&rtc, 0, sizeof(_RTC));
	rtc.regStatus1 |= 0x02;
}

u8 rtcRead() 
{ 
	//INFO("MMU Read RTC 0x%02X (%03i)\n", rtc._REG, rtc.bitsCount);
	return (rtc._REG); 
}

void rtcWrite(u16 val)
{
	//INFO("MMU Write RTC 0x%02X (%03i)\n", val, rtc.bitsCount);
	rtc._DD  = (val & 0x10) >> 4;
	rtc._SIO = rtc._DD?(val & 0x01):rtc._prevSIO;
	rtc._SCK = (val & 0x20)?((val & 0x02) >> 1):rtc._prevSCK;
	rtc._CS  = (val & 0x40)?((val & 0x04) >> 2):rtc._prevCS;

	switch (rtc.cmdStat)
	{
		case 0:
			if ( (!rtc._prevCS) && (rtc._prevSCK) && (rtc._CS) && (rtc._SCK) )
			{
				rtc.cmdStat = 1;
				rtc.bitsCount = 0;
				rtc.cmd = 0;
			}
		break;

		case 1:
			if (!rtc._CS) 
			{
				rtc.cmdStat = 0;
				break;
			}

			if (rtc._SCK && rtc._DD) break;
			if (!rtc._SCK && !rtc._DD) break;

			rtc.cmd |= (rtc._SIO << rtc.bitsCount );
			rtc.bitsCount ++;
			if (rtc.bitsCount == 7)
			{
				if ( (rtc.cmd & 0x06) != 0x06 )
				{
					rtc.cmdStat = 0;
					//INFO("RTC uknown command 0x02%X\n", rtc.cmd);
					break;
				}
				rtc.cmdStat = 2;
				rtc.cmd >>= 4;
				rtc.cmd &= 0x07;
				//INFO("RTC command 0x%02X\n", rtc.cmd);
			}

		break;
		case 2:
			if (!rtc._CS) 
			{
				rtc.cmdStat = 0;
				break;
			}
			if((rtc._prevSCK) && (!rtc._SCK))
			{
				rtc.bitsCount = 0;
				if (rtc.cmd == 0x04)
				{
					if ((rtc.regStatus2 & 0x0F) == 0x04)
						cmdBitsSize[rtc.cmd] = 24;
					else
						cmdBitsSize[rtc.cmd] = 8;
				}
				if (rtc._SIO)
				{
					rtc.cmdStat = 4;
					rtcRecv();
				}
				else
				{
					rtc.cmdStat = 3;
				}
			}
		break;

		case 3:			// write:
			if( (rtc._prevSCK) && (!rtc._SCK) )
			{
				if(rtc._SIO) rtc.data[rtc.bitsCount >> 3] |= (1 << (rtc.bitsCount & 0x07));
				rtc.bitsCount++;
				if (rtc.bitsCount == cmdBitsSize[rtc.cmd])
				{
					rtcSend();
					rtc.cmdStat = 0;
				}
			}
		break;

		case 4:			// read:
			if( (rtc._prevSCK) && (!rtc._SCK) )
			{
				rtc._REG = val;
				if(rtc.data[rtc.bitsCount >> 3] >> (rtc.bitsCount & 0x07) & 0x01) 
					rtc._REG |= 0x01;
				else
					rtc._REG &= ~0x01;

				rtc.bitsCount++;
				if (rtc.bitsCount == cmdBitsSize[rtc.cmd])
					rtc.cmdStat = 0;
			}
		break;

	}

	rtc._prevSIO = rtc._SIO;
	rtc._prevSCK = rtc._SCK;
	rtc._prevCS  = rtc._CS;
}
