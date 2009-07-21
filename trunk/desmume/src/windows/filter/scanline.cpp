#include "filter.h"
#include "types.h"
#include <stdio.h>
#include <string.h>

typedef u64 uint64;

extern CACHE_ALIGN u16 fadeOutColors[17][0x8000];

extern int scanline_filter_a, scanline_filter_b;

// stretches a single line
inline void DoubleLine16( uint16 *lpDst, uint16 *lpSrc, unsigned int Width){
	while(Width--){
		*lpDst++ = *lpSrc;
		*lpDst++ = fadeOutColors[scanline_filter_a][(*lpSrc++)];
	}
}

inline void DoubleLine16_2( uint16 *lpDst, uint16 *lpSrc, unsigned int Width){
	while(Width--){
		*lpDst++ = fadeOutColors[scanline_filter_a][(*lpSrc)];
		*lpDst++ = fadeOutColors[scanline_filter_b][(*lpSrc++)];
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
			DoubleLine16_2 (lpDst, lpSrc, Src.Width), lpDst += dstPitch;
			//memset (lpDst, 0, 512*2), lpDst += dstPitch;
	else
		for (H = 0; H < srcHeight; H++, lpSrc += srcPitch)
			memcpy (lpDst, lpSrc, Src.Width << 1), lpDst += dstPitch,
			memset (lpDst, 0, 512*2), lpDst += dstPitch;
}