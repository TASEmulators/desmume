#ifndef _THROTTLE_H_
#define _THROTTLE_H_

extern int FastForward;
void IncreaseSpeed();
void DecreaseSpeed();

void InitSpeedThrottle();
void SpeedThrottle();

void AutoFrameSkip_NextFrame();
int AutoFrameSkip_GetSkipAmount(int min=0, int max=9);

#endif
