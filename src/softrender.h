/// The VERGE 3 Project is originally by Ben Eirich and is made available via
///  the BSD License.
///
/// Please see LICENSE in the project's root directory for the text of the
/// licensing agreement.  The CREDITS file in the same directory enumerates the
/// folks involved in this public build.
///
/// If you have altered this source file, please log your name, date, and what
/// changes you made below this line.


#ifndef SOFTRENDER_H
#define SOFTRENDER_H

#include <string.h>
#include "softrender_config.h"

namespace softrender {


class image
{
public:
	int width, height, pitch;
	int cx1, cy1, cx2, cy2;
	int bpp, shell;
	void *data;

	image();
	image(int xres, int yres);
	~image();
	void delete_data();
	void SetClip(int x1, int y1, int x2, int y2);
	void GetClip(int &x1, int &y1, int &x2, int &y2);
};

extern int vid_bpp, vid_xres, vid_yres, vid_bytesperpixel, transColor;
extern bool vid_window;
extern image *screen;
extern int alpha, ialpha;
extern int skewlines[];

int SetLucent(int percent);

//void run2xSAI(image *src, image *dest);

class renderbase {
private:

	void Oval(int x, int y, int xe, int ye, int color, int Fill, image *dest);
public:
	//render methods that do not depend on the pixel format
	void   Line(int x, int y, int xe, int ye, int color, image *dest);
	void   Box(int x, int y, int xe, int ye, int color, image *dest);
	void   Rect(int x, int y, int xe, int ye, int color, image *dest);
	void   Sphere(int x, int y, int xradius, int yradius, int color, image *dest);
	void   Circle(int x, int y, int xradius, int yradius, int color, image *dest);
	void   RectVGrad(int x, int y, int xe, int ye, int color, int color2, image *dest);
	void   RectHGrad(int x, int y, int xe, int ye, int color, int color2, image *dest);
	void   RectRGrad(int x1, int y1, int x2, int y2, int color1, int color2, image *dest);
	void   Rect4Grad(int x1, int y1, int x2, int y2, int color1, int color2, int color3, int color4, image *dest);

	//things that werent originally even blitter-specific
	void GrabRegion(int sx1, int sy1, int sx2, int sy2, int dx, int dy, image *s, image *d);
	
private:
	bool	textBoxBorder;
	template<typename FONT>
	void print_char(int scale, int x, int y, int color, char c, image *dest)
	{
		int height = FONT::height();
		int width = FONT::width(c);
		int ofs = FONT::haveContour()?1:0;

		if (FONT::haveContour())
		{
			for (int yc=0; yc<height+2; yc++)
				for (int xc=0; xc<width+2; xc++)
				{
					if(FONT::contour(c,xc,yc)) {
						for(int xi=0;xi<scale;xi++)
							for(int yi=0;yi<scale;yi++)
								PutPixel((xc*scale+x)+xi,(yc*scale+y)+ yi,MakeColor(0,0,0), dest);
					}
				}
		}

		for (int yc=0; yc<height; yc++)
			for (int xc=0; xc<width; xc++)
			{
				if(FONT::pixel(c,xc,yc)) {
					for(int xi=0;xi<scale;xi++)
						for(int yi=0;yi<scale;yi++)
							PutPixel((xc*scale+x)+xi+ofs,(yc*scale+y)+ yi+ofs,color, dest);
				}
			}
	}

public:
	void setTextBoxBorder(bool enabled) { textBoxBorder = enabled; }
	template<typename FONT>
	void PrintString(int scale, int x, int y, int color, char *str, image *dest)
	{
		int xc = x;
		int yc = y;

		int height = FONT::height();
		int boxSize= 0;
		if (FONT::haveContour()) 
			boxSize = ((FONT::width(*str)*scale+scale+1)*strlen(str));
		else
			boxSize = ((FONT::width(*str)*scale+scale)*strlen(str));

		int x1 = x;  // Remember where x where the line should start. -- Overkill 2005-12-28.
		for (; *str; ++str)
		{
			// New lines -- Overkill 2005-12-28.
			if (*str == '\n' || *str == '\r')
			{
				if (*str == '\r')
				{
					// Checks for \r\n so they aren't parsed as two seperate line breaks.
					if (!*++str) return;
					if (*str != '\n')
					{
						*--str;
					}
				}
				xc = x1;
				yc += height*scale + scale;
			} else {
				print_char<FONT>(scale, xc, yc, color, *str, dest);
				xc += FONT::width(*str)*scale + scale;
				if (FONT::haveContour()) xc += 1;
			}
		}
		if (textBoxBorder)
			Box(x, y, x+boxSize, y+height, MakeColor(0, 0, 0), dest);
	}


public:
	virtual int    MakeColor(int r, int g, int b)=0;
	virtual bool   GetColor(int c, int &r, int &g, int &b)=0;
	virtual void   Clear(int color, image *dest)=0;
	virtual int    ReadPixel(int x, int y, image *dest)=0;
	virtual void   PutPixel(int x, int y, int color, image *dest)=0;
	virtual void   VLine(int x, int y, int ye, int color, image *dest)=0;
	virtual void   HLine(int x, int y, int xe, int color, image *dest)=0;
	virtual void   Blit(int x, int y, image *src, image *dest)=0;
	virtual void   TBlit(int x, int y, image *src, image *dest)=0;
	//virtual void   AlphaBlit(int x, int y, image *src, image *alpha, image *dest);
	//virtual void   AdditiveBlit(int x, int y, image *src, image *dest);
	//virtual void   TAdditiveBlit(int x, int y, image *src, image *dest);
	//virtual void   SubtractiveBlit(int x, int y, image *src, image *dest);
	//virtual void   TSubtractiveBlit(int x, int y, image *src, image *dest);
	//virtual void   BlitTile(int x, int y, char *src, image *dest);
	//virtual void   TBlitTile(int x, int y, char *src, image *dest);
	//virtual void   ScaleBlit(int x, int y, int dw, int dh, image *src, image *dest);
	//virtual void   TScaleBlit(int x, int y, int dw, int dh, image *src, image *dest);
	//virtual void   WrapBlit(int x, int y, image *src, image *dst);
	//virtual void   TWrapBlit(int x, int y, image *src, image *dst);
	//virtual void   Silhouette(int x, int y, int c, image *src, image *dst);
	//virtual void   RotScale(int x, int y, float angle, float scale, image *src, image *dest);
	//virtual void   Mosaic(int xf, int yf, image *src);
	//virtual void   Timeless(int x, int y1, int y, image *src, image *dest);
	//virtual void   BlitWrap(int x, int y, image *src, image *dest);
	//virtual void   ColorFilter(int filter, image *img);
	//virtual void   Triangle(int x1, int y1, int x2, int y2, int x3, int y3, int c, image *dest);
	//virtual void   FlipBlit(int x, int y, int fx, int fy, image *src, image *dest);
	//virtual image* ImageFrom8bpp(byte *src, int width, int height, byte *pal);
	//virtual image* ImageFrom24bpp(byte *src, int width, int height);
	//virtual image* ImageFrom32bpp(byte *src, int width, int height);
	//// Overkill (2007-05-04)
	//virtual int    HSVtoColor(int h, int s, int v);
	//// Overkill (2007-05-04)
	//virtual void    GetHSV(int color, int &h, int &s, int &v);
	//// Overkill (2007-05-04)
	//virtual void    HueReplace(int hue_find, int hue_tolerance, int hue_replace, image *img);
	//// Overkill (2007-05-04)
	//virtual void	ColorReplace(int color_find, int color_replace, image *img);
};

class Trender16: public renderbase
{
public:
	virtual int    MakeColor(int r, int g, int b);
	virtual bool   GetColor(int c, int &r, int &g, int &b);
	virtual void   Clear(int color, image *dest);
	virtual int    ReadPixel(int x, int y, image *dest);
	virtual void   PutPixel(int x, int y, int color, image *dest);
	virtual void   VLine(int x, int y, int ye, int color, image *dest);
	virtual void   HLine(int x, int y, int xe, int color, image *dest);
	virtual void   Blit(int x, int y, image *src, image *dest);
	virtual void   TBlit(int x, int y, image *src, image *dest);
};

class Trender32: public renderbase
{
public:
	virtual int    MakeColor(int r, int g, int b);
	virtual bool   GetColor(int c, int &r, int &g, int &b);
	virtual void   Clear(int color, image *dest);
	virtual int    ReadPixel(int x, int y, image *dest);
	virtual void   PutPixel(int x, int y, int color, image *dest);
	virtual void   VLine(int x, int y, int ye, int color, image *dest);
	virtual void   HLine(int x, int y, int xe, int color, image *dest);
	virtual void   Blit(int x, int y, image *src, image *dest);
	virtual void   TBlit(int x, int y, image *src, image *dest);
};


class Trender15 : public Trender16
{
public:
	virtual int    MakeColor(int r, int g, int b);
	virtual bool   GetColor(int c, int &r, int &g, int &b);
};

class Trender51 : public Trender16
{
public:
	virtual int    MakeColor(int r, int g, int b);
	virtual bool   GetColor(int c, int &r, int &g, int &b);
};

extern Trender32 render32;
extern Trender16 render16;
extern Trender15 render15;
extern Trender51 render51;


} //end namespace

#endif
