//THIS SPEED THROTTLE WAS TAKEN FROM FCEUX.
//Copyright (C) 2002 Xodnizel
//(the code might look quite different by now, though...)

#include "../common.h"
#include "../types.h"
#include "../debug.h"
#include "../console.h"
#include <windows.h>

int FastForward=0;
static u64 tmethod,tfreq,afsfreq;
static const u64 core_desiredfps = 3920763; //59.8261
static u64 desiredfps = core_desiredfps;
static float desiredspf = 65536.0f / core_desiredfps;
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
	desiredspf = 65536.0f / desiredfps;
	printf("Throttle fps scaling increased to: %f\n",desiredFpsScaler/256.0);
}

void DecreaseSpeed(void) {

	if(desiredFpsScalerIndex != sizeof(desiredFpsScalers)/sizeof(desiredFpsScalers[0]) - 1)
		desiredFpsScalerIndex++;
	u64 desiredFpsScaler = desiredFpsScalers[desiredFpsScalerIndex];
	desiredfps = core_desiredfps * desiredFpsScaler / 256;
	desiredspf = 65536.0f / desiredfps;
	printf("Throttle fps scaling decreased to: %f\n",desiredFpsScaler/256.0);
}

static u64 GetCurTime(void)
{
	if(tmethod)
	{
		u64 tmp;
		QueryPerformanceCounter((LARGE_INTEGER*)&tmp);
		return tmp;
	}
	else
	{
		return (u64)GetTickCount();
	}
}

void InitSpeedThrottle(void)
{
	tmethod=0;
	if(QueryPerformanceFrequency((LARGE_INTEGER*)&afsfreq))
		tmethod=1;
	else
		afsfreq=1000;
	tfreq = afsfreq << 16;
}

void SpeedThrottle()
{
	static u64 ttime, ltime;

waiter:
	if(FastForward)
		return;

	ttime = GetCurTime();

	if((ttime - ltime) < (tfreq / desiredfps))
	{
		u64 sleepy;
		sleepy = (tfreq / desiredfps) - (ttime - ltime);  
		sleepy *= 1000;
		if(tfreq >= 65536)
			sleepy /= afsfreq;
		else
			sleepy = 0;
		if(sleepy >= 10)
			Sleep((DWORD)(sleepy / 2)); // reduce it further beacuse Sleep usually sleeps for more than the amount we tell it to
		else if(sleepy > 0) // spin for <1 millisecond waits
			SwitchToThread(); // limit to other threads on the same CPU core for other short waits
		goto waiter;
	}
	if( (ttime-ltime) >= (tfreq*4/desiredfps))
		ltime=ttime;
	else
		ltime+=tfreq/desiredfps;
}


// auto frameskip

static u64 beginticks=0, endticks=0, diffticks=0;
static std::vector<float> diffs;
static const int SLIDING_WINDOW_SIZE = 32;
static float fSkipFrames = 0;
static float fSkipFramesError = 0;
static int minSkip = 0, maxSkip = 9;

static float maxDiff(const std::vector<float>& diffs)
{
	float maximum = 0;
	for(int i = 0; i < diffs.size(); i++)
		if(maximum < diffs[i])
			maximum = diffs[i];
	return maximum;
}

static void addDiff(float diff, std::vector<float>& diffs)
{
	// limit to about double the maximum, to reduce the impact of the occasional huge diffs
	// (using the last or average entry is a bad idea because some games do their own frameskipping)
	float maximum = maxDiff(diffs);
	if(!diffs.empty() && diff > maximum * 2 + 0.0001f)
		diff = maximum * 2 + 0.0001f;

	diffs.push_back(diff);

	if(diffs.size() > SLIDING_WINDOW_SIZE)
		diffs.erase(diffs.begin());
}

static float average(const std::vector<float>& diffs)
{
	float avg = 0;
	for(int i = 0; i < diffs.size(); i++)
		avg += diffs[i];
	if(diffs.size())
		avg /= diffs.size();
	return avg;
}

void AutoFrameSkip_NextFrame()
{
	endticks = GetCurTime();
	diffticks = endticks - beginticks;

	float diff = (float)diffticks / afsfreq;
	addDiff(diff,diffs);

	float avg = average(diffs);
	float overby = (avg - (desiredspf + (fSkipFrames + 2) * 0.000025f)) * 8;
	// try to avoid taking too long to catch up to the game running fast again
	if(overby < 0 && fSkipFrames > 4)
		overby = -fSkipFrames / 4;
	else if(avg < desiredspf)
		overby *= 8;

	fSkipFrames += overby;
	if(fSkipFrames < minSkip-1)
		fSkipFrames = minSkip-1;
	if(fSkipFrames > maxSkip+1)
		fSkipFrames = maxSkip+1;

	//printf("avg = %g, overby = %g, skipframes = %g\n", avg, overby, fSkipFrames);

	beginticks = GetCurTime();
}

int AutoFrameSkip_GetSkipAmount(int min, int max)
{
	int rv = (int)fSkipFrames;
	fSkipFramesError += fSkipFrames - rv;
	while(fSkipFramesError >= 1.0f)
	{
		fSkipFramesError -= 1.0f;
		rv++;
	}
	while(fSkipFramesError <= -1.0f)
	{
		fSkipFramesError += 1.0f;
		rv--;
	}
	if(rv < min)
		rv = min;
	if(rv > max)
		rv = max;
	minSkip = min;
	maxSkip = max;

	//printf("SKIPPED: %d\n", rv);

	return rv;
}


