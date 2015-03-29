/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2012 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include "Disassembler.h"
#include "bits.h"

#define ROR(i, j)   ((((u32)(i))>>(j)) | (((u32)(i))<<(32-(j))))

const char Condition[16][3] = {
      "EQ",
      "NE",
      "CS",
      "CC",
      "MI",
      "PL",
      "VS",
      "VC",
      "HI",
      "LS",
      "GE",
      "LT",
      "GT",
      "LE",
      "",
      ""
      };
      
const char Registre[16][4] = {
      "R0",
      "R1",
      "R2",
      "R3",
      "R4",
      "R5",
      "R6",
      "R7",
      "R8",
      "R9",
      "R10",
      "R11",
      "R12",
      "SP",
      "LR",
      "PC",
      };

const char MSR_FIELD[16][5] = {
     "",
     "c",
     "x",
     "xc",
     "s",
     "sc",
     "sx",
     "sxc",
     "f",
     "fc",
     "fx",
     "fxc",
     "fs",
     "fsc",
     "fsx",
     "fsxc"
     };
     
#define DATAPROC_LSL_IMM(nom, s)      char tmp[10] = "";\
     if(((i>>7)&0x1F)!=0)\
          sprintf(tmp, ", LSL #%X", (int)((i>>7)&0x1F));\
     sprintf(txt, "%s%s%s %s, %s, %s%s",\
                                     #nom,\
                                     Condition[CONDITION(i)],\
                                     s,\
                                     Registre[REG_POS(i,12)],\
                                     Registre[REG_POS(i,16)],\
                                     Registre[REG_POS(i,0)],\
                                     tmp);
#define DATAPROC_ROR_IMM(nom, s)      char tmp[10] = "";\
     if(((i>>7)&0x1F)!=0)\
          sprintf(tmp, ", RRX");\
     sprintf(txt, "%s%s%s %s, %s, %s%s",\
                                     #nom,\
                                     Condition[CONDITION(i)],\
                                     s,\
                                     Registre[REG_POS(i,12)],\
                                     Registre[REG_POS(i,16)],\
                                     Registre[REG_POS(i,0)],\
                                     tmp\
                                     );

#define DATAPROC_REG_SHIFT(nom, shift,s)      sprintf(txt, "%s%s%s %s, %s, %s, %s %s",\
                                     #nom,\
                                     Condition[CONDITION(i)],\
                                     s,\
                                     Registre[REG_POS(i,12)],\
                                     Registre[REG_POS(i,16)],\
                                     Registre[REG_POS(i,0)],\
                                     #shift,\
                                     Registre[REG_POS(i,8)]\
                                     );

#define DATAPROC_IMM_SHIFT(nom, shift, s)      sprintf(txt, "%s%s%s %s, %s, %s, %s #%X",\
                                     #nom,\
                                     Condition[CONDITION(i)],\
                                     s,\
                                     Registre[REG_POS(i,12)],\
                                     Registre[REG_POS(i,16)],\
                                     Registre[REG_POS(i,0)],\
                                     #shift,\
                                     (int)((i>>7)&0x1F)\
                                     );
                                     
#define DATAPROC_IMM_VALUE(nom,s)  u32 shift_op = ROR((i&0xFF), (i>>7)&0x1E);\
                                   sprintf(txt, "%s%s%s %s, %s, #%X",\
                                        #nom,\
                                        Condition[CONDITION(i)],\
                                        s,\
                                        Registre[REG_POS(i,12)],\
                                        Registre[REG_POS(i,16)],\
                                        (int)shift_op\
                                        );
                                        
#define DATAPROC_ONE_OP_LSL_IMM(nom, s, v) char tmp[10] = "";\
     if(((i>>7)&0x1F)!=0)\
          sprintf(tmp, ", LSL #%X", (int)((i>>7)&0x1F));\
     sprintf(txt, "%s%s%s %s, %s%s",\
                                     #nom,\
                                     Condition[CONDITION(i)],\
                                     s,\
                                     Registre[REG_POS(i,v)],\
                                     Registre[REG_POS(i,0)],\
                                     tmp); 
                                      
#define DATAPROC_ONE_OP_ROR_IMM(nom, s, v)      char tmp[10] = "";\
     if(((i>>7)&0x1F)==0)\
          sprintf(tmp, ", RRX");\
     else\
          sprintf(tmp, ", ROR %d", (int)((i>>7)&0x1F));\
     sprintf(txt, "%s%s%s %s, %s%s",\
                                     #nom,\
                                     Condition[CONDITION(i)],\
                                     s,\
                                     Registre[REG_POS(i,v)],\
                                     Registre[REG_POS(i,0)],\
                                     tmp\
                                     );

#define DATAPROC_ONE_OP_REG_SHIFT(nom, shift,s, v)      sprintf(txt, "%s%s%s %s, %s, %s %s",\
                                     #nom,\
                                     Condition[CONDITION(i)],\
                                     s,\
                                     Registre[REG_POS(i,v)],\
                                     Registre[REG_POS(i,0)],\
                                     #shift,\
                                     Registre[REG_POS(i,8)]\
                                     );

#define DATAPROC_ONE_OP_IMM_SHIFT(nom, shift, s, v)      sprintf(txt, "%s%s%s %s, %s, %s #%X",\
                                     #nom,\
                                     Condition[CONDITION(i)],\
                                     s,\
                                     Registre[REG_POS(i,v)],\
                                     Registre[REG_POS(i,0)],\
                                     #shift,\
                                     (int)((i>>7)&0x1F)\
                                     );
                                     
#define DATAPROC_ONE_OP_IMM_VALUE(nom, s, v)\
                                     u32 shift_op = ROR((i&0xFF), (i>>7)&0x1E);\
                                     sprintf(txt, "%s%s%s %s, #%X",\
                                     #nom,\
                                     Condition[CONDITION(i)],\
                                     s,\
                                     Registre[REG_POS(i,v)],\
                                     (int)shift_op\
                                     );

#define SIGNEXTEND_24(i) (((i)&0xFFFFFF)|(0xFF000000*BIT23(i)))

#define LDRSTR_LSL_IMM(nom, op, op2, op3)      char tmp[10] = "";\
     if(((i>>7)&0x1F)!=0)\
          sprintf(tmp, ", LSL #%X", (int)((i>>7)&0x1F));\
     sprintf(txt, "%s%s %s, [%s%s, %s%s%s%s",\
                                     #nom,\
                                     Condition[CONDITION(i)],\
                                     Registre[REG_POS(i,12)],\
                                     Registre[REG_POS(i,16)],\
                                     op2,\
                                     op,\
                                     Registre[REG_POS(i,0)],\
                                     tmp,\
                                     op3);
#define LDRSTR_ROR_IMM(nom, op, op2, op3)      char tmp[10] = "";\
     if(((i>>7)&0x1F)!=0)\
          sprintf(tmp, ", RRX");\
     sprintf(txt, "%s%s %s, [%s%s, %s%s%s%s",\
                                     #nom,\
                                     Condition[CONDITION(i)],\
                                     Registre[REG_POS(i,12)],\
                                     Registre[REG_POS(i,16)],\
                                     op2,\
                                     op,\
                                     Registre[REG_POS(i,0)],\
                                     tmp,\
                                     op3\
                                     );

#define LDRSTR_IMM_SHIFT(nom, shift, op, op2, op3)\
                                     sprintf(txt, "%s%s %s, [%s%s, %s%s, %s #%X%s",\
                                     #nom,\
                                     Condition[CONDITION(i)],\
                                     Registre[REG_POS(i,12)],\
                                     Registre[REG_POS(i,16)],\
                                     op2,\
                                     op,\
                                     Registre[REG_POS(i,0)],\
                                     #shift,\
                                     (int)((i>>7)&0x1F),\
                                     op3\
                                     );
                                     
#define RegList(nb)      char lreg[100] = "";\
     int prec = 0;\
     int j;\
     for(j = 0; j < nb; j++)\
     {\
          if(prec)\
          {\
               if((!BIT_N(i, j+1))||(j==nb-1))\
               {\
                    sprintf(lreg + strlen(lreg), "%s,", Registre[j]);\
                    prec = 0;\
               }\
          }\
          else\
          {\
               if(BIT_N(i, j))\
               {\
                    if((BIT_N(i, j+1))&&(j!=nb-1))\
                    {\
                         sprintf(lreg + strlen(lreg), "%s-", Registre[j]);\
                         prec = 1;\
                    }\
                    else\
                         sprintf(lreg + strlen(lreg), "%s,", Registre[j]);\
               }\
          }\
     }\
     if(*lreg) lreg[strlen(lreg)-1]='\0';

static char * OP_UND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "--<UNDEFINED>--");
     return txt;
}
 
//-----------------------AND------------------------------------
static char * OP_AND_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(AND, "");
     return txt;
}

static char * OP_AND_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(AND, LSL, "");
     return txt;
}

static char * OP_AND_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(AND, LSR, "");
     return txt;
}

static char * OP_AND_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(AND, LSR, "");
     return txt;
}

static char * OP_AND_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(AND, ASR, "");
     return txt;
}

static char * OP_AND_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(AND, ASR, "");
     return txt;
}

static char * OP_AND_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(AND, "");
     return txt;
}

static char * OP_AND_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(AND, ROR, "");
     return txt;
}

static char * OP_AND_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(AND, "");
     return txt;
}

static char * OP_AND_S_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(AND, "S");
     return txt;
}

static char * OP_AND_S_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(AND, LSL, "S");
     return txt;
}

static char * OP_AND_S_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(AND, LSR, "S");
     return txt;
}

static char * OP_AND_S_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(AND, LSR, "S");
     return txt;
}

static char * OP_AND_S_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(AND, ASR, "S");
     return txt;
}

static char * OP_AND_S_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(AND, ASR, "S");
     return txt;
}

static char * OP_AND_S_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(AND, "S");
     return txt;
}

static char * OP_AND_S_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(AND, ROR, "S");
     return txt;
}

static char * OP_AND_S_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(AND, "S");
     return txt;
}

//--------------EOR------------------------------
static char * OP_EOR_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(EOR, "");
     return txt;
}

static char * OP_EOR_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(EOR, LSL, "");
     return txt;
}

static char * OP_EOR_LSR_IMM(u32 adr, u32 i, char * txt)
{
    DATAPROC_IMM_SHIFT(EOR, LSR, "");
    return txt;
}
    
static char * OP_EOR_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(EOR, LSR, "");
     return txt;
}

static char * OP_EOR_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(EOR, ASR, "");
     return txt;
}

static char * OP_EOR_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(EOR, ASR, "");
     return txt;
}

static char * OP_EOR_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(EOR, "");
     return txt;
}

static char * OP_EOR_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(EOR, ROR, "");
     return txt;
}

static char * OP_EOR_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(EOR, "");
     return txt;
}

static char * OP_EOR_S_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(EOR, "S");
     return txt;
}

static char * OP_EOR_S_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(EOR, LSL, "S");
     return txt;
}

static char * OP_EOR_S_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(EOR, LSR, "S");
     return txt;
}

static char * OP_EOR_S_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(EOR, LSR, "S");
     return txt;
}

static char * OP_EOR_S_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(EOR, ASR, "S");
     return txt;
}

static char * OP_EOR_S_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(EOR, ASR, "S");
     return txt;
}

static char * OP_EOR_S_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(EOR, "S");
     return txt;
}

static char * OP_EOR_S_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(EOR, ROR, "S");
     return txt;
}

static char * OP_EOR_S_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(EOR, "S");
     return txt;
}

//-------------SUB-------------------------------------

static char * OP_SUB_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(SUB, "");
     return txt;
}

static char * OP_SUB_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SUB, LSL, "");
     return txt;
}

static char * OP_SUB_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(SUB, LSR, "");
     return txt;
}

static char * OP_SUB_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SUB, LSR, "");
     return txt;
}

static char * OP_SUB_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(SUB, ASR, "");
     return txt;
}

static char * OP_SUB_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SUB, ASR, "");
return txt;}

static char * OP_SUB_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(SUB, "");
return txt;}

static char * OP_SUB_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SUB, ROR, "");
return txt;}

static char * OP_SUB_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(SUB, "");
return txt;}

static char * OP_SUB_S_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(SUB, "S");
return txt;}

static char * OP_SUB_S_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SUB, LSL, "S");
return txt;}

static char * OP_SUB_S_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(SUB, LSR, "S");
return txt;}

static char * OP_SUB_S_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SUB, LSR, "S");
return txt;}

static char * OP_SUB_S_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(SUB, ASR, "S");
return txt;}

static char * OP_SUB_S_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SUB, ASR, "S");
return txt;}

static char * OP_SUB_S_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(SUB, "S");
return txt;}

static char * OP_SUB_S_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SUB, ROR, "S");
return txt;}

static char * OP_SUB_S_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(SUB, "S");
return txt;}

//------------------RSB------------------------

static char * OP_RSB_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(RSB, "");
return txt;}

static char * OP_RSB_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSB, LSL, "");
return txt;}

static char * OP_RSB_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(RSB, LSR, "");
return txt;}

static char * OP_RSB_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSB, LSR, "");
return txt;}

static char * OP_RSB_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(RSB, ASR, "");
return txt;}

static char * OP_RSB_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSB, ASR, "");
return txt;}

static char * OP_RSB_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(RSB, "");
return txt;}

static char * OP_RSB_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSB, ROR, "");
return txt;}

static char * OP_RSB_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(RSB, "");
return txt;}

static char * OP_RSB_S_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(RSB, "S");
return txt;}

static char * OP_RSB_S_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSB, LSL, "S");
return txt;}

static char * OP_RSB_S_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(RSB, LSR, "S");
return txt;}

static char * OP_RSB_S_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSB, LSR, "S");
return txt;}

static char * OP_RSB_S_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(RSB, ASR, "S");
return txt;}

static char * OP_RSB_S_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSB, ASR, "S");
return txt;}

static char * OP_RSB_S_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(RSB, "S");
return txt;}

static char * OP_RSB_S_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSB, ROR, "S");
return txt;}

static char * OP_RSB_S_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(RSB, "S");
return txt;}

//------------------ADD-----------------------------------

static char * OP_ADD_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(ADD, "");
return txt;}

static char * OP_ADD_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADD, LSL, "");
return txt;}

static char * OP_ADD_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(ADD, LSR, "");
return txt;}

static char * OP_ADD_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADD, LSR, "");
return txt;}

static char * OP_ADD_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(ADD, ASR, "");
return txt;}

static char * OP_ADD_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADD, ASR, "");
return txt;}

static char * OP_ADD_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(ADD, "");
return txt;}

static char * OP_ADD_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADD, ROR, "");
return txt;}

static char * OP_ADD_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(ADD, "");
return txt;}

static char * OP_ADD_S_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(ADD, "S");
return txt;}

static char * OP_ADD_S_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADD, LSL, "S");
return txt;}

static char * OP_ADD_S_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(ADD, LSR, "S");
return txt;}

static char * OP_ADD_S_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADD, LSR, "S");
return txt;}

static char * OP_ADD_S_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(ADD, ASR, "S");
return txt;}

static char * OP_ADD_S_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADD, ASR, "S");
return txt;}

static char * OP_ADD_S_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(ADD, "S");
return txt;}

static char * OP_ADD_S_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADD, ROR, "S");
return txt;}

static char * OP_ADD_S_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(ADD, "S");
return txt;}

//------------------ADC-----------------------------------

static char * OP_ADC_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(ADC, "");
return txt;}

static char * OP_ADC_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADC, LSL, "");
return txt;}

static char * OP_ADC_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(ADC, LSR, "");
return txt;}

static char * OP_ADC_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADC, LSR, "");
return txt;}

static char * OP_ADC_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(ADC, ASR, "");
return txt;}

static char * OP_ADC_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADC, ASR, "");
return txt;}

static char * OP_ADC_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(ADC, "");
return txt;}

static char * OP_ADC_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADC, ROR, "");
return txt;}

static char * OP_ADC_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(ADC, "");
return txt;}

static char * OP_ADC_S_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(ADC, "S");
return txt;}

static char * OP_ADC_S_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADC, LSL, "S");
return txt;}

static char * OP_ADC_S_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(ADC, LSR, "S");
return txt;}

static char * OP_ADC_S_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADC, LSR, "S");
return txt;}

static char * OP_ADC_S_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(ADC, ASR, "S");
return txt;}

static char * OP_ADC_S_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADC, ASR, "S");
return txt;}

static char * OP_ADC_S_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(ADC, "S");
return txt;}

static char * OP_ADC_S_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ADC, ROR, "S");
return txt;}

static char * OP_ADC_S_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(ADC, "S");
return txt;}

//-------------SBC-------------------------------------

static char * OP_SBC_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(SBC, "");
return txt;}

static char * OP_SBC_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SBC, LSL, "");
return txt;}

static char * OP_SBC_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(SBC, LSR, "");
return txt;}

static char * OP_SBC_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SBC, LSR, "");
return txt;}

static char * OP_SBC_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(SBC, ASR, "");
return txt;}

static char * OP_SBC_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SBC, ASR, "");
return txt;}

static char * OP_SBC_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(SBC, "");
return txt;}

static char * OP_SBC_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SBC, ROR, "");
return txt;}

static char * OP_SBC_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(SBC, "");
return txt;}

static char * OP_SBC_S_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(SBC, "S");
return txt;}

static char * OP_SBC_S_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SBC, LSL, "S");
return txt;}

static char * OP_SBC_S_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(SBC, LSR, "S");
return txt;}

static char * OP_SBC_S_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SBC, LSR, "S");
return txt;}

static char * OP_SBC_S_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(SBC, ASR, "S");
return txt;}

static char * OP_SBC_S_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SBC, ASR, "S");
return txt;}

static char * OP_SBC_S_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(SBC, "S");
return txt;}

static char * OP_SBC_S_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(SBC, ROR, "S");
return txt;}

static char * OP_SBC_S_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(SBC, "S");
return txt;}

//---------------RSC----------------------------------

static char * OP_RSC_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(RSC, "");
return txt;}

static char * OP_RSC_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSC, LSL, "");
return txt;}

static char * OP_RSC_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(RSC, LSR, "");
return txt;}

static char * OP_RSC_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSC, LSR, "");
return txt;}

static char * OP_RSC_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(RSC, ASR, "");
return txt;}

static char * OP_RSC_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSC, ASR, "");
return txt;}

static char * OP_RSC_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(RSC, "");
return txt;}

static char * OP_RSC_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSC, ROR, "");
return txt;}

static char * OP_RSC_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(RSC, "");
return txt;}

static char * OP_RSC_S_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(RSC, "S");
return txt;}

static char * OP_RSC_S_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSC, LSL, "S");
return txt;}

static char * OP_RSC_S_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(RSC, LSR, "S");
return txt;}

static char * OP_RSC_S_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSC, LSR, "S");
return txt;}

static char * OP_RSC_S_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(RSC, ASR, "S");
return txt;}

static char * OP_RSC_S_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSC, ASR, "S");
return txt;}

static char * OP_RSC_S_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(RSC, "S");
return txt;}

static char * OP_RSC_S_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(RSC, ROR, "S");
return txt;}

static char * OP_RSC_S_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(RSC, "S");
return txt;}

//-------------------TST----------------------------

static char * OP_TST_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_LSL_IMM(TST, "", 16);
return txt;}

static char * OP_TST_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(TST, LSL, "", 16);
return txt;}

static char * OP_TST_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(TST, LSR, "", 16);
return txt;}

static char * OP_TST_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(TST, LSR, "", 16);
return txt;}

static char * OP_TST_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(TST, ASR, "", 16);
return txt;}

static char * OP_TST_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(TST, ASR, "", 16);
return txt;}

static char * OP_TST_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_ROR_IMM(TST, "", 16);
return txt;}

static char * OP_TST_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(TST, ROR, "", 16);
return txt;}

static char * OP_TST_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_VALUE(TST, "", 16);
return txt;}

//-------------------TEQ----------------------------

static char * OP_TEQ_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_LSL_IMM(TEQ, "", 16);
return txt;}

static char * OP_TEQ_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(TEQ, LSL, "", 16);
return txt;}

static char * OP_TEQ_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(TEQ, LSR, "", 16);
return txt;}

static char * OP_TEQ_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(TEQ, LSR, "", 16);
return txt;}

static char * OP_TEQ_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(TEQ, ASR, "", 16);
return txt;}

static char * OP_TEQ_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(TEQ, ASR, "", 16);
return txt;}

static char * OP_TEQ_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_ROR_IMM(TEQ, "", 16);
return txt;}

static char * OP_TEQ_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(TEQ, ROR, "", 16);
return txt;}

static char * OP_TEQ_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_VALUE(TEQ, "", 16);
return txt;}

//-------------CMP-------------------------------------

static char * OP_CMP_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_LSL_IMM(CMP, "", 16);
return txt;}

static char * OP_CMP_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(CMP, LSL, "", 16);
return txt;}

static char * OP_CMP_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(CMP, LSR, "", 16);
return txt;}

static char * OP_CMP_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(CMP, LSR, "", 16);
return txt;}

static char * OP_CMP_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(CMP, ASR, "", 16);
return txt;}

static char * OP_CMP_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(CMP, ASR, "", 16);
return txt;}

static char * OP_CMP_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_ROR_IMM(CMP, "", 16);
return txt;}

static char * OP_CMP_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(CMP, ROR, "", 16);
return txt;}

static char * OP_CMP_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_VALUE(CMP, "", 16);
return txt;}

//---------------CMN---------------------------

static char * OP_CMN_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_LSL_IMM(CMN, "", 16);
return txt;}

static char * OP_CMN_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(CMN, LSL, "", 16);
return txt;}

static char * OP_CMN_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(CMP, ASR, "", 16);
return txt;}

static char * OP_CMN_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(CMN, LSR, "", 16);
return txt;}

static char * OP_CMN_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(CMN, ASR, "", 16);
return txt;}

static char * OP_CMN_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(CMN, ASR, "", 16);
return txt;}

static char * OP_CMN_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_ROR_IMM(CMN, "", 16);
return txt;}

static char * OP_CMN_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(CMN, ROR, "", 16);
return txt;}

static char * OP_CMN_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_VALUE(CMN, "", 16);
return txt;}

//------------------ORR-------------------

static char * OP_ORR_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(ORR, "");
return txt;}

static char * OP_ORR_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ORR, LSL, "");
return txt;}

static char * OP_ORR_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(ORR, LSR, "");
return txt;}

static char * OP_ORR_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ORR, LSR, "");
return txt;}

static char * OP_ORR_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(ORR, ASR, "");
return txt;}

static char * OP_ORR_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ORR, ASR, "");
return txt;}

static char * OP_ORR_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(ORR, "");
return txt;}

static char * OP_ORR_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ORR, ROR, "");
return txt;}

static char * OP_ORR_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(ORR, "");
return txt;}

static char * OP_ORR_S_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(ORR, "S");
return txt;}

static char * OP_ORR_S_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ORR, LSL, "S");
return txt;}

static char * OP_ORR_S_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(ORR, LSR, "S");
return txt;}

static char * OP_ORR_S_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ORR, LSR, "S");
return txt;}

static char * OP_ORR_S_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(ORR, ASR, "S");
return txt;}

static char * OP_ORR_S_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ORR, ASR, "S");
return txt;}

static char * OP_ORR_S_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(ORR, "S");
return txt;}

static char * OP_ORR_S_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(ORR, ROR, "S");
return txt;}

static char * OP_ORR_S_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(ORR, "S");
return txt;}

//------------------MOV-------------------

static char * OP_MOV_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_LSL_IMM(MOV, "", 12);
return txt;}

static char * OP_MOV_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MOV, LSL, "", 12);
return txt;}

static char * OP_MOV_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(MOV, LSR, "", 12);
return txt;}

static char * OP_MOV_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MOV, LSR, "", 12);
return txt;}

static char * OP_MOV_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(MOV, ASR, "", 12);
return txt;}

static char * OP_MOV_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MOV, ASR, "", 12);
return txt;}

static char * OP_MOV_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_ROR_IMM(MOV, "", 12);
return txt;}

static char * OP_MOV_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MOV, ROR, "", 12);
return txt;}

static char * OP_MOV_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_VALUE(MOV, "", 12);
return txt;}

static char * OP_MOV_S_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_LSL_IMM(MOV, "S", 12);
return txt;}

static char * OP_MOV_S_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MOV, LSL, "S", 12);
return txt;}

static char * OP_MOV_S_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(MOV, LSR, "S", 12);
return txt;}

static char * OP_MOV_S_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MOV, LSR, "S", 12);
return txt;}

static char * OP_MOV_S_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(MOV, ASR, "S", 12);
return txt;}

static char * OP_MOV_S_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MOV, ASR, "S", 12);
return txt;}

static char * OP_MOV_S_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_ROR_IMM(MOV, "S", 12);
return txt;}

static char * OP_MOV_S_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MOV, ROR, "S", 12);
return txt;}

static char * OP_MOV_S_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_VALUE(MOV, "S", 12);
return txt;}

//------------------BIC-------------------

static char * OP_BIC_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(BIC, "");
return txt;}

static char * OP_BIC_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(BIC, LSL, "");
return txt;}

static char * OP_BIC_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(BIC, LSR, "");
return txt;}

static char * OP_BIC_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(BIC, LSR, "");
return txt;}

static char * OP_BIC_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(BIC, ASR, "");
return txt;}

static char * OP_BIC_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(BIC, ASR, "");
return txt;}

static char * OP_BIC_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(BIC, "");
return txt;}

static char * OP_BIC_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(BIC, ROR, "");
return txt;}

static char * OP_BIC_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(BIC, "");
return txt;}

static char * OP_BIC_S_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_LSL_IMM(BIC, "S");
return txt;}

static char * OP_BIC_S_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(BIC, LSL, "S");
return txt;}

static char * OP_BIC_S_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(BIC, LSR, "S");
return txt;}

static char * OP_BIC_S_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(BIC, LSR, "S");
return txt;}

static char * OP_BIC_S_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_SHIFT(BIC, ASR, "S");
return txt;}

static char * OP_BIC_S_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(BIC, ASR, "S");
return txt;}

static char * OP_BIC_S_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ROR_IMM(BIC, "S");
return txt;}

static char * OP_BIC_S_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_REG_SHIFT(BIC, ROR, "S");
return txt;}

static char * OP_BIC_S_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_IMM_VALUE(BIC, "S");
return txt;}

//------------------MVN-------------------

static char * OP_MVN_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_LSL_IMM(MVN, "", 12);
return txt;}

static char * OP_MVN_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MVN, LSL, "", 12);
return txt;}

static char * OP_MVN_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(MVN, LSR, "", 12);
return txt;}

static char * OP_MVN_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MVN, LSR, "", 12);
return txt;}

static char * OP_MVN_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(MVN, ASR, "", 12);
return txt;}

static char * OP_MVN_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MVN, ASR, "", 12);
return txt;}

static char * OP_MVN_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_ROR_IMM(MVN, "", 12);
return txt;}

static char * OP_MVN_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MVN, ROR, "", 12);
return txt;}

static char * OP_MVN_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_VALUE(MVN, "", 12);
return txt;}

static char * OP_MVN_S_LSL_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_LSL_IMM(MVN, "S", 12);
return txt;}

static char * OP_MVN_S_LSL_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MVN, LSL, "S", 12);
return txt;}

static char * OP_MVN_S_LSR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(MVN, LSR, "S", 12);
return txt;}

static char * OP_MVN_S_LSR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MVN, LSR, "S", 12);
return txt;}

static char * OP_MVN_S_ASR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_SHIFT(MOV, ASR, "S", 12);
return txt;}

static char * OP_MVN_S_ASR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MVN, ASR, "S", 12);
return txt;}

static char * OP_MVN_S_ROR_IMM(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_ROR_IMM(MVN, "S", 12);
return txt;}

static char * OP_MVN_S_ROR_REG(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_REG_SHIFT(MVN, ROR, "S", 12);
return txt;}

static char * OP_MVN_S_IMM_VAL(u32 adr, u32 i, char * txt)
{
     DATAPROC_ONE_OP_IMM_VALUE(MVN, "S", 12);
return txt;}


//-------------MUL------------------------

static char * OP_MUL(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "MUL%s %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_MLA(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "MLA%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)], Registre[REG_POS(i,12)]);
return txt;}

static char * OP_MUL_S(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "MUL%sS %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_MLA_S(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "MLA%sS %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)], Registre[REG_POS(i,12)]);
return txt;}


//----------UMUL--------------------------

static char * OP_UMULL(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "UMULL%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_UMLAL(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "UMLAL%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_UMULL_S(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "UMULL%sS %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_UMLAL_S(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "UMLAL%sS %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

//----------SMUL--------------------------

static char * OP_SMULL(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMULL%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_SMLAL(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMLAL%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_SMULL_S(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMULL%sS %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_SMLAL_S(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMLAL%sS %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

//---------------SWP------------------------------

static char * OP_SWP(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SWP%s %s, %s, [%s]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,0)], Registre[REG_POS(i,16)]);
return txt;}

static char * OP_SWPB(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SWPB%s %s, %s, [%s]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,0)], Registre[REG_POS(i,16)]);
return txt;}

//------------LDRH-----------------------------

static char * OP_LDRH_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRH%s %s, [%s, #%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRH_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRH%s %s, [%s, -#%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRH_P_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRH%s %s, [%s, %s]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRH_M_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRH%s %s, [%s, -%s]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRH_PRE_INDE_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRH%s %s, [%s, #%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRH_PRE_INDE_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRH%s %s, [%s, -#%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRH_PRE_INDE_P_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRH%s %s, [%s, %s]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRH_PRE_INDE_M_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRH%s %s, [%s, -%s]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRH_POS_INDE_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRH%s %s, [%s], #%X", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRH_POS_INDE_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRH%s %s, [%s], -#%X", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRH_POS_INDE_P_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRH%s %s, [%s], %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRH_POS_INDE_M_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRH%s %s, [%s], -%s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

//------------STRH-----------------------------

static char * OP_STRH_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRH%s %s, [%s, #%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_STRH_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRH%s %s, [%s, -#%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_STRH_P_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRH%s %s, [%s, %s]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_STRH_M_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRH%s %s, [%s, -%s]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_STRH_PRE_INDE_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRH%s %s, [%s, #%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_STRH_PRE_INDE_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRH%s %s, [%s, -#%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_STRH_PRE_INDE_P_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRH%s %s, [%s, %s]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_STRH_PRE_INDE_M_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRH%s %s, [%s, -%s]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_STRH_POS_INDE_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRH%s %s, [%s], #%X", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_STRH_POS_INDE_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRH%s %s, [%s], -#%X", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_STRH_POS_INDE_P_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRH%s %s, [%s], %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_STRH_POS_INDE_M_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRH%s %s, [%s], -%s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

//----------------LDRSH--------------------------

static char * OP_LDRSH_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSH%s %s, [%s, #%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRSH_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSH%s %s, [%s, -#%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRSH_P_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSH%s %s, [%s, %s]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRSH_M_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSH%s %s, [%s, -%s]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRSH_PRE_INDE_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSH%s %s, [%s, #%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRSH_PRE_INDE_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSH%s %s, [%s, -#%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRSH_PRE_INDE_P_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSH%s %s, [%s, %s]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRSH_PRE_INDE_M_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSH%s %s, [%s, -%s]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRSH_POS_INDE_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSH%s %s, [%s], #%X", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRSH_POS_INDE_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSH%s %s, [%s], -#%X", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRSH_POS_INDE_P_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSH%s %s, [%s], %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRSH_POS_INDE_M_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSH%s %s, [%s], -%s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

//----------------------LDRSB----------------------

static char * OP_LDRSB_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSB%s %s, [%s, #%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRSB_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSB%s %s, [%s, -#%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRSB_P_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSB%s %s, [%s, %s]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRSB_M_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSB%s %s, [%s, -%s]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRSB_PRE_INDE_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSB%s %s, [%s, #%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRSB_PRE_INDE_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSB%s %s, [%s, -#%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRSB_PRE_INDE_P_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSB%s %s, [%s, %s]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRSB_PRE_INDE_M_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSB%s %s, [%s, -%s]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRSB_POS_INDE_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSB%s %s, [%s], #%X", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRSB_POS_INDE_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSB%s %s, [%s], -#%X", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(((i>>4)&0xF0)|(i&0xF)));
return txt;}

static char * OP_LDRSB_POS_INDE_P_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSB%s %s, [%s], %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_LDRSB_POS_INDE_M_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSB%s %s, [%s], -%s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

//--------------MRS--------------------------------

static char * OP_MRS_CPSR(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "MRS%s %s, CPSR", Condition[CONDITION(i)], Registre[REG_POS(i,12)]);
return txt;}

static char * OP_MRS_SPSR(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "MRS%s %s, SPSR", Condition[CONDITION(i)], Registre[REG_POS(i,12)]);
return txt;}

//--------------MSR--------------------------------

static char * OP_MSR_CPSR(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "MSR%s CPSR_%s, %s", Condition[CONDITION(i)], MSR_FIELD[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_MSR_SPSR(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "MSR%s SPSR_%s, %s", Condition[CONDITION(i)], MSR_FIELD[REG_POS(i,16)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_MSR_CPSR_IMM_VAL(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "MSR%s CPSR_%s, #%X", Condition[CONDITION(i)], MSR_FIELD[REG_POS(i,16)], (int)ROR((i&0xFF), ((i>>7)&0x1E)));
return txt;}

static char * OP_MSR_SPSR_IMM_VAL(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "MSR%s SPSR_%s, #%X", Condition[CONDITION(i)], MSR_FIELD[REG_POS(i,16)], (int)ROR((i&0xFF), (i>>7)&0x1E));
return txt;}

//-----------------BRANCH--------------------------

static char * OP_BX(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "BX%s %s", Condition[CONDITION(i)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_BLX_REG(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "BLX%s %s", Condition[CONDITION(i)], Registre[REG_POS(i,0)]);
return txt;}

static char * OP_B(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "BLX%s %08X", Condition[CONDITION(i)], (int)(adr+(SIGNEXTEND_24(i)<<2)+8));
     return txt;}
     sprintf(txt, "B%s %08X", Condition[CONDITION(i)], (int)(adr+(SIGNEXTEND_24(i)<<2)+8));
return txt;}

static char * OP_BL(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "BLX%s %08X", Condition[CONDITION(i)], (int)(adr+(SIGNEXTEND_24(i)<<2)+10));
     return txt;}
     sprintf(txt, "BL%s %08X", Condition[CONDITION(i)], (int)(adr+(SIGNEXTEND_24(i)<<2)+8));
return txt;}

//----------------CLZ-------------------------------

static char * OP_CLZ(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "CLZ%s %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,0)]);
return txt;}


//--------------------QADD--QSUB------------------------------

static char * OP_QADD(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "QADD%s %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,0)], Registre[REG_POS(i,16)]);
return txt;}

static char * OP_QSUB(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "QSUB%s %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,0)], Registre[REG_POS(i,16)]);
return txt;}

static char * OP_QDADD(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "QDADD%s %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,0)], Registre[REG_POS(i,16)]);
return txt;}

static char * OP_QDSUB(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "QDSUB%s %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,0)], Registre[REG_POS(i,16)]);
return txt;}

//-----------------SMUL-------------------------------

static char * OP_SMUL_B_B(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMULBB%s %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_SMUL_B_T(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMULBT%s %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_SMUL_T_B(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMULTB%s %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_SMUL_T_T(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMULTT%s %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

//-----------SMLA----------------------------

static char * OP_SMLA_B_B(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMLABB%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)], Registre[REG_POS(i,12)]);
return txt;}

static char * OP_SMLA_B_T(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMLABT%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)], Registre[REG_POS(i,12)]);
return txt;}

static char * OP_SMLA_T_B(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMLATB%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)], Registre[REG_POS(i,12)]);
return txt;}

static char * OP_SMLA_T_T(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMLATT%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)], Registre[REG_POS(i,12)]);
return txt;}

//--------------SMLAL---------------------------------------

static char * OP_SMLAL_B_B(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMLABB%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_SMLAL_B_T(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMLABT%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_SMLAL_T_B(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMLATB%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_SMLAL_T_T(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMLATT%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

//--------------SMULW--------------------

static char * OP_SMULW_B(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMULWB%s %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

static char * OP_SMULW_T(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMULWT%s %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)]);
return txt;}

//--------------SMLAW-------------------
static char * OP_SMLAW_B(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMLAWB%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)], Registre[REG_POS(i,12)]);
return txt;}

static char * OP_SMLAW_T(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SMLAWT%s %s, %s, %s, %s", Condition[CONDITION(i)], Registre[REG_POS(i,16)], Registre[REG_POS(i,0)], Registre[REG_POS(i,8)], Registre[REG_POS(i,12)]);
return txt;}

//------------LDR---------------------------

static char * OP_LDR_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
	if(REG_POS(i,16) == 15)
		sprintf(txt, "LDR%s %s, [%08X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], (adr + 8 + (int)(i&0x7FF)));
	else
		sprintf(txt, "LDR%s %s, [%s, #%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDR_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
	if(REG_POS(i,16) == 15)
		sprintf(txt, "LDR%s %s, [%08X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], (adr + 8 - (int)(i&0x7FF)));
	else
		sprintf(txt, "LDR%s %s, [%s, -#%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDREX(u32 adr, u32 i, char * txt)
{
	sprintf(txt, "LDREX%s %s, [%s]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)]);
return txt;}

static char * OP_LDR_P_LSL_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(LDR, "", "", "]");
return txt;}

static char * OP_LDR_M_LSL_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(LDR, "-", "", "]");
return txt;}

static char * OP_LDR_P_LSR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDR, LSR, "", "", "]");
return txt;}

static char * OP_LDR_M_LSR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDR, LSR, "M", "", "]");
return txt;}

static char * OP_LDR_P_ASR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDR, ASR, "", "", "]");
return txt;}

static char * OP_LDR_M_ASR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDR, ASR, "-", "", "]");
return txt;}

static char * OP_LDR_P_ROR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(LDR, "", "", "]");
return txt;}

static char * OP_LDR_M_ROR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(LDR, "-", "", "]");
return txt;}

static char * OP_LDR_P_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDR%s %s, [%s, #%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDR_M_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDR%s %s, [%s, -#%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDR_P_LSL_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(LDR, "", "", "]!");
return txt;}

static char * OP_LDR_M_LSL_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(LDR, "-", "", "]!");
return txt;}

static char * OP_LDR_P_LSR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDR, LSR, "", "", "]!");
return txt;}

static char * OP_LDR_M_LSR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDR, LSR, "-", "", "]!");
return txt;}

static char * OP_LDR_P_ASR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDR, ASR, "", "", "]!");
return txt;}

static char * OP_LDR_M_ASR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDR, ASR, "-", "", "]!");
return txt;}

static char * OP_LDR_P_ROR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(LDR, "", "", "]!");
return txt;}

static char * OP_LDR_M_ROR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(LDR, "-", "", "]!");
return txt;}

static char * OP_LDR_P_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDR%s %s, [%s], #%X!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDR_P_IMM_OFF_POSTIND2(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDR%s %s, [%s], #%X!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDR_M_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDR%s %s, [%s], -#%X!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDR_P_LSL_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(LDR, "", "]", "");
return txt;}

static char * OP_LDR_M_LSL_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(LDR, "-", "]", "");
return txt;}

static char * OP_LDR_P_LSR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDR, LSR, "", "]", "");
return txt;}

static char * OP_LDR_M_LSR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDR, LSR, "-", "]", "");
return txt;}

static char * OP_LDR_P_ASR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDR, ASR, "", "]", "");
return txt;}

static char * OP_LDR_M_ASR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDR, ASR, "-", "]", "");
return txt;}

static char * OP_LDR_P_ROR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(LDR, "", "]", "");
return txt;}

static char * OP_LDR_M_ROR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(LDR, "-", "]", "");
return txt;}

//-----------------LDRB-------------------------------------------

static char * OP_LDRB_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRB%s %s, [%s, #%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDRB_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRB%s %s, [%s, -#%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDRB_P_LSL_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(LDRB, "", "", "]");
return txt;}

static char * OP_LDRB_M_LSL_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(LDRB, "-", "", "]");
return txt;}

static char * OP_LDRB_P_LSR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRB, LSR, "", "", "]");
return txt;}

static char * OP_LDRB_M_LSR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRB, LSR, "M", "", "]");
return txt;}

static char * OP_LDRB_P_ASR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRB, ASR, "", "", "]");
return txt;}

static char * OP_LDRB_M_ASR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRB, ASR, "-", "", "]");
return txt;}

static char * OP_LDRB_P_ROR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(LDRB, "", "", "]");
return txt;}

static char * OP_LDRB_M_ROR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(LDRB, "-", "", "]");
return txt;}

static char * OP_LDRB_P_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRB%s %s, [%s, #%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDRB_M_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRB%s %s, [%s, -#%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDRB_P_LSL_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(LDRB, "", "", "]!");
return txt;}

static char * OP_LDRB_M_LSL_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(LDRB, "-", "", "]!");
return txt;}

static char * OP_LDRB_P_LSR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRB, LSR, "", "", "]!");
return txt;}

static char * OP_LDRB_M_LSR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRB, LSR, "-", "", "]!");
return txt;}

static char * OP_LDRB_P_ASR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRB, ASR, "", "", "]!");
return txt;}

static char * OP_LDRB_M_ASR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRB, ASR, "-", "", "]!");
return txt;}

static char * OP_LDRB_P_ROR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(LDRB, "", "", "]!");
return txt;}

static char * OP_LDRB_M_ROR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(LDRB, "-", "", "]!");
return txt;}

static char * OP_LDRB_P_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRB%s %s, [%s], #%X!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDRB_M_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRB%s %s, [%s], -#%X!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDRB_P_LSL_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(LDRB, "", "]", "");
return txt;}

static char * OP_LDRB_M_LSL_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(LDRB, "-", "]", "");
return txt;}

static char * OP_LDRB_P_LSR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRB, LSR, "", "]", "");
return txt;}

static char * OP_LDRB_M_LSR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRB, LSR, "-", "]", "");
return txt;}

static char * OP_LDRB_P_ASR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRB, ASR, "", "]", "");
return txt;}

static char * OP_LDRB_M_ASR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRB, ASR, "-", "]", "");
return txt;}

static char * OP_LDRB_P_ROR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(LDRB, "", "]", "");
return txt;}

static char * OP_LDRB_M_ROR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(LDRB, "-", "]", "");
return txt;}

//----------------------STR--------------------------------

static char * OP_STR_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STR%s %s, [%s, #%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_STR_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STR%s %s, [%s, -#%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_STREX(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STREX%s %s, %s, [%s]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,0)], Registre[REG_POS(i,16)]);
return txt;}

static char * OP_STR_P_LSL_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(STR, "", "", "]");
return txt;}

static char * OP_STR_M_LSL_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(STR, "-", "", "]");
return txt;}

static char * OP_STR_P_LSR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STR, LSR, "", "", "]");
return txt;}

static char * OP_STR_M_LSR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STR, LSR, "M", "", "]");
return txt;}

static char * OP_STR_P_ASR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STR, ASR, "", "", "]");
return txt;}

static char * OP_STR_M_ASR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STR, ASR, "-", "", "]");
return txt;}

static char * OP_STR_P_ROR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(STR, "", "", "]");
return txt;}

static char * OP_STR_M_ROR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(STR, "-", "", "]");
return txt;}

static char * OP_STR_P_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STR%s %s, [%s, #%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_STR_M_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STR%s %s, [%s, -#%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_STR_P_LSL_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(STR, "", "", "]!");
return txt;}

static char * OP_STR_M_LSL_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(STR, "-", "", "]!");
return txt;}

static char * OP_STR_P_LSR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STR, LSR, "", "", "]!");
return txt;}

static char * OP_STR_M_LSR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STR, LSR, "-", "", "]!");
return txt;}

static char * OP_STR_P_ASR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STR, ASR, "", "", "]!");
return txt;}

static char * OP_STR_M_ASR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STR, ASR, "-", "", "]!");
return txt;}

static char * OP_STR_P_ROR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(STR, "", "", "]!");
return txt;}

static char * OP_STR_M_ROR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(STR, "-", "", "]!");
return txt;}

static char * OP_STR_P_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STR%s %s, [%s], #%X!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_STR_M_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STR%s %s, [%s], -#%X!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_STR_P_LSL_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(STR, "", "]", "");
return txt;}

static char * OP_STR_M_LSL_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(STR, "-", "]", "");
return txt;}

static char * OP_STR_P_LSR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STR, LSR, "", "]", "");
return txt;}

static char * OP_STR_M_LSR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STR, LSR, "-", "]", "");
return txt;}

static char * OP_STR_P_ASR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STR, ASR, "", "]", "");
return txt;}

static char * OP_STR_M_ASR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STR, ASR, "-", "]", "");
return txt;}

static char * OP_STR_P_ROR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(STR, "", "]", "");
return txt;}

static char * OP_STR_M_ROR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(STR, "-", "]", "");
return txt;}

//-----------------------STRB-------------------------------------

static char * OP_STRB_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRB%s %s, [%s, #%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_STRB_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRB%s %s, [%s, -#%X]", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_STRB_P_LSL_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(STRB, "", "", "]");
return txt;}

static char * OP_STRB_M_LSL_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(STRB, "-", "", "]");
return txt;}

static char * OP_STRB_P_LSR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRB, LSR, "", "", "]");
return txt;}

static char * OP_STRB_M_LSR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRB, LSR, "M", "", "]");
return txt;}

static char * OP_STRB_P_ASR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRB, ASR, "", "", "]");
return txt;}

static char * OP_STRB_M_ASR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRB, ASR, "-", "", "]");
return txt;}

static char * OP_STRB_P_ROR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(STRB, "", "", "]");
return txt;}

static char * OP_STRB_M_ROR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(STRB, "-", "", "]");
return txt;}

static char * OP_STRB_P_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRB%s %s, [%s, #%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_STRB_M_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRB%s %s, [%s, -#%X]!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_STRB_P_LSL_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(STRB, "", "", "]!");
return txt;}

static char * OP_STRB_M_LSL_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(STRB, "-", "", "]!");
return txt;}

static char * OP_STRB_P_LSR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRB, LSR, "", "", "]!");
return txt;}

static char * OP_STRB_M_LSR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRB, LSR, "-", "", "]!");
return txt;}

static char * OP_STRB_P_ASR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRB, ASR, "", "", "]!");
return txt;}

static char * OP_STRB_M_ASR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRB, ASR, "-", "", "]!");
return txt;}

static char * OP_STRB_P_ROR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(STRB, "", "", "]!");
return txt;}

static char * OP_STRB_M_ROR_IMM_OFF_PREIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(STRB, "-", "", "]!");
return txt;}

static char * OP_STRB_P_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRB%s %s, [%s], #%X!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_STRB_M_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRB%s %s, [%s], -#%X!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_STRB_P_LSL_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(STRB, "", "]", "");
return txt;}

static char * OP_STRB_M_LSL_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(STRB, "-", "]", "");
return txt;}

static char * OP_STRB_P_LSR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRB, LSR, "", "]", "");
return txt;}

static char * OP_STRB_M_LSR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRB, LSR, "-", "]", "");
return txt;}

static char * OP_STRB_P_ASR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRB, ASR, "", "]", "");
return txt;}

static char * OP_STRB_M_ASR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRB, ASR, "-", "]", "");
return txt;}

static char * OP_STRB_P_ROR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(STRB, "", "]", "");
return txt;}

static char * OP_STRB_M_ROR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(STRB, "-", "]", "");
return txt;}

//-----------------------LDRBT-------------------------------------

#if 0
static char * OP_LDRBT_P_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRBT%s %s, [%s], #%X!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDRBT_M_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRBT%s %s, [%s], -#%X!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_LDRBT_P_LSL_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(LDRBT, "", "]", "");
return txt;}

static char * OP_LDRBT_M_LSL_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(LDRBT, "-", "]", "");
return txt;}

static char * OP_LDRBT_P_LSR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRBT, LSR, "", "]", "");
return txt;}

static char * OP_LDRBT_M_LSR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRBT, LSR, "-", "]", "");
return txt;}

static char * OP_LDRBT_P_ASR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRBT, ASR, "", "]", "");
return txt;}

static char * OP_LDRBT_M_ASR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(LDRBT, ASR, "-", "]", "");
return txt;}

static char * OP_LDRBT_P_ROR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(LDRBT, "", "]", "");
return txt;}

static char * OP_LDRBT_M_ROR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(LDRBT, "-", "]", "");
return txt;}

//----------------------STRBT----------------------------

static char * OP_STRBT_P_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRBT%s %s, [%s], #%X!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_STRBT_M_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRBT%s %s, [%s], -#%X!", Condition[CONDITION(i)], Registre[REG_POS(i,12)], Registre[REG_POS(i,16)], (int)(i&0x7FF));
return txt;}

static char * OP_STRBT_P_LSL_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(STRBT, "", "]", "");
return txt;}

static char * OP_STRBT_M_LSL_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_LSL_IMM(STRBT, "-", "]", "");
return txt;}

static char * OP_STRBT_P_LSR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRBT, LSR, "", "]", "");
return txt;}

static char * OP_STRBT_M_LSR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRBT, LSR, "-", "]", "");
return txt;}

static char * OP_STRBT_P_ASR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRBT, ASR, "", "]", "");
return txt;}

static char * OP_STRBT_M_ASR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_IMM_SHIFT(STRBT, ASR, "-", "]", "");
return txt;}

static char * OP_STRBT_P_ROR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(STRBT, "", "]", "");
return txt;}

static char * OP_STRBT_M_ROR_IMM_OFF_POSTIND(u32 adr, u32 i, char * txt)
{
     LDRSTR_ROR_IMM(STRBT, "-", "]", "");
return txt;}
#endif

//---------------------LDM-----------------------------

static char * OP_LDMIA(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMIA%s %s, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_LDMIB(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMIB%s %s, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_LDMDA(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMDA%s %s, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_LDMDB(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMDB%s %s, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_LDMIA_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMIA%s %s!, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_LDMIB_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMIB%s %s!, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_LDMDA_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMDA%s %s!, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_LDMDB_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMDB%s %s!, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_LDMIA2(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMIA%s %s, {%s}^", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_LDMIB2(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMIB%s %s, {%s}^", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_LDMDA2(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMDA%s %s, {%s}^", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_LDMDB2(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMDB%s %s, {%s}^", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_LDMIA2_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMIA%s %s!, {%s}^", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
     if(BIT15(i)==0) sprintf(txt, "%s ?????", txt);
return txt;}

static char * OP_LDMIB2_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMIB%s %s!, {%s}^", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
     if(BIT15(i)==0) sprintf(txt, "%s ?????", txt);
return txt;}

static char * OP_LDMDA2_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMDA%s %s!, {%s}^", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
     if(BIT15(i)==0) sprintf(txt, "%s ?????", txt);
return txt;}

static char * OP_LDMDB2_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "LDMDB%s %s!, {%s}^", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
     if(BIT15(i)==0) sprintf(txt, "%s ?????", txt);
return txt;}

//------------------------------STM----------------------------------

static char * OP_STMIA(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMIA%s %s, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_STMIB(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMIB%s %s, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_STMDA(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMDA%s %s, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_STMDB(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMDB%s %s, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_STMIA_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMIA%s %s!, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_STMIB_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMIB%s %s!, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_STMDA_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMDA%s %s!, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_STMDB_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMDB%s %s!, {%s}", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_STMIA2(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMIA%s %s, {%s}^", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_STMIB2(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMIB%s %s, {%s}^", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_STMDA2(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMDA%s %s, {%s}^", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_STMDB2(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMDB%s %s, {%s}^", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_STMIA2_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMIA%s %s!, {%s}^ ?????", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_STMIB2_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMIB%s %s!, {%s}^ ?????", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
return txt;}

static char * OP_STMDA2_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMDA%s %s!, {%s}^ ?????", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
     return txt;
}

static char * OP_STMDB2_W(u32 adr, u32 i, char * txt)
{
     RegList(16);
     sprintf(txt, "STMDB%s %s!, {%s}^ ?????", Condition[CONDITION(i)], Registre[REG_POS(i,16)], lreg);
     return txt;
}

//---------------------STC----------------------------------

static char * OP_STC_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "STC2 CP%X, CR%X, [%s, #%X]", (int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)],(int)((i&0xFF)<<2));
          return txt;
     }
     sprintf(txt, "STC%s %X, CP%X, [%s, #%X]",Condition[CONDITION(i)], (int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)],(int)((i&0xFF)<<2));
     return txt;
}

static char * OP_STC_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "STC2 CP%X, CR%X, [%s, #-%X]",(int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)],(int)((i&0xFF)<<2));
          return txt;
     }
     sprintf(txt, "STC%s CP%X, CR%X, [%s, #-%X]",Condition[CONDITION(i)], (int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_STC_P_PREIND(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "STC2 CP%X, CR%X, [%s, #%X]!",(int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
          return txt;
     }
     sprintf(txt, "STC%s CP%X, CR%X, [%s, #%X]!",Condition[CONDITION(i)], (int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_STC_M_PREIND(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "STC2 CP%X, CR%X, [%s, #-%X]!",(int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
          return txt;
     }
     sprintf(txt, "STC%s CP%X, CR%X, [%s, #-%X]!",Condition[CONDITION(i)], (int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_STC_P_POSTIND(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "STC2 CP%X, CR%X, [%s], #%X",(int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
          return txt;
     }
     sprintf(txt, "STC%s CP%X, CR%X, [%s], #%X",Condition[CONDITION(i)], (int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_STC_M_POSTIND(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "STC2 CP%X, CR%X, [%s], #-%X",(int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
          return txt;
     }
     sprintf(txt, "STC%s CP%X, CR%X, [%s], #-%X",Condition[CONDITION(i)], (int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_STC_OPTION(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "STC2 CP%X, CR%X, [%s], {%X}",(int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)(i&0xFF));
          return txt;
     }
     sprintf(txt, "STC%s CP%X, CR%X, [%s], {%X}",Condition[CONDITION(i)], (int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)(i&0xFF));
     return txt;
}

//---------------------LDC----------------------------------

static char * OP_LDC_P_IMM_OFF(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "LDC2 CP%X, CR%X, [%s, #%X]",(int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
          return txt;
     }
     sprintf(txt, "LDC%s CP%X, CR%X, [%s, #%X]",Condition[CONDITION(i)], (int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_LDC_M_IMM_OFF(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "LDC2 CP%X, CR%X, [%s, #-%X]",(int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
          return txt;
     }
     sprintf(txt, "LDC%s CP%X, CR%X, [%s, #-%X]",Condition[CONDITION(i)], (int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_LDC_P_PREIND(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "LDC2 CP%X, CR%X, [%s, #%X]!",(int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
          return txt;
     }
     sprintf(txt, "LDC%s CP%X, CR%X, [%s, #%X]!",Condition[CONDITION(i)], (int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_LDC_M_PREIND(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "LDC2 CP%X, CR%X, [%s, #-%X]!",(int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
          return txt;
     }
     sprintf(txt, "LDC%s CP%X, CR%X, [%s, #-%X]!",Condition[CONDITION(i)], (int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_LDC_P_POSTIND(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "LDC2 CP%X, CR%X, [%s], #%X",(int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
          return txt;
     }
     sprintf(txt, "LDC%s CP%X, CR%X, [%s], #%X",Condition[CONDITION(i)],  (int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_LDC_M_POSTIND(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "LDC2 CP%X, CR%X, [%s], #-%X", (int)REG_POS(i, 8), (int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
          return txt;
     }
     sprintf(txt, "LDC%s CP%X, CR%X, [%s], #-%X",Condition[CONDITION(i)],  (int)REG_POS(i, 8),(int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_LDC_OPTION(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "LDC2 CP%X, CR%X, [%s], {%X}", (int)REG_POS(i, 8), (int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)(i&0xFF));
          return txt;
     }
     sprintf(txt, "LDC%s CP%X, CR%X, [%s], {%X}",Condition[CONDITION(i)],  (int)REG_POS(i, 8), (int)REG_POS(i, 12), Registre[REG_POS(i, 16)], (int)(i&0xFF));
     return txt;
}


/*
 *
 * The Enhanced DSP Extension LDRD and STRD instructions.
 *
 */
static char *
OP_LDRD_STRD_POST_INDEX(u32 adr, u32 i, char * txt) {
  const char *direction =
    BIT5(i) ? "STR" : "LDR";
  /* U bit - set = add, clear = sub */
  char sign = BIT23(i) ? '+' : '-';
  int txt_index = 0;

  txt_index += sprintf( &txt[txt_index], "%s%sD R%d, [R%d], ",
                        direction, Condition[CONDITION(i)],
                        (int)REG_POS(i, 12),
                        (int)REG_POS(i, 16));

  /* I bit - set = immediate,  clear = reg */
  if ( BIT22(i)) {
    sprintf( &txt[txt_index], "#%c%d",
             sign, (int)(((i>>4) & 0xF0) | (i&0xF)) );
  }
  else {
    sprintf( &txt[txt_index], "%cR%d",
             sign, (int)REG_POS(i, 0));
  }
  return txt;
}
static char *
OP_LDRD_STRD_OFFSET_PRE_INDEX(u32 adr, u32 i, char * txt) {
  const char *direction =
    BIT5(i) ? "STR" : "LDR";
  /* U bit - set = add, clear = sub */
  char sign = BIT23(i) ? '+' : '-';
  int txt_index = 0;

  txt_index += sprintf( &txt[txt_index], "%s%sD R%d, [R%d, ",
                        direction, Condition[CONDITION(i)],
                        (int)REG_POS(i, 12),
                        (int)REG_POS(i, 16));

  /* I bit - set = immediate,  clear = reg */
  if ( BIT22(i)) {
    if ( BIT21(i)) {
      /* pre-index */
      sprintf( &txt[txt_index], "#%c%d]!",
             sign, (int)(((i>>4)&0xF0)|(i&0xF)));
    }
    else {
      /* offset */
      sprintf( &txt[txt_index], "#%c%d]",
             sign, (int)(((i>>4)&0xF0)|(i&0xF)));
    }
  }
  else {
    if ( BIT21(i)) {
      /* pre-index */
      sprintf( &txt[txt_index], "%c%d]!",
             sign, (int)REG_POS(i, 0));
    }
    else {
      /* offset */
      sprintf( &txt[txt_index], "%c%d]",
             sign, (int)REG_POS(i, 0));
    }
  }

  return txt;
}


//----------------MCR-----------------------

static char * OP_MCR(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "MCR2 CP%d, %X, %s, CR%d, CR%d, %X",(int)REG_POS(i, 8), (int)((i>>21)&7), Registre[REG_POS(i, 12)], (int)REG_POS(i, 16), (int)REG_POS(i, 0), (int)((i>>5)&0x7));
          return txt;
     }
     sprintf(txt, "MCR%s CP%d, %X, %s, CR%d, CR%d, %X",Condition[CONDITION(i)], (int)REG_POS(i, 8), (int)((i>>21)&7), Registre[REG_POS(i, 12)], (int)REG_POS(i, 16), (int)REG_POS(i, 0), (int)((i>>5)&0x7));
     return txt;
}

//----------------MRC-----------------------

static char * OP_MRC(u32 adr, u32 i, char * txt)
{
     if(CONDITION(i)==0xF)
     {
          sprintf(txt, "MRC2 CP%d, %X, %s, CR%d, CR%d, %X",(int)REG_POS(i, 8), (int)((i>>21)&7), Registre[REG_POS(i, 12)], (int)REG_POS(i, 16), (int)REG_POS(i, 0), (int)((i>>5)&0x7));
          return txt;
     }
     sprintf(txt, "MRC%s CP%d, %X, %s, CR%d, CR%d, %X",Condition[CONDITION(i)], (int)REG_POS(i, 8), (int)((i>>21)&7), Registre[REG_POS(i, 12)], (int)REG_POS(i, 16), (int)REG_POS(i, 0), (int)((i>>5)&0x7));
     return txt;
}

//--------------SWI--------------------------

static char * OP_SWI(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SWI%s %X",Condition[CONDITION(i)], (int)((i&0xFFFFFF)>>16));
     return txt;
}

//----------------BKPT-------------------------
static char * OP_BKPT(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "BKPT #%X",(int)(((i>>4)&0xFFF)|(i&0xF)));
     return txt;
}

//----------------CDP-----------------------

static char *  OP_CDP(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "CDP-------------------------------");
     return txt;
}

//------------------------------------------------------------
//                         THUMB
//------------------------------------------------------------
#define REG_NUM(i, n) (((i)>>n)&0x7)

static char * OP_UND_THUMB(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "--<UNDEFINED>--");
     return txt;
}
 
static char * OP_LSL_0(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LSL %s, %s, #0", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_LSL(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LSL %s, %s, #%X", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], (int)((i>>6) & 0x1F));
     return txt;
}

static char * OP_LSR_0(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LSR %s, %s, #0", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_LSR(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LSR %s, %s, #%X", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], (int)((i>>6) & 0x1F));
     return txt;
}

static char * OP_ASR_0(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "ASR %s, %s, #0", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_ASR(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "ASR %s, %s, #%X", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], (int)((i>>6) & 0x1F));
     return txt;
}

static char * OP_ADD_REG(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "ADD %s, %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], Registre[REG_NUM(i, 6)]);
     return txt;
}

static char * OP_SUB_REG(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SUB %s, %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], Registre[REG_NUM(i, 6)]);
     return txt;
}

static char * OP_ADD_IMM3(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "ADD %s, %s, #%X", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], (int)REG_NUM(i, 6));
     return txt;
}

static char * OP_SUB_IMM3(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SUB %s, %s, #%X", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], (int)REG_NUM(i, 6));
     return txt;
}

static char * OP_MOV_IMM8(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "MOV %s, #%X", Registre[REG_NUM(i, 8)], (int)(i&0xFF));
     return txt;
}

static char * OP_CMP_IMM8(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "CMP %s, #%X", Registre[REG_NUM(i, 8)], (int)(i&0xFF));
     return txt;
}

static char * OP_ADD_IMM8(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "ADD %s, #%X", Registre[REG_NUM(i, 8)], (int)(i&0xFF));
     return txt;
}

static char * OP_SUB_IMM8(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SUB %s, #%X", Registre[REG_NUM(i, 8)], (int)(i&0xFF));
     return txt;
}

static char * OP_AND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "AND %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_EOR(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "EOR %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_LSL_REG(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LSL %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_LSR_REG(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LSR %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_ASR_REG(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "ASR %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_ADC_REG(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "ADC %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_SBC_REG(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SBC %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_ROR_REG(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "ROR %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_TST(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "TST %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_NEG(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "NEG %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_CMP(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "CMP %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_CMN(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "CMN %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_ORR(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "ORR %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_MUL_REG(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "MUL %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_BIC(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "BIC %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_MVN(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "MVN %s, %s", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)]);
     return txt;
}

static char * OP_ADD_SPE(u32 adr, u32 i, char * txt)
{
     u8 Rd = (i&7) | ((i>>4)&8);
     sprintf(txt, "ADD %s, %s", Registre[Rd], Registre[REG_POS(i, 3)]);
     return txt;
}

static char * OP_CMP_SPE(u32 adr, u32 i, char * txt)
{
     u8 Rd = (i&7) | ((i>>4)&8);
     sprintf(txt, "CMP %s, %s", Registre[Rd], Registre[REG_POS(i, 3)]);
     return txt;
}

static char * OP_MOV_SPE(u32 adr, u32 i, char * txt)
{
     u8 Rd = (i&7) | ((i>>4)&8);
     sprintf(txt, "MOV %s, %s", Registre[Rd], Registre[REG_POS(i, 3)]);
     return txt;
}

static char * OP_BX_THUMB(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "BX %s", Registre[REG_POS(i, 3)]);
     return txt;
}

static char * OP_BLX_THUMB(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "BLX %s", Registre[REG_POS(i, 3)]);
     return txt;
}

static char * OP_LDR_PCREL(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDR %s, [PC, #%X]", Registre[REG_NUM(i, 8)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_STR_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STR %s, [%s, %s]", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], Registre[REG_NUM(i, 6)]);
     return txt;
}

static char * OP_STRH_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRH %s, [%s, %s]", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], Registre[REG_NUM(i, 6)]);
     return txt;
}

static char * OP_STRB_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRB %s, [%s, %s]", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], Registre[REG_NUM(i, 6)]);
     return txt;
}

static char * OP_LDRSB_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSB %s, [%s, %s]", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], Registre[REG_NUM(i, 6)]);
     return txt;
}

static char * OP_LDR_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDR %s, [%s, %s]", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], Registre[REG_NUM(i, 6)]);
     return txt;
}

static char * OP_LDRH_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRH %s, [%s, %s]", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], Registre[REG_NUM(i, 6)]);
     return txt;
}

static char * OP_LDRB_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRB %s, [%s, %s]", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], Registre[REG_NUM(i, 6)]);
     return txt;
}

static char * OP_LDRSH_REG_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRSH %s, [%s, %s]", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], Registre[REG_NUM(i, 6)]);
     return txt;
}

static char * OP_STR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STR %s, [%s, #%X]", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], (int)((i>>4)&0x7C));
     return txt;
}

static char * OP_LDR_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDR %s, [%s, #%X]", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], (int)((i>>4)&0x7C));
     return txt;
}

static char * OP_STRB_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRB %s, [%s, #%X]", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], (int)((i>>6)&0x1F));
     return txt;
}

static char * OP_LDRB_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRB %s, [%s, #%X]", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], (int)((i>>6)&0x1F));
     return txt;
}

static char * OP_STRH_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STRH %s, [%s, #%X]", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], (int)((i>>5)&0x3E));
     return txt;
}

static char * OP_LDRH_IMM_OFF(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDRH %s, [%s, #%X]", Registre[REG_NUM(i, 0)], Registre[REG_NUM(i, 3)], (int)((i>>5)&0x3E));
     return txt;
}

static char * OP_STR_SPREL(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "STR %s, [SP, #%X]", Registre[REG_NUM(i, 8)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_LDR_SPREL(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "LDR %s, [SP, #%X]", Registre[REG_NUM(i, 8)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_ADD_2PC(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "ADD %s, PC, #%X", Registre[REG_NUM(i, 8)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_ADD_2SP(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "ADD %s, SP, #%X", Registre[REG_NUM(i, 8)], (int)((i&0xFF)<<2));
     return txt;
}

static char * OP_ADJUST_P_SP(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "ADD SP, #%X", (int)((i&0x7F)<<2));
     return txt;
}

static char * OP_ADJUST_M_SP(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SUB SP, #%X", (int)((i&0x7F)<<2));
     return txt;
}

static char * OP_PUSH(u32 adr, u32 i, char * txt)
{
     RegList(8);
     sprintf(txt, "PUSH {%s}", lreg);
     return txt;
}

static char * OP_PUSH_LR(u32 adr, u32 i, char * txt)
{
     RegList(8);
     sprintf(txt, "PUSH {%s, LR}", lreg);
     return txt;
}

static char * OP_POP(u32 adr, u32 i, char * txt)
{
     RegList(8);
     sprintf(txt, "POP {%s}", lreg);
     return txt;
}

static char * OP_POP_PC(u32 adr, u32 i, char * txt)
{
     RegList(8);
     sprintf(txt, "POP {%s, PC}", lreg);
     return txt;
}

static char * OP_BKPT_THUMB(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "BKPT");
     return txt;
}

static char * OP_STMIA_THUMB(u32 adr, u32 i, char * txt)
{
     RegList(8);
     sprintf(txt, "STMIA %s!, {%s}", Registre[REG_NUM(i, 8)], lreg);
     return txt;
}

static char * OP_LDMIA_THUMB(u32 adr, u32 i, char * txt)
{
     RegList(8);
     sprintf(txt, "LDMIA %s!, {%s}", Registre[REG_NUM(i, 8)], lreg);
     return txt;
}

static char * OP_B_COND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "B%s #%X", Condition[(i>>8)&0xF], (int)(adr+(((s32)((signed char)(i&0xFF)))<<1)+4));
     return txt;
}

static char * OP_SWI_THUMB(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "SWI #%X", (int)(i & 0xFF));
     return txt;
}

#define SIGNEEXT_IMM11(i)     (((i)&0x7FF) | (BIT10(i) * 0xFFFFF800))

static char * OP_B_UNCOND(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "B #%X", (int)(adr+(SIGNEEXT_IMM11(i)<<1)+4));
     return txt;
}

u32 part = 0;

static char * OP_BLX(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "BLX #%X", (int)(part + ((i&0x7FF)<<1))&0xFFFFFFFC);
     return txt;
}

static char * OP_BL_10(u32 adr, u32 i, char * txt)
{
     part = adr+4 + (SIGNEEXT_IMM11(i)<<12);
     sprintf(txt, "calculating high part of the address");
     return txt;

}

static char * OP_BL_11(u32 adr, u32 i, char * txt)
{
     sprintf(txt, "BL #%X", (int)(part + ((i&0x7FF)<<1))&0xFFFFFFFC);
     return txt;
} 



#define TABDECL(x) x

const DisasmOpFunc des_arm_instructions_set[4096] = {
#include "instruction_tabdef.inc"
};

const DisasmOpFunc des_thumb_instructions_set[1024] = {
#include "thumb_tabdef.inc"
};
