#include "filter/filter.h"

class VideoInfo
{
public:

	int width;
	int height;

	int rotation;
	int screengap;

	int currentfilter;

	CACHE_ALIGN u8 filteredbuffer[4*256*192*4];

	enum {
		NONE,
		HQ2X,
		_2XSAI,
		SUPER2XSAI,
		SUPEREAGLE,
		SCANLINE,
		BILINEAR
	};


	void reset() {
		width = 256;
		height = 384;
	}

	void setfilter(int filter) {

		currentfilter = filter;

		switch(filter) {

			case NONE:
				width = 256;
				height = 384;
				break;
			default:
				width = 512;
				height = 768;
				break;
		}
	}

	SSurface src;
	SSurface dst;

	u16* finalBuffer() const
	{
		if(currentfilter == NONE)
			return (u16*)GPU_screen;
		else return (u16*)filteredbuffer;
	}

	void filter() {

		src.Height = 384;
		src.Width = 256;
		src.Pitch = 512;
		src.Surface = (u8*)GPU_screen;

		dst.Height = 768;
		dst.Width = 512;
		dst.Pitch = 1024;
		dst.Surface = (u8*)filteredbuffer;

		switch(currentfilter)
		{
			case NONE:
				break;
			case HQ2X:
				RenderHQ2X(src, dst);
				break;
			case _2XSAI:
				Render2xSaI (src, dst);
				break;
			case SUPER2XSAI:
				RenderSuper2xSaI (src, dst);
				break;
			case SUPEREAGLE:
				RenderSuperEagle (src, dst);
				break;
			case SCANLINE:
				RenderScanline(src, dst);
				break;
			case BILINEAR:
				RenderBilinear(src, dst);
				break;
		}
	}

	int size() {
		return width*height;
	}

	int ratio() {
		return width / 256;
	}

	int rotatedwidth() {
		switch(rotation) {
			case 0:
				return width;
			case 90:
				return height;
			case 180:
				return width;
			case 270:
				return height;
			default:
				return 0;
		}
	}

	int rotatedheight() {
		switch(rotation) {
			case 0:
				return height;
			case 90:
				return width;
			case 180:
				return height;
			case 270:
				return width;
			default:
				return 0;
		}
	}

	int rotatedwidthgap() {
		switch(rotation) {
			case 0:
				return width;
			case 90:
				return height + screengap;
			case 180:
				return width;
			case 270:
				return height + screengap;
			default:
				return 0;
		}
	}

	int rotatedheightgap() {
		switch(rotation) {
			case 0:
				return height + screengap;
			case 90:
				return width;
			case 180:
				return height + screengap;
			case 270:
				return width;
			default:
				return 0;
		}
	}
};
