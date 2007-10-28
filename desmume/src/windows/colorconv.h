#ifndef COLORCONV_H
#define COLORCONV_H

// Convert B5G5R5 color format into R8G8B8 color format
unsigned int ColorConv_B5R5R5ToR8G8B8(const unsigned int color)
{
	return	(((color&31)<<16)<<3)		|	// red
			((((color>>5)&31)<<8)<<3)	|	// green
			(((color>>10)&31)<<3);			// blue
}

#endif // COLORCONV_H
