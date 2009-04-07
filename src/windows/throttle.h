#ifndef _THROTTLE_H_
#define _THROTTLE_H_

void InitSpeedThrottle();
int SpeedThrottle();
bool ThrottleIsBehind();
extern int FastForward;
void IncreaseSpeed();
void DecreaseSpeed();

#endif
