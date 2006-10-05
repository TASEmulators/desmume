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

#include "cp15.h"
#include <math.h>
#include "MMU.h"
#include "debug.h"

extern BOOL execute;

u32 bios_nop(armcpu_t * cpu)
{
     //sprintf(biostxt, "PROC %d, SWI PO IMPLEMENTE %08X R0:%08X", cpu->proc_ID, (cpu->instruction)&0x1F, cpu->R[0]);
     //execute = FALSE;
     //log::ajouter(biostxt);
     return 3;
}

u32 delayLoop(armcpu_t * cpu)
{
     return cpu->R[0] * 4;
}

//u32 oldmode[2];

u32 intrWaitARM(armcpu_t * cpu)
{
     u32 intrFlagAdr;// = (((armcp15_t *)(cpu->coproc[15]))->DTCMRegion&0xFFFFF000)+0x3FF8;
     u32 intr;
     u32 intrFlag = 0;
     
     //execute = FALSE;
     if(cpu->proc_ID) 
     {
      intrFlagAdr = 0x380FFF8;
     } else {
      intrFlagAdr = (((armcp15_t *)(cpu->coproc[15]))->DTCMRegion&0xFFFFF000)+0x3FF8;
     }
     intr = MMU_readWord(cpu->proc_ID, intrFlagAdr);
     intrFlag = cpu->R[1] & intr;
     
     if(intrFlag)
     {
          // si une(ou plusieurs) des interruptions que l'on attend s'est(se sont) produite(s)
          // on efface son(les) occurence(s).
          intr ^= intrFlag;
          MMU_writeWord(cpu->proc_ID, intrFlagAdr, intr);
          //cpu->switchMode(oldmode[cpu->proc_ID]);
          return 1;
     }
         
     cpu->R[15] = cpu->instruct_adr;
     cpu->next_instruction = cpu->R[15];
     cpu->waitIRQ = 1;
     //oldmode[cpu->proc_ID] = cpu->switchMode(SVC);

     return 1;
}

u32 waitVBlankARM(armcpu_t * cpu)
{
     u32 intrFlagAdr;// = (((armcp15_t *)(cpu->coproc[15]))->DTCMRegion&0xFFFFF000)+0x3FF8;
     u32 intr;
     u32 intrFlag = 0;
     
     //execute = FALSE;
     if(cpu->proc_ID) 
     {
      intrFlagAdr = 0x380FFF8;
     } else {
      intrFlagAdr = (((armcp15_t *)(cpu->coproc[15]))->DTCMRegion&0xFFFFF000)+0x3FF8;
     }
     intr = MMU_readWord(cpu->proc_ID, intrFlagAdr);
     intrFlag = 1 & intr;
     
     if(intrFlag)
     {
          // si une(ou plusieurs) des interruptions que l'on attend s'est(se sont) produite(s)
          // on efface son(les) occurence(s).
          intr ^= intrFlag;
          MMU_writeWord(cpu->proc_ID, intrFlagAdr, intr);
          //cpu->switchMode(oldmode[cpu->proc_ID]);
          return 1;
     }
         
     cpu->R[15] = cpu->instruct_adr;
     cpu->next_instruction = cpu->R[15];
     cpu->waitIRQ = 1;
     //oldmode[cpu->proc_ID] = cpu->switchMode(SVC);

     return 1;
}

u32 wait4IRQ(armcpu_t* cpu)
{
     //execute= FALSE;
     if(cpu->wirq)
     {
          if(!cpu->waitIRQ)
          {
               cpu->waitIRQ = 0;
               cpu->wirq = 0;
               //cpu->switchMode(oldmode[cpu->proc_ID]);
               return 1;
          }
          cpu->R[15] = cpu->instruct_adr;
          cpu->next_instruction = cpu->R[15];
          return 1;
     }
     cpu->waitIRQ = 1;
     cpu->wirq = 1;
     cpu->R[15] = cpu->instruct_adr;
     cpu->next_instruction = cpu->R[15];
     //oldmode[cpu->proc_ID] = cpu->switchMode(SVC);
     return 1;
}

u32 devide(armcpu_t* cpu)
{
     s32 num = (s32)cpu->R[0];
     s32 dnum = (s32)cpu->R[1];
     
     if(dnum==0) return 0;
     
     cpu->R[0] = (u32)(num / dnum);
     cpu->R[1] = (u32)(num % dnum);
     cpu->R[3] = (u32) (((s32)cpu->R[0])<0 ? -cpu->R[0] : cpu->R[0]);
     
     return 6;
}

u32 copy(armcpu_t* cpu)
{
     u32 src = cpu->R[0];
     u32 dst = cpu->R[1];
     u32 cnt = cpu->R[2];

     switch(BIT26(cnt))
     {
          case 0:
               src &= 0xFFFFFFFE;
               dst &= 0xFFFFFFFE;
               switch(BIT24(cnt))
               {
                    case 0:
                         cnt &= 0x1FFFFF;
                         while(cnt)
                         {
                              MMU_writeHWord(cpu->proc_ID, dst, MMU_readHWord(cpu->proc_ID, src));
                              cnt--;
                              dst+=2;
                              src+=2;
                         }
                         break;
                    case 1:
                         {
                              u32 val = MMU_readHWord(cpu->proc_ID, src);
                              cnt &= 0x1FFFFF;
                              while(cnt)
                              {
                                   MMU_writeHWord(cpu->proc_ID, dst, val);
                                   cnt--;
                                   dst+=2;
                              }
                         }
                         break;
               }
               break;
          case 1:
               src &= 0xFFFFFFFC;
               dst &= 0xFFFFFFFC;
               switch(BIT24(cnt))
               {
                    case 0:
                         cnt &= 0x1FFFFF;
                         while(cnt)
                         {
                              MMU_writeWord(cpu->proc_ID, dst, MMU_readWord(cpu->proc_ID, src));
                              cnt--;
                              dst+=4;
                              src+=4;
                         }
                         break;
                    case 1:
                         {
                              u32 val = MMU_readWord(cpu->proc_ID, src);
                              cnt &= 0x1FFFFF;
                              while(cnt)
                              {
                                   MMU_writeWord(cpu->proc_ID, dst, val);
                                   cnt--;
                                   dst+=4;
                              }
                         }
                         break;
               }
               break;
     }
     return 1;
}

u32 fastCopy(armcpu_t* cpu)
{
     u32 src = cpu->R[0] & 0xFFFFFFFC;
     u32 dst = cpu->R[1] & 0xFFFFFFFC;
     u32 cnt = cpu->R[2];

     switch(BIT24(cnt))
     {
          case 0:
               cnt &= 0x1FFFFF;
               while(cnt)
               {
                    MMU_writeWord(cpu->proc_ID, dst, MMU_readWord(cpu->proc_ID, src));
                    cnt--;
                    dst+=4;
                    src+=4;
               }
               break;
          case 1:
               {
                    u32 val = MMU_readWord(cpu->proc_ID, src);
                    cnt &= 0x1FFFFF;
                    while(cnt)
                    {
                         MMU_writeWord(cpu->proc_ID, dst, val);
                         cnt--;
                         dst+=4;
                    }
               }
               break;
     }
     return 1;
}

u32 LZ77UnCompVram(armcpu_t* cpu)
{
  int i1, i2;
  u32 source = cpu->R[0];
  u32 dest = cpu->R[1];

  u32 header = MMU_readWord(cpu->proc_ID, source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return 0;    
  
  int byteCount = 0;
  int byteShift = 0;
  u32 writeValue = 0;
  
  int len = header >> 8;

  while(len > 0) {
    u8 d = MMU_readByte(cpu->proc_ID, source++);

    if(d) {
      for(i1 = 0; i1 < 8; i1++) {
        if(d & 0x80) {
          u16 data = MMU_readByte(cpu->proc_ID, source++) << 8;
          data |= MMU_readByte(cpu->proc_ID, source++);
          int length = (data >> 12) + 3;
          int offset = (data & 0x0FFF);
          u32 windowOffset = dest + byteCount - offset - 1;
          for(i2 = 0; i2 < length; i2++) {
            writeValue |= (MMU_readByte(cpu->proc_ID, windowOffset++) << byteShift);
            byteShift += 8;
            byteCount++;

            if(byteCount == 2) {
              MMU_writeHWord(cpu->proc_ID, dest, writeValue);
              dest += 2;
              byteCount = 0;
              byteShift = 0;
              writeValue = 0;
            }
            len--;
            if(len == 0)
              return 0;
          }
        } else {
          writeValue |= (MMU_readByte(cpu->proc_ID, source++) << byteShift);
          byteShift += 8;
          byteCount++;
          if(byteCount == 2) {
            MMU_writeHWord(cpu->proc_ID, dest, writeValue);
            dest += 2;
            byteCount = 0;
            byteShift = 0;
            writeValue = 0;
          }
          len--;
          if(len == 0)
            return 0;
        }
        d <<= 1;
      }
    } else {
      for(i1 = 0; i1 < 8; i1++) {
        writeValue |= (MMU_readByte(cpu->proc_ID, source++) << byteShift);
        byteShift += 8;
        byteCount++;
        if(byteCount == 2) {
          MMU_writeHWord(cpu->proc_ID, dest, writeValue);
          dest += 2;      
          byteShift = 0;
          byteCount = 0;
          writeValue = 0;
        }
        len--;
        if(len == 0)
          return 0;
      }
    }
  }
  return 1;
}

u32 LZ77UnCompWram(armcpu_t* cpu)
{
  int i1, i2;
  u32 source = cpu->R[0];
  u32 dest = cpu->R[1];

  u32 header = MMU_readWord(cpu->proc_ID, source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return 0;  
  
  int len = header >> 8;

  while(len > 0) {
    u8 d = MMU_readByte(cpu->proc_ID, source++);

    if(d) {
      for(i1 = 0; i1 < 8; i1++) {
        if(d & 0x80) {
          u16 data = MMU_readByte(cpu->proc_ID, source++) << 8;
          data |= MMU_readByte(cpu->proc_ID, source++);
          int length = (data >> 12) + 3;
          int offset = (data & 0x0FFF);
          u32 windowOffset = dest - offset - 1;
          for(i2 = 0; i2 < length; i2++) {
            MMU_writeByte(cpu->proc_ID, dest++, MMU_readByte(cpu->proc_ID, windowOffset++));
            len--;
            if(len == 0)
              return 0;
          }
        } else {
          MMU_writeByte(cpu->proc_ID, dest++, MMU_readByte(cpu->proc_ID, source++));
          len--;
          if(len == 0)
            return 0;
        }
        d <<= 1;
      }
    } else {
      for(i1 = 0; i1 < 8; i1++) {
        MMU_writeByte(cpu->proc_ID, dest++, MMU_readByte(cpu->proc_ID, source++));
        len--;
        if(len == 0)
          return 0;
      }
    }
  }
  return 1;
}

u32 RLUnCompVram(armcpu_t* cpu)
{
  int i;
  u32 source = cpu->R[0];
  u32 dest = cpu->R[1];

  u32 header = MMU_readWord(cpu->proc_ID, source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return 0;  
  
  int len = header >> 8;
  int byteCount = 0;
  int byteShift = 0;
  u32 writeValue = 0;

  while(len > 0) {
    u8 d = MMU_readByte(cpu->proc_ID, source++);
    int l = d & 0x7F;
    if(d & 0x80) {
      u8 data = MMU_readByte(cpu->proc_ID, source++);
      l += 3;
      for(i = 0;i < l; i++) {
        writeValue |= (data << byteShift);
        byteShift += 8;
        byteCount++;

        if(byteCount == 2) {
          MMU_writeHWord(cpu->proc_ID, dest, writeValue);
          dest += 2;
          byteCount = 0;
          byteShift = 0;
          writeValue = 0;
        }
        len--;
        if(len == 0)
          return 0;
      }
    } else {
      l++;
      for(i = 0; i < l; i++) {
        writeValue |= (MMU_readByte(cpu->proc_ID, source++) << byteShift);
        byteShift += 8;
        byteCount++;
        if(byteCount == 2) {
          MMU_writeHWord(cpu->proc_ID, dest, writeValue);
          dest += 2;
          byteCount = 0;
          byteShift = 0;
          writeValue = 0;
        }
        len--;
        if(len == 0)
          return 0;
      }
    }
  }
  return 1;
}

u32 RLUnCompWram(armcpu_t* cpu)
{
  int i;
  u32 source = cpu->R[0];
  u32 dest = cpu->R[1];

  u32 header = MMU_readWord(cpu->proc_ID, source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return 0;  
  
  int len = header >> 8;

  while(len > 0) {
    u8 d = MMU_readByte(cpu->proc_ID, source++);
    int l = d & 0x7F;
    if(d & 0x80) {
      u8 data = MMU_readByte(cpu->proc_ID, source++);
      l += 3;
      for(i = 0;i < l; i++) {
        MMU_writeByte(cpu->proc_ID, dest++, data);
        len--;
        if(len == 0)
          return 0;
      }
    } else {
      l++;
      for(i = 0; i < l; i++) {
        MMU_writeByte(cpu->proc_ID, dest++,  MMU_readByte(cpu->proc_ID, source++));
        len--;
        if(len == 0)
          return 0;
      }
    }
  }
  return 1;
}

u32 UnCompHuffman(armcpu_t* cpu)
{
  u32 source, dest, writeValue, header, treeStart, mask;
  u32 data;
  u8 treeSize, currentNode, rootNode;
  int byteCount, byteShift, len, pos;
  int writeData;

  source = cpu->R[0];
  dest = cpu->R[1];

  header = MMU_readByte(cpu->proc_ID, source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return 0;  
  
  treeSize = MMU_readByte(cpu->proc_ID, source++);

  treeStart = source;

  source += ((treeSize+1)<<1)-1; // minus because we already skipped one byte
  
  len = header >> 8;

  mask = 0x80000000;
  data = MMU_readByte(cpu->proc_ID, source);
  source += 4;

  pos = 0;
  rootNode = MMU_readByte(cpu->proc_ID, treeStart);
  currentNode = rootNode;
  writeData = 0;
  byteShift = 0;
  byteCount = 0;
  writeValue = 0;

  if((header & 0x0F) == 8) {
    while(len > 0) {
      // take left
      if(pos == 0)
        pos++;
      else
        pos += (((currentNode & 0x3F)+1)<<1);
      
      if(data & mask) {
        // right
        if(currentNode & 0x40)
          writeData = 1;
        currentNode = MMU_readByte(cpu->proc_ID, treeStart+pos+1);
      } else {
        // left
        if(currentNode & 0x80)
          writeData = 1;
        currentNode = MMU_readByte(cpu->proc_ID, treeStart+pos);
      }
      
      if(writeData) {
        writeValue |= (currentNode << byteShift);
        byteCount++;
        byteShift += 8;

        pos = 0;
        currentNode = rootNode;
        writeData = 0;

        if(byteCount == 4) {
          byteCount = 0;
          byteShift = 0;
          MMU_writeByte(cpu->proc_ID, dest, writeValue);
          writeValue = 0;
          dest += 4;
          len -= 4;
        }
      }
      mask >>= 1;
      if(mask == 0) {
        mask = 0x80000000;
        data = MMU_readByte(cpu->proc_ID, source);
        source += 4;
      }
    }
  } else {
    int halfLen = 0;
    int value = 0;
    while(len > 0) {
      // take left
      if(pos == 0)
        pos++;
      else
        pos += (((currentNode & 0x3F)+1)<<1);

      if((data & mask)) {
        // right
        if(currentNode & 0x40)
          writeData = 1;
        currentNode = MMU_readByte(cpu->proc_ID, treeStart+pos+1);
      } else {
        // left
        if(currentNode & 0x80)
          writeData = 1;
        currentNode = MMU_readByte(cpu->proc_ID, treeStart+pos);
      }
      
      if(writeData) {
        if(halfLen == 0)
          value |= currentNode;
        else
          value |= (currentNode<<4);

        halfLen += 4;
        if(halfLen == 8) {
          writeValue |= (value << byteShift);
          byteCount++;
          byteShift += 8;
          
          halfLen = 0;
          value = 0;

          if(byteCount == 4) {
            byteCount = 0;
            byteShift = 0;
            MMU_writeByte(cpu->proc_ID, dest, writeValue);
            dest += 4;
            writeValue = 0;
            len -= 4;
          }
        }
        pos = 0;
        currentNode = rootNode;
        writeData = 0;
      }
      mask >>= 1;
      if(mask == 0) {
        mask = 0x80000000;
        data = MMU_readByte(cpu->proc_ID, source);
        source += 4;
      }
    }    
  }
  return 1;
}

u32 BitUnPack(armcpu_t* cpu)
{
  u32 source,dest,header,base,d,temp;
  int len,bits,revbits,dataSize,data,bitwritecount,mask,bitcount,addBase;
  u8 b;

  source = cpu->R[0];
  dest = cpu->R[1];
  header = cpu->R[2];
  
  len = MMU_readHWord(cpu->proc_ID, header);
  // check address
  bits = MMU_readByte(cpu->proc_ID, header+2);
  revbits = 8 - bits; 
  // u32 value = 0;
  base = MMU_readByte(cpu->proc_ID, header+4);
  addBase = (base & 0x80000000) ? 1 : 0;
  base &= 0x7fffffff;
  dataSize = MMU_readByte(cpu->proc_ID, header+3);

  data = 0; 
  bitwritecount = 0; 
  while(1) {
    len -= 1;
    if(len < 0)
      break;
    mask = 0xff >> revbits; 
    b = MMU_readByte(cpu->proc_ID, source); 
    source++;
    bitcount = 0;
    while(1) {
      if(bitcount >= 8)
        break;
      d = b & mask;
      temp = d >> bitcount;
      if(!temp && addBase) {
        temp += base;
      }
      data |= temp << bitwritecount;
      bitwritecount += dataSize;
      if(bitwritecount >= 32) {
        MMU_writeByte(cpu->proc_ID, dest, data);
        dest += 4;
        data = 0;
        bitwritecount = 0;
      }
      mask <<= bits;
      bitcount += bits;
    }
  }
  return 1;
}

u32 Diff8bitUnFilterWram(armcpu_t* cpu)
{
  u32 source,dest,header;
  u8 data,diff;
  int len;

  source = cpu->R[0];
  dest = cpu->R[1];

  header = MMU_readByte(cpu->proc_ID, source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff) & 0xe000000) == 0))
    return 0;  

  len = header >> 8;

  data = MMU_readByte(cpu->proc_ID, source++);
  MMU_writeByte(cpu->proc_ID, dest++, data);
  len--;
  
  while(len > 0) {
    diff = MMU_readByte(cpu->proc_ID, source++);
    data += diff;
    MMU_writeByte(cpu->proc_ID, dest++, data);
    len--;
  }
  return 1;
}

u32 Diff16bitUnFilter(armcpu_t* cpu)
{
  u32 source,dest,header;
  u16 data;
  int len;

  source = cpu->R[0];
  dest = cpu->R[1];

  header = MMU_readByte(cpu->proc_ID, source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return 0;  
  
  len = header >> 8;

  data = MMU_readHWord(cpu->proc_ID, source);
  source += 2;
  MMU_writeHWord(cpu->proc_ID, dest, data);
  dest += 2;
  len -= 2;
  
  while(len >= 2) {
    u16 diff = MMU_readHWord(cpu->proc_ID, source);
    source += 2;
    data += diff;
    MMU_writeHWord(cpu->proc_ID, dest, data);
    dest += 2;
    len -= 2;
  }
  return 1;
}

u32 bios_sqrt(armcpu_t* cpu)
{
     cpu->R[0] = (u32)sqrt((double)(cpu->R[0]));
     return 1;
}

u32 setHaltCR(armcpu_t* cpu)
{ 
     MMU_writeByte(cpu->proc_ID, 0x4000300+cpu->proc_ID, cpu->R[0]);
     return 1;
}

u32 getPitchTab(armcpu_t* cpu)
{ 
     return 1;
}

u32 getVolumeTab(armcpu_t* cpu)
{ 
     return 1;
}

u32 (* ARM9_swi_tab[32])(armcpu_t* cpu)={
         bios_nop,
         bios_nop,
         bios_nop,
         delayLoop,
         intrWaitARM,
         waitVBlankARM,
         wait4IRQ,
         bios_nop,
         bios_nop,
         devide,
         bios_nop,
         copy,
         fastCopy,
         bios_sqrt,
         bios_nop,
         bios_nop,
         BitUnPack,
         LZ77UnCompWram,
         LZ77UnCompVram,
         UnCompHuffman,
         RLUnCompWram,
         RLUnCompVram,
         Diff8bitUnFilterWram,
         bios_nop,
         Diff16bitUnFilter,
         bios_nop,
         bios_nop,
         bios_nop,
         bios_nop,
         bios_nop,
         bios_nop,
         setHaltCR,
};

u32 (* ARM7_swi_tab[32])(armcpu_t* cpu)={
         bios_nop,
         bios_nop,
         bios_nop,
         delayLoop,
         intrWaitARM,
         waitVBlankARM,
         wait4IRQ,
         wait4IRQ,
         bios_nop,
         devide,
         bios_nop,
         copy,
         fastCopy,
         bios_sqrt,
         bios_nop,
         bios_nop,
         BitUnPack,
         LZ77UnCompWram,
         LZ77UnCompVram,
         UnCompHuffman,
         RLUnCompWram,
         RLUnCompVram,
         Diff8bitUnFilterWram,
         bios_nop,
         bios_nop,
         bios_nop,
         bios_nop,
         getPitchTab,
         getVolumeTab,
         bios_nop,
         bios_nop,
         setHaltCR,
};
