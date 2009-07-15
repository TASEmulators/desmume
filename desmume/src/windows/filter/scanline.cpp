#include "filter.h"
#include "types.h"
#include <stdio.h>
#include <string.h>

typedef u64 uint64;

// stretches a single line
inline void DoubleLine16( uint16 *lpDst, uint16 *lpSrc, unsigned int Width){
	while(Width--){
		*lpDst++ = *lpSrc;
		*lpDst++ = *lpSrc++;
	}
}

void RenderScanline( SSurface Src, SSurface Dst)
{
	uint16 *lpSrc;
	unsigned int H;

	const uint32 srcHeight = Src.Height;

	const unsigned int srcPitch = Src.Pitch >> 1;
	lpSrc = reinterpret_cast<uint16 *>(Src.Surface);

	const unsigned int dstPitch = Dst.Pitch >> 1;
	uint16 *lpDst = (uint16*)Dst.Surface;
	if(Src.Width != 512)
		for (H = 0; H < srcHeight; H++, lpSrc += srcPitch)
			DoubleLine16 (lpDst, lpSrc, Src.Width), lpDst += dstPitch,
			memset (lpDst, 0, 512*2), lpDst += dstPitch;
	else
		for (H = 0; H < srcHeight; H++, lpSrc += srcPitch)
			memcpy (lpDst, lpSrc, Src.Width << 1), lpDst += dstPitch,
			memset (lpDst, 0, 512*2), lpDst += dstPitch;
}