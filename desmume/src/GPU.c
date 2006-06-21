/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

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

#include <string.h>
#include <stdlib.h>

#include "GPU.h"
#include "debug.h"

extern BOOL click;

//#define DEBUG_TRI

u16 GPU_screen[2*256*192];

short sizeTab[4][4][2] =
{
      {{256,256}, {512, 256}, {256, 512}, {512, 512}},
      {{128,128}, {256, 256}, {512, 512}, {1024, 1024}},
//      {{128,128}, {256, 256}, {512, 256}, {512, 512}},
      {{512,1024}, {1024, 512}, {0, 0}, {0, 0}},
      {{0, 0}, {0, 0}, {0, 0}, {0, 0}}
};

size sprSizeTab[4][4] = 
{
     {{8, 8}, {16, 8}, {8, 16}, {8, 8}},
     {{16, 16}, {32, 8}, {8, 32}, {8, 8}},
     {{32, 32}, {32, 16}, {16, 32}, {8, 8}},
     {{64, 64}, {64, 32}, {32, 64}, {8, 8}},
};

s8 mode2type[8][4] = 
{
      {0, 0, 0, 0},
      {0, 0, 0, 1},
      {0, 0, 1, 1},
      {0, 0, 0, 1},
      {0, 0, 1, 1},
      {0, 0, 1, 1},
      {3, 3, 2, 3},
      {0, 0, 0, 0}
};

void lineText(GPU * gpu, u8 num, u16 l, u16 * DST);
void lineRot(GPU * gpu, u8 num, u16 l, u16 * DST);
void lineExtRot(GPU * gpu, u8 num, u16 l, u16 * DST);

void (*modeRender[8][4])(GPU * gpu, u8 num, u16 l, u16 * DST)=
{
     {lineText, lineText, lineText, lineText},     //0
     {lineText, lineText, lineText, lineRot},      //1
     {lineText, lineText, lineRot, lineRot},       //2
     {lineText, lineText, lineText, lineExtRot},   //3
     {lineText, lineText, lineRot, lineExtRot},    //4
     {lineText, lineText, lineExtRot, lineExtRot}, //5
     {lineText, lineText, lineText, lineText},     //6
     {lineText, lineText, lineText, lineText},     //7
};

#if 0
GPU::GPU(u8 l) : lcd(l)
{
     prop = 0;
     BGProp[0] = BGProp[1] = BGProp[2] = BGProp[3] = 0;
     BGBmpBB[0] = 0;BGBmpBB[1] = 0;BGBmpBB[2] = 0;BGBmpBB[3] = 0;
     BGChBB[0] = 0;BGChBB[1] = 0;BGProp[2] = 0;BGChBB[3] = 0;
     BGScrBB[0] = 0;BGScrBB[1] = 0;BGScrBB[2] = 0;BGScrBB[3] = 0;
     BGExtPalSlot[0] = BGExtPalSlot[1] = BGExtPalSlot[2] = BGExtPalSlot[3] = 0;
     BGSize[0][0] = BGSize[1][0] = BGSize[2][0] = BGSize[3][0] = 256;
     BGSize[0][1] = BGSize[1][1] = BGSize[2][1] = BGSize[3][1] = 256;
     BGSX[0] = BGSX[1] = BGSX[2] = BGSX[3] = 0;
     BGSY[0] = BGSY[1] = BGSY[2] = BGSY[3] = 0;
     BGX[0] = BGX[1] = BGX[2] = BGX[3] = 0;
     BGY[0] = BGY[1] = BGY[2] = BGY[3] = 0;
     BGPA[0] = BGPA[1] = BGPA[2] = BGPA[3] = 0;
     BGPB[0] = BGPB[1] = BGPB[2] = BGPB[3] = 0;
     BGPC[0] = BGPC[1] = BGPC[2] = BGPC[3] = 0;
     BGPD[0] = BGPD[1] = BGPD[2] = BGPD[3] = 0;
     BGIndex[0] = BGIndex[1] = BGIndex[2] = BGIndex[3] = 0; 
     dispBG[0] = dispBG[1] = dispBG[2] = dispBG[3] = TRUE;
     nbBGActif = 0;
     
     spriteRender = sprite1D;
     
     if(lcd)
     {
          oam = (OAM *)(ARM9.ARM9_OAM+0x400);
          sprMem = ARM9.ARM9_BOBJ;
     }
     else
     {
          oam = (OAM *)(ARM9.ARM9_OAM);
          sprMem = ARM9.ARM9_AOBJ;
     }
}
#endif
 
GPU * GPUInit(u8 l)
{
     GPU * g;

     g = (GPU *) malloc(sizeof(GPU));
     memset(g, 0, sizeof(GPU));

     g->lcd = l;
     g->BGSize[0][0] = g->BGSize[1][0] = g->BGSize[2][0] = g->BGSize[3][0] = 256;
     g->BGSize[0][1] = g->BGSize[1][1] = g->BGSize[2][1] = g->BGSize[3][1] = 256;
     g->dispBG[0] = g->dispBG[1] = g->dispBG[2] = g->dispBG[3] = TRUE;
     
     g->spriteRender = sprite1D;
     
     if(g->lcd)
     {
          g->oam = (OAM *)(ARM9.ARM9_OAM+0x400);
          g->sprMem = ARM9.ARM9_BOBJ;
     }
     else
     {
          g->oam = (OAM *)(ARM9.ARM9_OAM);
          g->sprMem = ARM9.ARM9_AOBJ;
     }

     return g;
}

void GPUDeInit(GPU * gpu) {
     free(gpu);
}

/*void GPU::ligne(u16 * buffer, u16 l)
{
     u16 * dst =  buffer + l*256;
     u16 spr[256];
     u8 sprPrio[256];
     u8 bgprio;
     
     u32 c = ((u16 *)ARM9.ARM9_VMEM)[0+lcd*0x200];
     c |= (c<<16);
     
     for(u8 i = 0; i< 128; ++i)
     {
          ((u32 *)dst)[i] = c;
          ((u32 *)spr)[i] = c;
          ((u16 *)sprPrio)[i] = (4<<8) | (4);
     }
     
     if(!nbBGActif)
     {
          spriteRender(this, l, dst, sprPrio);
          return;
     }
     
     spriteRender(this, l, spr, sprPrio);
     
     if((BGProp[ordre[0]]&3)!=3)
     {
          for(u16 i = 0; i < 128; ++i)
               ((u32 *)dst)[i] = ((u32 *)spr)[i];
     }
     
     for(u8 i = 0; i < nbBGActif; ++i)
     {
          modeRender[prop&7][ordre[i]](this, ordre[i], l, dst);
          bgprio = BGProp[ordre[i]]&3;
          for(u16 i = 0; i < 256; ++i)
               if(bgprio>=sprPrio[i]) 
                    dst[i] = spr[i];
     }
}
*/

void GPU_setVideoProp(GPU * gpu, u32 p)
{
     gpu->prop = p;
     
     gpu->nbBGActif = 0;
     if(p&(1<<4))
     {
          gpu->sprBlock = 5 + ((p>>20)&3);
          if((gpu->lcd)&&(((p>>20)&3)==3))
               gpu->sprBlock = 7;
     }     
     else
          gpu->sprBlock = 5;
     
     if((p&(1<<22))&&(!gpu->lcd))
          gpu->sprBMPBlock = 8;
     else
          gpu->sprBMPBlock = 7;
     
     GPU_setBGProp(gpu, 3, ((u16 *)ARM9.ARM9_REG)[gpu->lcd*0x800+7]);
     GPU_setBGProp(gpu, 2, ((u16 *)ARM9.ARM9_REG)[gpu->lcd*0x800+6]);
     GPU_setBGProp(gpu, 1, ((u16 *)ARM9.ARM9_REG)[gpu->lcd*0x800+5]);
     GPU_setBGProp(gpu, 0, ((u16 *)ARM9.ARM9_REG)[gpu->lcd*0x800+4]);
     
     if(p&(1<<4))
          gpu->spriteRender = sprite1D;
     else
          gpu->spriteRender = sprite2D;
          
     if((p&(1<<11))&&gpu->dispBG[3])
     {
          gpu->ordre[0] = 3;
          gpu->BGIndex[3] = 1;
          ++gpu->nbBGActif;
     }
     else
          gpu->BGIndex[3] = 0;
     
     if((p&(1<<10))&&gpu->dispBG[2])
     {
          if(gpu->nbBGActif)
               if((gpu->BGProp[2]&3)>(gpu->BGProp[3]&3))
               {
                    gpu->ordre[0] = 2;
                    gpu->BGIndex[2] = 1;
                    gpu->ordre[1] = 3;
                    gpu->BGIndex[3] = 2;
               }
               else
               {
                    gpu->ordre[1] = 2;
                    gpu->BGIndex[2] = 2;
               }     
          else
          {
               gpu->ordre[0] = 2;
               gpu->BGIndex[2] = 1;
          }
          ++gpu->nbBGActif;
     }
     else
          gpu->BGIndex[2] = 0;
     
     if((p&(1<<9))&&gpu->dispBG[1])
     {
          if(!gpu->nbBGActif)
          {
               gpu->ordre[0] = 1;
               gpu->BGIndex[1] = 1;
          }
          else
          {
               u8 i = 0;
	       s8 j;
               for(; (i < gpu->nbBGActif) && ((gpu->BGProp[gpu->ordre[i]]&3)>=(gpu->BGProp[1]&3)); ++i);
               for(j = gpu->nbBGActif-1; j >= i; --j)
               {
                    gpu->ordre[j+1] = gpu->ordre[j];
                    ++gpu->BGIndex[gpu->ordre[j]];
               }
               gpu->ordre[i] = 1;
               gpu->BGIndex[1] = i+1;
          }
          ++gpu->nbBGActif;
     }
     else
          gpu->BGIndex[1] = 0;
     
     if((p&(1<<8))&&(!(p&(1<<3)))&&gpu->dispBG[0])
     {
          if(!gpu->nbBGActif)
          {
               gpu->ordre[0] = 0;
               gpu->BGIndex[0] = 1;
          }
          else
          {
               u8 i = 0;
	       s8 j;
               for(; (i < gpu->nbBGActif) && ((gpu->BGProp[gpu->ordre[i]]&3)>=(gpu->BGProp[0]&3)); ++i);
               for(j = gpu->nbBGActif-1; j >= i; --j)
               {
                    gpu->ordre[j+1] = gpu->ordre[j];
                    ++gpu->BGIndex[gpu->ordre[j]];
               }
               gpu->ordre[i] = 0;
               gpu->BGIndex[0] = i+1;
          }
          ++gpu->nbBGActif;
     }
     else
          gpu->BGIndex[0] = 0;
     
     #ifdef DEBUG_TRI
     log::ajouter("------------------");
     for(u8 i = 0; i < gpu->nbBGActif; ++i)
     {
          sprintf(logbuf, "bg %d prio %d", gpu->ordre[i], gpu->BGProp[gpu->ordre[i]]&3);
          log::ajouter(logbuf);
     }
     log::ajouter("_________________");
     #endif
}

void GPU_setBGProp(GPU * gpu, u16 num, u16 p)
{
     u8 index;
     if((gpu->nbBGActif)&&(index = gpu->BGIndex[num]))
     {
          --index;
          if((gpu->BGProp[num]&3)<(p&3))
          {
               #ifdef DEBUG_TRI
               sprintf(logbuf, "INF NEW bg %d prio %d %d", num, p&3, index);
               log::ajouter(logbuf);
               for(u8 i = 0; i < gpu->nbBGActif; ++i)
               {
                    sprintf(logbuf, "bg %d prio %d", gpu->ordre[i], gpu->BGProp[gpu->ordre[i]]&3);
                    log::ajouter(logbuf);
               }
               #endif
               u8 i = 0;
               for(; (i<index) && (((gpu->BGProp[gpu->ordre[i]]&3)>(p&3)) || (((gpu->BGProp[gpu->ordre[i]]&3)==(p&3))&&(gpu->ordre[i]>num))); ++i);
               #ifdef DEBUG_TRI
               sprintf(logbuf, "new i %d old %d", i, index);
               log::ajouter(logbuf);
               #endif
               if(i!=index)
               {
		    s8 j;
                    for(j = index-1; j>=i; --j)
                    {
                         gpu->ordre[j+1] = gpu->ordre[j];
                         ++gpu->BGIndex[gpu->ordre[j]];
                    }
                    gpu->ordre[i] = num;
                    gpu->BGIndex[num] = i + 1;
               }
               #ifdef DEBUG_TRI
               log::ajouter("");
               for(u8 i = 0; i < gpu->nbBGActif; ++i)
               {
                    sprintf(logbuf, "bg %d prio %d", gpu->ordre[i], gpu->BGProp[gpu->ordre[i]]&3);
                    log::ajouter(logbuf);
               }
               log::ajouter("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
               #endif
          }
          else
          if((gpu->BGProp[num]&3)>(p&3))
          {
               #ifdef DEBUG_TRI
               sprintf(logbuf, "SUP NEW bg %d prio %d", num, p&3);
               log::ajouter(logbuf);
               for(u8 i = 0; i < gpu->nbBGActif; ++i)
               {
                    sprintf(logbuf, "bg %d prio %d", gpu->ordre[i], gpu->BGProp[gpu->ordre[i]]&3);
                    log::ajouter(logbuf);
               }
               #endif
               u8 i = gpu->nbBGActif-1;
               for(; (i>index) && (((gpu->BGProp[gpu->ordre[i]]&3)<(p&3)) || (((gpu->BGProp[gpu->ordre[i]]&3)==(p&3))&&(gpu->ordre[i]<num))); --i);
               #ifdef DEBUG_TRI
               sprintf(logbuf, "new i %d old %d", i, index);
               log::ajouter(logbuf);
               #endif
               if(i!=index)
               {
		    s8 j;
                    for(j = index; j<i; ++j)
                    {
                         gpu->ordre[j] = gpu->ordre[j+1];
                         --gpu->BGIndex[gpu->ordre[j]];
                    }
                    gpu->ordre[i] = num;
                    gpu->BGIndex[num] = i + 1;
               }
               #ifdef DEBUG_TRI
               log::ajouter("");
               for(u8 i = 0; i < gpu->nbBGActif; ++i)
               {
                    sprintf(logbuf, "bg %d prio %d", gpu->ordre[i], gpu->BGProp[gpu->ordre[i]]&3);
                    log::ajouter(logbuf);
               }
               log::ajouter("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
               #endif
          }
     }
     gpu->BGProp[num] = p;
     if(gpu->lcd)
     {
          gpu->BGBmpBB[num] = ((u8 *)ARM9.ARM9_BBG) + ((p>>8)&0x1F)*0x4000;
          gpu->BGChBB[num] = ((u8 *)ARM9.ARM9_BBG) + ((p>>2)&0xF)*0x4000;
          gpu->BGScrBB[num] = ((u16 *)ARM9.ARM9_BBG) + 0x400*((p>>8)&0x1F);
     }
     else
     {
          gpu->BGBmpBB[num] = ((u8 *)ARM9.ARM9_ABG) + ((p>>8)&0x1F)*0x4000;
          gpu->BGChBB[num] = ((u8 *)ARM9.ARM9_ABG) + ((p>>2)&0xF)*0x4000 + ((gpu->prop>>24)&7)*0x10000;
          gpu->BGScrBB[num] = ((u16 *)ARM9.ARM9_ABG) + 0x400*((p>>8)&0x1F) + ((gpu->prop>>27)&7)*0x8000;
     }
     
     /*if(!(p&(1<<7)))
          BGExtPalSlot[num] = 0;
     else
          if(!(prop&(1<<30)))
               BGExtPalSlot[num] = 0;
          else*/
               switch(num)
               {
                    case 0 :
                         gpu->BGExtPalSlot[num] = (p&(1<<13))?2:0;
                         break;
                    case 1 :
                         gpu->BGExtPalSlot[num] = (p&(1<<13))?3:1;
                         break;
                    default :
                            gpu->BGExtPalSlot[num] = num;
                            break;
                  }
                  
     /*if(!(prop&(3<<16)))
     {
          BGSize[num][0] = lcdSizeTab[p>>14][0];
          BGSize[num][1] = lcdSizeTab[p>>14][1];
          return;
     }*/
     gpu->BGSize[num][0] = sizeTab[mode2type[gpu->prop&7][num]][p>>14][0];
     gpu->BGSize[num][1] = sizeTab[mode2type[gpu->prop&7][num]][p>>14][1];
}

void GPU_remove(GPU * gpu, u8 num)
{
     u8 index;
     //!!!!AJOUTER UN CRITICAL SECTION
     if(index = gpu->BGIndex[num])
     {
	  u8 i;
          --index;
          --gpu->nbBGActif;
          for(i = index; i < gpu->nbBGActif; ++i)
          {
               gpu->ordre[i] = gpu->ordre[i+1];
               --gpu->BGIndex[gpu->ordre[i]];
          }
          gpu->BGIndex[num] = 0;
     }
     gpu->dispBG[num] = !gpu->dispBG[num];
}

void GPU_addBack(GPU * gpu, u8 num)
{
     if((!gpu->BGIndex[num])&&(gpu->prop&((1<<8)<<num)))
     {
          u8 i = 0;
	  s8 j;
          u8 p = gpu->BGProp[num]&3;
          for(; (i<gpu->nbBGActif) && (((gpu->BGProp[gpu->ordre[i]]&3)>p) || (((gpu->BGProp[gpu->ordre[i]]&3)==p)&&(gpu->ordre[i]>num))); ++i);
          for(j = gpu->nbBGActif-1; j >= i; --j)
          {
               gpu->ordre[j+1] = gpu->ordre[j];
               ++gpu->BGIndex[gpu->ordre[j]];
          }
          gpu->ordre[i] = num;
          gpu->BGIndex[num] = i+1;
          ++gpu->nbBGActif;
     }
     gpu->dispBG[num] = !gpu->dispBG[num];
}

void GPU_scrollX(GPU * gpu, u8 num, u16 v)
{
     gpu->BGSX[num] = v;
}

void GPU_scrollY(GPU * gpu, u8 num, u16 v)
{
     gpu->BGSY[num] = v;
}

void GPU_scrollXY(GPU * gpu, u8 num, u32 v)
{
     gpu->BGSX[num] = (v & 0xFFFF);
     gpu->BGSY[num] = (v >> 16);
}

void GPU_setX(GPU * gpu, u8 num, u32 v)
{
     gpu->BGX[num] = (((s32)(v<<4))>>4);
     /*sprintf(logbuf, "setX %08X", BGX[num]);
     log::ajouter(logbuf);*/
}

void GPU_setXH(GPU * gpu, u8 num, u16 v)
{
     gpu->BGX[num] = (((s32)((s16)(v<<4)))<<12) | (gpu->BGX[num]&0xFFFF);
}

void GPU_setXL(GPU * gpu, u8 num, u16 v)
{
     gpu->BGX[num] = (gpu->BGX[num]&0xFFFF0000) | v;
}

void GPU_setY(GPU * gpu, u8 num, u32 v)
{
     gpu->BGY[num] = (((s32)(v<<4))>>4);
     /*sprintf(logbuf, "setY %08X", BGY[num]);
     log::ajouter(logbuf);*/
}

void GPU_setYH(GPU * gpu, u8 num, u16 v)
{
     gpu->BGY[num] = (((s32)((s16)(v<<4)))<<12) | (gpu->BGY[num]&0xFFFF);
}

void GPU_setYL(GPU * gpu, u8 num, u16 v)
{
     gpu->BGY[num] = (gpu->BGY[num]&0xFFFF0000) | v;
}

void GPU_setPA(GPU * gpu, u8 num, u16 v)
{
     gpu->BGPA[num] = (s32)v;
     /*sprintf(logbuf, "PA %04X", BGPA[num]);
     log::ajouter(logbuf);*/
}

void GPU_setPB(GPU * gpu, u8 num, u16 v)
{
     gpu->BGPB[num] = (s32)v;
     /*sprintf(logbuf, "PB %04X", BGPB[num]);
     log::ajouter(logbuf);*/
}

void GPU_setPC(GPU * gpu, u8 num, u16 v)
{
     gpu->BGPC[num] = (s32)v;
     /*sprintf(logbuf, "PC %04X", BGPC[num]);
     log::ajouter(logbuf);*/
}

void GPU_setPD(GPU * gpu, u8 num, u16 v)
{
     gpu->BGPD[num] = (s32)v;
     /*sprintf(logbuf, "PD %04X", BGPD[num]);
     log::ajouter(logbuf);*/
}
       
void GPU_setPAPB(GPU * gpu, u8 num, u32 v)
{
     gpu->BGPA[num] = (s16)v;
     gpu->BGPB[num] = (s16)(v>>16);
}

void GPU_setPCPD(GPU * gpu, u8 num, u32 v)
{
     gpu->BGPC[num] = (s16)v;
     gpu->BGPD[num] = (s16)(v>>16);     
}

INLINE void textBG2(GPU * gpu, u8 num, u16 * DST, u16 X, u16 Y, u16 LG)
{
     u32 bgprop = gpu->BGProp[num];
     u16 lg = gpu->BGSize[num][0];
     u16 ht = gpu->BGSize[num][1];
     u16 tmp = ((Y&(ht-1))>>3);
     u16 * map = gpu->BGScrBB[num] + (tmp&31) * 32;
     u16 * dst = DST;
     
     if(tmp>31)
          switch(bgprop>>14)
          {
               case 2 :
                    map += 32*32;
                    break;
               case 3 :
                    map += 32*64;
                    break;
          }
     u8 * tile = (u8 * )gpu->BGChBB[num];
     
     if((!tile)||(!gpu->BGScrBB[num])) return;
     
     u16 xoff = X;

     if(!(bgprop&(1<<7)))
     {
          u16 * pal = ((u16 *)ARM9.ARM9_VMEM) + gpu->lcd*0x200;
          u16 yoff = ((Y&7)<<2);
	  u16 x;
          
          /*if(xoff&1)
          {
               tmp = ((xoff&(lg-1))>>3);
               u16 * mapinfo = map + (tmp&0x1F);
               if(tmp>31)
                    mapinfo += 32*32;
               u8 * ligne = (u8 * )tile + (((*mapinfo)&0x3FF)*0x20) + (((*mapinfo)& 0x800 ? (7*4)-yoff : yoff));
               if((*mapinfo)& 0x800)
                    *dst = pal[*(ligne+3-(((xoff&7)>>1)&0xF)) + ((*mapinfo>>12)&0xF)*0x10];
               else
                    *dst = pal[*(ligne+ (((xoff&7)>>1)>>4)) + ((*mapinfo>>12)&0xF)*0x10];
               
               tmp = (((xoff+LG)&lg-1)>>3);
               mapinfo = map + (tmp&0x1F);
               if(tmp>31)
                    mapinfo += 32*32;
               ligne = tile + (((*mapinfo)&0x3FF)*0x20) + (((*mapinfo)& 0x800 ? (7*4)-yoff : yoff));
               if((*mapinfo)& 0x800)
                    *dst = pal[*(ligne+3-(((xoff&7)>>1)>>4)) + ((*mapinfo>>12)&0xF)*0x10];
               else
                    *dst = pal[*(ligne+ (((xoff&7)>>1)&0xF)) + ((*mapinfo>>12)&0xF)*0x10];
               
               xoff+=1;
               LG -=2;
          }*/
          
          for(x = 0; x < LG;)
          {
               tmp = ((xoff&(lg-1))>>3);
               u16 * mapinfo = map + (tmp&0x1F);
               if(tmp>31)
                    mapinfo += 32*32;
               u8 * ligne = (u8 * )tile + (((*mapinfo)&0x3FF)*0x20) + (((*mapinfo)& 0x800 ? (7*4)-yoff : yoff));
               u16 xfin = x + (8 - (xoff&7));
               
               if((*mapinfo)& 0x400)
               {
                    ligne += 3 - ((xoff&7)>>1);
                    for(; x < xfin; )
                    {
                         if((*ligne)>>4)
                         *dst = pal[((*ligne)>>4) + ((*mapinfo>>12)&0xF)*0x10];
                         //else *dst = 0x7FFF;
                         ++dst;++x, ++xoff;
                         if((*ligne)&0xF)
                         *dst = pal[((*ligne)&0xF) + ((*mapinfo>>12)&0xF)*0x10];
                         //else *dst = 0x7FFF;
                         ++dst;--ligne;
                         ++x, ++xoff;
                    }
               }
               else
               {
                   ligne += ((xoff&7)>>1);
                   for(; x < xfin; )
                   {
                         if((*ligne)&0xF)
                        *dst = pal[((*ligne)&0xF) + ((*mapinfo>>12)&0xF)*0x10];
                         //else *dst = 0x7FFF;
                        ++dst;++x, ++xoff;
                         if((*ligne)>>4)
                        *dst = pal[((*ligne)>>4) + ((*mapinfo>>12)&0xF)*0x10];
                         //else *dst = 0x7FFF;
                        ++dst;++ligne;
                        ++x, ++xoff;
                   }
               }
          }
          return;
     }
     
     if(!(gpu->prop&(1<<30)))
     {
          u16 yoff = ((Y&7)<<3);
          u16 * pal = ((u16 *)ARM9.ARM9_VMEM) + gpu->lcd*0x200;
	  u16 x;
          
          for(x = 0; x < LG;)
          {
               tmp = ((xoff&(lg-1))>>3);
               u16 * mapinfo = map + (tmp&31);
               if(tmp>31)
                    mapinfo += 32*32;
               u8 * ligne = (u8 * )tile + (((*mapinfo)&0x3FF)*0x40) + (((*mapinfo)& 0x800 ? (7*8)-yoff : yoff));
               u16 xfin = x + (8 - (xoff&7));
               
               if((*mapinfo)& 0x400)
               {
                    ligne += (7 - (xoff&7));
                    for(; x < xfin; ++x, ++xoff)
                    {
                         if(*ligne)
                         *dst = pal[*ligne];
                         //else *dst = 0x7FFF;
                         ++dst;--ligne;
                    }
               }
               else
               {
                    ligne += (xoff&7);
                    for(; x < xfin; ++x, ++xoff)
                    {
                         if(*ligne)
                         *dst = pal[*ligne];
                         //else *dst = 0x7FFF;
                         ++dst;++ligne;
                    }
               }
          }
          return;
     }
          u16 * pal = ((u16 *)ARM9.ExtPal[gpu->lcd][gpu->BGExtPalSlot[num]]);
          
          if(!pal) return;
               
          u16 yoff = ((Y&7)<<3);
	  u16 x;
          
          for(x = 0; x < LG;)
          {
               tmp = ((xoff&(lg-1))>>3);
               u16 * mapinfo = (u16 *)map + (tmp&0x1F);
               if(tmp>31)
                    mapinfo += 32*32;
               u8 * ligne = (u8 * )tile + (((*mapinfo)&0x3FF)*0x40) + (((*mapinfo)& 0x800 ? (7*8)-yoff : yoff));
               u16 xfin = x + (8 - (xoff&7));
               
               if((*mapinfo)& 0x400)
               {
                    ligne += (7 - (xoff&7));
                    for(; x < xfin; ++x, ++xoff)
                    {
                         if(*ligne)
                              *dst = pal[*ligne + ((*mapinfo>>12)&0xF)*0x100];
                         //else *dst = 0x7FFF;
                         ++dst;--ligne;
                    }
               }
               else
               {
                    ligne += (xoff&7);
                    for(; x < xfin; ++x, ++xoff)
                    {
                         if(*ligne)
                              *dst = pal[*ligne + ((*mapinfo>>12)&0xF)*0x100];
                         //else *dst = 0x7FFF;
                         ++dst;++ligne;
                    }
               }
          }
}

INLINE void rotBG2(GPU * gpu, u8 num, u16 * DST, u16 H, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, u16 LG)
{
     u32 bgprop = gpu->BGProp[num];
     
     s32 x = X + (s32)PB*(s32)H;
     s32 y = Y + (s32)PD*(s32)H;
     
     
     s32 dx = (s32)PA;
     s32 dy = (s32)PC;
     
     s32 auxX;
     s32 auxY;
     
     s32 lg = gpu->BGSize[num][0];
     s32 ht = gpu->BGSize[num][1];
     s32 lgmap = (lg>>3);
     
     u8 * map = (u8 *)gpu->BGScrBB[num];
     u8 * tile = (u8 *)gpu->BGChBB[num];
     u16 * dst = DST;
     u8 mapinfo;
     u8 coul;
     
     if((!tile)||(!map)) return;
     u16 * pal = ((u16 *)ARM9.ARM9_VMEM) + gpu->lcd*0x200;
     u32 i;
     for(i = 0; i < LG; ++i)
     {
          auxX = x>>8;
          auxY = y>>8;
          
          if(bgprop&(1<<13))
          {
               auxX &= (lg-1);
               auxY &= (ht-1);
          }
          
          if ((auxX >= 0) && (auxX < lg) && (auxY >= 0) && (auxY < ht))
          {
               mapinfo = map[(auxX>>3) + ((auxY>>3) * lgmap)];
               coul = tile[mapinfo*64 + ((auxY&7)<<3) + (auxX&7)];
               if(coul)
                    *dst = pal[coul];
          }
          ++dst;
          x += dx;
          y += dy;
     }
}

INLINE void extRotBG2(GPU * gpu, u8 num, u16 * DST, u16 H, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, s16 LG)
{
     u32 bgprop = gpu->BGProp[num];
     
     s32 x = X + (s32)PB*(s32)H;
     s32 y = Y + (s32)PD*(s32)H;
     
     s32 dx = PA;
     s32 dy = PC;
     
     s32 auxX;
     s32 auxY;
     
     s16 lg = gpu->BGSize[num][0];
     s16 ht = gpu->BGSize[num][1];
     u16 lgmap = (lg>>3);
     
     u8 * tile = (u8 *)gpu->BGChBB[num];
     u16 * dst = DST;
     u16 mapinfo;
     u8 coul;

     switch(((bgprop>>2)&1)|((bgprop>>6)&2))
     {
          case 0 :
          case 1 :
               {
               u16 * map = gpu->BGScrBB[num];
               u16 * pal = ((u16 *)ARM9.ExtPal[gpu->lcd][gpu->BGExtPalSlot[num]]);
	       u16 i;
               if(!pal) return;
               for(i = 0; i < LG; ++i)
               {
                    auxX = x>>8;
                    auxY = y>>8;
                    
                    if(bgprop&(1<<13))
                    {
                         auxX &= (lg-1);
                         auxY &= (ht-1);
                    }
                    if ((auxX >= 0) && (auxX < lg) && (auxY >= 0) && (auxY < ht))
                    {
                         mapinfo = map[(auxX>>3) + (auxY>>3) * lgmap];
                         u16 x1 = (mapinfo & 0x400) ? 7 - (auxX&7) : (auxX&7);
                         u16 y1 = (mapinfo & 0x800) ? 7 - (auxY&7) : (auxY&7);
                         coul = tile[(mapinfo&0x3FF)*64 + x1 + (y1<<3)];
                         if(coul)
                              *dst = pal[coul + (mapinfo>>12)*0x100];
                         //else *dst = 0x7FFF;                    
                    }
                    //else *dst = 0x7FFF;
                    ++dst;
                    x += dx;
                    y += dy;
               }
               }
               return;
          case 2 :
               {
               u8 * map = (u8 *)gpu->BGBmpBB[num];
               u16 * pal = ((u16 *)ARM9.ARM9_VMEM) + gpu->lcd*0x200;
	       u16 i;
               for(i = 0; i < LG; ++i)
               {
                    auxX = x>>8;
                    auxY = y>>8;
                    if(bgprop&(1<<13))
                    {
                         auxX &= (lg-1);
                         auxY &= (ht-1);
                    }
                    if ((auxX >= 0) && (auxX < lg) && (auxY >= 0) && (auxY < ht))
                    {
                         mapinfo = map[auxX + auxY * lg];
                         if(mapinfo)
                              *dst = pal[mapinfo];
                         //else *dst = 0x7FFF;
                    }
                    //else *dst = 0x7FFF;
                    ++dst;
                    x += dx;
                    y += dy;
               }
               }
               return;
          case 3 :
               {
               u16 * map = (u16 *)gpu->BGBmpBB[num];
	       u16 i;
               for(i = 0; i < LG; ++i)
               {
                    auxX = x>>8;
                    auxY = y>>8;
                    if(bgprop&(1<<13))
                    {
                         auxX &= (lg-1);
                         auxY &= (ht-1);
                    }
                    if ((auxX >= 0) && (auxX < lg) && (auxY >= 0) && (auxY < ht))
                    {
                         mapinfo = map[auxX + auxY * lg];
                         if(mapinfo)
                              *dst = mapinfo;
                         //else *dst = 0x7FFF;
                    }
                    //else *dst = 0x7FFF;
                    ++dst;
                    x += dx;
                    y += dy;
               }
               }
               return;
     }
}

void lineText(GPU * gpu, u8 num, u16 l, u16 * DST)
{
          textBG2(gpu, num, DST, gpu->BGSX[num], l + gpu->BGSY[num], 256);
}

void lineRot(GPU * gpu, u8 num, u16 l, u16 * DST)
{
     rotBG2(gpu, num, DST, l, 
              gpu->BGX[num],
              gpu->BGY[num],
              gpu->BGPA[num],
              gpu->BGPB[num],
              gpu->BGPC[num],
              gpu->BGPD[num],
              256);
}

void lineExtRot(GPU * gpu, u8 num, u16 l, u16 * DST)
{
     extRotBG2(gpu, num, DST, l, 
              gpu->BGX[num],
              gpu->BGY[num],
              gpu->BGPA[num],
              gpu->BGPB[num],
              gpu->BGPC[num],
              gpu->BGPD[num],
              256);
}

void textBG(GPU * gpu, u8 num, u16 * DST)
{
     u32 i;
     for(i = 0; i < gpu->BGSize[num][1]; ++i)
          textBG2(gpu, num, DST + i*gpu->BGSize[num][0], 0, i, gpu->BGSize[num][0]);
}

void rotBG(GPU * gpu, u8 num, u16 * DST)
{
     u32 i;
     for(i = 0; i < gpu->BGSize[num][1]; ++i)
          rotBG2(gpu, num, DST + i*gpu->BGSize[num][0], i, 0, 0, 256, 0, 0, 256, gpu->BGSize[num][0]);
}

void extRotBG(GPU * gpu, u8 num, u16 * DST)
{
     u32 i;
     for(i = 0; i < gpu->BGSize[num][1]; ++i)
          extRotBG2(gpu, num, DST + i*gpu->BGSize[num][0], i, 0, 0, 256, 0, 0, 256, gpu->BGSize[num][0]);
}

#define nbShow 128
               /*u16 * src = (u16 *)(gpu->sprMem + ((aux->attr2&0x3FF)<<gpu->sprBMPBlock) + (y*sprSize.x));
               if(aux->attr1&(1<<12))
               {
                    x = sprSize.x -x - 1;
                    for(u16 i = 0; i < lg; ++i, --x, ++sprX)
                    {
                         u8 c = src[x];
                         if((c>>15) && (prioTab[sprX]>=prio))
                         {
                              dst[sprX] = c;
                              prioTab[sprX] = prio;
                         }
                         else dst[sprX] = 0x7FFF;
                    }
                    continue;
               }
               for(u16 i = 0; i < lg; ++i, ++x, ++sprX)
               {
                    u16 c = src[x];
                    if((c>>15) && (prioTab[sprX]>=prio))
                     {
                          dst[sprX] = c;
                          prioTab[sprX] = prio;
                     }
                     else dst[sprX] = 0x7FFF;
               }
               continue;*/
/*(aux->attr2&0x3E0)*64 + (aux->attr2&0x1F)*8*/

void sprite1D(GPU * gpu, u16 l, u16 * dst, u8 * prioTab)
{
     OAM * aux = gpu->oam + (nbShow-1);// + 127;
     
     u8 block = gpu->sprBlock;
     u16 i;
     
     for(i = 0; i<nbShow; ++i, --aux)
     {
          s32 sprX = aux->attr1 & 0x1FF;
          sprX = ((s32)(sprX<<23))>>23;
          s32 sprY = aux->attr0 & 0xFF;
          s32 x = 0;
          
          size sprSize = sprSizeTab[(aux->attr1>>14)][(aux->attr0>>14)];
          
          u32 lg = sprSize.x;
          
          if(sprY>192)
               sprY = (s32)((s8)(aux->attr0 & 0xFF));

          if( ((aux->attr0&(1<<9))&&(!(aux->attr0&(1<<8)))) ||
              (l<sprY)||(l>=sprY+sprSize.y) ||
              (sprX==256) )
              continue;
          
          if(sprX<0)
          {
               if(sprX+sprSize.x<=0) continue;
               lg += sprX;
               x = -sprX;
               sprX = 0;
          }
          else
               if(sprX+sprSize.x>256)
                    lg = 255 - sprX;
               
          s32 y = l - sprY;
          u8 prio = (aux->attr2>>10)&3;
          
          if(aux->attr1&(1<<13)) y = sprSize.y - y -1;
          
          if((aux->attr0&(3<<10))==(3<<10))
          {
               u16 * src = (u16 *)(gpu->sprMem) +(aux->attr2&0x3FF)*16 + (y<<gpu->sprBMPBlock);
	       u16 i;
               if(aux->attr1&(1<<12))
               {
                    x = sprSize.x -x - 1;
                    for(i = 0; i < lg; ++i, --x, ++sprX)
                    {
                         u8 c = src[x];
                         if((c>>15) && (prioTab[sprX]>=prio))
                         {
                              dst[sprX] = c;
                              prioTab[sprX] = prio;
                         }
                         //else dst[sprX] = 0x7FFF;
                    }
                    continue;
               }
               for(i = 0; i < lg; ++i, ++x, ++sprX)
               {
                    u16 c = src[x];
                    if((c>>15) && (prioTab[sprX]>=prio))
                     {
                          dst[sprX] = c;
                          prioTab[sprX] = prio;
                     }
                     //else dst[sprX] = 0x7FFF;
               }//
               continue;
          }
          
          if(aux->attr0&(1<<13))
          {
               u8 * src = gpu->sprMem + ((aux->attr2&0x3FF)<<block) + ((y>>3)*sprSize.x*8) + ((y&0x7)*8);
               u16 * pal;
	       u16 i;
               
               if(gpu->prop&(1<<31))
                    pal = (u16 *)ARM9.ObjExtPal[gpu->lcd][0]+((aux->attr2>>12)*0x100);
               else
                    pal = ((u16 *)(ARM9.ARM9_VMEM +0x200)) + gpu->lcd*0x200;
               
               if(aux->attr1&(1<<12))
               {
                    x = sprSize.x -x - 1;
                    for(i = 0; i < lg; ++i, --x, ++sprX)
                    {
                         u8 c = src[(x&0x7) + ((x&0xFFF8)<<3)];
                         if((c) && (prioTab[sprX]>=prio))
                         {
                              dst[sprX] = pal[c];
                              prioTab[sprX] = prio;
                         }
                         //else dst[sprX] = 0x7FFF;
                    }
                    continue;
               }
               
               for(i = 0; i < lg; ++i, ++x, ++sprX)
               {
                     u8 c = src[(x&0x7) + ((x&0xFFF8)<<3)];
                     
                     if((c) && (prioTab[sprX]>=prio))
                     {
                          dst[sprX] = pal[c];
                          prioTab[sprX] = prio;
                     }
                     //else dst[sprX] = 0x7FFF;
               }  
               continue;
          }
          u8 * src = gpu->sprMem + ((aux->attr2&0x3FF)<<block) + ((y>>3)*sprSize.x*4) + ((y&0x7)*4);
          u16 * pal = ((u16 *)(ARM9.ARM9_VMEM +0x200)) + gpu->lcd*0x200;
          if(x&1)
          {
               if(aux->attr1&(1<<12))
               {
                    s32 x1 = ((sprSize.x-x)>>1);
                    u8 c = src[(x1&0x3) + ((x1&0xFFFC)<<3)];
                    if((c&0xF)&&(prioTab[sprX]>=prio))
                    {
                         dst[sprX] = pal[(c&0xF)+((aux->attr2>>12)*0x10)];
                         prioTab[sprX] = prio;
                    }
                    x1 = ((sprSize.x-x-lg)>>1);
                    c = src[(x1&0x3) + ((x1&0xFFFC)<<3)];
                    if((c&0xF)&&(prioTab[sprX]>=prio))
                    {
                         dst[sprX+lg-1] = pal[(c&0xF)+((aux->attr2>>12)*0x10)];
                         prioTab[sprX+lg-1] = prio;
                    }
               }
               else
               {
                    s32 x1 = (x>>1);
                    u8 c = src[(x1&0x3) + ((x1&0xFFFC)<<3)];
                    if((c>>4)&&(prioTab[sprX]>=prio))
                    {
                         dst[sprX] = pal[(c>>4)+((aux->attr2>>12)*0x10)];
                         prioTab[sprX] = prio;
                    }
                    x1 = ((x+lg-1)>>1);
                    c = src[(x1&0x3) + ((x1&0xFFFC)<<3)];
                    if((c>>4)&&(prioTab[sprX]>=prio))
                    {
                         dst[sprX+lg-1] = pal[(c>>4)+((aux->attr2>>12)*0x10)];
                         prioTab[sprX+lg-1] = prio;
                    }
               }
               ++sprX;
               ++x;
          }
          lg >>= 1;
          x >>= 1;
          if(aux->attr1&(1<<12))
          {
	       u16 i;
               x = (sprSize.x>>1) - x -1;
               for(i = 0; i < lg; ++i, --x)
               {
                    u8 c = src[(x&0x3) + ((x&0xFFFC)<<3)];
                    if((c>>4)&&(prioTab[sprX]>=prio))
                    {
                    dst[sprX] = pal[(c>>4)+((aux->attr2>>12)*0x10)];
                    prioTab[sprX] = prio;
                    }
                    //else dst[sprX] = 0x7FFF;
                     ++sprX;
                     
                    if((c&0xF)&&(prioTab[sprX]>=prio))
                    {
                    dst[sprX] = pal[(c&0xF)+((aux->attr2>>12)*0x10)];
                    prioTab[sprX] = prio;
                    }
                    //else dst[sprX] = 0x7FFF;
                    ++sprX;
               }
               continue;
          }
               
	  u16 i;
          for(i = 0; i < lg; ++i, ++x)
          {
               u8 c = src[(x&0x3) + ((x&0xFFFC)<<3)];
                     
               if((c&0xF)&&(prioTab[sprX]>=prio))
               {
                    dst[sprX] = pal[(c&0xF)+((aux->attr2>>12)*0x10)];
                    prioTab[sprX] = prio;
               }
               //else dst[sprX] = 0x7FFF;
               ++sprX;
               
               if((c>>4)&&(prioTab[sprX]>=prio))
               {
                    dst[sprX] = pal[(c>>4)+((aux->attr2>>12)*0x10)];
                    prioTab[sprX] = prio;
               }
               //else dst[sprX] = 0x7FFF;
               ++sprX;
          }  
     }
}

               /*u16 * src = (u16 *)(gpu->sprMem + ((aux->attr2&0x3FF)<<gpu->sprBMPBlock) + (y*sprSize.x));
               if(aux->attr1&(1<<12))
               {
                    x = sprSize.x -x - 1;
                    for(u16 i = 0; i < lg; ++i, --x, ++sprX)
                    {
                         u8 c = src[x];
                         if((c) && (prioTab[sprX]>=prio))
                         {
                              dst[sprX] = c;
                              prioTab[sprX] = prio;
                         }
                         //else dst[sprX] = 0x7FFF;
                    }
                    continue;
               }
               for(u16 i = 0; i < lg; ++i, ++x, ++sprX)
               {
                    u16 c = src[x];
                    if((c) && (prioTab[sprX]>=prio))
                     {
                          dst[sprX] = c;
                          prioTab[sprX] = prio;
                     }
                     //else dst[sprX] = 0x7FFF;
               }
               continue;*/
/*gpu->sprBMPBlock*/

void sprite2D(GPU * gpu, u16 l, u16 * dst, u8 * prioTab)
{
     u16 i;
     OAM * aux = gpu->oam + (nbShow-1);// + 127;
     
     for(i = 0; i<nbShow; ++i, --aux)
     {
          s32 sprX = aux->attr1 & 0x1FF;
          sprX = ((s32)(sprX<<23))>>23;
          s32 sprY = aux->attr0 & 0xFF;
          s32 x = 0;
          
          size sprSize = sprSizeTab[(aux->attr1>>14)][(aux->attr0>>14)];
          
          u32 lg = sprSize.x;
          
          if(sprY>192)
               sprY = (s32)((s8)(aux->attr0 & 0xFF));

          if( ((aux->attr0&(1<<9))&&(!(aux->attr0&(1<<8)))) ||
              (l<sprY)||(l>=sprY+sprSize.y) ||
              (sprX==256) )
              continue;
          
          if(sprX<0)
          {
               if(sprX+sprSize.x<=0) continue;
               lg += sprX;
               x = -sprX;
               sprX = 0;
          }
          else
               if(sprX+sprSize.x>256)
                    lg = 255 - sprX;
               
          s32 y = l - sprY;
          u8 prio = (aux->attr2>>10)&3;
          
          if(aux->attr1&(1<<13)) y = sprSize.y - y -1;
          
          if((aux->attr0&(3<<10))==(3<<10))
          {
               u16 * src = (u16 *)(gpu->sprMem) +(aux->attr2&0x3E0)*64 + (aux->attr2&0x1F)*8 + (y<<8);
	       u16 i;
               if(aux->attr1&(1<<12))
               {
                    x = sprSize.x -x - 1;
                    for(i = 0; i < lg; ++i, --x, ++sprX)
                    {
                         u8 c = src[x];
                         if((c>>15) && (prioTab[sprX]>=prio))
                         {
                              dst[sprX] = c;
                              prioTab[sprX] = prio;
                         }
                         //else dst[sprX] = 0x7FFF;
                    }
                    continue;
               }
               for(i = 0; i < lg; ++i, ++x, ++sprX)
               {
                    u16 c = src[x];
                    if((c>>15) && (prioTab[sprX]>=prio))
                     {
                          dst[sprX] = c;
                          prioTab[sprX] = prio;
                     }
                     //else dst[sprX] = 0x7FFF;
               }//
               continue;
          }
          
          if(aux->attr0&(1<<13))
          {
               u8 * src = gpu->sprMem + ((aux->attr2&0x3FF)<<5) + ((y>>3)<<10) + ((y&0x7)*8);
               u16 * pal = ((u16 *)(ARM9.ARM9_VMEM +0x200)) + gpu->lcd*0x200;
               u16 i;
               
               if(aux->attr1&(1<<12))
               {
                    x = sprSize.x -x - 1;
                    for(i = 0; i < lg; ++i, --x, ++sprX)
                    {
                         u8 c = src[(x&0x7) + ((x&0xFFF8)<<3)];
                         if((c) && (prioTab[sprX]>=prio))
                         {
                              dst[sprX] = pal[c];
                              prioTab[sprX] = prio;
                         }
                         //else dst[sprX] = 0x7FFF;
                    }
                    continue;
               }
               
               for(i = 0; i < lg; ++i, ++x, ++sprX)
               {
                     u8 c = src[(x&0x7) + ((x&0xFFF8)<<3)];
                     
                     if((c) && (prioTab[sprX]>=prio))
                     {
                          dst[sprX] = pal[c];
                          prioTab[sprX] = prio;
                     }
                     //else dst[sprX] = 0x7FFF;
               }  
               continue;
          }
          u8 * src = gpu->sprMem + ((aux->attr2&0x3FF)<<5) + ((y>>3)<<10) + ((y&0x7)*4);
          u16 * pal = ((u16 *)(ARM9.ARM9_VMEM +0x200)) + gpu->lcd*0x200;
          if(x&1)
          {
               if(aux->attr1&(1<<12))
               {
                    s32 x1 = ((sprSize.x-x)>>1);
                    u8 c = src[(x1&0x3) + ((x1&0xFFFC)<<3)];
                    if((c&0xF)&&(prioTab[sprX]>=prio))
                    {
                         dst[sprX] = pal[(c&0xF)+((aux->attr2>>12)*0x10)];
                         prioTab[sprX] = prio;
                    }
                    x1 = ((sprSize.x-x-lg)>>1);
                    c = src[(x1&0x3) + ((x1&0xFFFC)<<3)];
                    if((c&0xF)&&(prioTab[sprX]>=prio))
                    {
                         dst[sprX+lg-1] = pal[(c&0xF)+((aux->attr2>>12)*0x10)];
                         prioTab[sprX+lg-1] = prio;
                    }
               }
               else
               {
                    s32 x1 = (x>>1);
                    u8 c = src[(x1&0x3) + ((x1&0xFFFC)<<3)];
                    if((c>>4)&&(prioTab[sprX]>=prio))
                    {
                         dst[sprX] = pal[(c>>4)+((aux->attr2>>12)*0x10)];
                         prioTab[sprX] = prio;
                    }
                    x1 = ((x+lg-1)>>1);
                    c = src[(x1&0x3) + ((x1&0xFFFC)<<3)];
                    if((c>>4)&&(prioTab[sprX]>=prio))
                    {
                         dst[sprX+lg-1] = pal[(c>>4)+((aux->attr2>>12)*0x10)];
                         prioTab[sprX+lg-1] = prio;
                    }
               }
               ++sprX;
               ++x;
          }
          lg >>= 1;
          x >>= 1;
          if(aux->attr1&(1<<12))
          {
	       u16 i;
               x = (sprSize.x>>1) - x -1;
               for(i = 0; i < lg; ++i, --x)
               {
                    u8 c = src[(x&0x3) + ((x&0xFFFC)<<3)];
                    if((c>>4)&&(prioTab[sprX]>=prio))
                    {
                    dst[sprX] = pal[(c>>4)+((aux->attr2>>12)*0x10)];
                    prioTab[sprX] = prio;
                    }
                    //else dst[sprX] = 0x7FFF;
                     ++sprX;
                     
                    if((c&0xF)&&(prioTab[sprX]>=prio))
                    {
                    dst[sprX] = pal[(c&0xF)+((aux->attr2>>12)*0x10)];
                    prioTab[sprX] = prio;
                    }
                    //else dst[sprX] = 0x7FFF;
                    ++sprX;
               }
               continue;
          }
               
	  u16 i;
          for(i = 0; i < lg; ++i, ++x)
          {
               u8 c = src[(x&0x3) + ((x&0xFFFC)<<3)];
                     
               if((c&0xF)&&(prioTab[sprX]>=prio))
               {
                    dst[sprX] = pal[(c&0xF)+((aux->attr2>>12)*0x10)];
                    prioTab[sprX] = prio;
               }
               //else dst[sprX] = 0x7FFF;
               ++sprX;
               
               if((c>>4)&&(prioTab[sprX]>=prio))
               {
                    dst[sprX] = pal[(c>>4)+((aux->attr2>>12)*0x10)];
                    prioTab[sprX] = prio;
               }
               //else dst[sprX] = 0x7FFF;
               ++sprX;
          }  
     }
}
