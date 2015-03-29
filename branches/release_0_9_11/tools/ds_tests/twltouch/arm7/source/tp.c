/*
    Read touchscreen positions.
 
    Returns 1 if touched, and the average position in xpos and ypos. Otherwise, if not touched, returns 0.
 */

#include <nds.h>

void CDC_ReadRegistersEx(u32 page, u32 reg, u8* buf, u32 size);
 
u32 TouchRead(u32* xpos, u32* ypos)
{
	int i;
	u8 buf[20];
	u16 x[5];
	u16 y[5];
	u32 xavg;
	u32 yavg;
	u8 state;
 
 
	state = CDC_ReadRegisterEx(3,9);
	if ( (state & 0xC0) == 0x40 )
		return 0;
 
	state = CDC_ReadRegisterEx(3, 14);
	if (state & 2)
		return 2;
 
 
	CDC_ReadRegistersEx(252, 1, buf, 20);
 
	xavg = 0;
	yavg = 0;
 
	for(i=0; i<5; i++)
	{
		x[i] = buf[i*2+1] | (buf[i*2+0]<<8);
		y[i] = buf[10+i*2+1] | (buf[10+i*2+0]<<8);
 
		if (x[i] & 0xF000)
			return 3;
 
		if (y[i] & 0xF000)
			return 4;
 
		xavg += x[i];
		yavg += y[i];
	}
 
	xavg /= 5;
	yavg /= 5;
 
	*xpos = xavg;
	*ypos = yavg;
 
	return 1;
}

