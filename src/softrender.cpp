/// The VERGE 3 Project is originally by Ben Eirich and is made available via
///  the BSD License.
///
/// Please see LICENSE in the project's root directory for the text of the
/// licensing agreement.  The CREDITS file in the same directory enumerates the
/// folks involved in this public build.
///
/// If you have altered this source file, please log your name, date, and what
/// changes you made below this line.


/****************************************************************
	xerxes engine
	vid_manager.cpp
 ****************************************************************/

#include "softrender.h"
#include <math.h>
#include <string.h>

#include "softrender_v3sysfont.h"

namespace softrender {

	//instantiations
	Trender32 render32;
	Trender16 render16;
	Trender15 render15;
	Trender51 render51;


	template<typename T> 
	T sgn(const T& a) { 
		if (a<0)
			return -1;
		else if (a>0)
			return +1;
		else return 0;
	}

	template<typename T> 
	T abs(const T& a) { 
		if (a<0)
			return -a;
		else return a;
	}

	template<typename T>
	void SWAP(T& a, T& b) {
		T temp = a;
		a = b;
		b = temp;
	}


/***************************** data *****************************/

bool vid_initd = false;
bool vid_window = true;
int vid_bpp, vid_xres, vid_yres, vid_bytesperpixel;
int transColor;
image *screen;

/****************************************************************/

image::image()
{
}


image::image(int xres, int yres)
{
	width = pitch = xres;
	height = yres;
	cx1 = 0;
	cy1 = 0;
	cx2 = width - 1;
	cy2 = height - 1;
	bpp = vid_bpp;
	shell = 0;
	data = new char[width*height*vid_bytesperpixel];
	//switch (vid_bpp)
	//{
	//	case 15:
	//	case 16:
	//	case 61: data = new word[width * height];
	//			 break;
	//	case 32: data = new quad[width * height];
	//			 break;
	//}
}

void image::delete_data()
{
	delete[] (char *)data;
}

image::~image()
{
	if (data && !shell)
		delete_data();
}


void image::SetClip(int x1, int y1, int x2, int y2)
{
	cx1 = x1 >= 0 ? x1 : 0;
	cy1 = y1 >= 0 ? y1 : 0;
	cx1 = cx1 < width ? cx1 : width-1;
	cy1 = cy1 < height ? cy1 : height-1;
	cx2 = x2 >= 0 ? x2 : 0;
	cy2 = y2 >= 0 ? y2 : 0;
	cx2 = cx2 < width ? cx2 : width-1;
	cy2 = cy2 < height ? cy2 : height-1;
}


void image::GetClip(int &x1, int &y1, int &x2, int &y2)
{
	x1 = cx1;
	y1 = cy1;
	x2 = cx2;
	y2 = cy2;
}


//generic render methods

//template<typename FONT>
//void renderbase::print_char(char c, image *dest)
//{
//	int height = FONT::height(c);
//}

void renderbase::Line(int x, int y, int xe, int ye, int color, image *dest)
{
	int dx = xe - x, dy = ye - y,
		xg = sgn(dx), yg = sgn(dy),
		i = 0;
	float slope = 0;

	if (abs(dx) >= abs(dy))
	{
		slope = (float) dy / (float) dx;
		for (i=0; i!=dx; i+=xg)
			PutPixel(x+i, y+(int)(slope*i), color, dest);
	}
	else
	{
		slope = (float) dx / (float) dy;
		for (i=0; i!=dy; i+=yg)
			PutPixel(x+(int)(slope*i), y+i, color, dest);
	}
	PutPixel(xe, ye, color, dest);
}

void renderbase::Box(int x, int y, int x2, int y2, int color, image *dest)
{
	if (x2<x) SWAP(x,x2);
	if (y2<y) SWAP(y,y2);
	HLine(x, y, x2, color, dest);
	HLine(x, y2, x2, color, dest);
	VLine(x, y+1, y2-1, color, dest);
	VLine(x2, y+1, y2-1, color, dest);
}

void renderbase::Rect(int x, int y, int x2, int y2, int color, image *dest)
{
	if (y2<y) SWAP(y,y2);
	for (; y<=y2; y++)
		HLine(x, y, x2, color, dest);
}

void renderbase::Sphere(int x, int y, int xradius, int yradius, int color, image *dest)
{
	Oval(x-xradius, y-yradius, x+xradius-1, y+yradius-1, color, 1, dest);
}


void renderbase::Circle(int x, int y, int xradius, int yradius, int color, image *dest)
{
	Oval(x-xradius, y-yradius, x+xradius-1, y+yradius-1, color, 0, dest);
}

void renderbase::Oval(int x, int y, int xe, int ye, int color, int Fill, image *dest)
{
	int m=xe-x, n=ye-y,
		//mi=m/2,  //mbg 9/5/05 this variable is not being used. why? probably unnecessary
		ni=n/2,
		dx=4*m*m,
		dy=4*n*n,
		r=m*n*n,
		rx=2*r,
		ry=0,
		xx=m,
		lasty=9999;

	y+=ni;
	if (Fill)
		HLine(x, y, x+xx-1, color, dest);
	else {
		PutPixel(x, y, color, dest);
		PutPixel(x+xx, y, color, dest);
	}

	xe=x, ye=y;
	if (ni+ni==n)
	{
		ry=-2*m*m;
	}
	else
	{
		ry=2*m*m;
		ye++;

		if (Fill)
			HLine(xe, ye, xe+xx-1, color, dest);
		else {
			PutPixel(xe, ye, color, dest);
			PutPixel(xe+xx, ye, color, dest);
		}
	}

	while (xx>0)
	{
		if (r<=0)
		{
			xx-=2;
			x++, xe++;
			rx-=dy;
			r+=rx;
		}
		if (r>0)
		{
			y--, ye++;
			ry+=dx;
			r-=ry;
		}

		if (Fill && y != lasty)
		{
			HLine(x, y, x+xx-1, color, dest);
			HLine(xe, ye, xe+xx-1, color, dest);
		}
		else {
			PutPixel(x, y, color, dest);
			PutPixel(x+xx, y, color, dest);
			PutPixel(xe, ye, color, dest);
			PutPixel(xe+xx, ye, color, dest);
		}
		lasty = y;
	}
}



void renderbase::GrabRegion(int sx1, int sy1, int sx2, int sy2, int dx, int dy, image *s, image *d)
{
	int dcx1, dcy1, dcx2, dcy2;
	d->GetClip(dcx1, dcy1, dcx2, dcy2);

	if (sx1>sx2) SWAP(sx1, sx2);
	if (sy1>sy2) SWAP(sy1, sy2);
	int grabwidth = sx2 - sx1;
	int grabheight = sy2 - sy1;
	d->SetClip(dx, dy, dx+grabwidth, dy+grabheight);
	Blit(dx-sx1, dy-sy1, s, d);

	d->SetClip(dcx1, dcy1, dcx2, dcy2);
}

// Overkill 2006-02-04
void renderbase::RectVGrad(int x, int y, int x2, int y2, int color, int color2, image *dest)
{
	int r, g, b;
	int color_r, color_g, color_b;
	int color_r2, color_g2, color_b2;
	int i = 0;

	GetColor(color, color_r, color_g, color_b);
	GetColor(color2, color_r2, color_g2, color_b2);
	
	if (y2 < y) SWAP(y,y2);

	for(i = 0; i <= y2 - y; i++)
	{
		r = ((i * (color_r2 - color_r) / (y2 - y)) + color_r);
		g = ((i * (color_g2 - color_g) / (y2 - y)) + color_g);
		b = ((i * (color_b2 - color_b) / (y2 - y)) + color_b);
		HLine(x, y + i, x2, MakeColor(r, g, b), dest);
	}
}

// Overkill 2006-02-04
void renderbase::RectHGrad(int x, int y, int x2, int y2, int color, int color2, image *dest)
{
	int r, g, b;
	int color_r, color_g, color_b;
	int color_r2, color_g2, color_b2;
	int i = 0;

	GetColor(color, color_r, color_g, color_b);
	GetColor(color2, color_r2, color_g2, color_b2);
	
	if (x2 < x) SWAP(x,x2);

	for(i = 0; i <= x2 - x; i++)
	{
		r = ((i * (color_r2 - color_r) / (x2 - x)) + color_r);
		g = ((i * (color_g2 - color_g) / (x2 - x)) + color_g);
		b = ((i * (color_b2 - color_b) / (x2 - x)) + color_b);
		VLine(x + i, y, y2, MakeColor(r, g, b), dest);
	}
}

// janus 2006-07-22

// radial gradient: color1 is edge color, color2 is center color
void renderbase::RectRGrad(int x1, int y1, int x2, int y2, int color1, int color2, image *dest) 
{		
  int r1, r2, g1, g2, b1, b2;
  GetColor(color1, r1, g1, b1);
  GetColor(color2, r2, g2, b2);
	if (x2 < x1) SWAP(x1,x2);
	if (y2 < y1) SWAP(y1,y2);
  int w = (x2 - x1) + 1;
  int h = (y2 - y1) + 1;
  int z;
  if ((w == 0) || (h == 0)) return;
  // lookup table to eliminate the sqrt-per-pixel
  static bool tableInitialized = false;
  static struct pythagorasTable { byte v[256]; } pythagorasLookup[256];
  if (!tableInitialized) {
    // we initialize this table on the fly the first time this function is invoked
    // the output of the formula is not in the value range we require for 8bpp so we scale it
    // if you want a nonscaled gradient, set this constant to 1.0f
    const float pythagorasConstant = 0.707106781186547f;
    for (unsigned a = 0; a < 256; a++) {
      for (unsigned b = 0; b < 256; b++) {
        float value = sqrt((float)(a * a) + (b * b)) * pythagorasConstant;
        if (value <= 0)
          pythagorasLookup[a].v[b] = 0;
        else if (value >= 255)
          pythagorasLookup[a].v[b] = 255;
        else
          pythagorasLookup[a].v[b] = unsigned(value) & 0xFF;
      }
    }
    tableInitialized = true;
  }
  // color lookup table to reduce per-pixel blending computations
  int colorTable[256];
  for (int i = 0; i < 256; i++) {
    unsigned r = r2 + ((r1 - r2) * i / 255);
    unsigned g = g2 + ((g1 - g2) * i / 255);
    unsigned b = b2 + ((b1 - b2) * i / 255);
    colorTable[i] = MakeColor(r, g, b);
  }
  // x and y weight tables which unfortunately must be dynamically allocated due to the fact that their size depends on the size of the rect
  byte* xTable = new byte[w];
  byte* yTable = new byte[h];
  {
    // the float->int conversions in here are kind of expensive, it might be alright to do this in integer.
    // haven't tested the math enough to be sure though, so float it is ('cause i'm lazy)
    float xMul = 255.0f / (w / 2), yMul = 255.0f / (h / 2);
    int xOffset = (w / 2), yOffset = (h / 2);
    // these tables convert x/y coordinates into 0-255 weights which can be used to index the sqrt table
    for (int x = 0; x < w; x++) {
      xTable[x] = unsigned((float)abs(x - xOffset) * xMul) & 0xFF;
    }
    for (int y = 0; y < h; y++) {
      yTable[y] = unsigned((float)abs(y - yOffset) * yMul) & 0xFF;
    }
  }
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      z = pythagorasLookup[yTable[y]].v[xTable[x]];
      PutPixel(x+x1, y+y1, colorTable[z], dest);
    }
  }
  // cleanup locals
  delete[] xTable;
  delete[] yTable;
}

// 4-corner gradient: color1-4 are edges top left, top right, bottom left, bottom right
void renderbase::Rect4Grad(int x1, int y1, int x2, int y2, int color1, int color2, int color3, int color4, image *dest) 
{		
  int cr[4], cg[4], cb[4];
  GetColor(color1, cr[0], cg[0], cb[0]);
  GetColor(color2, cr[1], cg[1], cb[1]);
  GetColor(color3, cr[2], cg[2], cb[2]);
  GetColor(color4, cr[3], cg[3], cb[3]);
	if (x2 < x1) SWAP(x1,x2);
	if (y2 < y1) SWAP(y1,y2);
  int w = (x2 - x1) + 1;
  int h = (y2 - y1) + 1;
  if ((w == 0) || (h == 0)) return;
  unsigned wDiv = w-1;
  unsigned hDiv = h-1;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      // calculate x and y weights
      unsigned aX = x * 255 / wDiv;
      unsigned aY = y * 255 / hDiv;
      // calculate per-color weights
      unsigned a[4]; // tl tr bl br
      a[0] = (aX^0xFF) * (aY^0xFF) / 255;
      a[1] = aX * (aY^0xFF) / 255;
      a[2] = (aX^0xFF) * aY / 255;
      a[3] = 255 - (a[0] + a[1] + a[2]);
      // blend
      unsigned r = ((cr[0] * a[0]) + (cr[1] * a[1]) + (cr[2] * a[2]) + (cr[3] * a[3])) / 255;
      unsigned g = ((cg[0] * a[0]) + (cg[1] * a[1]) + (cg[2] * a[2]) + (cg[3] * a[3])) / 255;
      unsigned b = ((cb[0] * a[0]) + (cb[1] * a[1]) + (cb[2] * a[2]) + (cb[3] * a[3])) / 255;
      PutPixel(x+x1, y+y1, MakeColor(r, g, b), dest);
    }
  }
}

int Trender32::MakeColor(int r, int g, int b)
{
	return ((r<<16)|(g<<8)|b);
}


bool Trender32::GetColor(int c, int &r, int &g, int &b)
{
//	if (c == transColor) return false;
	b = c & 0xff;
	g = (c >> 8) & 0xff;
	r = (c >> 16) & 0xff;
    return true;
}



void Trender32::Clear(int color, image *dest)
{
	int *d = (int *)dest->data;
	int bytes = dest->pitch * dest->height;
	while (bytes--)
		*d++ = color;
}


int Trender32::ReadPixel(int x, int y, image *source)
{
	quad *ptr = (quad*)source->data;
	return ptr[(y * source->pitch) + x];
}


void Trender32::PutPixel(int x, int y, int color, image *dest)
{
	int *ptr = (int *)dest->data;
	if (x<dest->cx1 || x>dest->cx2 || y<dest->cy1 || y>dest->cy2)
		return;
	ptr[(y * dest->pitch) + x] = color;
}


void Trender32::HLine(int x, int y, int xe, int color, image *dest)
{
	int *d = (int *) dest->data;
	int cx1=0, cy1=0, cx2=0, cy2=0;
	if (xe<x) SWAP(x,xe);
	dest->GetClip(cx1, cy1, cx2, cy2);
	if (x>cx2 || y>cy2 || xe<cx1 || y<cy1)
		return;
	if (xe>cx2) xe=cx2;
	if (x<cx1)  x =cx1;

	d += (y * dest->pitch) + x;
	for (; x<=xe; x++)
		*d++ = color;
}

void Trender32::VLine(int x, int y, int ye, int color, image *dest)
{
	int *d = (int *) dest->data;
	int cx1=0, cy1=0, cx2=0, cy2=0;
	if (ye<y) SWAP(y,ye);
	dest->GetClip(cx1, cy1, cx2, cy2);
	if (x>cx2 || y>cy2 || x<cx1 || ye<cy1)
		return;
	if (ye>cy2) ye=cy2;
	if (y<cy1)  y =cy1;

	d += (y * dest->pitch) + x;
	for (; y<=ye; y++, d+=dest->pitch)
		*d = color;
}

void Trender32::Blit(int x, int y, image *src, image *dest)
{
	int *s=(int *)src->data,
		*d=(int *)dest->data;
	int spitch=src->pitch,
		dpitch=dest->pitch;
	int xlen=src->width,
		ylen=src->height;
	int cx1=0, cy1=0,
		cx2=0, cy2=0;

	dest->GetClip(cx1, cy1, cx2, cy2);
	if (x>cx2 || y>cy2 || x+xlen<cx1 || y+ylen<cy1)
		return;

	if (x+xlen>cx2) xlen = cx2-x+1;
	if (y+ylen>cy2) ylen = cy2-y+1;
	if (x<cx1) 	{
		s +=(cx1-x);
		xlen-=(cx1-x);
		x  =cx1;
	}
	if (y<cy1) {
		s +=(cy1-y)*spitch;
		ylen-=(cy1-y);
		y  =cy1;
	}

	d += (y * dpitch) + x;
	for (xlen *= 4; ylen--; s+=spitch, d+=dpitch)
		memcpy(d, s, xlen);
}

void Trender32::TBlit(int x, int y, image *src, image *dest)
{
	int *s=(int *)src->data,c,
		*d=(int *)dest->data;
	int spitch=src->pitch,
		dpitch=dest->pitch;
	int xlen=src->width,
		ylen=src->height;
	int cx1=0, cy1=0,
		cx2=0, cy2=0;

	dest->GetClip(cx1, cy1, cx2, cy2);
	if (x>cx2 || y>cy2 || x+xlen<cx1 || y+ylen<cy1)
		return;

	if (x+xlen > cx2) xlen=cx2-x+1;
	if (y+ylen > cy2) ylen=cy2-y+1;
	if (x<cx1) {
		s +=(cx1-x);
		xlen-=(cx1-x);
		x  =cx1;
	}
	if (y<cy1) {
		s +=(cy1-y)*spitch;
		ylen-=(cy1-y);
		y  =cy1;
	}
	d+=y*dpitch+x;
	for (; ylen; ylen--)
	{
		for (x=0; x<xlen; x++)
		{
			c=s[x];
			if (c != transColor) d[x]=c;
		}
		s+=spitch;
		d+=dpitch;
	}
}


/********************** 16bpp blitter code **********************/

int Trender16::MakeColor(int r, int g, int b)
{
	return (((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}

bool Trender16::GetColor(int c, int &r, int &g, int &b)
{
//	if (c == transColor) return false;
	b = (c & 0x1F) << 3;
	g = ((c >> 5) & 0x3f) << 2;
	r = ((c >> 11) & 0x1f) << 3;
    return true;
}


void Trender16::Clear(int color, image *dest)
{
	word *d = (word *)dest->data;
	int bytes = dest->pitch * dest->height;
	while (bytes--)
		*d++ = color;
}


int Trender16::ReadPixel(int x, int y, image *source)
{
	word *ptr = (word*)source->data;
	return ptr[(y * source->pitch) + x];
}


void Trender16::PutPixel(int x, int y, int color, image *dest)
{
	word *ptr = (word *)dest->data;
	if (x<dest->cx1 || x>dest->cx2 || y<dest->cy1 || y>dest->cy2)
		return;
	ptr[(y * dest->pitch) + x] = color;
}

void Trender16::VLine(int x, int y, int ye, int color, image *dest)
{
	word *d = (word *) dest->data;
	int cx1=0, cy1=0, cx2=0, cy2=0;
	if (ye<y) SWAP(y,ye);
	dest->GetClip(cx1, cy1, cx2, cy2);
	if (x>cx2 || y>cy2 || x<cx1 || ye<cy1)
		return;
	if (ye>cy2) ye=cy2;
	if (y<cy1)  y =cy1;

	d += (y * dest->pitch) + x;
	for (; y<=ye; y++, d+=dest->pitch)
		*d = color;
}

void Trender16::HLine(int x, int y, int xe, int color, image *dest)
{
	word *d = (word *) dest->data;
	int cx1=0, cy1=0, cx2=0, cy2=0;
	if (xe<x) SWAP(x,xe);
	dest->GetClip(cx1, cy1, cx2, cy2);
	if (x>cx2 || y>cy2 || xe<cx1 || y<cy1)
		return;
	if (xe>cx2) xe=cx2;
	if (x<cx1)  x =cx1;

	d += (y * dest->pitch) + x;
	for (; x<=xe; x++)
		*d++ = color;
}

void Trender16::Blit(int x, int y, image *src, image *dest)
{
	word *s=(word *)src->data,
		 *d=(word *)dest->data;
	int spitch=src->pitch,
		dpitch=dest->pitch;
	int xlen=src->width,
		ylen=src->height;
	int cx1=0, cy1=0,
		cx2=0, cy2=0;

	dest->GetClip(cx1, cy1, cx2, cy2);
	if (x>cx2 || y>cy2 || x+xlen<cx1 || y+ylen<cy1)
		return;

	if (x+xlen>cx2) xlen = cx2-x+1;
	if (y+ylen>cy2) ylen = cy2-y+1;
	if (x<cx1) 	{
		s +=(cx1-x);
		xlen-=(cx1-x);
		x  =cx1;
	}
	if (y<cy1) {
		s +=(cy1-y)*spitch;
		ylen-=(cy1-y);
		y  =cy1;
	}

	d += (y * dpitch) + x;
	for (xlen *= 2; ylen--; s+=spitch, d+=dpitch)
		memcpy(d, s, xlen);
}

void Trender16::TBlit(int x, int y, image *src, image *dest)
{
	word *s=(word *)src->data,c,
		 *d=(word *)dest->data;
	int spitch=src->pitch,
		dpitch=dest->pitch;
	int xlen=src->width,
		ylen=src->height;
	int cx1=0, cy1=0,
		cx2=0, cy2=0;

	dest->GetClip(cx1, cy1, cx2, cy2);
	if (x>cx2 || y>cy2 || x+xlen<cx1 || y+ylen<cy1)
		return;

	if (x+xlen > cx2) xlen=cx2-x+1;
	if (y+ylen > cy2) ylen=cy2-y+1;
	if (x<cx1) {
		s +=(cx1-x);
		xlen-=(cx1-x);
		x  =cx1;
	}
	if (y<cy1) {
		s +=(cy1-y)*spitch;
		ylen-=(cy1-y);
		y  =cy1;
	}
	d+=y*dpitch+x;
	for (; ylen; ylen--)
	{
		for (x=0; x<xlen; x++)
		{
			c=s[x];
			if (c != transColor) d[x]=c;
		}
		s+=spitch;
		d+=dpitch;
	}
}



/********************** 15bpp blitter code **********************/

int Trender15::MakeColor(int r, int g, int b)
{
	return (((r>>3)<<10)|((g>>3)<<5)|(b>>3))|0x8000; //0x8000 for opaque
}


bool Trender15::GetColor(int c, int &r, int &g, int &b)
{
//	if (c == transColor) return false;
	b = (c & 0x1F) << 3;
	g = ((c >> 5) & 0x1f) << 3;
	r = ((c >> 10) & 0x1f) << 3;
    return true;
}

/********************** 51bpp (15bpp with high bit blue) blitter code **********************/

int Trender51::MakeColor(int r, int g, int b)
{
	return (((b>>3)<<10)|((g>>3)<<5)|(r>>3))|0x8000; //0x8000 for opaque
}


bool Trender51::GetColor(int c, int &r, int &g, int &b)
{
//	if (c == transColor) return false;
	r = (c & 0x1F) << 3;
	g = ((c >> 5) & 0x1f) << 3;
	b = ((c >> 10) & 0x1f) << 3;
    return true;
}


} //end namespace
