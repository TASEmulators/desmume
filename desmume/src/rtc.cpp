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
#include <string.h>
#include "saves.h"
#ifdef WIN32
#include "windows/main.h"
#endif
#include "movie.h"

static time_t CalcTimeDifference(void)
{
	time_t t1, t2;
	tm *tm_tmp;

	time(&t1);
	tm_tmp = gmtime(&t1);
	t2 = mktime(tm_tmp);

	return (t2 - t1);
}

time_t gmmktime(struct tm *timeptr)
{
	return mktime(timeptr) - CalcTimeDifference();
}

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

	u8 cmdBitsSize[8];
} _RTC;

_RTC	rtc;

SFORMAT SF_RTC[]={
	{ "R000", 1, 1, &rtc.regStatus1},
	{ "R010", 1, 1, &rtc.regStatus2},
	{ "R020", 1, 1, &rtc.regAdjustment},
	{ "R030", 1, 1, &rtc.regFree},

	{ "R040", 1, 1, &rtc._prevSCK},
	{ "R050", 1, 1, &rtc._prevCS},
	{ "R060", 1, 1, &rtc._prevSIO},
	{ "R070", 1, 1, &rtc._SCK},
	{ "R080", 1, 1, &rtc._CS},
	{ "R090", 1, 1, &rtc._SIO},
	{ "R100", 1, 1, &rtc._DD},
	{ "R110", 2, 1, &rtc._REG},

	{ "R120", 1, 1, &rtc.cmd},
	{ "R130", 1, 1, &rtc.cmdStat},
	{ "R140", 1, 1, &rtc.bitsCount},
	{ "R150", 1, 8, &rtc.data[0]},

	{ "R160", 1, 8, &rtc.cmdBitsSize[0]},

	{ 0 }
};

static const u8 kDefaultCmdBitsSize[8] = {8, 8, 56, 24, 0, 24, 8, 8};

static inline u8 toBCD(u8 x)
{
	return ((x / 10) << 4) | (x % 10);
}

bool moviemode=false;

struct tm* rtcGetTime(void)
{
	struct tm *tm = NULL;
	if(movieMode == MOVIEMODE_INACTIVE) {
		time_t	timer;
		time(&timer);
		tm = localtime(&timer);
	}
	else {
		//now, you might think it is silly to go through all these conniptions
		//when we could just assume that there are 60fps and base the seconds on frameCounter/60
		//but, we were imagining that one day we might need more precision

		const u32 arm9rate_unitsperframe = 560190<<1;
		const u32 arm9rate_unitspersecond = (u32)(arm9rate_unitsperframe * 59.8261);

		u64 totalcycles = (u64)arm9rate_unitsperframe * currFrameCounter;
		u64 totalseconds=totalcycles/arm9rate_unitspersecond;

		time_t timer;
		struct tm t;

		// store start time into structure tm
		timer = (time_t) currMovieData.rtcStart;
		memcpy(&t, gmtime(&timer), sizeof(struct tm));
		// advance it according to the frame counter
		t.tm_sec += totalseconds;
		// then, normalize it
		timer = gmmktime(&t);
		tm = gmtime(&timer);
	}
	tm->tm_year = 100 + (tm->tm_year % 100); // 20XX
	return tm;
}

static void rtcRecv()
{
	//INFO("RTC Read command 0x%02X\n", (rtc.cmd >> 1));

	memset(&rtc.data[0], 0, sizeof(rtc.data));
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
				struct tm *tm = rtcGetTime();
				rtc.data[0] = toBCD(tm->tm_year % 100);
				rtc.data[1] = toBCD(tm->tm_mon + 1);
				rtc.data[2] = toBCD(tm->tm_mday);
				rtc.data[3] = (tm->tm_wday + 7) & 7;
				if (rtc.data[3] == 7) rtc.data[3] = 6;
				if (!(rtc.regStatus1 & 0x02)) tm->tm_hour %= 12;
				rtc.data[4] = ((tm->tm_hour < 12) ? 0x00 : 0x40) | toBCD(tm->tm_hour);
				rtc.data[5] =  toBCD(tm->tm_min);
				rtc.data[6] =  toBCD(tm->tm_sec);
				break;
			}
		case 3:				// time
			{
				//INFO("RTC: read time\n");
				struct tm *tm = rtcGetTime();
				if (!(rtc.regStatus1 & 0x02)) tm->tm_hour %= 12;
				rtc.data[0] = ((tm->tm_hour < 12) ? 0x00 : 0x40) | toBCD(tm->tm_hour);
				rtc.data[1] =  toBCD(tm->tm_min);
				rtc.data[2] =  toBCD(tm->tm_sec);
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
	//INFO("RTC write command 0x%02X\n", (rtc.cmd >> 1));
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
	memset(&rtc, 0, sizeof(rtc));
	memcpy(&rtc.cmdBitsSize[0],kDefaultCmdBitsSize,8);

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
							rtc.cmdBitsSize[rtc.cmd >> 1] = 24;
						else
							rtc.cmdBitsSize[rtc.cmd >> 1] = 8;
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
				if (rtc.bitsCount == rtc.cmdBitsSize[rtc.cmd >> 1])
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
				if((rtc.data[(rtc.bitsCount >> 3)] >> (rtc.bitsCount & 0x07)) & 0x01) 
					rtc._REG |= 0x01;
				else
					rtc._REG &= ~0x01;

				rtc.bitsCount++;
				if (rtc.bitsCount == rtc.cmdBitsSize[rtc.cmd >> 1] || (!(val & 0x04)))
					rtc.cmdStat = 0;
			}
		break;

	}

	rtc._prevSIO = rtc._SIO;
	rtc._prevSCK = rtc._SCK;
	rtc._prevCS  = rtc._CS;
}
