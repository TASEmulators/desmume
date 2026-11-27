/**     Code adapted from Exult source code by Forgotten
 **	Scale.cc - Trying to scale with bilinear interpolation.
 **
 **	Written: 6/14/00 - JSF
 **/

#include "filter.h"
#include "types.h"
#include "filter_simd.h"

int systemRedShift    = 16;
int systemGreenShift  = 8;
int systemBlueShift   = 0;
/*
#define RGB1(r,g,b) ((r)>>3) << systemRedShift |\
  ((g) >> 3) << systemGreenShift |\
  ((b) >> 3) << systemBlueShift\
*/
#define RGB1(r,g,b) (((r))<<systemRedShift) | (((g)) << systemGreenShift) | (((b)) << systemBlueShift)

//kind of a waste of memory, it'd be nice if we could share this with other filters
//then again, we may want to multi-thread them later, so.. we should allocate filter objects which carry their own buffers
//the largest conceivable prescale... 32x!
//
// These scratch rows must be per-thread: VideoFilter runs the same filter entry
// point concurrently on one horizontal slice per worker thread, so a shared
// buffer made the threads overwrite each other's rows (about 60% of the output
// was wrong with four threads).
static thread_local u8 row_cur[3*(256+8)*32];
static thread_local u8 row_next[3*(256+8)*32];

static void fill_rgb_row_16(u16 *from, int src_width, u8 *row, int width)
{
  u8 *copy_start = row + src_width*3;
  u8 *all_stop = row + width*3;
  while (row < copy_start) {
    u16 color = *from++;
    *row++ = ((color >> systemRedShift) & 0x1f) << 3;
    *row++ = ((color >> systemGreenShift) & 0x1f) << 3;
    *row++ = ((color >> systemBlueShift) & 0x1f) << 3;
  }
  // any remaining elements to be written to 'row' are a replica of the
  // preceding pixel
  u8 *p = row-3;
  while (row < all_stop) {
    // we're guaranteed three elements per pixel; could unroll the loop
    // further, especially with a Duff's Device, but the gains would be
    // probably limited (judging by profiler output)
    *row++ = *p++;
    *row++ = *p++;
    *row++ = *p++;
  }
}

static void fill_rgb_row_32(u32 *from, int src_width, u8 *row, int width)
{
  u8 *copy_start = row + src_width*3;
  u8 *all_stop = row + width*3;
  while (row < copy_start) {
    u32 color = *from++;
    *row++ = ((color >> (systemRedShift)) ) ;
    *row++ = ((color >> (systemGreenShift)) ) ;
    *row++ = ((color >> (systemBlueShift)) ) ;
  }
  // any remaining elements to be written to 'row' are a replica of the
  // preceding pixel
  u8 *p = row-3;
  while (row < all_stop) {
    // we're guaranteed three elements per pixel; could unroll the loop
    // further, especially with a Duff's Device, but the gains would be
    // probably limited (judging by profiler output)
    *row++ = *p++;
    *row++ = *p++;
    *row++ = *p++;
  }
}

void Bilinear(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
              u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u8 *rgb_row_cur = row_cur;
  u8 *rgb_row_next = row_next;

  u16 *to = (u16 *)dstPtr;
  u16 *to_odd = (u16 *)(dstPtr + dstPitch);

  int from_width = width;
  u16 *from = (u16 *)srcPtr;
  fill_rgb_row_16(from, from_width, rgb_row_cur, width+1);

  for(int y = 0; y < height; y++) {
    u16 *from_orig = from;
    u16 *to_orig = to;

    if (y+1 < height)
      fill_rgb_row_16(from+width, from_width, rgb_row_next,
                   width+1);
    else
      fill_rgb_row_16(from, from_width, rgb_row_next, width+1);

    // every pixel in the src region, is extended to 4 pixels in the
    // destination, arranged in a square 'quad'; if the current src
    // pixel is 'a', then in what follows 'b' is the src pixel to the
    // right, 'c' is the src pixel below, and 'd' is the src pixel to
    // the right and down
    u8 *cur_row  = rgb_row_cur;
    u8 *next_row = rgb_row_next;
    u8 *ar = cur_row++;
    u8 *ag = cur_row++;
    u8 *ab = cur_row++;
    u8 *cr = next_row++;
    u8 *cg = next_row++;
    u8 *cb = next_row++;
    for(int x=0; x < width; x++) {
      u8 *br = cur_row++;
      u8 *bg = cur_row++;
      u8 *bb = cur_row++;
      u8 *dr = next_row++;
      u8 *dg = next_row++;
      u8 *db = next_row++;

      // upper left pixel in quad: just copy it in
      *to++ = RGB1(*ar, *ag, *ab);

      // upper right
      *to++ = RGB1((*ar+*br)>>1, (*ag+*bg)>>1, (*ab+*bb)>>1);

      // lower left
      *to_odd++ = RGB1((*ar+*cr)>>1, (*ag+*cg)>>1, (*ab+*cb)>>1);

      // lower right
      *to_odd++ = RGB1((*ar+*br+*cr+*dr)>>2,
                      (*ag+*bg+*cg+*dg)>>2,
                      (*ab+*bb+*cb+*db)>>2);

      // 'b' becomes 'a', 'd' becomes 'c'
      ar = br;
      ag = bg;
      ab = bb;
      cr = dr;
      cg = dg;
      cb = db;
    }

    // the "next" rgb row becomes the current; the old current rgb row is
    // recycled and serves as the new "next" row
    u8 *temp;
    temp = rgb_row_cur;
    rgb_row_cur = rgb_row_next;
    rgb_row_next = temp;

    // update the pointers for start of next pair of lines
    from = (u16 *)((u8 *)from_orig + srcPitch);
    to = (u16 *)((u8 *)to_orig + (dstPitch << 1));
    to_odd = (u16 *)((u8 *)to + dstPitch);
  }
}

void BilinearPlus(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                  u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u8 *rgb_row_cur = row_cur;
  u8 *rgb_row_next = row_next;

  u16 *to = (u16 *)dstPtr;
  u16 *to_odd = (u16 *)(dstPtr + dstPitch);

  int from_width = width;
  u16 *from = (u16 *)srcPtr;
  fill_rgb_row_16(from, from_width, rgb_row_cur, width+1);

  for(int y = 0; y < height; y++) {
    u16 *from_orig = from;
    u16 *to_orig = to;

    if (y+1 < height)
      fill_rgb_row_16(from+width, from_width, rgb_row_next,
                   width+1);
    else
      fill_rgb_row_16(from, from_width, rgb_row_next, width+1);

    // every pixel in the src region, is extended to 4 pixels in the
    // destination, arranged in a square 'quad'; if the current src
    // pixel is 'a', then in what follows 'b' is the src pixel to the
    // right, 'c' is the src pixel below, and 'd' is the src pixel to
    // the right and down
    u8 *cur_row  = rgb_row_cur;
    u8 *next_row = rgb_row_next;
    u8 *ar = cur_row++;
    u8 *ag = cur_row++;
    u8 *ab = cur_row++;
    u8 *cr = next_row++;
    u8 *cg = next_row++;
    u8 *cb = next_row++;
    for(int x=0; x < width; x++) {
      u8 *br = cur_row++;
      u8 *bg = cur_row++;
      u8 *bb = cur_row++;
      u8 *dr = next_row++;
      u8 *dg = next_row++;
      u8 *db = next_row++;

      // upper left pixel in quad: just copy it in
      //*to++ = manip.rgb(*ar, *ag, *ab);
#ifdef USE_ORIGINAL_BILINEAR_PLUS
      *to++ = RGB(
                  (((*ar)<<2) +((*ar)) + (*cr+*br+*br) )>> 3,
                  (((*ag)<<2) +((*ag)) + (*cg+*bg+*bg) )>> 3,
                  (((*ab)<<2) +((*ab)) + (*cb+*bb+*bb) )>> 3);
#else
      *to++ = RGB1(
                  (((*ar)<<3) +((*ar)<<1) + (*cr+*br+*br+*cr) )>> 4,
                  (((*ag)<<3) +((*ag)<<1) + (*cg+*bg+*bg+*cg) )>> 4,
                  (((*ab)<<3) +((*ab)<<1) + (*cb+*bb+*bb+*cb) )>> 4);
#endif

      // upper right
      *to++ = RGB1((*ar+*br)>>1, (*ag+*bg)>>1, (*ab+*bb)>>1);

      // lower left
      *to_odd++ = RGB1((*ar+*cr)>>1, (*ag+*cg)>>1, (*ab+*cb)>>1);

      // lower right
      *to_odd++ = RGB1((*ar+*br+*cr+*dr)>>2,
                      (*ag+*bg+*cg+*dg)>>2,
                      (*ab+*bb+*cb+*db)>>2);

      // 'b' becomes 'a', 'd' becomes 'c'
      ar = br;
      ag = bg;
      ab = bb;
      cr = dr;
      cg = dg;
      cb = db;
    }

    // the "next" rgb row becomes the current; the old current rgb row is
    // recycled and serves as the new "next" row
    u8 *temp;
    temp = rgb_row_cur;
    rgb_row_cur = rgb_row_next;
    rgb_row_next = temp;

    // update the pointers for start of next pair of lines
    from = (u16 *)((u8 *)from_orig + srcPitch);
    to = (u16 *)((u8 *)to_orig + (dstPitch << 1));
    to_odd = (u16 *)((u8 *)to + dstPitch);
  }
}

void Bilinear32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                u8 *dstPtr, u32 dstPitch, int width, int height)
{
#ifdef DESMUME_FILTER_SIMD_ENABLED
  if (width <= 0 || height <= 0)
    return;

  const filter_simd::Vec4u32 rgbMask = filter_simd::set1(0x00FFFFFF);
  for (int y = 0; y < height; y++) {
    const u32 *src0 = reinterpret_cast<const u32 *>(srcPtr + y * srcPitch);
    const u32 *src1 = (y + 1 < height) ?
        reinterpret_cast<const u32 *>(srcPtr + (y + 1) * srcPitch) : src0;
    u32 *dst0 = reinterpret_cast<u32 *>(dstPtr + (y * 2) * dstPitch);
    u32 *dst1 = reinterpret_cast<u32 *>(dstPtr + (y * 2 + 1) * dstPitch);

    int x = 0;
    for (; width - x >= 5; x += 4) {
      const filter_simd::Vec4u32 a =
          filter_simd::bitAnd(filter_simd::load(src0 + x), rgbMask);
      const filter_simd::Vec4u32 b =
          filter_simd::bitAnd(filter_simd::load(src0 + x + 1), rgbMask);
      const filter_simd::Vec4u32 c =
          filter_simd::bitAnd(filter_simd::load(src1 + x), rgbMask);
      const filter_simd::Vec4u32 d =
          filter_simd::bitAnd(filter_simd::load(src1 + x + 1), rgbMask);
      const filter_simd::Vec4u32 ab = filter_simd::average2RgbFloor(a, b);
      const filter_simd::Vec4u32 ac = filter_simd::average2RgbFloor(a, c);
      const filter_simd::Vec4u32 abcd = filter_simd::average4RgbFloor(a, b, c, d);

      filter_simd::store(dst0 + x * 2, filter_simd::interleaveLow(a, ab));
      filter_simd::store(dst0 + x * 2 + 4, filter_simd::interleaveHigh(a, ab));
      filter_simd::store(dst1 + x * 2, filter_simd::interleaveLow(ac, abcd));
      filter_simd::store(dst1 + x * 2 + 4, filter_simd::interleaveHigh(ac, abcd));
    }

    for (; x < width; x++) {
      const u32 a = src0[x];
      const u32 b = (x + 1 < width) ? src0[x + 1] : a;
      const u32 c = src1[x];
      const u32 d = (x + 1 < width) ? src1[x + 1] : c;
      const u32 ar = (a >> systemRedShift) & 0xFF;
      const u32 ag = (a >> systemGreenShift) & 0xFF;
      const u32 ab = (a >> systemBlueShift) & 0xFF;
      const u32 br = (b >> systemRedShift) & 0xFF;
      const u32 bg = (b >> systemGreenShift) & 0xFF;
      const u32 bb = (b >> systemBlueShift) & 0xFF;
      const u32 cr = (c >> systemRedShift) & 0xFF;
      const u32 cg = (c >> systemGreenShift) & 0xFF;
      const u32 cb = (c >> systemBlueShift) & 0xFF;
      const u32 dr = (d >> systemRedShift) & 0xFF;
      const u32 dg = (d >> systemGreenShift) & 0xFF;
      const u32 db = (d >> systemBlueShift) & 0xFF;

      dst0[x * 2] = RGB1(ar, ag, ab);
      dst0[x * 2 + 1] = RGB1((ar + br) >> 1, (ag + bg) >> 1, (ab + bb) >> 1);
      dst1[x * 2] = RGB1((ar + cr) >> 1, (ag + cg) >> 1, (ab + cb) >> 1);
      dst1[x * 2 + 1] = RGB1((ar + br + cr + dr) >> 2,
                              (ag + bg + cg + dg) >> 2,
                              (ab + bb + cb + db) >> 2);
    }
  }
#else
  u8 *rgb_row_cur = row_cur;
  u8 *rgb_row_next = row_next;

  u32 *to = (u32 *)dstPtr;
  u32 *to_odd = (u32 *)(dstPtr + dstPitch);

  int from_width = width;

  u32 *from = (u32 *)srcPtr;
  fill_rgb_row_32(from, from_width, rgb_row_cur, width+1);

  for(int y = 0; y < height; y++) {
    u32 *from_orig = from;
    u32 *to_orig = to;

    // The next source row starts one pitch away, not width+1 pixels away:
    // sampling at "from + width + 1" shifted the whole vertical blend one
    // pixel to the right and read past the end of the last row.
    u32 *from_next = (u32 *)((u8 *)from + srcPitch);

    if (y+1 < height)
      fill_rgb_row_32(from_next, from_width, rgb_row_next,
                   width+1);
    else
      fill_rgb_row_32(from, from_width, rgb_row_next, width+1);

    // every pixel in the src region, is extended to 4 pixels in the
    // destination, arranged in a square 'quad'; if the current src
    // pixel is 'a', then in what follows 'b' is the src pixel to the
    // right, 'c' is the src pixel below, and 'd' is the src pixel to
    // the right and down
    u8 *cur_row  = rgb_row_cur;
    u8 *next_row = rgb_row_next;
    u8 *ar = cur_row++;
    u8 *ag = cur_row++;
    u8 *ab = cur_row++;
    u8 *cr = next_row++;
    u8 *cg = next_row++;
    u8 *cb = next_row++;
    for(int x=0; x < width; x++) {
      u8 *br = cur_row++;
      u8 *bg = cur_row++;
      u8 *bb = cur_row++;
      u8 *dr = next_row++;
      u8 *dg = next_row++;
      u8 *db = next_row++;

      // upper left pixel in quad: just copy it in
      *to++ = RGB1(*ar, *ag, *ab);

      // upper right
      *to++ = RGB1((*ar+*br)>>1, (*ag+*bg)>>1, (*ab+*bb)>>1);

      // lower left
      *to_odd++ = RGB1((*ar+*cr)>>1, (*ag+*cg)>>1, (*ab+*cb)>>1);

      // lower right
      *to_odd++ = RGB1((*ar+*br+*cr+*dr)>>2,
                      (*ag+*bg+*cg+*dg)>>2,
                      (*ab+*bb+*cb+*db)>>2);

      // 'b' becomes 'a', 'd' becomes 'c'
      ar = br;
      ag = bg;
      ab = bb;
      cr = dr;
      cg = dg;
      cb = db;
    }

    // the "next" rgb row becomes the current; the old current rgb row is
    // recycled and serves as the new "next" row
    u8 *temp;
    temp = rgb_row_cur;
    rgb_row_cur = rgb_row_next;
    rgb_row_next = temp;

    // update the pointers for start of next pair of lines
    from = (u32 *)((u8 *)from_orig + srcPitch);
    to = (u32 *)((u8 *)to_orig + (dstPitch << 1));
    to_odd = (u32 *)((u8 *)to + dstPitch);
  }
#endif
}

void BilinearPlus32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
                    u8 *dstPtr, u32 dstPitch, int width, int height)
{
  u8 *rgb_row_cur = row_cur;
  u8 *rgb_row_next = row_next;

  u32 *to = (u32 *)dstPtr;
  u32 *to_odd = (u32 *)(dstPtr + dstPitch);

  int from_width = width;

  u32 *from = (u32 *)srcPtr;
  fill_rgb_row_32(from, from_width, rgb_row_cur, width+1);

  for(int y = 0; y < height; y++) {
    u32 *from_orig = from;
    u32 *to_orig = to;

    // The next source row starts one pitch away, not width+1 pixels away:
    // sampling at "from + width + 1" shifted the whole vertical blend one
    // pixel to the right and read past the end of the last row.
    u32 *from_next = (u32 *)((u8 *)from + srcPitch);

    if (y+1 < height)
      fill_rgb_row_32(from_next, from_width, rgb_row_next,
                   width+1);
    else
      fill_rgb_row_32(from, from_width, rgb_row_next, width+1);

    // every pixel in the src region, is extended to 4 pixels in the
    // destination, arranged in a square 'quad'; if the current src
    // pixel is 'a', then in what follows 'b' is the src pixel to the
    // right, 'c' is the src pixel below, and 'd' is the src pixel to
    // the right and down
    u8 *cur_row  = rgb_row_cur;
    u8 *next_row = rgb_row_next;
    u8 *ar = cur_row++;
    u8 *ag = cur_row++;
    u8 *ab = cur_row++;
    u8 *cr = next_row++;
    u8 *cg = next_row++;
    u8 *cb = next_row++;
    for(int x=0; x < width; x++) {
      u8 *br = cur_row++;
      u8 *bg = cur_row++;
      u8 *bb = cur_row++;
      u8 *dr = next_row++;
      u8 *dg = next_row++;
      u8 *db = next_row++;

      // upper left pixel in quad: just copy it in
      //*to++ = manip.rgb(*ar, *ag, *ab);
#ifdef USE_ORIGINAL_BILINEAR_PLUS
      *to++ = RGB(
                  (((*ar)<<2) +((*ar)) + (*cr+*br+*br) )>> 3,
                  (((*ag)<<2) +((*ag)) + (*cg+*bg+*bg) )>> 3,
                  (((*ab)<<2) +((*ab)) + (*cb+*bb+*bb) )>> 3);
#else
      *to++ = RGB1(
                  (((*ar)<<3) +((*ar)<<1) + (*cr+*br+*br+*cr) )>> 4,
                  (((*ag)<<3) +((*ag)<<1) + (*cg+*bg+*bg+*cg) )>> 4,
                  (((*ab)<<3) +((*ab)<<1) + (*cb+*bb+*bb+*cb) )>> 4);
#endif

      // upper right
      *to++ = RGB1((*ar+*br)>>1, (*ag+*bg)>>1, (*ab+*bb)>>1);

      // lower left
      *to_odd++ = RGB1((*ar+*cr)>>1, (*ag+*cg)>>1, (*ab+*cb)>>1);

      // lower right
      *to_odd++ = RGB1((*ar+*br+*cr+*dr)>>2,
                      (*ag+*bg+*cg+*dg)>>2,
                      (*ab+*bb+*cb+*db)>>2);

      // 'b' becomes 'a', 'd' becomes 'c'
      ar = br;
      ag = bg;
      ab = bb;
      cr = dr;
      cg = dg;
      cb = db;
    }

    // the "next" rgb row becomes the current; the old current rgb row is
    // recycled and serves as the new "next" row
    u8 *temp;
    temp = rgb_row_cur;
    rgb_row_cur = rgb_row_next;
    rgb_row_next = temp;

    // update the pointers for start of next pair of lines
    from = (u32 *)((u8 *)from_orig + srcPitch);
    to = (u32 *)((u8 *)to_orig + (dstPitch << 1));
    to_odd = (u32 *)((u8 *)to + dstPitch);
  }
}
void RenderBilinear (SSurface Src, SSurface Dst)
{

    unsigned char *lpSrc, *lpDst;

    lpSrc = Src.Surface;
    lpDst = Dst.Surface;

    Bilinear32 (lpSrc, Src.Pitch*2,
                lpSrc,
                lpDst, Dst.Pitch*2, Src.Width, Src.Height);
}

