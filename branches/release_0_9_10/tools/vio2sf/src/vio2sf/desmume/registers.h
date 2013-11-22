/*  Copyright (C) 2006 Theo Berkau

    Ideas borrowed from Stephane Dallongeville's SCSP core

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

#ifndef REGISTERS_H
#define REGISTERS_H

#define REG_REGION_MASK                         0x0FFFEF80
#define REG_BASE_DISPx                          0x04000000
#define REG_BASE_DISPA                          0x04000000
#define REG_BASE_DISPB                          0x04001000
#define REG_BASE_DMA                            0x04000080
#define REG_BASE_SIORTCTIMERS                   0x04000100
#define REG_BASE_ROMIPC                         0x04000180
#define REG_BASE_MEMIRQ                         0x04000200
#define REG_BASE_MATH                           0x04000280
#define REG_BASE_OTHER                          0x04000300
#define REG_BASE_RCVPORTS                       0x04100000

// Display Engine A
#define REG_DISPA_DISPCNT                       0x04000000
#define REG_DISPA_VCOUNT                        0x04000006
#define REG_DISPA_BG0CNT                        0x04000008
#define REG_DISPA_BG1CNT                        0x0400000A
#define REG_DISPA_BG2CNT                        0x0400000C
#define REG_DISPA_BG3CNT                        0x0400000E
#define REG_DISPA_BG0HOFS                       0x04000010
#define REG_DISPA_BG0VOFS                       0x04000012
#define REG_DISPA_BG1HOFS                       0x04000014
#define REG_DISPA_BG1VOFS                       0x04000016
#define REG_DISPA_BG2HOFS                       0x04000018
#define REG_DISPA_BG2VOFS                       0x0400001A
#define REG_DISPA_BG3HOFS                       0x0400001C
#define REG_DISPA_BG3VOFS                       0x0400001E
#define REG_DISPA_BG2PA                         0x04000020
#define REG_DISPA_BG2PB                         0x04000022
#define REG_DISPA_BG2PC                         0x04000024
#define REG_DISPA_BG2PD                         0x04000026
#define REG_DISPA_BG2XL                         0x04000028
#define REG_DISPA_BG2XH                         0x0400002A
#define REG_DISPA_BG2YL                         0x0400002C
#define REG_DISPA_BG2YH                         0x0400002E
#define REG_DISPA_BG3PA                         0x04000030
#define REG_DISPA_BG3PB                         0x04000032
#define REG_DISPA_BG3PC                         0x04000034
#define REG_DISPA_BG3PD                         0x04000036
#define REG_DISPA_BG3XL                         0x04000038
#define REG_DISPA_BG3XH                         0x0400003A
#define REG_DISPA_BG3YL                         0x0400003C
#define REG_DISPA_BG3YH                         0x0400003E
#define REG_DISPA_WIN0H                         0x04000040
#define REG_DISPA_WIN1H                         0x04000042
#define REG_DISPA_WIN0V                         0x04000044
#define REG_DISPA_WIN1V                         0x04000046
#define REG_DISPA_WININ                         0x04000048
#define REG_DISPA_WINOUT                        0x0400004A
#define REG_DISPA_MOSAIC                        0x0400004C
#define REG_DISPA_BLDCNT                        0x04000050
#define REG_DISPA_BLDALPHA                      0x04000052
#define REG_DISPA_BLDY                          0x04000054
#define REG_DISPA_MASTERBRIGHT                  0x0400006C

// DMA
#define REG_DMA0SAD                             0x040000B0
#define REG_DMA0DAD                             0x040000B4
#define REG_DMA0CNTL                            0x040000B8
#define REG_DMA0CNTH                            0x040000BA
#define REG_DMA1SAD                             0x040000BC
#define REG_DMA1DAD                             0x040000C0
#define REG_DMA1CNTL                            0x040000C4
#define REG_DMA1CNTH                            0x040000C6
#define REG_DMA2SAD                             0x040000C8
#define REG_DMA2DAD                             0x040000CC
#define REG_DMA2CNTL                            0x040000D0
#define REG_DMA2CNTH                            0x040000D2
#define REG_DMA3SAD                             0x040000D4
#define REG_DMA3DAD                             0x040000D8
#define REG_DMA3CNTL                            0x040000DC
#define REG_DMA3CNTH                            0x040000DE
#define REG_DMA0FILL                            0x040000E0
#define REG_DMA1FILL                            0x040000E4
#define REG_DMA2FILL                            0x040000E8
#define REG_DMA3FILL                            0x040000EC

// Timers
#define REG_TM0CNTL                             0x04000100
#define REG_TM0CNTH                             0x04000102
#define REG_TM1CNTL                             0x04000104
#define REG_TM1CNTH                             0x04000106
#define REG_TM2CNTL                             0x04000108
#define REG_TM2CNTH                             0x0400010A
#define REG_TM3CNTL                             0x0400010C
#define REG_TM3CNTH                             0x0400010E

// SIO/Keypad Input/RTC
#define REG_SIODATA32                           0x04000120
#define REG_SIOCNT                              0x04000128
#define REG_KEYINPUT                            0x04000130
#define REG_KEYCNT                              0x04000132
#define REG_RCNT                                0x04000134
#define REG_EXTKEYIN                            0x04000136
#define REG_RTC                                 0x04000138

// IPC
#define REG_IPCSYNC                             0x04000180
#define REG_IPCFIFOCNT                          0x04000184
#define REG_IPCFIFOSEND                         0x04000188

// ROM
#define REG_AUXSPICNT                           0x040001A0
#define REG_AUXSPIDATA                          0x040001A2
#define REG_GCROMCTRL                           0x040001A4
#define REG_GCCMDOUT                            0x040001A8
#define REG_ENCSEED0L                           0x040001B0
#define REG_ENCSEED1L                           0x040001B4
#define REG_ENCSEED0H                           0x040001B8
#define REG_ENCSEED1H                           0x040001BC
#define REG_SPICNT                              0x040001C0
#define REG_SPIDATA                             0x040001C2

// Memory/IRQ
#define REG_EXMEMCNT                            0x04000204
#define REG_WIFIWAITCNT                         0x04000206
#define REG_IME                                 0x04000208
#define REG_IE                                  0x04000210
#define REG_IF                                  0x04000214
#define REG_VRAMCNTA                            0x04000240
#define REG_VRAMSTAT                            0x04000240
#define REG_VRAMCNTB                            0x04000241
#define REG_WRAMSTAT                            0x04000241
#define REG_VRAMCNTC                            0x04000242
#define REG_VRAMCNTD                            0x04000243
#define REG_VRAMCNTE                            0x04000244
#define REG_VRAMCNTF                            0x04000245
#define REG_VRAMCNTG                            0x04000246
#define REG_WRAMCNT                             0x04000247
#define REG_VRAMCNTH                            0x04000248
#define REG_VRAMCNTI                            0x04000249

// Math
#define REG_DIVCNT                              0x04000280
#define REG_DIVNUMER                            0x04000290
#define REG_DIVDENOM                            0x04000298
#define REG_DIVRESULT                           0x040002A0
#define REG_DIVREMRESULT                        0x040002A8
#define REG_SQRTCNT                             0x040002B0
#define REG_SQRTRESULT                          0x040002B4
#define REG_SQRTPARAM                           0x040002B8

// Other 
#define REG_POSTFLG                             0x04000300
#define REG_HALTCNT                             0x04000301
#define REG_POWCNT1                             0x04000304
#define REG_POWCNT2                             0x04000304
#define REG_BIOSPROT                            0x04000308

#define REG_DISPB_DISPCNT                       0x04001000
#define REG_DISPB_BG0CNT                        0x04001008
#define REG_DISPB_BG1CNT                        0x0400100A
#define REG_DISPB_BG2CNT                        0x0400100C
#define REG_DISPB_BG3CNT                        0x0400100E
#define REG_DISPB_BG0HOFS                       0x04001010
#define REG_DISPB_BG0VOFS                       0x04001012
#define REG_DISPB_BG1HOFS                       0x04001014
#define REG_DISPB_BG1VOFS                       0x04001016
#define REG_DISPB_BG2HOFS                       0x04001018
#define REG_DISPB_BG2VOFS                       0x0400101A
#define REG_DISPB_BG3HOFS                       0x0400101C
#define REG_DISPB_BG3VOFS                       0x0400101E
#define REG_DISPB_BG2PA                         0x04001020
#define REG_DISPB_BG2PB                         0x04001022
#define REG_DISPB_BG2PC                         0x04001024
#define REG_DISPB_BG2PD                         0x04001026
#define REG_DISPB_BG2XL                         0x04001028
#define REG_DISPB_BG2XH                         0x0400102A
#define REG_DISPB_BG2YL                         0x0400102C
#define REG_DISPB_BG2YH                         0x0400102E
#define REG_DISPB_BG3PA                         0x04001030
#define REG_DISPB_BG3PB                         0x04001032
#define REG_DISPB_BG3PC                         0x04001034
#define REG_DISPB_BG3PD                         0x04001036
#define REG_DISPB_BG3XL                         0x04001038
#define REG_DISPB_BG3XH                         0x0400103A
#define REG_DISPB_BG3YL                         0x0400103C
#define REG_DISPB_BG3YH                         0x0400103E
#define REG_DISPB_WIN0H                         0x04001040
#define REG_DISPB_WIN1H                         0x04001042
#define REG_DISPB_WIN0V                         0x04001044
#define REG_DISPB_WIN1V                         0x04001046
#define REG_DISPB_WININ                         0x04001048
#define REG_DISPB_WINOUT                        0x0400104A
#define REG_DISPB_MOSAIC                        0x0400104C
#define REG_DISPB_BLDCNT                        0x04001050
#define REG_DISPB_BLDALPHA                      0x04001052
#define REG_DISPB_BLDY                          0x04001054
#define REG_DISPB_MASTERBRIGHT                  0x0400106C

// Receive ports
#define REG_IPCFIFORECV                         0x04100000
#define REG_GCDATAIN                            0x04100010





#define REG_DISPB                               0x00001000
// core A and B specific
#define REG_DISPx_DISPCNT                       0x04000000
#define REG_DISPx_VCOUNT                        0x04000006
#define REG_DISPx_BG0CNT                        0x04000008
#define REG_DISPx_BG1CNT                        0x0400000A
#define REG_DISPx_BG2CNT                        0x0400000C
#define REG_DISPx_BG3CNT                        0x0400000E
#define REG_DISPx_BG0HOFS                       0x04000010
#define REG_DISPx_BG0VOFS                       0x04000012
#define REG_DISPx_BG1HOFS                       0x04000014
#define REG_DISPx_BG1VOFS                       0x04000016
#define REG_DISPx_BG2HOFS                       0x04000018
#define REG_DISPx_BG2VOFS                       0x0400001A
#define REG_DISPx_BG3HOFS                       0x0400001C
#define REG_DISPx_BG3VOFS                       0x0400001E
#define REG_DISPx_BG2PA                         0x04000020
#define REG_DISPx_BG2PB                         0x04000022
#define REG_DISPx_BG2PC                         0x04000024
#define REG_DISPx_BG2PD                         0x04000026
#define REG_DISPx_BG2XL                         0x04000028
#define REG_DISPx_BG2XH                         0x0400002A
#define REG_DISPx_BG2YL                         0x0400002C
#define REG_DISPx_BG2YH                         0x0400002E
#define REG_DISPx_BG3PA                         0x04000030
#define REG_DISPx_BG3PB                         0x04000032
#define REG_DISPx_BG3PC                         0x04000034
#define REG_DISPx_BG3PD                         0x04000036
#define REG_DISPx_BG3XL                         0x04000038
#define REG_DISPx_BG3XH                         0x0400003A
#define REG_DISPx_BG3YL                         0x0400003C
#define REG_DISPx_BG3YH                         0x0400003E
#define REG_DISPx_WIN0H                         0x04000040
#define REG_DISPx_WIN1H                         0x04000042
#define REG_DISPx_WIN0V                         0x04000044
#define REG_DISPx_WIN1V                         0x04000046
#define REG_DISPx_WININ                         0x04000048
#define REG_DISPx_WINOUT                        0x0400004A
#define REG_DISPx_MOSAIC                        0x0400004C
#define REG_DISPx_BLDCNT                        0x04000050
#define REG_DISPx_BLDALPHA                      0x04000052
#define REG_DISPx_BLDY                          0x04000054
#define REG_DISPx_MASTERBRIGHT                  0x0400006C
// core A specific
#define REG_DISPA_DISPSTAT                      0x04000004
#define REG_DISPA_DISP3DCNT                     0x04000060
#define REG_DISPA_DISPCAPCNT                    0x04000064
#define REG_DISPA_DISPMMEMFIFO                  0x04000068


#define eng_3D_RDLINES_COUNT   0x04000320
#define eng_3D_EDGE_COLOR      0x04000330
#define eng_3D_ALPHA_TEST_REF  0x04000340
#define eng_3D_CLEAR_COLOR     0x04000350
#define eng_3D_CLEAR_DEPTH     0x04000354
#define eng_3D_CLRIMAGE_OFFSET 0x04000356
#define eng_3D_FOG_COLOR       0x04000358
#define eng_3D_FOG_OFFSET      0x0400035C
#define eng_3D_FOG_TABLE       0x04000360
#define eng_3D_TOON_TABLE      0x04000380
#define eng_3D_GXFIFO          0x04000400

// 3d commands
#define cmd_3D_MTX_MODE        0x04000440
#define cmd_3D_MTX_PUSH        0x04000444
#define cmd_3D_MTX_POP         0x04000448
#define cmd_3D_MTX_STORE       0x0400044C
#define cmd_3D_MTX_RESTORE     0x04000450
#define cmd_3D_MTX_IDENTITY    0x04000454
#define cmd_3D_MTX_LOAD_4x4    0x04000458
#define cmd_3D_MTX_LOAD_4x3    0x0400045C
#define cmd_3D_MTX_MULT_4x4    0x04000460
#define cmd_3D_MTX_MULT_4x3    0x04000464
#define cmd_3D_MTX_MULT_3x3    0x04000468
#define cmd_3D_MTX_SCALE       0x0400046C
#define cmd_3D_MTX_TRANS       0x04000470
#define cmd_3D_COLOR           0x04000480
#define cmd_3D_NORMA           0x04000484
#define cmd_3D_TEXCOORD        0x04000488
#define cmd_3D_VTX_16          0x0400048C
#define cmd_3D_VTX_10          0x04000490
#define cmd_3D_VTX_XY          0x04000494
#define cmd_3D_VTX_XZ          0x04000498
#define cmd_3D_VTX_YZ          0x0400049C
#define cmd_3D_VTX_DIFF        0x040004A0
#define cmd_3D_POLYGON_ATTR    0x040004A4
#define cmd_3D_TEXIMAGE_PARAM  0x040004A8
#define cmd_3D_PLTT_BASE       0x040004AC
#define cmd_3D_DIF_AMB         0x040004C0
#define cmd_3D_SPE_EMI         0x040004C4
#define cmd_3D_LIGHT_VECTOR    0x040004C8
#define cmd_3D_LIGHT_COLOR     0x040004CC
#define cmd_3D_SHININESS       0x040004D0
#define cmd_3D_BEGIN_VTXS      0x04000500
#define cmd_3D_END_VTXS        0x04000504
#define cmd_3D_SWAP_BUFFERS    0x04000540
#define cmd_3D_VIEWPORT        0x04000580
#define cmd_3D_BOX_TEST        0x040005C0
#define cmd_3D_POS_TEST        0x040005C4
#define cmd_3D_VEC_TEST        0x040005C8

#define eng_3D_GXSTAT          0x04000600
#define eng_3D_RAM_COUNT       0x04000604
#define eng_3D_DISP_1DOT_DEPTH 0x04000610
#define eng_3D_POS_RESULT      0x04000620
#define eng_3D_VEC_RESULT      0x04000630
#define eng_3D_CLIPMTX_RESULT  0x04000640
#define eng_3D_VECMTX_RESULT   0x04000680
  

#endif
