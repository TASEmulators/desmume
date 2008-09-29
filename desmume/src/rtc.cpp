/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright (C) 2006-2008 DeSmuME team

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

#include "rtc.h"
#include "common.h"
#include "debug.h"
#include "armcpu.h"
#include <time.h>

const u8 valRTC[100]=	{	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
						0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
						0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
						0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
						0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
						0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
						0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
						0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
						0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
						0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99	};


typedef struct
{
	u16	reg;

	u8	regStatus1;
	u8	regStatus2;

	u8	cmd;

	u8	dataWrite;
	u8	bitPosWrite;

	u32	dataRead1;
	u32	dataRead2;
	u8	bitPosRead;
	u8	bitSizeRead;
	
	u8	cmdSize;
	
	u8	stat;		// 0 - write, 1 - read
} _RTC;

_RTC	rtc;

void rtcPost(u8 data);

void rtcInit()
{
	memset(&rtc, 0, sizeof(_RTC));
}

//====================================================== RTC write
INLINE void rtcPost(u8 data)
{
	if (rtc.bitPosRead != rtc.bitSizeRead) return;

	rtc.bitPosRead = 0;

	if ((data & 0x0F) == 0x06)
		rtc.cmd = reverseBitsInByte(data);
	else
		if ((data & 0xF0) == 0x60)
			rtc.cmd = data;
			else 
			{
				//printlog("RTC ERROR: command not supported\n");
				return;
			}
			
	rtc.stat = (rtc.cmd & 0x01);
	rtc.cmd = (rtc.cmd & 0x0E)>>1;
	//printlog("+++++RTC: execute command 0x%02X (%s)\n", rtc.cmd, rtc.stat?"read":"write");
	
	if (rtc.stat)
	{
		rtc.bitSizeRead = 8;
		rtc.dataRead1 = 0;
		rtc.dataRead2 = 0;
		//printlog("RTC: read %X\n", rtc.cmd);
		switch (rtc.cmd)
		{
			case 0:				// status register 1
				//printlog("RTC: read status 1 (%X) %s\n", rtc.regStatus1, rtc.revBits?"rev":"fwd");
				rtc.dataRead1 = rtc.regStatus1;
				rtc.regStatus1 &= 0x0F;
				break;
			case 1:				// status register 2
				//printlog("RTC: read status 2 %s\n", rtc.revBits?"rev":"fwd");
				rtc.dataRead1 = rtc.regStatus2;
				break;
			case 2:				// date & time
				{
					time_t	tm;
					time(&tm);
					struct tm *tm_local= localtime(&tm);
					
					rtc.dataRead2 = ( ((valRTC[tm_local->tm_hour] & 0x3F) << 0) 
									| ((tm_local->tm_hour>12?1:0) << 6)
									| (valRTC[tm_local->tm_min] << 8) 
									| (valRTC[tm_local->tm_sec] << 16));

					rtc.dataRead1 = ( (valRTC[tm_local->tm_year-100] << 0)
									| (valRTC[tm_local->tm_mon+1] << 8)
									| (valRTC[tm_local->tm_mday] << 16)
									| (valRTC[tm_local->tm_wday] << 24));

					rtc.bitSizeRead = 7 << 3;
					break;
				}
			case 3:				// time
				{
					//printlog("RTC: read time\n");
					time_t	tm;
					time(&tm);
					struct tm *tm_local= localtime(&tm);
					rtc.dataRead1 = ( ((valRTC[tm_local->tm_hour] & 0x3F) << 0) 
									| ((tm_local->tm_hour>12?1:0) << 6)
									| (valRTC[tm_local->tm_min] << 8) 
									| (valRTC[tm_local->tm_sec] << 16));
					
					rtc.bitSizeRead = 3 << 3;
					break;
				}
			case 4:				// freq/alarm 1
				//printlog("RTC: read freq");
				break;
			case 5:				// alarm 2
				//printlog("RTC: read alarm 2\n");
				break;
			case 6:				// clock adjust
				//printlog("RTC: read clock adjust\n");
				break;
			case 7:				// free register
				//printlog("RTC: read free register\n");
				break;
			default:
				rtc.bitSizeRead = 0;
				break;
		}
	}
	else
		{
			//printlog("RTC: write %X\n", rtc.cmd);
			switch (rtc.cmd)
			{
				case 0:				// status1
					//printlog("RTC: write status 1 (%X)\n", data);
					rtc.regStatus1 = data;
					if (rtc.regStatus1 & 0x10) 
					{
						//printlog("IRQ7\n");
						NDS_makeARM7Int(7);
					}
					break;
				case 1:				// status register 2
					rtc.regStatus2 = data;
					//printlog("RTC: write status 2 (%X)\n", data);
					break;
				case 2:				// date & time
					//printlog("RTC: write date & time (%X)\n", data);
					break;
				case 3:				// time
					//printlog("RTC: write time (%X)\n", data);
					break;
				case 4:				// freq/alarm 1
					//printlog("RTC: write freq (%X)", data);
					break;
				case 5:				// alarm 2
					//printlog("RTC: write alarm 2 (%X)\n", data);
					break;
				case 6:				// clock adjust
					//printlog("RTC: write clock adjust (%X)\n", data);
					break;
				case 7:				// free register
					//printlog("RTC: write free register (%X)\n", data);
					break;

			}
		}
}

INLINE u8 rtcRead()
{
	u8	val;

	if (rtc.bitPosRead == rtc.bitSizeRead)
					rtcPost(rtc.cmd);

	if (rtc.bitPosRead > 31)
		val =  ((rtc.dataRead2 >> rtc.bitPosRead) & 0x01);
	else
		val =  ((rtc.dataRead1 >> rtc.bitPosRead) & 0x01);
	
	rtc.bitPosRead++;
	return val;
}

INLINE void rtcWrite(u16 val)
{
	rtc.reg = val;

	if (!(rtc.reg & 0x02))
	{
		rtc.dataWrite |= ((val & 0x01) << rtc.bitPosWrite);
		
		rtc.bitPosWrite++;
		if (rtc.bitPosWrite == 8)
		{
			rtcPost(rtc.dataWrite);
			rtc.bitPosWrite = 0;
			rtc.dataWrite = 0;
		}
	}
}