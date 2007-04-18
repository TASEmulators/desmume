/* $Id: opengl_collector_3Demu.c,v 1.4 2007-04-18 13:37:33 masscat Exp $
 */
/*  
	Copyright (C) 2006-2007 Ben Jaques, shash

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * This is a 3D emulation plugin. It uses OpenGL to perform the rendering.
 * There is no platform specific code. Platform specific code is executed
 * via a set of helper functions that must be defined should a particular
 * platform use this plugin.
 *
 * The NDS 3D commands are collected until the flush command is issued. At this
 * point the OpenGL function calls that correspnd to the set of commands are called.
 * This approach is taken to allowing simple OpenGL context switching should
 * OpenGL also be being used for other purposes (for example rendering the screen).
 */

/*
 * FIXME: This is a Work In Progress
 * - The NDS command set should be checked to to ensure that it corresponds to a
 *   valid OpenGL command sequence.
 * - Two sets of matrices should be maintained (maybe). One for rendering and one
 *   for NDS test commands. Any matrix commands should be executed immediately on
 *   the NDS test matix set (as well as stored).
 * - Most of the OpenGL/emulation stuff has been copied from shash's Windows
 *   3D code. There maybe optimisations and/or problems arising from the change
 *   of approach or the cut and paste.
 * - More of the 3D needs to be emulated (correctly or at all) :).
 */

#ifdef HAVE_GL_GL_H
#ifdef HAVE_GL_GLU_H

#include <stdio.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include "types.h"

#include "render3D.h"
#include "matrix.h"
#include "MMU.h"
#include "bits.h"

#include "opengl_collector_3Demu.h"

#if 0
#define LOG( fmt, ...) fprintf( stdout, "OpenGL Collector: "); \
fprintf( stdout, fmt, ##__VA_ARGS__)
#else
#define LOG( fmt, ...)
#endif

#if 0
#define LOG_CALL_LIST( fmt, ...) fprintf( stdout, "OpenGL Collector: Call list: "); \
fprintf( stdout, fmt, ##__VA_ARGS__)
#else
#define LOG_CALL_LIST( fmt, ...)
#endif

#if 0
#define LOG_ERROR( fmt, ...) fprintf( stderr, "OpenGL Collector error: "); \
fprintf( stderr, fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR( fmt, ...)
#endif

#define LOG_MATRIX( matrix) \
LOG( "%f, %f, %f, %f\n", matrix[0], matrix[1], matrix[2], matrix[3]); \
LOG( "%f, %f, %f, %f\n", matrix[4], matrix[5], matrix[6], matrix[7]); \
LOG( "%f, %f, %f, %f\n", matrix[8], matrix[9], matrix[10], matrix[11]); \
LOG( "%f, %f, %f, %f\n", matrix[12], matrix[13], matrix[14], matrix[15])

#define USE_BGR_ORDER 1

static int
not_set( void) {
  LOG_ERROR( "platform code not setup\n");
  return 0;
}

static void
nothingness( void) {
}

int (*begin_opengl_ogl_collector_platform)( void) = not_set;
void (*end_opengl_ogl_collector_platform)( void) = nothingness;
int (*initialise_ogl_collector_platform)( void) = not_set;


#define fix2float(v)    (((float)((s32)(v))) / (float)(1<<12))
#define fix10_2float(v) (((float)((s32)(v))) / (float)(1<<9))



static u8  GPU_screen3D[256*256*4]={0};

/*
 * The matrices
 */
static int current_matrix_mode = 0;
static MatrixStack	mtxStack[4];
static float		mtxCurrent [4][16];
static float		mtxTemporal[16];

static u32 disp_3D_control = 0;

static u32 textureFormat=0, texturePalette=0;
static u32 lastTextureFormat=0, lastTexturePalette=0;
static unsigned int oglTextureID=0;
static u8 texMAP[1024*2048*4], texMAP2[2048*2048*4];
static float invTexWidth  = 1.f;
static float invTexHeight = 1.f;
static int texCoordinateTransform = 0;
static int t_texture_coord = 0, s_texture_coord = 0;


enum command_type {
  NOP_CMD = 0x00,
  MTX_MODE_CMD = 0x10,
  MTX_PUSH_CMD = 0x11,
  MTX_POP_CMD = 0x12,
  MTX_STORE_CMD = 0x13,
  MTX_RESTORE_CMD = 0x14,
  MTX_IDENTITY_CMD = 0x15,
  MTX_LOAD_4x4_CMD = 0x16,
  MTX_LOAD_4x3_CMD = 0x17,
  MTX_MULT_4x4_CMD = 0x18,
  MTX_MULT_4x3_CMD = 0x19,
  MTX_MULT_3x3_CMD = 0x1a,
  MTX_SCALE_CMD = 0x1b,
  MTX_TRANS_CMD = 0x1c,
  COLOR_CMD = 0x20,
  NORMAL_CMD = 0x21,
  TEXCOORD_CMD = 0x22,
  VTX_16_CMD = 0x23,
  VTX_10_CMD = 0x24,
  VTX_XY_CMD = 0x25,
  VTX_XZ_CMD = 0x26,
  VTX_YZ_CMD = 0x27,
  VTX_DIFF_CMD = 0x28,
  POLYGON_ATTR_CMD = 0x29,
  TEXIMAGE_PARAM_CMD = 0x2a,
  PLTT_BASE_CMD = 0x2b,
  DIF_AMB_CMD = 0x30,
  SPE_EMI_CMD = 0x31,
  LIGHT_VECTOR_CMD = 0x32,
  LIGHT_COLOR_CMD = 0x33,
  SHININESS_CMD = 0x34,
  BEGIN_VTXS_CMD = 0x40,
  END_VTXS_CMD = 0x41,
  SWAP_BUFFERS_CMD = 0x50,
  VIEWPORT_CMD = 0x60,
  BOX_TEST_CMD = 0x70,
  POS_TEST_CMD = 0x71,
  VEC_TEST_CMD = 0x72,

  /* The next ones are not NDS commands */
  CLEAR_COLOUR_CMD = 0x80,
  CLEAR_DEPTH_CMD = 0x81,
  FOG_COLOUR_CMD = 0x82,
  FOG_OFFSET_CMD = 0x83,
  CONTROL_CMD = 0x84,
  ALPHA_FUNCTION_CMD = 0x85
};

#define LAST_CMD_VALUE 0x84

#define ADD_RENDER_PARM_CMD( cmd) render_states[current_render_state].cmds[render_states[current_render_state].write_index++] = cmd

static const char *primitive_type_names[] = {
  "Triangles",
  "Quads",
  "Tri strip",
  "Quad strip"
};

// Accelerationg tables
static float float16table[65536];
static float float10Table[1024];
static float float10RelTable[1024];
static float normalTable[1024];

#define NUM_RENDER_STATES 2
int current_render_state;
static struct render_state {
  int write_index;

  /* FIXME: how big to make this? */
  u32 cmds[100*1024];

  //int cmds_drawn;
} render_states[NUM_RENDER_STATES];

#define GET_DRAW_STATE_INDEX( current_index) (((current_index) - 1) & (NUM_RENDER_STATES - 1))
#define GET_NEXT_RENDER_STATE_INDEX( current_index) (((current_index) + 1) & (NUM_RENDER_STATES - 1))


struct cmd_processor {
  u32 num_parms;
  void (*processor_fn)( struct render_state *state,
                        const u32 *parms);
};
/*static int (*cmd_processors[LAST_CMD_VALUE+1])( struct render_state *state,
                                                const u32 *parms);
*/
static struct cmd_processor cmd_processors[LAST_CMD_VALUE+1];







static void
set_gl_matrix_mode( int mode) {
  switch ( mode & 0x3) {
  case 0:
    glMatrixMode( GL_PROJECTION);
    break;

  case 1:
    glMatrixMode( GL_MODELVIEW);
    break;

  case 2:
    /* FIXME: more here? */
    glMatrixMode( GL_MODELVIEW);
    break;

  case 3:
    //glMatrixMode( GL_TEXTURE);
    break;
  }
}

static void
SetupTexture (unsigned int format, unsigned int palette) {
  if(format == 0) // || disableTexturing)
    {
      LOG("Texture format is zero\n");
      return;
    }
  else
    {
      unsigned short *pal = NULL;
      unsigned int sizeX = (1<<(((format>>20)&0x7)+3));
      unsigned int sizeY = (1<<(((format>>23)&0x7)+3));
      unsigned int mode = (unsigned short)((format>>26)&0x7);
      unsigned char * adr = (unsigned char *)(ARM9Mem.ARM9_LCD + ((format&0xFFFF)<<3));
      unsigned short param = (unsigned short)((format>>30)&0xF);
      unsigned short param2 = (unsigned short)((format>>16)&0xF);
      unsigned int imageSize = sizeX*sizeY;
      unsigned int paletteSize = 0;
      unsigned int palZeroTransparent = (1-((format>>29)&1))*255; // shash: CONVERT THIS TO A TABLE :)
      unsigned int x=0, y=0;

      if (mode == 0)
        glDisable (GL_TEXTURE_2D);
      else
        glEnable (GL_TEXTURE_2D);

      switch(mode)
        {
        case 1:
          {
            paletteSize = 256;
            pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
            break;
          }
        case 2:
          {
            paletteSize = 4;
            pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<3));
            imageSize >>= 2;
            break;
          }
        case 3:
          {
            paletteSize = 16;
            pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
            imageSize >>= 1;
            break;
          }
        case 4:
          {
            paletteSize = 256;
            pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
            break;
          }
        case 5:
          {
            paletteSize = 0;
            pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
            break;
          }
        case 6:
          {
            paletteSize = 256;
            pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
            break;
          }
        case 7:
          {
            paletteSize = 0;
            break;
          }
        }

      //if (!textureCache.IsCached((u8*)pal, paletteSize, adr, imageSize, mode, palZeroTransparent))
      {
        unsigned char * dst = texMAP;// + sizeX*sizeY*3;

        LOG("Setting up texture mode %d\n", mode);

        switch(mode)
          {
          case 1:
            {
              for(x = 0; x < imageSize; x++, dst += 4)
                {
                  unsigned short c = pal[adr[x]&31], alpha = (adr[x]>>5);
                  dst[0] = (unsigned char)((c & 0x1F)<<3);
                  dst[1] = (unsigned char)((c & 0x3E0)>>2);
                  dst[2] = (unsigned char)((c & 0x7C00)>>7);
                  dst[3] = ((alpha<<2)+(alpha>>1))<<3;
                }

              break;
            }
          case 2:
            {
              for(x = 0; x < imageSize; ++x)
                {
                  unsigned short c = pal[(adr[x])&0x3];
                  dst[0] = ((c & 0x1F)<<3);
                  dst[1] = ((c & 0x3E0)>>2);
                  dst[2] = ((c & 0x7C00)>>7);
                  dst[3] = ((adr[x]&3) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
                  dst += 4;

                  c = pal[((adr[x])>>2)&0x3];
                  dst[0] = ((c & 0x1F)<<3);
                  dst[1] = ((c & 0x3E0)>>2);
                  dst[2] = ((c & 0x7C00)>>7);
                  dst[3] = (((adr[x]>>2)&3) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
                  dst += 4;

                  c = pal[((adr[x])>>4)&0x3];
                  dst[0] = ((c & 0x1F)<<3);
                  dst[1] = ((c & 0x3E0)>>2);
                  dst[2] = ((c & 0x7C00)>>7);
                  dst[3] = (((adr[x]>>4)&3) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
                  dst += 4;

                  c = pal[(adr[x])>>6];
                  dst[0] = ((c & 0x1F)<<3);
                  dst[1] = ((c & 0x3E0)>>2);
                  dst[2] = ((c & 0x7C00)>>7);
                  dst[3] = (((adr[x]>>6)&3) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
                  dst += 4;
                }
            }
            break;
          case 3:
            {
              for(x = 0; x < imageSize; x++)
                {
                  unsigned short c = pal[adr[x]&0xF];
                  dst[0] = ((c & 0x1F)<<3);
                  dst[1] = ((c & 0x3E0)>>2);
                  dst[2] = ((c & 0x7C00)>>7);
                  dst[3] = (((adr[x])&0xF) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
                  dst += 4;

                  c = pal[((adr[x])>>4)];
                  dst[0] = ((c & 0x1F)<<3);
                  dst[1] = ((c & 0x3E0)>>2);
                  dst[2] = ((c & 0x7C00)>>7);
                  dst[3] = (((adr[x]>>4)&0xF) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
                  dst += 4;
                }
            }
            break;

          case 4:
            {
              for(x = 0; x < imageSize; ++x)
                {
                  unsigned short c = pal[adr[x]];
                  dst[0] = (unsigned char)((c & 0x1F)<<3);
                  dst[1] = (unsigned char)((c & 0x3E0)>>2);
                  dst[2] = (unsigned char)((c & 0x7C00)>>7);
                  dst[3] = (adr[x] == 0) ? palZeroTransparent : 255;//(c>>15)*255;
                  dst += 4;
                }
            }
            break;

          case 5:
            {
              // UNOPTIMIZED
              unsigned short * pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
              unsigned short * slot1 = (unsigned short*)((unsigned char *)(ARM9Mem.ARM9_LCD + ((format&0xFFFF)<<3)/2 + 0x20000));
              unsigned int * map = ((unsigned int *)adr), i = 0;
              unsigned int * dst = (unsigned int *)texMAP;

              for (y = 0; y < (sizeY/4); y ++)
                {
                  for (x = 0; x < (sizeX/4); x ++, i++)
                    {
                      u32 currBlock	= map[i], sy;
                      u16 pal1		= slot1[i];
                      u16 pal1offset	= (pal1 & 0x3FFF)<<1;
                      u8  mode		= pal1>>14;

                      for (sy = 0; sy < 4; sy++)
                        {
                          // Texture offset
                          u32 xAbs = (x<<2);
                          u32 yAbs = ((y<<2) + sy);
                          u32 currentPos = xAbs + yAbs*sizeX;

                          // Palette							
                          u8  currRow		= (u8)((currBlock >> (sy*8)) & 0xFF);
#define RGB16TO32(col,alpha) (((alpha)<<24) | ((((col) & 0x7C00)>>7)<<16) | ((((col) & 0x3E0)>>2)<<8) | (((col) & 0x1F)<<3))
#define RGB32(r,g,b,a) (((a)<<24) | ((r)<<16) | ((g)<<8) | (b))

                          switch (mode)
                            {
                            case 0:
                              {
                                u16 col0 = pal[pal1offset+((currRow>>0)&3)];
                                u16 col1 = pal[pal1offset+((currRow>>2)&3)];
                                u16 col2 = pal[pal1offset+((currRow>>4)&3)];
                                u16 col3 = pal[pal1offset+((currRow>>6)&3)];

                                dst[currentPos+0] = RGB16TO32(col0, 255);
                                dst[currentPos+1] = RGB16TO32(col1, 255);
                                dst[currentPos+2] = RGB16TO32(col2, 255);
                                dst[currentPos+3] = RGB16TO32(col3, 128);

                                break;
                              }
                            case 1:
                              {
                                u16 col0 = pal[pal1offset+((currRow>>0)&3)];
                                u16 col1 = pal[pal1offset+((currRow>>2)&3)];
                                u16 col3 = pal[pal1offset+((currRow>>6)&3)];

                                u32 col0R = ((col0 & 0x7C00)>>7);
                                u32 col0G = ((col0 & 0x3E0 )>>2);
                                u32 col0B = ((col0 & 0x1F  )<<3);
                                u32 col0A = ((pal1offset+((currRow>>0)&1)) == 0) ? palZeroTransparent : 255;

                                u32 col1R = ((col1 & 0x7C00)>>7);
                                u32 col1G = ((col1 & 0x3E0 )>>2);
                                u32 col1B = ((col1 & 0x1F  )<<3);
                                u32	col1A = ((pal1offset+((currRow>>2)&1)) == 0) ? palZeroTransparent : 255;

                                dst[currentPos+0] = RGB16TO32(col0, 255);
                                dst[currentPos+1] = RGB16TO32(col1, 255);
                                dst[currentPos+2] = RGB32(	(col0R+col1R)>>1, 
                                                                (col0G+col1G)>>1, 
                                                                (col0B+col1B)>>1, 
                                                                (col0A+col1A)>>1);
                                dst[currentPos+3] = RGB16TO32(col3, 128);
										
                                break;
                              }
                            case 2:
                              {
                                u16 col0 = pal[pal1offset+((currRow>>0)&3)];
                                u16 col1 = pal[pal1offset+((currRow>>2)&3)];
                                u16 col2 = pal[pal1offset+((currRow>>4)&3)];
                                u16 col3 = pal[pal1offset+((currRow>>6)&3)];

                                dst[currentPos+0] = RGB16TO32(col0, 255);
                                dst[currentPos+1] = RGB16TO32(col1, 255);
                                dst[currentPos+2] = RGB16TO32(col2, 255);
                                dst[currentPos+3] = RGB16TO32(col3, 255);

                                break;
                              }
                            case 3:
                              {					
                                u16 col0 = pal[pal1offset+((currRow>>0)&3)];
                                u16 col1 = pal[pal1offset+((currRow>>2)&3)];

                                u32 col0R = ((col0 & 0x7C00)>>7);
                                u32 col0G = ((col0 & 0x3E0 )>>2);
                                u32 col0B = ((col0 & 0x1F  )<<3);
                                u32 col0A = ((pal1offset+((currRow>>0)&1)) == 0) ? palZeroTransparent : 255;

                                u32 col1R = ((col1 & 0x7C00)>>7);
                                u32 col1G = ((col1 & 0x3E0 )>>2);
                                u32 col1B = ((col1 & 0x1F  )<<3);
                                u32	col1A = ((pal1offset+((currRow>>2)&1)) == 0) ? palZeroTransparent : 255;

                                dst[currentPos+0] = RGB32(col0R, col0G, col0B, col0A);
                                dst[currentPos+1] = RGB32(col1R, col1G, col1B, col1A);
                                dst[currentPos+2] = RGB32(	(col0R*5+col1R*3)>>3, 
                                                                (col0G*5+col1G*3)>>3, 
                                                                (col0B*5+col1B*3)>>3, 
                                                                (col0A*5+col1A*3)>>3);

                                dst[currentPos+3] = RGB32(	(col0R*3+col1R*5)>>3, 
                                                                (col0G*3+col1G*5)>>3, 
                                                                (col0B*3+col1B*5)>>3, 
                                                                (col0A*3+col1A*5)>>3);
                                break;
                              }
                            }
                        }
                    }
                }
					
              break;
            }
          case 6:
            {
              for(x = 0; x < imageSize; x++)
                {
                  unsigned short c = pal[adr[x]&7];
                  dst[0] = (unsigned char)((c & 0x1F)<<3);
                  dst[1] = (unsigned char)((c & 0x3E0)>>2);
                  dst[2] = (unsigned char)((c & 0x7C00)>>7);
                  dst[3] = (adr[x]&0xF8);
                  dst += 4;
                }
              break;
            }
          case 7:
            {
              unsigned short * map = ((unsigned short *)adr);
              for(x = 0; x < imageSize; ++x)
                {
                  unsigned short c = map[x];
                  dst[0] = ((c & 0x1F)<<3);
                  dst[1] = ((c & 0x3E0)>>2);
                  dst[2] = ((c & 0x7C00)>>7);
                  dst[3] = (c>>15)*255;
                  dst += 4;
                }
            }
            break;
          }

        glBindTexture(GL_TEXTURE_2D, oglTextureID);
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, sizeX, sizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, texMAP);
        //textureCache.SetTexture ( texMAP, sizeX, sizeY, (u8*)pal, paletteSize, adr, imageSize, mode, palZeroTransparent);

	/*
          switch ((format>>18)&3)
          {
          case 0:
          {
          textureCache.SetTexture ( texMAP, sizeX, sizeY, (u8*)pal, paletteSize, adr, imageSize, mode);
          break;
          }

          case 1:
          {
          u32 *src = (u32*)texMAP, *dst = (u32*)texMAP2;

          for (int y = 0; y < sizeY; y++)
          {
          for (int x = 0; x < sizeX; x++)
          {
          dst[y*sizeX*2 + x] = dst[y*sizeX*2 + (sizeX*2-x-1)] = src[y*sizeX + x];
          }
          }

          sizeX <<= 1;
          textureCache.SetTexture ( texMAP2, sizeX, sizeY, (u8*)pal, paletteSize, adr, imageSize, mode);
          break;
          }

          case 2:
          {
          u32 *src = (u32*)texMAP;

          for (int y = 0; y < sizeY; y++)
          {
          memcpy (&src[(sizeY*2-y-1)*sizeX], &src[y*sizeX], sizeX*4);
          }

          sizeY <<= 1;
          textureCache.SetTexture ( texMAP, sizeX, sizeY, (u8*)pal, paletteSize, adr, imageSize, mode);
          break;
          }

          case 3:
          {
          u32 *src = (u32*)texMAP, *dst = (u32*)texMAP2;

          for (int y = 0; y < sizeY; y++)
          {
          for (int x = 0; x < sizeX; x++)
          {
          dst[y*sizeX*2 + x] = dst[y*sizeX*2 + (sizeX*2-x-1)] = src[y*sizeX + x];
          }
          }

          sizeX <<= 1;

          for (int y = 0; y < sizeY; y++)
          {
          memcpy (&dst[(sizeY*2-y-1)*sizeX], &dst[y*sizeX], sizeX*4);
          }

          sizeY <<= 1;
          textureCache.SetTexture ( texMAP2, sizeX, sizeY, (u8*)pal, paletteSize, adr, imageSize, mode);
          break;
          }
          }
	*/
      }

      invTexWidth  = 1.f/((float)sizeX*(1<<4));//+ 1;
      invTexHeight = 1.f/((float)sizeY*(1<<4));//+ 1;
		
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

      glMatrixMode (GL_TEXTURE);
      glLoadIdentity ();
      glScaled (invTexWidth, invTexHeight, 1.f);

      set_gl_matrix_mode( current_matrix_mode);

      // S Coordinate options
      if (!BIT16(format))
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
      else
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);

      // T Coordinate options
      if (!BIT17(format))
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
      else
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);

      //flipS = BIT18(format);
      //flipT = BIT19(format);

      texCoordinateTransform = (format>>30);
    }
}


static void
setup_mode23_tex_coord( float x, float y, float z) {
  float *textureMatrix = mtxCurrent[3]; 
  int s2 =	(int)((	x * textureMatrix[0] +  
                        y * textureMatrix[4] +  
                        z * textureMatrix[8]) + s_texture_coord);
  int t2 =	(int)((	x * textureMatrix[1] +  
                        y * textureMatrix[5] +  
                        z * textureMatrix[9]) + t_texture_coord); 

  glTexCoord2i( s2, t2);
}




static void
process_begin_vtxs( struct render_state *state,
                    const u32 *parms) {
  u32 prim_type = parms[0] & 0x3;

  LOG("Begin: %s\n", primitive_type_names[prim_type]);

  /* setup the texture */
  if ( disp_3D_control & 0x1) {
    if (textureFormat  != lastTextureFormat ||
        texturePalette != lastTexturePalette)
      {
        LOG("Setting up texture %08x\n", textureFormat);
        SetupTexture (textureFormat, texturePalette);
      
        lastTextureFormat = textureFormat;
        lastTexturePalette = texturePalette;
      }
  }
  else {
    glDisable (GL_TEXTURE_2D);
  }

  switch (prim_type) {
  case 0:
    glBegin( GL_TRIANGLES);
    break;
  case 1:
    glBegin( GL_QUADS);
    break;
  case 2:
    glBegin( GL_TRIANGLE_STRIP);
    break;
  case 3:
    glBegin( GL_QUAD_STRIP);
    break;
  }
}

static void
process_end_vtxs( struct render_state *state,
                  const u32 *parms) {
  LOG("End\n");
  glEnd();
}

static void
process_viewport( struct render_state *state,
                  const u32 *parms) {
  LOG("FIXME: Viewport %d,%d,%d,%d\n", parms[0] & 0xff, (parms[0] >> 8) & 0xff,
      (parms[0] >> 16) & 0xff, (parms[0] >> 24) & 0xff);
}

static void
process_polygon_attr( struct render_state *state,
                      const u32 *parms) {
  LOG("FIXME: polygon attr\n");
}

static void
process_normal( struct render_state *state,
                const u32 *parms) {
  LOG("Normal %08x\n", parms[0]);

  float normal[3] = { normalTable[parms[0] &1023],
                      normalTable[(parms[0]>>10)&1023],
                      normalTable[(parms[0]>>20)&1023]};

  if (texCoordinateTransform == 2)
    {
      setup_mode23_tex_coord( normal[0], normal[1], normal[2]);
    }

  glNormal3fv(normal);
}

static void
process_teximage_param( struct render_state *state,
                        const u32 *parms) {
  LOG("texture param %08x\n", parms[0]);

  textureFormat = parms[0];
}

static void
process_pltt_base( struct render_state *state,
                   const u32 *parms) {
  LOG("texture palette base %08x\n", parms[0]);

  texturePalette = parms[0];
}

static void
process_texcoord( struct render_state *state,
                        const u32 *parms) {
  LOG("texture coord %08x\n", parms[0]);

  t_texture_coord = (s16)(parms[0] >> 16);
  s_texture_coord = (s16)(parms[0] & 0xFFFF);

  if ( texCoordinateTransform == 1)
    {
      float *textureMatrix = mtxCurrent[3];
      int s2 =(int)( s_texture_coord * textureMatrix[0] +
                     t_texture_coord * textureMatrix[4] +
                     (1.f/16.f) * textureMatrix[8] +
                     (1.f/16.f) * textureMatrix[12]);
      int t2 =(int)( s_texture_coord * textureMatrix[1] +
                     t_texture_coord * textureMatrix[5] +
                     (1.f/16.f) * textureMatrix[9] +
                     (1.f/16.f) * textureMatrix[13]);

      LOG("texture coord (pre-trans) s=%d t=%d\n", s_texture_coord, t_texture_coord);
      LOG_MATRIX( textureMatrix);
      LOG("texture coord (trans) s=%d t=%d\n", s2, t2);
      glTexCoord2i (s2, t2);
    }
  else
    {
      LOG("texture coord s=%d t=%d\n", s_texture_coord, t_texture_coord);
      glTexCoord2i (s_texture_coord,t_texture_coord);
    }
}

static void
process_mtx_mode( struct render_state *state,
                  const u32 *parms) {
  LOG("Set current matrix %08x\n", parms[0]);

  current_matrix_mode = parms[0] & 0x3;

  set_gl_matrix_mode( current_matrix_mode);
}


static void
process_mtx_identity( struct render_state *state,
                      const u32 *parms) {
  LOG("Load identity\n");

  MatrixIdentity (mtxCurrent[current_matrix_mode]);

  if (current_matrix_mode == 2)
    MatrixIdentity (mtxCurrent[1]);


  glLoadIdentity();
}

static void
process_mtx_push( struct render_state *state,
                  const u32 *parms) {
  LOG("Matrix push\n");

  MatrixStackPushMatrix (&mtxStack[current_matrix_mode],
                         mtxCurrent[current_matrix_mode]);
}

static void
process_mtx_pop( struct render_state *state,
                 const u32 *parms) {
  s32 index = parms[0];
  LOG("Matrix pop\n");

  MatrixCopy (mtxCurrent[current_matrix_mode],
              MatrixStackPopMatrix (&mtxStack[current_matrix_mode], index));

  if (current_matrix_mode == 2) {
    MatrixCopy (mtxCurrent[1], mtxCurrent[2]);
  }

  if ( current_matrix_mode < 3)
    glLoadMatrixf( mtxCurrent[current_matrix_mode]);
}

static void
process_mtx_store( struct render_state *state,
                   const u32 *parms) {
  LOG("Matrix store\n");

  MatrixStackLoadMatrix (&mtxStack[current_matrix_mode],
                         parms[0] & 31,
                         mtxCurrent[current_matrix_mode]);
}

static void
process_mtx_restore( struct render_state *state,
                   const u32 *parms) {
  LOG("Matrix restore\n");

  MatrixCopy (mtxCurrent[current_matrix_mode],
              MatrixStackGetPos(&mtxStack[current_matrix_mode], parms[0]&31));

  if (current_matrix_mode == 2) {
    MatrixCopy (mtxCurrent[1], mtxCurrent[2]);
  }

  if ( current_matrix_mode < 3)
    glLoadMatrixf( mtxCurrent[current_matrix_mode]);
}

static void
process_mtx_load_4x4( struct render_state *state,
                      const u32 *parms) {
  int i;
  LOG("Load 4x4 (%d):\n", current_matrix_mode);
  LOG("%08x, %08x, %08x, %08x\n", parms[0], parms[1], parms[2], parms[3]);
  LOG("%08x, %08x, %08x, %08x\n", parms[4], parms[5], parms[6], parms[7]);
  LOG("%08x, %08x, %08x, %08x\n", parms[8], parms[9], parms[10], parms[11]);
  LOG("%08x, %08x, %08x, %08x\n", parms[12], parms[13], parms[14], parms[15]);

  for ( i = 0; i < 16; i++) {
    mtxCurrent[current_matrix_mode][i] = fix2float(parms[i]);
  }

  if (current_matrix_mode == 2)
    MatrixCopy (mtxCurrent[1], mtxCurrent[2]);

  LOG("%f, %f, %f, %f\n",
      mtxCurrent[current_matrix_mode][0], mtxCurrent[current_matrix_mode][1],
      mtxCurrent[current_matrix_mode][2], mtxCurrent[current_matrix_mode][3]);
  LOG("%f, %f, %f, %f\n",
      mtxCurrent[current_matrix_mode][4], mtxCurrent[current_matrix_mode][5],
      mtxCurrent[current_matrix_mode][6], mtxCurrent[current_matrix_mode][7]);
  LOG("%f, %f, %f, %f\n",
      mtxCurrent[current_matrix_mode][8], mtxCurrent[current_matrix_mode][9],
      mtxCurrent[current_matrix_mode][10], mtxCurrent[current_matrix_mode][11]);
  LOG("%f, %f, %f, %f\n",
      mtxCurrent[current_matrix_mode][12], mtxCurrent[current_matrix_mode][13],
      mtxCurrent[current_matrix_mode][14], mtxCurrent[current_matrix_mode][15]);

  if ( current_matrix_mode < 3)
    glLoadMatrixf( mtxCurrent[current_matrix_mode]);
}

static void
process_mtx_load_4x3( struct render_state *state,
                      const u32 *parms) {
  int i;

  LOG("Load 4x3 (%d):\n", current_matrix_mode);
  LOG("%08x, %08x, %08x, 0.0\n", parms[0], parms[1], parms[2]);
  LOG("%08x, %08x, %08x, 0.0\n", parms[3], parms[4], parms[5]);
  LOG("%08x, %08x, %08x, 0.0\n", parms[6], parms[7], parms[8]);
  LOG("%08x, %08x, %08x, 1.0\n", parms[9], parms[10], parms[11]);

  mtxCurrent[current_matrix_mode][3] = mtxCurrent[current_matrix_mode][7] =
    mtxCurrent[current_matrix_mode][11] = 0.0f;
  mtxCurrent[current_matrix_mode][15] = 1.0f;
  mtxCurrent[current_matrix_mode][0] = fix2float(parms[0]);
  mtxCurrent[current_matrix_mode][1] = fix2float(parms[1]);
  mtxCurrent[current_matrix_mode][2] = fix2float(parms[2]);
  mtxCurrent[current_matrix_mode][4] = fix2float(parms[3]);
  mtxCurrent[current_matrix_mode][5] = fix2float(parms[4]);
  mtxCurrent[current_matrix_mode][6] = fix2float(parms[5]);
  mtxCurrent[current_matrix_mode][8] = fix2float(parms[6]);
  mtxCurrent[current_matrix_mode][9] = fix2float(parms[7]);
  mtxCurrent[current_matrix_mode][10] = fix2float(parms[8]);
  mtxCurrent[current_matrix_mode][12] = fix2float(parms[9]);
  mtxCurrent[current_matrix_mode][13] = fix2float(parms[10]);
  mtxCurrent[current_matrix_mode][14] = fix2float(parms[11]);

  if (current_matrix_mode == 2)
    MatrixCopy (mtxCurrent[1], mtxCurrent[2]);

  LOG("%f, %f, %f, %f\n",
      mtxCurrent[current_matrix_mode][0], mtxCurrent[current_matrix_mode][1],
      mtxCurrent[current_matrix_mode][2], mtxCurrent[current_matrix_mode][3]);
  LOG("%f, %f, %f, %f\n",
      mtxCurrent[current_matrix_mode][4], mtxCurrent[current_matrix_mode][5],
      mtxCurrent[current_matrix_mode][6], mtxCurrent[current_matrix_mode][7]);
  LOG("%f, %f, %f, %f\n",
      mtxCurrent[current_matrix_mode][8], mtxCurrent[current_matrix_mode][9],
      mtxCurrent[current_matrix_mode][10], mtxCurrent[current_matrix_mode][11]);
  LOG("%f, %f, %f, %f\n",
      mtxCurrent[current_matrix_mode][12], mtxCurrent[current_matrix_mode][13],
      mtxCurrent[current_matrix_mode][14], mtxCurrent[current_matrix_mode][15]);

  if ( current_matrix_mode < 3)
    glLoadMatrixf( mtxCurrent[current_matrix_mode]);
}


static void
process_mtx_trans( struct render_state *state,
                   const u32 *parms) {
  float trans[3];

  trans[0] = fix2float(parms[0]);
  trans[1] = fix2float(parms[1]);
  trans[2] = fix2float(parms[2]);

  LOG("translate %lf,%lf,%lf (%d)\n", trans[0], trans[1], trans[2],
      current_matrix_mode);

  MatrixTranslate (mtxCurrent[current_matrix_mode], trans);

  if (current_matrix_mode == 2)
    MatrixTranslate (mtxCurrent[1], trans);


  if ( current_matrix_mode == 2)
    glLoadMatrixf( mtxCurrent[1]);
  else if ( current_matrix_mode < 3)
    glLoadMatrixf( mtxCurrent[current_matrix_mode]);
}

static void
process_mtx_scale( struct render_state *state,
                   const u32 *parms) {
  float scale[3];

  LOG("scale %08x, %08x, %08x (%d)\n", parms[0], parms[1], parms[2],
      current_matrix_mode);

  scale[0] = fix2float(parms[0]);
  scale[1] = fix2float(parms[1]);
  scale[2] = fix2float(parms[2]);

  MatrixScale (mtxCurrent[current_matrix_mode], scale);

  if (current_matrix_mode == 2)
    MatrixScale (mtxCurrent[1], scale);

  LOG("scale %f,%f,%f\n", scale[0], scale[1], scale[2]);

  if ( current_matrix_mode == 2)
    glLoadMatrixf( mtxCurrent[1]);
  else if ( current_matrix_mode < 3)
    glLoadMatrixf( mtxCurrent[current_matrix_mode]);
}

static void
process_mtx_mult_3x3( struct render_state *state,
                      const u32 *parms) {
  static float mult_matrix[16];

  LOG("Mult 3x3 (%d):\n", current_matrix_mode);

  mult_matrix[3] = mult_matrix[7] =
    mult_matrix[11] = 0.0f;
  mult_matrix[12] = mult_matrix[13] =
    mult_matrix[14] = 0.0f;
  mult_matrix[15] = 1.0f;
  mult_matrix[0] = fix2float(parms[0]);
  mult_matrix[1] = fix2float(parms[1]);
  mult_matrix[2] = fix2float(parms[2]);
  mult_matrix[4] = fix2float(parms[3]);
  mult_matrix[5] = fix2float(parms[4]);
  mult_matrix[6] = fix2float(parms[5]);
  mult_matrix[8] = fix2float(parms[6]);
  mult_matrix[9] = fix2float(parms[7]);
  mult_matrix[10] = fix2float(parms[8]);
  
  MatrixMultiply (mtxCurrent[current_matrix_mode], mult_matrix);

  if (current_matrix_mode == 2)
    MatrixMultiply (mtxCurrent[1], mult_matrix);

  if ( current_matrix_mode == 2)
    glLoadMatrixf( mtxCurrent[1]);
  else if ( current_matrix_mode < 3)
    glLoadMatrixf( mtxCurrent[current_matrix_mode]);
}

static void
process_colour( struct render_state *state,
                const u32 *parms) {
  s8 red = (parms[0] & 0x1f) << 3;
  s8 green = ((parms[0] >> 5) & 0x1f) << 3;
  s8 blue = ((parms[0] >> 10) & 0x1f) << 3;
  LOG("colour %d,%d,%d\n", red, green, blue);
  glColor3ub( red, green, blue);
}

static void
process_vtx_16( struct render_state *state,
                const u32 *parms) {
  float x = float16table[parms[0] & 0xFFFF];
  float y = float16table[parms[0] >> 16];
  float z = float16table[parms[1] & 0xFFFF];
  LOG("vtx16 %08x %08x\n", parms[0], parms[1]);

  LOG("vtx16 %f,%f,%f\n", x, y, z);

  if (texCoordinateTransform == 3)
    { 
      setup_mode23_tex_coord( x, y, z);
    } 

  glVertex3f( x, y, x);
}

static void
process_vtx_10( struct render_state *state,
                const u32 *parms) {
  float x = float10Table[(parms[0]) & 0x3ff];
  float y = float10Table[(parms[0] >> 10) & 0x3ff];
  float z = float10Table[(parms[0] >> 20) & 0x3ff];
  LOG("vtx10 %08x\n", parms[0]);

  LOG("vtx10 %f,%f,%f\n", x, y, z);
  if (texCoordinateTransform == 3)
    { 
      setup_mode23_tex_coord( x, y, z);
    } 

  glVertex3f( x, y, x);
}


static void
process_control( struct render_state *state,
                 const u32 *parms) {
  LOG("Control set to %08x\n", parms[0]);
  disp_3D_control = parms[0];

  if( disp_3D_control & 1)
    {
      glEnable (GL_TEXTURE_2D);
    }
  else
    {
      glDisable (GL_TEXTURE_2D);
    }

  if( disp_3D_control & (1<<2))
    {
      glEnable(GL_ALPHA_TEST);
    }
  else
    {
      glDisable(GL_ALPHA_TEST);
    }

}

static void
process_alpha_function( struct render_state *state,
                        const u32 *parms) {
  LOG("Alpha function %08x\n", parms[0]);
  glAlphaFunc (GL_GREATER, (parms[0]&31)/31.f);
}

static void
process_clear_depth( struct render_state *state,
                     const u32 *parms) {
  u32 depth24b;
  u32 v = parms[0] & 0x7FFFF;
  LOG("Clear depth %08x\n", parms[0]);

  depth24b = (v * 0x200)+((v+1)/0x8000);

  glClearDepth(depth24b / ((float)(1<<24)));
}

static void
process_clear_colour( struct render_state *state,
                      const u32 *parms) {
  u32 v = parms[0];
  LOG("Clear colour %08x\n", v);

  glClearColor(	((float)(v&0x1F))/31.0f,
                ((float)((v>>5)&0x1F))/31.0f, 
                ((float)((v>>10)&0x1F))/31.0f, 
                ((float)((v>>16)&0x1F))/31.0f);

}


/*
 * The rendering function.
 */
static void
draw_3D_area( void) {
  struct render_state *state = &render_states[GET_DRAW_STATE_INDEX(current_render_state)];
  int i;
  GLenum errCode;


  /*** OpenGL BEGIN ***/
  if ( !begin_opengl_ogl_collector_platform()) {
    LOG_ERROR( "platform failed for begin opengl for draw\n");
    return;
  }

  LOG("\n------------------------------------\n");
  LOG("Start of render\n");
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  for ( i = 0; i < state->write_index; i++) {
    u32 cmd = state->cmds[i];
    //LOG("Render cmd: %08x\n", state->cmds[i]);

    if ( cmd < LAST_CMD_VALUE + 1) {
      if ( cmd_processors[cmd].processor_fn != NULL) {
        cmd_processors[cmd].processor_fn( state, &state->cmds[i+1]);
      }
      else {
        LOG_ERROR("Unhandled %02x\n", cmd);
      }
      i += cmd_processors[cmd].num_parms;
    }
  }

  glFlush ();

  if ((errCode = glGetError()) != GL_NO_ERROR) {
    const GLubyte *errString;

    errString = gluErrorString(errCode);
    LOG_ERROR( "openGL error during 3D emulation: %s\n", errString);
  }

#ifdef USE_BGR_ORDER
  glReadPixels(0,0,256,192,GL_BGRA,GL_UNSIGNED_BYTE,GPU_screen3D);
#else
  glReadPixels(0,0,256,192,GL_RGBA,GL_UNSIGNED_BYTE,GPU_screen3D);
#endif

  if ((errCode = glGetError()) != GL_NO_ERROR) {
    const GLubyte *errString;

    errString = gluErrorString(errCode);
    LOG_ERROR( "openGL error during glReadPixels: %s\n", errString);
  }

  LOG("End of render\n------------------------------------\n");

  end_opengl_ogl_collector_platform();
  return;
}


static void
init_openGL( void) {
  /* OpenGL BEGIN */
  if ( !begin_opengl_ogl_collector_platform()) {
    LOG_ERROR( "platform failed to begin opengl for initialisation\n");
    return;
  }

  /* Set the background black */
  glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

  /* Enables Depth Testing */
  glEnable( GL_DEPTH_TEST );
  glEnable(GL_TEXTURE_2D);

  glGenTextures (1, &oglTextureID);

  glViewport (0, 0, 256, 192);

  end_opengl_ogl_collector_platform();
  /*** OpenGL END ***/
}


static char
nullFunc1_3Dgl_collect(void) {
  return 1;
}

static void
nullFunc2_3Dgl_collect(void) {
}
static void
nullFunc3_3Dgl_collect(unsigned long v) {
}
static void nullFunc4_3Dgl_collect(signed long v){}
static void nullFunc5_3Dgl_collect(unsigned int v){}
static void nullFunc6_3Dgl_collect(unsigned int one,
                                   unsigned int two, unsigned int v){}
static int  nullFunc7_3Dgl_collect(void) {return 0;}
static long nullFunc8_3Dgl_collect(unsigned int index){ return 0; }
static void nullFunc9_3Dgl_collect(int line, unsigned short * DST) { };


static int num_primitives[4];

static char
init_3Dgl_collect( void) {
  int i;

  LOG("Initialising 3D renderer for OpenGL Collector\n");

  if ( !initialise_ogl_collector_platform()) {
    LOG_ERROR( "Platform initialisation failed\n");
    return 0;
  }

  MatrixStackSetMaxSize(&mtxStack[0], 1);		// Projection stack
  MatrixStackSetMaxSize(&mtxStack[1], 31);	// Coordinate stack
  MatrixStackSetMaxSize(&mtxStack[2], 31);	// Directional stack
  MatrixStackSetMaxSize(&mtxStack[3], 1);		// Texture stack

  MatrixInit (mtxCurrent[0]);
  MatrixInit (mtxCurrent[1]);
  MatrixInit (mtxCurrent[2]);
  MatrixInit (mtxCurrent[3]);
  MatrixInit (mtxTemporal);

  current_render_state = 0;
  for ( i = 0; i < NUM_RENDER_STATES; i++) {
    render_states[i].write_index = 0;

  }

  for ( i = 0; i < 4; i++) {
    num_primitives[i] = 0;
  }

  for ( i = 0; i < LAST_CMD_VALUE+1; i++) {
    cmd_processors[i].processor_fn = NULL;
    cmd_processors[i].num_parms = 0;

    switch ( i) {
    case NOP_CMD:
      cmd_processors[i].num_parms = 0;
      break;
    case MTX_MODE_CMD:
      cmd_processors[i].processor_fn = process_mtx_mode;
      cmd_processors[i].num_parms = 1;
      break;
    case MTX_PUSH_CMD:
      cmd_processors[i].processor_fn = process_mtx_push;
      cmd_processors[i].num_parms = 0;
      break;
    case MTX_POP_CMD:
      cmd_processors[i].processor_fn = process_mtx_pop;
      cmd_processors[i].num_parms = 1;
      break;
    case MTX_STORE_CMD:
      cmd_processors[i].processor_fn = process_mtx_store;
      cmd_processors[i].num_parms = 1;
      break;
    case MTX_RESTORE_CMD:
      cmd_processors[i].processor_fn = process_mtx_restore;
      cmd_processors[i].num_parms = 1;
      break;
    case MTX_IDENTITY_CMD:
      cmd_processors[i].processor_fn = process_mtx_identity;
      cmd_processors[i].num_parms = 0;
      break;
    case MTX_LOAD_4x4_CMD:
      cmd_processors[i].processor_fn = process_mtx_load_4x4;
      cmd_processors[i].num_parms = 16;
      break;
    case MTX_LOAD_4x3_CMD:
      cmd_processors[i].processor_fn = process_mtx_load_4x3;
      cmd_processors[i].num_parms = 12;
      break;
    case MTX_MULT_4x4_CMD:
      cmd_processors[i].num_parms = 16;
      break;
    case MTX_MULT_4x3_CMD:
      cmd_processors[i].num_parms = 12;
      break;
    case MTX_MULT_3x3_CMD:
      cmd_processors[i].processor_fn = process_mtx_mult_3x3;
      cmd_processors[i].num_parms = 9;
      break;
    case MTX_SCALE_CMD:
      cmd_processors[i].processor_fn = process_mtx_scale;
      cmd_processors[i].num_parms = 3;
      break;
    case MTX_TRANS_CMD:
      cmd_processors[i].processor_fn = process_mtx_trans;
      cmd_processors[i].num_parms = 3;
      break;
    case COLOR_CMD:
      cmd_processors[i].processor_fn = process_colour;
      cmd_processors[i].num_parms = 1;
      break;
    case NORMAL_CMD:
      cmd_processors[i].processor_fn = process_normal;
      cmd_processors[i].num_parms = 1;
      break;
    case TEXCOORD_CMD:
      cmd_processors[i].processor_fn = process_texcoord;
      cmd_processors[i].num_parms = 1;
      break;
    case VTX_16_CMD:
      cmd_processors[i].processor_fn = process_vtx_16;
      cmd_processors[i].num_parms = 2;
      break;
    case VTX_10_CMD:
      cmd_processors[i].processor_fn = process_vtx_10;
      cmd_processors[i].num_parms = 1;
      break;
    case VTX_XY_CMD:
      cmd_processors[i].num_parms = 1;
      break;
    case VTX_XZ_CMD:
      cmd_processors[i].num_parms = 1;
      break;
    case VTX_YZ_CMD:
      cmd_processors[i].num_parms = 1;
      break;
    case VTX_DIFF_CMD:
      cmd_processors[i].num_parms = 1;
      break;
    case POLYGON_ATTR_CMD:
      cmd_processors[i].processor_fn = process_polygon_attr;
      cmd_processors[i].num_parms = 1;
      break;
    case TEXIMAGE_PARAM_CMD:
      cmd_processors[i].processor_fn = process_teximage_param;
      cmd_processors[i].num_parms = 1;
      break;
    case PLTT_BASE_CMD:
      cmd_processors[i].processor_fn = process_pltt_base;
      cmd_processors[i].num_parms = 1;
      break;
    case DIF_AMB_CMD:
      cmd_processors[i].num_parms = 1;
      break;
    case SPE_EMI_CMD:
      cmd_processors[i].num_parms = 1;
      break;
    case LIGHT_VECTOR_CMD:
      cmd_processors[i].num_parms = 1;
      break;
    case LIGHT_COLOR_CMD:
      cmd_processors[i].num_parms = 1;
      break;
    case SHININESS_CMD:
      cmd_processors[i].num_parms = 32;
      break;
    case BEGIN_VTXS_CMD:
      cmd_processors[i].processor_fn = process_begin_vtxs;
      cmd_processors[i].num_parms = 1;
      break;
    case END_VTXS_CMD:
      cmd_processors[i].processor_fn = process_end_vtxs;
      cmd_processors[i].num_parms = 0;
      break;
    case SWAP_BUFFERS_CMD:
      cmd_processors[i].num_parms = 1;
      break;
    case VIEWPORT_CMD:
      cmd_processors[i].processor_fn = process_viewport;
      cmd_processors[i].num_parms = 1;
      break;
    case BOX_TEST_CMD:
      cmd_processors[i].num_parms = 3;
      break;
    case POS_TEST_CMD:
      cmd_processors[i].num_parms = 2;
      break;
    case VEC_TEST_CMD:
      cmd_processors[i].num_parms = 1;
      break;

      /* The next ones are not NDS commands */
    case CLEAR_COLOUR_CMD:
      cmd_processors[i].processor_fn = process_clear_colour;
      cmd_processors[i].num_parms = 1;
      break;
    case CLEAR_DEPTH_CMD:
      cmd_processors[i].processor_fn = process_clear_depth;
      cmd_processors[i].num_parms = 1;
      break;

    case FOG_COLOUR_CMD:
      cmd_processors[i].num_parms = 1;
      break;
    case FOG_OFFSET_CMD:
      cmd_processors[i].num_parms = 1;
      break;

    case CONTROL_CMD:
      cmd_processors[i].processor_fn = process_control;
      cmd_processors[i].num_parms = 1;
      break;

    case ALPHA_FUNCTION_CMD:
      cmd_processors[i].processor_fn = process_alpha_function;
      cmd_processors[i].num_parms = 1;
      break;

    }
  }

  for (i = 0; i < 65536; i++)
    {
      float16table[i] = fix2float((signed short)i);
    }

  for (i = 0; i < 1024; i++)
    {
      float10RelTable[i] = ((signed short)(i<<6)) / (float)(1<<18);
      float10Table[i] = ((signed short)(i<<6)) / (float)(1<<12);
      normalTable[i] = ((signed short)(i<<6)) / (float)(1<<16);
    }

  init_openGL();

  return 1;
}




static void
Flush_3Dgl_collect( unsigned long val) {
  int i;
  int new_render_state = GET_NEXT_RENDER_STATE_INDEX(current_render_state);
  struct render_state *state = &render_states[current_render_state];
  struct render_state *new_state = &render_states[new_render_state];

  LOG("Flush %lu %d %d\n", val,
      state->write_index, render_states[new_render_state].write_index);

  current_render_state = new_render_state;
  draw_3D_area();
  new_state->write_index = 0;

  //LOG("End of render\n");
}

static void
SwapScreen_3Dgl_collect( unsigned int screen) {
  LOG("NEVER USED? SwapScreen %d\n", screen);
}


static void
call_list_3Dgl_collect(unsigned long v) {
  static u32 call_list_command = 0;
  static u32 total_num_parms = 0;
  static u32 current_parm = 0;
  static u32 parms[15];

  LOG_CALL_LIST("call list - %08x\n", v);

  if ( call_list_command == 0) {
    /* new call list command coming in */
    call_list_command = v;

    total_num_parms = cmd_processors[v & 0xff].num_parms;
    current_parm = 0;

    LOG_CALL_LIST("new command %02x total parms %d\n", v & 0xff,
                  total_num_parms);
  }
  else {
    LOG_CALL_LIST("Adding parm %d - %08x\n", current_parm, v);
    parms[current_parm] = v;
    current_parm += 1;
  }

  if ( current_parm == total_num_parms) {
    do {
      u32 cmd = call_list_command & 0xff;
      if ( cmd != 0) {
        int i;

        LOG_CALL_LIST("Cmd %02x complete:\n", cmd);

        if ( cmd == SWAP_BUFFERS_CMD) {
          Flush_3Dgl_collect( parms[0]);
        }
        else {
          /* put the command and parms on the list */
          ADD_RENDER_PARM_CMD( cmd);

          for ( i = 0; i < total_num_parms; i++) {
            ADD_RENDER_PARM_CMD( parms[i]);
            LOG_CALL_LIST("parm %08x\n", parms[i]);
          }
        }
      }
      call_list_command >>=8;

      total_num_parms = cmd_processors[call_list_command & 0xff].num_parms;
      current_parm = 0;

      LOG_CALL_LIST("Next cmd %02x parms %d\n", call_list_command & 0xff,
                    total_num_parms);
    } while ( call_list_command && total_num_parms == 0);
  }
}

static void
viewport_3Dgl_collect(unsigned long v) {
  //LOG("Viewport\n");
  ADD_RENDER_PARM_CMD( VIEWPORT_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
Begin_3Dgl_collect(unsigned long v) {
  //LOG("Begin\n");
  ADD_RENDER_PARM_CMD( BEGIN_VTXS_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
End_3Dgl_collect( void) {
  //LOG("END\n");
  ADD_RENDER_PARM_CMD( END_VTXS_CMD);
}


static void
fog_colour_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( FOG_COLOUR_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
fog_offset_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( FOG_OFFSET_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
clear_depth_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( CLEAR_DEPTH_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
clear_colour_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( CLEAR_COLOUR_CMD);
  ADD_RENDER_PARM_CMD( v);
}



static void
Vertex16b_3Dgl_collect( unsigned int v) {
  static int vertex16_count = 0;
  static u32 parm1;

  if ( vertex16_count == 0) {
    vertex16_count += 1;
    parm1 = v;
  }
  else {
    ADD_RENDER_PARM_CMD( VTX_16_CMD);
    ADD_RENDER_PARM_CMD( parm1);
    ADD_RENDER_PARM_CMD( v);
    vertex16_count = 0;
  }
}

static void
Vertex10b_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( VTX_10_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
Vertex3_cord_3Dgl_collect(unsigned int one, unsigned int two, unsigned int v) {
  LOG("NOT USED?: glVertex3_cord\n");
}

static void
Vertex_rel_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( VTX_DIFF_CMD);
  ADD_RENDER_PARM_CMD( v);
}


static void
matrix_mode_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( MTX_MODE_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
load_identity_matrix_3Dgl_collect(void) {
  ADD_RENDER_PARM_CMD( MTX_IDENTITY_CMD);
}


static void
load_4x4_matrix_3Dgl_collect(signed long v) {
  static int count_4x4 = 0;
  static u32 parms[15];

  if ( count_4x4 < 15) {
    parms[count_4x4] = v;
    count_4x4 += 1;
  }
  else {
    int i;

    ADD_RENDER_PARM_CMD( MTX_LOAD_4x4_CMD);

    for ( i = 0; i < 15; i++) {
      ADD_RENDER_PARM_CMD( parms[i]);
    }
    ADD_RENDER_PARM_CMD( v);
    count_4x4 = 0;
  }
}

static void
load_4x3_matrix_3Dgl_collect(signed long v) {
  static int count_4x3 = 0;
  static u32 parms[11];

  if ( count_4x3 < 11) {
    parms[count_4x3] = v;
    count_4x3 += 1;
  }
  else {
    int i;

    ADD_RENDER_PARM_CMD( MTX_LOAD_4x3_CMD);

    for ( i = 0; i < 11; i++) {
      ADD_RENDER_PARM_CMD( parms[i]);
    }
    ADD_RENDER_PARM_CMD( v);
    count_4x3 = 0;
  }
}


static void
multi_matrix_4x4_3Dgl_collect(signed long v) {
  static int count_4x4 = 0;
  static u32 parms[15];

  if ( count_4x4 < 15) {
    parms[count_4x4] = v;
    count_4x4 += 1;
  }
  else {
    int i;

    ADD_RENDER_PARM_CMD( MTX_MULT_4x4_CMD);

    for ( i = 0; i < 15; i++) {
      ADD_RENDER_PARM_CMD( parms[i]);
    }
    ADD_RENDER_PARM_CMD( v);
    count_4x4 = 0;
  }
}

static void
multi_matrix_4x3_3Dgl_collect(signed long v) {
  static int count_4x3 = 0;
  static u32 parms[11];

  if ( count_4x3 < 11) {
    parms[count_4x3] = v;
    count_4x3 += 1;
  }
  else {
    int i;

    ADD_RENDER_PARM_CMD( MTX_MULT_4x3_CMD);

    for ( i = 0; i < 11; i++) {
      ADD_RENDER_PARM_CMD( parms[i]);
    }
    ADD_RENDER_PARM_CMD( v);
    count_4x3 = 0;
  }
}

static void
multi_matrix_3x3_3Dgl_collect(signed long v) {
  static int count_3x3 = 0;
  static u32 parms[8];

  if ( count_3x3 < 8) {
    parms[count_3x3] = v;
    count_3x3 += 1;
  }
  else {
    int i;

    ADD_RENDER_PARM_CMD( MTX_MULT_3x3_CMD);

    for ( i = 0; i < 8; i++) {
      ADD_RENDER_PARM_CMD( parms[i]);
    }
    ADD_RENDER_PARM_CMD( v);
    count_3x3 = 0;
  }
}




static void
store_matrix_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( MTX_STORE_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
restore_matrix_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( MTX_RESTORE_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
push_matrix_3Dgl_collect( void) {
  ADD_RENDER_PARM_CMD( MTX_PUSH_CMD);
}

static void
pop_matrix_3Dgl_collect(signed long v) {
  ADD_RENDER_PARM_CMD( MTX_POP_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
translate_3Dgl_collect(signed long v) {
  static int count_trans = 0;
  static u32 parms[2];

  if ( count_trans < 2) {
    parms[count_trans] = v;
    count_trans += 1;
  }
  else {
    int i;

    ADD_RENDER_PARM_CMD( MTX_TRANS_CMD);

    for ( i = 0; i < 2; i++) {
      ADD_RENDER_PARM_CMD( parms[i]);
    }
    ADD_RENDER_PARM_CMD( v);
    count_trans = 0;
  }
}

static void
scale_3Dgl_collect(signed long v) {
  static int count_scale = 0;
  static u32 parms[2];

  if ( count_scale < 2) {
    parms[count_scale] = v;
    count_scale += 1;
  }
  else {
    int i;

    ADD_RENDER_PARM_CMD( MTX_SCALE_CMD);

    for ( i = 0; i < 2; i++) {
      ADD_RENDER_PARM_CMD( parms[i]);
    }
    ADD_RENDER_PARM_CMD( v);
    count_scale = 0;
  }
}

static void
PolyAttr_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( POLYGON_ATTR_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
TextImage_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( TEXIMAGE_PARAM_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
Colour_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( COLOR_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
material0_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( DIF_AMB_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
material1_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( SPE_EMI_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
shininess_3Dgl_collect(unsigned long v) {
  static int count_shine = 0;
  static u32 parms[31];

  if ( count_shine < 31) {
    parms[count_shine] = v;
    count_shine += 1;
  }
  else {
    int i;

    ADD_RENDER_PARM_CMD( SHININESS_CMD);

    for ( i = 0; i < 31; i++) {
      ADD_RENDER_PARM_CMD( parms[i]);
    }
    ADD_RENDER_PARM_CMD( v);
    count_shine = 0;
  }
}

static void
texture_palette_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( PLTT_BASE_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
texture_coord_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( TEXCOORD_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
light_direction_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( LIGHT_VECTOR_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
light_colour_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( LIGHT_COLOR_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
normal_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( NORMAL_CMD);
  ADD_RENDER_PARM_CMD( v);
}

static void
control_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( CONTROL_CMD);
  ADD_RENDER_PARM_CMD( v);  
}

static void
alpha_function_3Dgl_collect(unsigned long v) {
  ADD_RENDER_PARM_CMD( ALPHA_FUNCTION_CMD);
  ADD_RENDER_PARM_CMD( v);  
}

static int
get_num_polygons_3Dgl_collect( void) {
  LOG_ERROR("I cannot do get_num_polygons\n");

  return 0;
}
static int
get_num_vertices_3Dgl_collect( void) {
  LOG_ERROR("I cannot do get_num_vertices\n");

  return 0;
}

static long
get_clip_matrix_3Dgl_collect(unsigned int index) {
  LOG_ERROR("I cannot do get_clip_matrix %d\n", index);

  return 0;
}

static long
get_direction_matrix_3Dgl_collect(unsigned int index) {
  LOG_ERROR("I cannot do get_direction_matrix %d\n", index);

  return 0;
}

static void
get_line_3Dgl_collect(int line, unsigned short *dst) {
  int i;
  u8 *screen3D = (u8 *)&GPU_screen3D[(192-(line%192))*256*4];

  for(i = 0; i < 256; i++)
    {
#ifdef USE_BGR_ORDER
      u32	r = screen3D[i*4+0],
        g = screen3D[i*4+1],
        b = screen3D[i*4+2],
        a = screen3D[i*4+3];
#else
      u32	r = screen3D[i*4+2],
        g = screen3D[i*4+1],
        b = screen3D[i*4+0],
        a = screen3D[i*4+3];
#endif

      r = (r*a)/255;
      g = (g*a)/255;
      b = (b*a)/255;

      if (r != 0 || g != 0 || b != 0)
        {
          dst[i] = (((r>>3)<<10) | ((g>>3)<<5) | (b>>3));
        }
    }
}


GPU3DInterface gpu3D_opengl_collector = {
  /* the Init function */
  init_3Dgl_collect,

  /* Viewport */
  viewport_3Dgl_collect,

  /* Clear colour */
  clear_colour_3Dgl_collect,

  /* Fog colour */
  fog_colour_3Dgl_collect,

  /* Fog offset */
  fog_offset_3Dgl_collect,

  /* Clear Depth */
  clear_depth_3Dgl_collect,

  /* Matrix Mode */
  matrix_mode_3Dgl_collect,

  /* Load Identity */
  load_identity_matrix_3Dgl_collect,

  /* Load 4x4 Matrix */
  load_4x4_matrix_3Dgl_collect,

  /* Load 4x3 Matrix */
  load_4x3_matrix_3Dgl_collect,

  /* Store Matrix */
  store_matrix_3Dgl_collect,

  /* Restore Matrix */
  restore_matrix_3Dgl_collect,

  /* Push Matrix */
  push_matrix_3Dgl_collect,

  /* Pop Matrix */
  pop_matrix_3Dgl_collect,

  /* Translate */
  translate_3Dgl_collect,

  /* Scale */
  scale_3Dgl_collect,

  /* Multiply Matrix 3x3 */
  multi_matrix_3x3_3Dgl_collect,

  /* Multiply Matrix 4x3 */
  multi_matrix_4x3_3Dgl_collect,

  /* Multiply Matrix 4x4 */
  multi_matrix_4x4_3Dgl_collect,

  /* Begin primitive */
  Begin_3Dgl_collect,
  /* End primitive */
  End_3Dgl_collect,

  /* Colour */
  Colour_3Dgl_collect,

  /* Vertex */
  Vertex16b_3Dgl_collect,
  Vertex10b_3Dgl_collect,
  Vertex3_cord_3Dgl_collect,
  Vertex_rel_3Dgl_collect,

  /* Swap Screen */
  SwapScreen_3Dgl_collect,

  /* Get Number of polygons */
  get_num_polygons_3Dgl_collect,

  /* Get number of vertices */
  get_num_vertices_3Dgl_collect,

  /* Flush */
  Flush_3Dgl_collect,

  /* poly attribute */
  PolyAttr_3Dgl_collect,

  /* Material 0 */
  material0_3Dgl_collect,

  /* Material 1 */
  material1_3Dgl_collect,

  /* Shininess */
  shininess_3Dgl_collect,

  /* Texture attributes */
  TextImage_3Dgl_collect,

  /* Texture palette */
  texture_palette_3Dgl_collect,

  /* Texture coordinate */
  texture_coord_3Dgl_collect,

  /* Light direction */
  light_direction_3Dgl_collect,

  /* Light colour */
  light_colour_3Dgl_collect,

  /* Alpha function */
  alpha_function_3Dgl_collect,

  /* Control */
  control_3Dgl_collect,

  /* normal */
  normal_3Dgl_collect,

  /* Call list */
  call_list_3Dgl_collect,

  /* Get clip matrix */
  get_clip_matrix_3Dgl_collect,

  /* Get direction matrix */
  get_direction_matrix_3Dgl_collect,

  /* get line */
  get_line_3Dgl_collect
};



#endif /* End of HAVE_GL_GLU_H */
#endif /* End of HAVE_GL_GL_H */
