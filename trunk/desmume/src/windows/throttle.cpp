//THIS SPEED THROTTLE IS TAKEN FROM FCEUX.
//Copyright (C) 2002 Xodnizel

#include "../common.h"
#include "../types.h"
#include "../debug.h"
#include "../console.h"
#include <windows.h>

int FastForward=0;
static u64 tmethod,tfreq;
static const u64 core_desiredfps = 3920763; //59.8261
static u64 desiredfps = core_desiredfps;
static int desiredFpsScalerIndex = 2;
static u64 desiredFpsScalers [] = {
	1024,
	512,
	256, // 100%
	//224,
	192,
	128, // 50%
	96,
	64, // 25%
	42,
	32,
	16,
};

void IncreaseSpeed(void) {

	if(desiredFpsScalerIndex)
		desiredFpsScalerIndex--;
	u64 desiredFpsScaler = desiredFpsScalers[desiredFpsScalerIndex];
	desiredfps = core_desiredfps * desiredFpsScaler / 256;
	printf("Throttle fps scaling increased to: %f\n",desiredFpsScaler/256.0);
}

void DecreaseSpeed(void) {

	if(desiredFpsScalerIndex != sizeof(desiredFpsScalers)/sizeof(desiredFpsScalers[0]) - 1)
		desiredFpsScalerIndex++;
	u64 desiredFpsScaler = desiredFpsScalers[desiredFpsScalerIndex];
	desiredfps = core_desiredfps * desiredFpsScaler / 256;
	printf("Throttle fps scaling decreased to: %f\n",desiredFpsScaler/256.0);
}

static u64 GetCurTime(void)
{
	if(tmethod)
	{
		u64 tmp;

		/* Practically, LARGE_INTEGER and u64 differ only by signness and name. */
		QueryPerformanceCounter((LARGE_INTEGER*)&tmp);

		return(tmp);
	}
	else
		return((u64)GetTickCount());

}

void InitSpeedThrottle(void)
{
	tmethod=0;
	if(QueryPerformanceFrequency((LARGE_INTEGER*)&tfreq))
	{
		tmethod=1;
	}
	else
		tfreq=1000;
	tfreq<<=16;    /* Adjustment for fps returned from FCEUI_GetDesiredFPS(). */
}

static bool behind=false;
bool ThrottleIsBehind() {
	return behind;
}

int SpeedThrottle(void)
{
	static u64 ttime,ltime;

	if(FastForward)
		return (0);

	behind = false;

waiter:

	ttime=GetCurTime();


	if( (ttime-ltime) < (tfreq/desiredfps) )
	{
		u64 sleepy;
		sleepy=(tfreq/desiredfps)-(ttime-ltime);  
		sleepy*=1000;
		if(tfreq>=65536)
			sleepy/=tfreq>>16;
		else
			sleepy=0;
		if(sleepy>100)
		{
			// block for a max of 100ms to
			// keep the gui responsive
			Sleep(100);
			return 1;
		}
		Sleep((DWORD)sleepy);
		goto waiter;
	}
	if( (ttime-ltime) >= (tfreq*4/desiredfps))
		ltime=ttime;
	else
	{
		ltime+=tfreq/desiredfps;

		if( (ttime-ltime) >= (tfreq/desiredfps) ) // Oops, we're behind!
		{
			behind = true;
			return 0;
		}
	}
	return(0);
}
