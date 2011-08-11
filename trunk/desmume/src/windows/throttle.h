#ifndef _THROTTLE_H_
#define _THROTTLE_H_

extern int FastForward;
extern bool FrameLimit;
void IncreaseSpeed();
void DecreaseSpeed();

void InitSpeedThrottle();
void SpeedThrottle();

void AutoFrameSkip_NextFrame();
void AutoFrameSkip_IgnorePreviousDelay();
int AutoFrameSkip_GetSkipAmount(int min=0, int max=9);

#endif
