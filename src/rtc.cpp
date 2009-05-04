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
#ifdef WIN32
#include "windows/main.h"
#endif
#include "movie.h"

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

struct movietime {

	int sec;
	int minute;
	int hour;
	int monthday;
	int month;
	int year;
	int weekday;
};

struct movietime movie;

bool moviemode=false;

void InitMovieTime(void)
{
	movie.year=9;
	movie.month=1;
	movie.monthday=1;
	movie.weekday=4;
}	

#ifdef WIN32
static void MovieTime(void) {

	//now, you might think it is silly to go through all these conniptions
	//when we could just assume that there are 60fps and base the seconds on frameCounter/60
	//but, we were imagining that one day we might need more precision

	const u32 arm9rate_unitsperframe = 560190<<1;
	const u32 arm9rate_unitspersecond = (u32)(arm9rate_unitsperframe * 59.8261);
	const u64 noon = (u64)arm9rate_unitspersecond * 60 * 60 * 12;
	           
	u64 frameCycles = (u64)arm9rate_unitsperframe * currFrameCounter;
	u64 totalcycles = frameCycles + noon;
	u32 totalseconds=totalcycles/arm9rate_unitspersecond;

	movie.sec=totalseconds % 60;
	movie.minute=totalseconds/60;
	movie.hour=movie.minute/60;

	//convert to sane numbers
	movie.minute=movie.minute % 60;
	movie.hour=movie.hour % 24;
}
#else
static void MovieTime(void)
{
}
#endif

static void rtcRecv()
{
	//INFO("RTC Read command 0x%02X\n", (rtc.cmd >> 1));
	memset(rtc.data, 0, sizeof(rtc.data));
	switch (rtc.cmd >> 1)
	{
		case 0:				// status register 1
			//INFO("RTC: read regstatus1 (0x%02X)\n", rtc.regStatus1);
			rtc.regStatus1 &= 0x0F;
			rtc.data[0] = rtc.regStatus1;
			//rtc.regStatus1 &= 0x7F;
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

				if(movieMode != MOVIEMODE_INACTIVE) {

					MovieTime();
					
					rtc.data[0]=toBCD(movie.year);
					rtc.data[1]=toBCD(movie.month);
					rtc.data[2]=toBCD(movie.monthday);
					rtc.data[3]=(movie.weekday + 6) & 7;
					if (!(rtc.regStatus1 & 0x02)) movie.hour %= 12;
					rtc.data[4] = ((movie.hour < 12) ? 0x00 : 0x40) | toBCD(movie.hour);		
					rtc.data[5]=toBCD(movie.minute);
					rtc.data[6]=toBCD(movie.sec);
					break;
				}
				else {

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
			}
		case 3:				// time
			{
				//INFO("RTC: read time\n");
				time_t	tm;
				time(&tm);
				struct tm *tm_local= localtime(&tm);

				if(movieMode != MOVIEMODE_INACTIVE) {

					MovieTime();

					if (!(rtc.regStatus1 & 0x02)) movie.hour %= 12;
					rtc.data[0] = ((movie.hour < 12) ? 0x00 : 0x40) | toBCD(movie.hour);
					rtc.data[1] =  toBCD(movie.minute);
					rtc.data[2] =  toBCD(movie.sec);
				}
				else {

					if (!(rtc.regStatus1 & 0x02)) tm_local->tm_hour %= 12;
					rtc.data[0] = ((tm_local->tm_hour < 12) ? 0x00 : 0x40) | toBCD(tm_local->tm_hour);
					rtc.data[1] =  toBCD(tm_local->tm_min);
					rtc.data[2] =  toBCD(tm_local->tm_sec);
					break;
				}
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
	//INFO("RTC write 0x%02X\n", (rtc.cmd >> 1));
	switch (rtc.cmd >> 1)
	{
		case 0:				// status register 1
			//INFO("RTC: write regstatus1 0x%02X\n", rtc.data[0]);
		//	rtc.regStatus1 &= 0xF1;
		//	rtc.regStatus1 |= (rtc.data[0] | 0x0E);
			rtc.regStatus1 = rtc.data[0];
			break;
		case 1:				// status register 2
			//INFO("RTC: write regstatus2 0x%02X\n", rtc.data[0]);
			rtc.regStatus2 = rtc.data[0];
			break;
		case 2:				// date & time
			//INFO("RTC: write date & time : %02X %02X %02X %02X %02X %02X %02X\n", rtc.data[0], rtc.data[1], rtc.data[2], rtc.data[3], rtc.data[4], rtc.data[5], rtc.data[6]);
		break;
		case 3:				// time
			//INFO("RTC: write time : %02X %02X %02X\n", rtc.data[0], rtc.data[1], rtc.data[2]);
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

u16 rtcRead() 
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
			if (rtc.bitsCount == 8)
			{
				//INFO("RTC command 0x%02X\n", rtc.cmd);

				// Little-endian command
				if((rtc.cmd & 0x0F) == 0x06)
				{
					u8 tmp = rtc.cmd;
					rtc.cmd = ((tmp & 0x80) >> 7) | ((tmp & 0x40) >> 5) | ((tmp & 0x20) >> 3) | ((tmp & 0x10) >> 1);
				}
				// Big-endian command
				else
				{
					rtc.cmd &= 0x0F;
				}

				if((rtc._prevSCK) && (!rtc._SCK))
				{
					rtc.bitsCount = 0;
					if ((rtc.cmd >> 1) == 0x04)
					{
						if ((rtc.regStatus2 & 0x0F) == 0x04)
							cmdBitsSize[rtc.cmd >> 1] = 24;
						else
							cmdBitsSize[rtc.cmd >> 1] = 8;
					}
					if (rtc.cmd & 0x01)
					{
						rtc.cmdStat = 4;
						rtcRecv();
					}
					else
					{
						rtc.cmdStat = 3;
					}
				}
			}

		break;

		case 3:			// write:
			if( (rtc._prevSCK) && (!rtc._SCK) )
			{
				if(rtc._SIO) rtc.data[rtc.bitsCount >> 3] |= (1 << (rtc.bitsCount & 0x07));
				rtc.bitsCount++;
				if (rtc.bitsCount == cmdBitsSize[rtc.cmd >> 1])
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
				if (rtc.bitsCount == cmdBitsSize[rtc.cmd >> 1])
					rtc.cmdStat = 0;
			}
		break;

	}

	rtc._prevSIO = rtc._SIO;
	rtc._prevSCK = rtc._SCK;
	rtc._prevCS  = rtc._CS;
}
