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

#ifndef RESOURCE_H
#define RESOURCE_H

#if 0
#include "../NDSSystem.hpp"

extern NDSSystem nds;
extern BOOL execute;
extern unsigned long glock;
void refreshAll();
#endif

#define IconDeSmuME         100

#define IDC_STATIC          101
#define IDM_OPEN            102
#define IDM_QUIT            103
#define IDC_FERMER          104
#define IDC_STEP            105
#define IDC_SETPNUM         106
#define IDC_SCROLLER        107
#define IDC_GO              108
#define IDC_AUTO_UPDATE     109

#define IDM_MEMORY          110
#define IDM_DISASSEMBLER    111
#define IDM_GAME_INFO       112
#define IDM_EXEC            113
#define IDM_PAUSE           114
#define IDM_RESET           115
#define IDM_IOREG           116
#define IDM_LOG             117
#define IDM_PAL             118
#define IDM_TILE            119
#define IDM_MAP             120
#define IDM_MBG0            121
#define IDM_MBG1            122
#define IDM_MBG2            123
#define IDM_MBG3            124
#define IDM_SBG0            125
#define IDM_SBG1            126
#define IDM_SBG2            127
#define IDM_SBG3            128
#define IDM_OAM             129
#define IDM_PRINTSCREEN          140
#define IDM_QUICK_PRINTSCREEN    141

#define IDD_MEM_VIEWER      301
#define IDC_8_BIT           302
#define IDC_16_BIT          303
#define IDC_32_BIT          304
#define IDC_MEM_BOX         305
#define IDC_GOTOMEM         306

#define IDD_DESASSEMBLEUR_VIEWER 401
#define IDC_DES_BOX             402
#define IDC_R0                  403
#define IDC_R1                  404
#define IDC_R2                  405
#define IDC_R3                  406
#define IDC_R4                  407
#define IDC_R5                  408
#define IDC_R6                  409
#define IDC_R7                  410
#define IDC_R8                  411
#define IDC_R9                  412
#define IDC_R10                 413
#define IDC_R11                 414
#define IDC_R12                 415
#define IDC_R13                 416
#define IDC_R14                 417
#define IDC_R15                 418
#define IDC_MODE                419
#define IDC_AUTO_DES            420
#define IDC_ARM                 421
#define IDC_THUMB               422
#define IDC_GOTODES             423
#define IDC_TMP                 424

#define IDD_GAME_INFO           501
#define IDC_NOM_JEU             502
#define IDC_CDE                 503
#define IDC_FAB                 504
#define IDC_TAILLE              505
#define IDC_ARM9_T              506
#define IDC_ARM7_T              507
#define IDC_DATA                508

#define IDD_IO_REG              601
#define IDC_INTHAND             602
#define IDC_IE                  603
#define IDC_IME                 604
#define IDC_DISPCNT             605
#define IDC_DISPSTAT            606
#define IDC_IPCSYNC             607
#define IDC_IPCFIFO             608

#define IDD_LOG                 701
#define IDC_LOG                 702
#define IDD_PAL                 703
#define IDD_TILE                704
#define IDC_PAL_SELECT          705
#define IDC_PALNUM              706
#define IDC_MEM_SELECT          707
#define IDC_Tile_BOX            708
#define IDC_BITMAP              709
#define IDC_256COUL             710
#define IDC_16COUL              711
#define IDC_MINI_TILE           712
#define IDC_TILENUM             713

#define IDD_MAP                 800
#define IDC_BG_SELECT           801
#define IDC_PAL                 803
#define IDC_PRIO                804
#define IDC_CHAR                805
#define IDC_SCR                 806
#define IDC_MSIZE               807
#define IDC_SCROLL              808

#define IDD_OAM                 900
#define IDC_SCR_SELECT          901
#define IDC_TILE                902
#define IDC_OAMNUM              903
#define IDC_COOR                904
#define IDC_DIM                 905
#define IDC_ROT                 906
#define IDC_MOS                 907
#define IDC_PROP0               908
#define IDC_PROP1               909
#define IDC_OAM_BOX             910
#endif
