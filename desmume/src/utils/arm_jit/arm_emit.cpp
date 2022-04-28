/*
	Copyright (C) 2022 Patrick Herlihy - https://byte4byte.com
	Copyright (C) 2022 DeSmuME team

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
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define R11 11
#define R10 10
#define FP 11
#define LR 14
#define PC 15
#define SP 13
#define R0 0
#define R1 1
#define R2 2
#define R3 3
#define R4 4
#define R5 5
#define R6 6
#define R7 7
#define R8 8
#define R9 9

#define emit_write_ptr(o, p1, p2) emit_write_ptr32(o, p1, p2)
#define emit_ptr(o, ptr, r) emit_movimm(o, ptr, r)
#define emit_add64(o, r1, r2, r3) emit_add(o, r1, r2, r3)
#define emit_mov64(o, r1, r2) emit_mov(o, r1, r2)
#define emit_read_ptr64_reg(o, s, r) emit_read_ptr32_reg(o, s, r)
#define emit_addimm64(o, r1, r2, r3) emit_addimm(o, r1, r2, r3)

static u_int rd_rn_rm(u_int rd, u_int rn, u_int rm)
{
  return((rn<<16)|(rd<<12)|rm);
}

static u_int rd_rn_rm_lsl(u_int rd, u_int rn, u_int rm, u_int shift)
{
  return((rn<<16)|(rd<<12)|rm)|((shift*8)<<4);
}

struct s_bytes;
typedef struct s_bytes t_bytes;

static void emit_setc(t_bytes *out, int reg);

static void output_w32(t_bytes *bytes, unsigned int word);

static int g_allocedSize = 0;

static u_int genimm(u_int imm,u_int *encoded)
{
  if(imm==0) {*encoded=0;return 1;}
  int i=32;
  while(i>0)
  {
    if(imm<256) {
      *encoded=((i&30)<<7)|imm;
      return 1;
    }
    imm=(imm>>2)|(imm<<30);i-=2;
  }
  return 0;
}

static void output_w32_FROM_PC(t_bytes *bytes, unsigned int word);
static void emit_movimm(t_bytes *out, u_int imm,u_int rt);
static void emit_movimm2pad(t_bytes *out, u_int imm,u_int rt);
static void emit_addnop(t_bytes *out, u_int r);

static void output_w32(t_bytes *bytes, u32 word);

static void output_w32_full(t_bytes *bytes, unsigned int word, int type, int label_id, int cond);

static void emit_movimm(t_bytes *out, u_int imm,u_int rt);
static void emit_mov(t_bytes *out, int rs,int rt);
static void emit_jmpreg(t_bytes *out, u_int r);

static u_int rd_rn_imm_shift(u_int rd, u_int rn, u_int imm, u_int shift)
{
  return((rn<<16)|(rd<<12)|(((32-shift)&30)<<7)|imm);
}


static u_int rd_rn_rm_asr(u_int rd, u_int rn, u_int rm, u_int shift)
{
  return((rn<<16)|(rd<<12)|rm)|((shift*8+(0xC-8))<<4);
}

static u_int rd_rn_rm_lsr(u_int rd, u_int rn, u_int rm, u_int shift)
{
  return((rn<<16)|(rd<<12)|rm)|((shift*8+(0xA - 8))<<4);
}

static u_int rd_rn_rm_ror(u_int rd, u_int rn, u_int rm, u_int shift)
{
  return((rn<<16)|(rd<<12)|rm)|((shift*8+(0xE - 8))<<4);
}


static void emit_mov_lsl(t_bytes *out, int rs, int rt, int shift) {
  output_w32(out, 0xe1a00000|rd_rn_rm_lsl(rt,0, rs, shift));
}
static void emit_mov_lsr(t_bytes *out, int rs, int rt, int shift) {
	output_w32(out, 0xE1A00000|rd_rn_rm_lsr(rt,0, rs, shift));
}
static void emit_mov_ror(t_bytes *out, int rs, int rt, int shift) {
	output_w32(out, 0xE1A00000|rd_rn_rm_ror(rt,0, rs, shift));
}
static void emit_mov_asr(t_bytes *out, int rs, int rt, int shift) {
	output_w32(out, 0xE1A00000|rd_rn_rm_asr(rt,0, rs, shift));
}
static void emit_mov_asl(t_bytes *out, int rs, int rt, int shift) {
	emit_mov_lsl(out, rs, rt, shift);
}

#define SHIFT_REG R0


static unsigned int shift_reg(int reg, int num) {
	return (reg<<12)|reg|(num<<8);
}
static void emit_lsl(t_bytes *out, int reg, int n) {
	if (n < 32) {
		output_w32(out, 0xE1A00000|rd_rn_rm_lsl(reg,0, reg, n));
	}
	else {
		emit_movimm(out, n, SHIFT_REG);
		output_w32(out, 0xE1A00010|shift_reg(reg, SHIFT_REG));
	}
}
static void emit_lsr(t_bytes *out, int reg, int n) {
	if (n < 32) {
		output_w32(out, 0xE1A00000|rd_rn_rm_lsr(reg,0, reg, n));
	}
	else {
		emit_movimm(out, n, SHIFT_REG);
		output_w32(out, 0xE1A00030|shift_reg(reg, SHIFT_REG));
	}
}
static void emit_asr(t_bytes *out, int reg, int n) {
	if (n < 32) {
		output_w32(out, 0xE1A00000|rd_rn_rm_asr(reg,0, reg, n));
	}
	else {
		emit_movimm(out, n, SHIFT_REG);
		output_w32(out, 0xE1A00050|shift_reg(reg, SHIFT_REG));
	}
}
static void emit_ror(t_bytes *out, int reg, int n) {
	if (n < 32) {
		output_w32(out, 0xE1A00000|rd_rn_rm_ror(reg,0, reg, n));
	}
	else {
		emit_movimm(out, n, SHIFT_REG);
		output_w32(out, 0xE1A00070|shift_reg(reg, SHIFT_REG));
	}
}
static void emit_rrx(t_bytes *out, int reg, int n) {

	emit_movimm(out, n, SHIFT_REG);
	output_w32(out, 0xE1A00060|shift_reg(reg, SHIFT_REG));
}
static void emit_lsl_reg(t_bytes *out, int reg, int nreg) {
	output_w32(out, 0xE1A00010|shift_reg(reg, nreg));
}
static void emit_lsr_reg(t_bytes *out, int reg, int nreg) {
	output_w32(out, 0xE1A00030|shift_reg(reg, nreg));
}
static void emit_asr_reg(t_bytes *out, int reg, int nreg) {
	output_w32(out, 0xE1A00050|shift_reg(reg, nreg));
}
static void emit_ror_reg(t_bytes *out, int reg, int nreg) {
	output_w32(out, 0xE1A00070|shift_reg(reg, nreg));
}
static void emit_rrx_reg(t_bytes *out, int reg, int nreg) {
	output_w32(out, 0xE1A00060|shift_reg(reg, nreg));
}

static unsigned int set_flags() {
	return 0x100000;
}

static void emit_lsls(t_bytes *out, int reg, int n) {
	if (n < 32) {
		output_w32(out, 0xE1A00000|rd_rn_rm_lsl(reg,0, reg, n)|set_flags());
    emit_setc(out, 5);
	}
	else {
		emit_movimm(out, n, SHIFT_REG);
		output_w32(out, 0xE1A00010|shift_reg(reg, SHIFT_REG)|set_flags());
    emit_setc(out, 5);
	}
}
static void emit_lsrs(t_bytes *out, int reg, int n) {
	if (n < 32) {
		output_w32(out, 0xE1A00000|rd_rn_rm_lsr(reg,0, reg, n)|set_flags());
    emit_setc(out, 5);
	}
	else {
		emit_movimm(out, n, SHIFT_REG);
		output_w32(out, 0xE1A00030|shift_reg(reg, SHIFT_REG)|set_flags());
    emit_setc(out, 5);
	}
}
static void emit_asrs(t_bytes *out, int reg, int n) {
	if (n < 32) {
		output_w32(out, 0xE1A00000|rd_rn_rm_asr(reg,0, reg, n)|set_flags());
    emit_setc(out, 5);
	}
	else {
		emit_movimm(out, n, SHIFT_REG);
		output_w32(out, 0xE1A00050|shift_reg(reg, SHIFT_REG)|set_flags());
    emit_setc(out, 5);
	}
}
static void emit_rors(t_bytes *out, int reg, int n) {
	if (n < 32) {
		output_w32(out, 0xE1A00000|rd_rn_rm_ror(reg,0, reg, n)|set_flags());
    emit_setc(out, 5);
	}
	else {
		emit_movimm(out, n, SHIFT_REG);
		output_w32(out, 0xE1A00070|shift_reg(reg, SHIFT_REG)|set_flags());
    emit_setc(out, 5);
	}
}
static void emit_rrxs(t_bytes *out, int reg, int n) {
	emit_movimm(out, n, SHIFT_REG);
	output_w32(out, 0xE1A00060|shift_reg(reg, SHIFT_REG)|set_flags());
  emit_setc(out, 5);
}
static void emit_lsls_reg(t_bytes *out, int reg, int nreg) {
	output_w32(out, 0xE1A00010|shift_reg(reg, nreg)|set_flags());
  emit_setc(out, 5);
}
static void emit_lsrs_reg(t_bytes *out, int reg, int nreg) {
	output_w32(out, 0xE1A00030|shift_reg(reg, nreg)|set_flags());
  emit_setc(out, 5);
}
static void emit_asrs_reg(t_bytes *out, int reg, int nreg) {
	output_w32(out, 0xE1A00050|shift_reg(reg, nreg)|set_flags());
  emit_setc(out, 5);
}
static void emit_rors_reg(t_bytes *out, int reg, int nreg) {
	output_w32(out, 0xE1A00070|shift_reg(reg, nreg)|set_flags());
  emit_setc(out, 5);
}
static void emit_rrxs_reg(t_bytes *out, int reg, int nreg) {
	output_w32(out, 0xE1A00060|shift_reg(reg, nreg)|set_flags());
	emit_setc(out, 5);
}

static void emit_mov(t_bytes *out, int rs,int rt)
{
  output_w32(out, 0xe1a00000|rd_rn_rm(rt,0,rs));
}

static void emit_mvns(t_bytes *out, int rs,int rt)
{
  output_w32(out, 0xE1E00000|rd_rn_rm(rt,0,rs)|set_flags());
}

static void emit_mov_cond(t_bytes *out, int rs,int rt, int cond)
{
  output_w32(out, 0x01a00000|rd_rn_rm(rt,0,rs)|(cond<<28));
}

static void emit_movs(t_bytes *out, int rs,int rt)
{
  output_w32(out, 0xe1b00000|rd_rn_rm(rt,0,rs));
}

static void emit_add(t_bytes *out, int rs1,int rs2,int rt)
{
  output_w32(out, 0xe0800000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_adds(t_bytes *out, int rs1,int rs2,int rt)
{
  output_w32(out, 0xe0900000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_adc(t_bytes *out, int rs1,int rs2,int rt)
{
  output_w32(out, 0xe0a00000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_adcs(t_bytes *out, int rs1,int rs2,int rt)
{
  output_w32(out, 0xe0b00000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_sbc(t_bytes *out, int rs1,int rs2,int rt)
{
  output_w32(out, 0xe0c00000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_sbcs(t_bytes *out, int rs1,int rs2,int rt)
{
  output_w32(out, 0xe0d00000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_neg(t_bytes *out, int rs, int rt)
{
  output_w32(out, 0xe2600000|rd_rn_rm(rt,rs,0));
}

static void emit_negs(t_bytes *out, int rs, int rt)
{
  output_w32(out, 0xe2700000|rd_rn_rm(rt,rs,0));
}

static void emit_sub(t_bytes *out, int rs1,int rs2,int rt)
{
  output_w32(out, 0xe0400000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_subs(t_bytes *out, int rs1,int rs2,int rt)
{
  output_w32(out, 0xe0500000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_zeroreg(t_bytes *out, int rt)
{
  output_w32(out, 0xe3a00000|rd_rn_rm(rt,0,0));
}

static void emit_movw(t_bytes *out, u_int imm,u_int rt)
{
  output_w32(out, 0xe3000000|rd_rn_rm(rt,0,0)|(imm&0xfff)|((imm<<4)&0xf0000));
}
static void emit_movt(t_bytes *out, u_int imm,u_int rt)
{
  output_w32(out, 0xe3400000|rd_rn_rm(rt,0,0)|((imm>>16)&0xfff)|((imm>>12)&0xf0000));
}
static void emit_movw_cond(t_bytes *out, u_int imm,u_int rt, int cond)
{
  output_w32(out, (0x03000000|rd_rn_rm(rt,0,0)|(imm&0xfff)|((imm<<4)&0xf0000))|(cond<<28));
}
static void emit_movt_cond(t_bytes *out, u_int imm,u_int rt, int cond)
{
  output_w32(out, (0x03400000|rd_rn_rm(rt,0,0)|((imm>>16)&0xfff)|((imm>>12)&0xf0000))|(cond<<28));
}

static void emit_movimms(t_bytes *out, u_int imm,u_int rt)
{
	u_int armval=0;
	genimm(imm,&armval);
	output_w32(out, 0xe3a00000|rd_rn_rm(rt,0,0)|armval|set_flags());
}

static void emit_movimm(t_bytes *out, u_int imm,u_int rt)
{
  u_int armval;
  if(genimm(imm,&armval)) {
    output_w32(out, 0xe3a00000|rd_rn_rm(rt,0,0)|armval);
  }else if(genimm(~imm,&armval)) {
    output_w32(out, 0xe3e00000|rd_rn_rm(rt,0,0)|armval);
  }else if(imm<65536) {
    #ifdef ARMv5_ONLY
    output_w32(out, 0xe3a00000|rd_rn_imm_shift(rt,0,imm>>8,8));
    output_w32(out, 0xe2800000|rd_rn_imm_shift(rt,rt,imm&0xff,0));
    #else
    emit_movw(out, imm,rt);
	emit_movt(out, 0, rt);
    #endif
  }else{
    #ifdef ARMv5_ONLY
    emit_loadlp(out, imm,rt);
    #else
    emit_movw(out, imm&0x0000FFFF,rt);
    emit_movt(out, imm&0xFFFF0000,rt);
    #endif
  }
}

static void emit_test(t_bytes *out, int rs, int rt)
{
  output_w32(out, 0xe1100000|rd_rn_rm(0,rs,rt));
}

static void emit_testimm(t_bytes *out, int rs,int imm);

static void emit_not(t_bytes *out, int rs,int rt)
{
  output_w32(out, 0xe1e00000|rd_rn_rm(rt,0,rs));
}

static void emit_and(t_bytes *out, u_int rs1,u_int rs2,u_int rt)
{
  output_w32(out, 0xe0000000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_or(t_bytes *out, u_int rs1,u_int rs2,u_int rt)
{
  output_w32(out, 0xe1800000|rd_rn_rm(rt,rs1,rs2));
}
static void emit_or_and_set_flags(t_bytes *out, int rs1,int rs2,int rt)
{
  output_w32(out, 0xe1900000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_xor(t_bytes *out, u_int rs1,u_int rs2,u_int rt)
{
  output_w32(out, 0xe0200000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_ands(t_bytes *out, u_int rs1,u_int rs2,u_int rt)
{
  output_w32(out, 0xe0100000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_ors(t_bytes *out, u_int rs1,u_int rs2,u_int rt)
{
 emit_or_and_set_flags(out, rs1,rs2,rt);
}
static void emit_xors(t_bytes *out, u_int rs1,u_int rs2,u_int rt)
{
  output_w32(out, 0xe0300000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_addimm(t_bytes *out, u_int rs,int imm,u_int rt)
{
  if(imm!=0) {
    u_int armval;
    if(genimm(imm,&armval)) {
      output_w32(out, 0xe2800000|rd_rn_rm(rt,rs,0)|armval);
    }else if(genimm(-imm,&armval)) {
      output_w32(out, 0xe2400000|rd_rn_rm(rt,rs,0)|armval);
    }else if(imm<0) {
      output_w32(out, 0xe2400000|rd_rn_imm_shift(rt,rs,(-imm)>>8,8));
      output_w32(out, 0xe2400000|rd_rn_imm_shift(rt,rt,(-imm)&0xff,0));
    }else{
      output_w32(out, 0xe2800000|rd_rn_imm_shift(rt,rs,imm>>8,8));
      output_w32(out, 0xe2800000|rd_rn_imm_shift(rt,rt,imm&0xff,0));
    }
  }
  else if(rs!=rt) emit_mov(out, rs,rt);
}

static void emit_addsimm(t_bytes *out, u_int rs,int imm,u_int rt)
{
  if(imm!=0) {
    u_int armval;
    if(genimm(imm,&armval)) {
      output_w32(out, 0xe2800000|set_flags()|rd_rn_rm(rt,rs,0)|armval);
    }else if(genimm(-imm,&armval)) {
      output_w32(out, 0xe2400000|set_flags()|rd_rn_rm(rt,rs,0)|armval);
    }else if(imm<0) {
      output_w32(out, 0xe2400000|set_flags()|rd_rn_imm_shift(rt,rs,(-imm)>>8,8));
      output_w32(out, 0xe2400000|set_flags()|rd_rn_imm_shift(rt,rt,(-imm)&0xff,0));
    }else{
      output_w32(out, 0xe2800000|set_flags()|rd_rn_imm_shift(rt,rs,imm>>8,8));
      output_w32(out, 0xe2800000|set_flags()|rd_rn_imm_shift(rt,rt,imm&0xff,0));
    }
  }
  else if(rs!=rt) emit_mov(out, rs,rt);
}

static void emit_addimm_and_set_flags(t_bytes *out,int imm,int rt)
{
  u_int armval;
  if(genimm(imm,&armval)) {
    output_w32(out, 0xe2900000|rd_rn_rm(rt,rt,0)|armval);
  }else if(genimm(-imm,&armval)) {
    output_w32(out, 0xe2500000|rd_rn_rm(rt,rt,0)|armval);
  }else if(imm<0) {
    output_w32(out, 0xe2400000|rd_rn_imm_shift(rt,rt,(-imm)>>8,8));
    output_w32(out, 0xe2500000|rd_rn_imm_shift(rt,rt,(-imm)&0xff,0));
  }else{
    output_w32(out, 0xe2800000|rd_rn_imm_shift(rt,rt,imm>>8,8));
    output_w32(out, 0xe2900000|rd_rn_imm_shift(rt,rt,imm&0xff,0));
  }
}

static void emit_cmpimm(t_bytes *out, int reg, int val);

static void emit_cmp(t_bytes *out, int rs,int rt)
{
  output_w32(out, 0xe1500000|rd_rn_rm(0,rs,rt));
}

static void emit_smulbb(t_bytes *out, u_int rs1, u_int rs2) {
	output_w32(out, 0xE1600080|(rs1<<16)|(rs2<<8)|rs1);
}

static void emit_smulbt(t_bytes *out, u_int rs1, u_int rs2) {
	output_w32(out, 0xE16000C0|(rs1<<16)|(rs2<<8)|rs1);
}

static void emit_smultb(t_bytes *out, u_int rs1, u_int rs2) {
	output_w32(out, 0xE16000A0|(rs1<<16)|(rs2<<8)|rs1);
}

static void emit_smultt(t_bytes *out, u_int rs1, u_int rs2) {
	output_w32(out, 0xE16000E0|(rs1<<16)|(rs2<<8)|rs1);
}


static void emit_smulwt(t_bytes *out, u_int rs1, u_int rs2) {
	output_w32(out, 0xE12000E0|(rs1<<16)|(rs1<<8)|rs2);
}
static void emit_smulwb(t_bytes *out, u_int rs1, u_int rs2) {
	output_w32(out, 0xE12000A0|(rs1<<16)|(rs1<<8)|rs2);
}

static void emit_smlawb(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2)
{
  output_w32(out, 0xE1200080|(hi<<16)|(lo<<12)|(rs1<<8)|rs2);
}
static void emit_smlawt(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2)
{
  output_w32(out, 0xE12000C0|(hi<<16)|(lo<<12)|(rs1<<8)|rs2);
}

static void emit_smlabb(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2)
{
  output_w32(out, 0xE1000080|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}
static void emit_smlabt(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2)
{
  output_w32(out, 0xE10000C0|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}
static void emit_smlatb(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2)
{
  output_w32(out, 0xE10000A0|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}
static void emit_smlatt(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2)
{
  output_w32(out, 0xE10000E0|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}

static void emit_smlalbb(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2)
{
  output_w32(out, 0xE1400080|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}
static void emit_smlalbt(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2)
{
  output_w32(out, 0xE14000C0|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}
static void emit_smlaltb(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2)
{
  output_w32(out, 0xE14000A0|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}
static void emit_smlaltt(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2)
{
  output_w32(out, 0xE14000E0|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}

static void emit_mul(t_bytes *out, u_int rs1,u_int rs2)
{
  output_w32(out, 0xe0000090|(rs1<<16)|(rs2<<8)|rs1);
}
static void emit_muls(t_bytes *out, u_int rs1,u_int rs2)
{
  output_w32(out, 0xe0000090|(rs1<<16)|(rs2<<8)|rs1|set_flags());
}

static void emit_mla(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2)
{
  output_w32(out, 0xE0200090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}
static void emit_mlas(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2)
{
  output_w32(out, 0xE0200090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}

static void emit_umull(t_bytes *out, u_int lo, u_int hi, u_int rs1, u_int rs2)
{
  output_w32(out, 0xe0800090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}
static void emit_umlal(t_bytes *out, u_int lo, u_int hi, u_int rs1, u_int rs2)
{
  output_w32(out, 0xe0a00090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}
static void emit_smull(t_bytes *out, u_int lo, u_int hi, u_int rs1, u_int rs2)
{
  output_w32(out, 0xe0c00090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}
static void emit_smulltemp(t_bytes *out, u_int hi, u_int lo,  u_int rs2)
{
  emit_mov(out, lo, R0);
  u_int rs1 = R0;
  output_w32(out, 0xe0c00090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}
static void emit_smlal(t_bytes *out, u_int lo, u_int hi, u_int rs1, u_int rs2)
{
  output_w32(out, 0xe0e00090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}

static void emit_umulls(t_bytes *out, u_int lo, u_int hi, u_int rs1, u_int rs2)
{
  output_w32(out, 0xe0800090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1|set_flags());
}
static void emit_umlals(t_bytes *out, u_int lo, u_int hi, u_int rs1, u_int rs2)
{
  output_w32(out, 0xe0a00090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1|set_flags());
}
static void emit_smulls(t_bytes *out, u_int lo, u_int hi, u_int rs1, u_int rs2)
{
  output_w32(out, 0xe0c00090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1|set_flags());
}
static void emit_smlals(t_bytes *out, u_int lo, u_int hi, u_int rs1, u_int rs2)
{
  output_w32(out, 0xe0e00090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1|set_flags());
}

static void emit_clz(t_bytes *out, int rs,int rt)
{
  output_w32(out,0xe16f0f10|rd_rn_rm(rt,0,rs));
}

static void emit_subcs(t_bytes *out, int rs1,int rs2,int rt)
{
  output_w32(out,0x20400000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_teq(t_bytes *out, int rs, int rt)
{
  output_w32(out,0xe1300000|rd_rn_rm(0,rs,rt));
}

typedef unsigned int (*JittedFunc)();

#define MOD_JUMP(ptr, cond, lbl_bytes, jmp_bytes) *(ptr) = 0x0a000000|(((lbl_bytes-jmp_bytes-8)>>2)&0xffffff)|(cond<<28);
#include "emit_core.cpp"

static void myfunc(const char *str) {
	puts(str);
}

static void emit_ldr(t_bytes *out, int rs, int rt) {
	output_w32(out, 0xe5900000|rd_rn_rm(rt,rs,0)|0);
}

static void emit_str(t_bytes *out, int rs, int rt) {
	output_w32(out, 0xe5800000|rd_rn_rm(rt,rs,0)|0);
}

static void emit_str_cond(t_bytes *out, int rs, int rt, int cond) {
	output_w32(out, 0x05800000|rd_rn_rm(rt,rs,0)|(cond<<28));
}

static void emit_ldrb(t_bytes *out, int rs, int rt) {
	output_w32(out, 0xE5D00000|rd_rn_rm(rt,rs,0)|0); // LDRB	R0, [R0] <-- armconverter.com
}

static void emit_ldrsb(t_bytes *out, int rs, int rt) {
	output_w32(out, 0xE1D000D0|rd_rn_rm(rt,rs,0)|0); // LDRSB	R0, [R0] <-- armconverter.com
}

static void emit_strb(t_bytes *out, int rs, int rt) {
	output_w32(out, 0xE5C00000|rd_rn_rm(rt,rs,0)|0);
}

static void emit_ldrsw(t_bytes *out, int rs, int rt) {
	output_w32(out, 0xE1D000F0|rd_rn_rm(rt,rs,0)|0); // LDRSB	R0, [R0] <-- armconverter.com
}

static void emit_ldrs(t_bytes *out, int rs, int rt) {
	emit_ldr(out, rs, rt);
}

static void emit_ldrw(t_bytes *out, int rs, int rt) {
	output_w32(out, 0xE1D000B0|rd_rn_rm(rt,rs,0)|0); // LDRB	R0, [R0] <-- armconverter.com
}

static void emit_strw(t_bytes *out, int rs, int rt) {
	output_w32(out, 0xE1C000B0|rd_rn_rm(rt,rs,0)|0);
}

static void emit_push(t_bytes *out, int rs) {
	emit_addimm(out, SP, -4, SP);
	emit_str(out, SP, rs);
}

static void emit_pop(t_bytes *out, int rs) {
	emit_ldr(out, SP, rs);
	emit_addimm(out, SP, 4, SP);
}


static void emit_str_ptr_cond(t_bytes *out, uintptr_t ptr, int reg, int cond) {
	emit_movimm(out, (uintptr_t)ptr, R10);
	emit_str_cond(out, R10, reg, cond);
}

static void printR1(unsigned int blah, unsigned int r) {
	//LOGE("R1 = %u", r);
}

static void emit_write_ptr32(t_bytes *out, uintptr_t ptr, uintptr_t val) {
	emit_movimm(out, ptr, R2);
	emit_movimm(out, val, R4);
	emit_str(out, R2, R4);
}

static void emit_write_ptr32_regptrTO_immFROM(t_bytes *out, int regptr, int val) { /* reg contains ptr */
	emit_movimm(out, (uintptr_t)val, R10);
	emit_str(out, regptr, R10);
}
static void emit_write_ptr32_regptrTO_regFROM(t_bytes *out, int regptr, int reg) { /* reg contains ptr */
	emit_str(out, regptr, reg);
}
static void emit_write_ptr32_regptrTO_regptrFROM(t_bytes *out, int regptrto, int regptrfrom) { /* reg contains ptr */
	emit_ldr(out, regptrfrom, regptrfrom);
	emit_str(out, regptrto, regptrfrom);
}
static void emit_read_ptr32_regptrFROM_regTO(t_bytes *out, int regptr, int regout) {
	emit_ldr(out, regptr, regout);
}

static void emit_write_ptr32_reg(t_bytes *out, uintptr_t ptr, int reg) {
	emit_movimm(out, (uintptr_t)ptr, R8);
	emit_str(out, R8, reg);
}

static void emit_writeBYTE_ptr32_regptrTO_immFROM(t_bytes *out, int regptr, int val) { /* reg contains ptr */
	emit_movimm(out, (uintptr_t)val, R9);
	emit_strb(out, regptr, R9);
}
static void emit_writeBYTE_ptr32_regptrTO_regFROM(t_bytes *out, int regptr, int reg) { /* reg contains ptr */
	emit_strb(out, regptr, reg);
}
static void emit_writeBYTE_ptr32_regptrTO_regptrFROM(t_bytes *out, int regptrto, int regptrfrom) { /* reg contains ptr */
	emit_ldrb(out, regptrfrom, regptrfrom);
	emit_strb(out, regptrto, regptrfrom);
}
static void emit_readBYTE_ptr32_regptrFROM_regTO(t_bytes *out, int regptr, int regout) {
	emit_ldrb(out, regptr, regout);
}

static void emit_writeWORD_ptr32_regptrTO_immFROM(t_bytes *out, int regptr, int val) { /* reg contains ptr */
	emit_movimm(out, (uintptr_t)val, R9);
	emit_strw(out, regptr, R9);
}
static void emit_writeWORD_ptr32_regptrTO_regFROM(t_bytes *out, int regptr, int reg) { /* reg contains ptr */
	emit_strw(out, regptr, reg);
}
static void emit_writeWORD_ptr32_regptrTO_regptrFROM(t_bytes *out, int regptrto, int regptrfrom) { /* reg contains ptr */
	emit_ldrw(out, regptrfrom, regptrfrom);
	emit_strw(out, regptrto, regptrfrom);
}
static void emit_readWORD_ptr32_regptrFROM_regTO(t_bytes *out, int regptr, int regout) {
	emit_ldrw(out, regptr, regout);
}

static void emit_add_ptr32_regptrTO_regFROM(t_bytes *out, int regptr, int reg) { /* reg contains ptr */
	emit_ldr(out, regptr, R10);
	emit_add(out, R10, reg, R10);
	emit_str(out, regptr, R10);
}
static void emit_adds_ptr32_regptrTO_regFROM(t_bytes *out, int regptr, int reg) { /* reg contains ptr */
	emit_ldr(out, regptr, R10);
	emit_adds(out, R10, reg, R10);
	emit_str(out, regptr, R10);
}
static void emit_add_ptr32_regptrTO_immFROM(t_bytes *out, int regptr, int val) { /* reg contains ptr */
	emit_movimm(out, (uintptr_t)val, R9);
	emit_ldr(out, regptr, R10);
	emit_add(out, R10, R9, R10);
	emit_str(out, regptr, R10);
}

static void emit_adds_ptr32_regptrTO_immFROM(t_bytes *out, int regptr, int val) { /* reg contains ptr */
	emit_movimm(out, (uintptr_t)val, R9);
	emit_ldr(out, regptr, R10);
	emit_adds(out, R10, R9, R10);
	emit_str(out, regptr, R10);
}

static void emit_sub_ptr32_regptrTO_regFROM(t_bytes *out, int regptr, int reg) { /* reg contains ptr */
	emit_ldr(out, regptr, R10);
	emit_sub(out, R10, reg, R10);
	emit_str(out, regptr, R10);
}
static void emit_subs_ptr32_regptrTO_regFROM(t_bytes *out, int regptr, int reg) { /* reg contains ptr */
	emit_ldr(out, regptr, R10);
	emit_subs(out, R10, reg, R10);
	emit_str(out, regptr, R10);
}
static void emit_sub_ptr32_regptrTO_immFROM(t_bytes *out, int regptr, int val) { /* reg contains ptr */
	emit_movimm(out, (uintptr_t)val, R9);
	emit_ldr(out, regptr, R10);
	emit_sub(out, R10, R9, R10);
	emit_str(out, regptr, R10);
}
static void emit_subs_ptr32_regptrTO_immFROM(t_bytes *out, int regptr, int val) { /* reg contains ptr */
	emit_movimm(out, (uintptr_t)val, R9);
	emit_ldr(out, regptr, R10);
	emit_subs(out, R10, R9, R10);
	emit_str(out, regptr, R10);
}

static void emit_neg_ptr32_regptr(t_bytes *out, int regptr) { /* reg contains ptr */
	emit_ldr(out, regptr, R10);
	emit_neg(out, R10, R10);
	emit_str(out, regptr, R10);
}

static void emit_negs_ptr32_regptr(t_bytes *out, int regptr) { /* reg contains ptr */
	emit_ldr(out, regptr, R10);
	emit_negs(out, R10, R10);
	emit_str(out, regptr, R10);
}

static void emit_ror_ptr32_regptr(t_bytes *out, int regptr, int shift) {
	emit_ldr(out, regptr, R10);
	emit_ror(out, R10, shift);
	emit_str(out, regptr, R10);
}

static void emit_rors_ptr32_regptr(t_bytes *out, int regptr, int shift) {
	emit_ldr(out, regptr, R10);
	emit_rors(out, R10, shift);
	emit_str(out, regptr, R10);
}

static void emit_rors_ptr32_regptr_reg(t_bytes *out, int regptr, int regimm) {
	emit_ldr(out, regptr, R10);
	emit_rors_reg(out, R10, regimm);
	emit_str(out, regptr, R10);
}

static void emit_ror_ptr32_regptr_reg(t_bytes *out, int regptr, int regimm) {
	emit_ldr(out, regptr, R10);
	emit_ror_reg(out, R10, regimm);
	emit_str(out, regptr, R10);
}

static void emit_cmpimm(t_bytes *out, int reg, int val) {
	u_int armval;
	if(genimm(val,&armval)) {
		output_w32(out, 0xe3500000|rd_rn_rm(0,reg,0)|armval);
	}
	else {
		emit_movimm(out, val, R10);
		emit_cmp(out, reg, R10);
	}
}

static void emit_testimm(t_bytes *out, int reg, int val) {
	u_int armval;
	if(genimm(val,&armval)) {
		output_w32(out, 0xe3100000|rd_rn_rm(0,reg,0)|armval);
	}
	else {
		emit_movimm(out, val, R10);
		emit_test(out, reg, R10);
	}
}


static void emit_orimm(t_bytes *out, int reg, int val, int rt) {
	u_int armval;
	if(genimm(val,&armval)) {
		output_w32(out, 0xe3800000|rd_rn_rm(rt,reg,0)|armval);
	}
	else {
		emit_movimm(out, val, R9);
		emit_or(out, reg, R9, rt);
	}
}

static void emit_xorimm(t_bytes *out, int reg, int val, int rt) {
	u_int armval;
	if(genimm(val,&armval)) {
		output_w32(out, 0xe2200000|rd_rn_rm(rt,reg,0)|armval);
	}
	else {
		emit_movimm(out, val, R10);
		emit_xor(out, reg, R10, rt);
	}
}

static void emit_andimm(t_bytes *out, int reg, int val, int rt) {
	u_int armval;
	if(genimm(val,&armval)) {
		output_w32(out, 0xe2000000|rd_rn_rm(rt,reg,0)|armval);
	}
	else {
		emit_movimm(out, val, R10);
		emit_and(out, reg, R10, rt);
	}
}

static void emit_andsimm(t_bytes *out, u_int reg, u_int val, u_int rt)
{
	u_int armval;
	if(genimm(val,&armval)) {
		output_w32(out, 0xe2000000|rd_rn_rm(rt,reg,0)|armval|set_flags());
	}
	else {
		emit_movimm(out, val, R10);
		emit_ands(out, reg, R10, rt);
	}
}

static void emit_subimm(t_bytes *out, int reg, int val, int rt) {
	emit_movimm(out, val, R10);
	emit_sub(out, reg, R10, rt);
}

static void emit_subsimm(t_bytes *out, int reg, int val, int rt) {
	emit_movimm(out, val, R10);
	emit_subs(out, reg, R10, rt);
}


static void emit_and_ptr32_regptr(t_bytes *out, int regptr, int bits) {
	emit_ldr(out, regptr, 12);
	emit_andimm(out, 12, bits, 12);
	emit_str(out, regptr, 12);
}

static void emit_or_ptr32_regptr(t_bytes *out, int regptr, int bits) {
	emit_ldr(out, regptr, 12);
	emit_orimm(out, 12, bits, 12);
	emit_str(out, regptr, 12);
}

static void emit_or_ptr8_regptr_reg(t_bytes *out, int regptr, int reg) {
	emit_ldrb(out, regptr, 12);
	emit_or(out, 12, reg, 12);
	emit_strb(out, regptr, 12);
}
static void emit_or_ptr8_regptr_imm(t_bytes *out, int regptr, int imm) {
	emit_ldrb(out, regptr, 12);
	emit_orimm(out, 12, imm, 12);
	emit_strb(out, regptr, 12);
}

static void emit_or_ptr32_regptr_reg(t_bytes *out, int regptr, int reg) {
	emit_ldr(out, regptr, 12);
	emit_or(out, 12, reg, 12);
	emit_str(out, regptr, 12);
}

static void emit_and_ptr32_regptr_reg(t_bytes *out, int regptr, int reg) {
	emit_ldr(out, regptr, 12);
	emit_and(out, 12, reg, 12);
	emit_str(out, regptr, 12);
}


static void emit_movimm_cond(t_bytes *out, int imm, int rt, int cond) {
	
	emit_movw_cond(out, imm&0x0000FFFF,rt, cond);
    emit_movt_cond(out, imm&0xFFFF0000,rt, cond);
}

#define Z 0
#define NE 1
#define CS 2
#define CC 3 
#define SS 4
#define O 6

static void emit_set_cond(t_bytes *out, int reg, int cond) {
	output_w32(out, 0xE3A00000|(reg<<12));
	output_w32(out, 0x03A00001|(reg<<12)|(cond<<28));
}

static void emit_setc(t_bytes *out, int reg) {
	emit_set_cond(out, reg, CS);
}

static void emit_sets(t_bytes *out, int reg) {
	emit_set_cond(out, reg, SS);
}

static void emit_setz(t_bytes *out, int reg) {
	emit_set_cond(out, reg, Z);
}

static void emit_setnz(t_bytes *out, int reg) {
	emit_set_cond(out, reg, NE);
}

static void emit_setnc(t_bytes *out, int reg) {
	emit_set_cond(out, reg, CC);
}
static void emit_seto(t_bytes *out, int reg) {
	emit_set_cond(out, reg, O);
}

static void emit_write_ptr32_reg_byte(t_bytes *out, uintptr_t ptr, int reg) {
	emit_movimm(out, (uintptr_t)ptr, R8);
	
	
	
	emit_strb(out, R8, reg);
}

static void emit_lea_reg_reg(t_bytes *out, int reg1, int reg, int scale, int outreg) {
	if (scale == 2)      emit_lsl(out, reg, 1);  // MOV r10, r10, LSL #1 (multiple by 2)
	else if (scale == 4) emit_lsl(out, reg, 2);  // MOV r10, r10, LSL #2 (multiple by 4)
	else if (scale == 8) emit_lsl(out, reg, 3);  // MOV r10, r10, LSL #3 (multiple by 8)
	
	emit_add(out, reg, reg1, R10);
	emit_mov(out,  R10,outreg );
}

static void emit_lea_ptr_reg(t_bytes *out, int ptr, int reg, int scale, int outreg) {
	emit_movimm(out, ptr, R10);
	
	if (scale == 2)      emit_lsl(out, reg, 1);  // MOV r10, r10, LSL #1 (multiple by 2)
	else if (scale == 4) emit_lsl(out, reg, 2);  // MOV r10, r10, LSL #2 (multiple by 4)
	else if (scale == 8) emit_lsl(out, reg, 3);  // MOV r10, r10, LSL #3 (multiple by 8)	
	
	emit_add(out, reg, R10, R10);
	emit_mov(out, R10, outreg);
}

/* uses R9, R10 */
static void emit_lea_ptr_val(t_bytes *out, int ptr, int val, int scale, int outreg) {
	emit_movimm(out, ptr, R10);
	emit_movimm(out, val, R9);
	
	if (scale == 2)      emit_lsl(out, R9, 2);  // MOV r10, r10, LSL #1 (multiple by 2)
	else if (scale == 4) emit_lsl(out, R9, 4);  // MOV r10, r10, LSL #2 (multiple by 4)
	else if (scale == 8) emit_lsl(out, R9, 8);  // MOV r10, r10, LSL #3 (multiple by 8)
	
	emit_add(out, R9, R10, R10);

	emit_mov(out, outreg, R10);
}

/* NOTE - think these 2 functions are inverted */
static void set_carry_flag(t_bytes *out) {
	output_w32(out, 0xE10F1000); // mrs R1, cpsr
	output_w32(out, 0xE3C11202); // bic R1, R1, #(1<<29)
	output_w32(out, 0xE128F001); // msr cpsr_f, R1
}

static void clear_carry_flag(t_bytes *out) {
	output_w32(out, 0xE10F1000); // mrs R1, cpsr
	output_w32(out, 0xE3811202); // orr R1, R1, #(1<<29)
	output_w32(out, 0xE128F001); // msr cpsr_f, R1
}

static void emit_read_ptr32_reg(t_bytes *out, uintptr_t ptr, int rs) {
	emit_movimm(out, ptr, R10);
	
	emit_ldr(out, R10, rs);
}

static void emit_read_ptr32_reg_byte(t_bytes *out, uintptr_t ptr, int rs) {
	emit_movimm(out, (uintptr_t)ptr, R8);
	emit_ldrb(out, R8, rs);
}


static void emit_var_copy32(t_bytes *out, uintptr_t from, uintptr_t to) {
	emit_read_ptr32_reg(out, from, R7);
	emit_write_ptr32_reg(out, to, R7);
}

static void emit_var_copy32_reg(t_bytes *out, int from, int to) {
	emit_str(out, to, from);
}

static void emit_set_ptr_imm(t_bytes *out, uintptr_t ptr, int set) {
	emit_movimm(out, set, R10);
	emit_write_ptr32_reg(out, ptr, R10);
}

static void emit_inc_ptr_imm(t_bytes *out, uintptr_t ptr, int add) {
	emit_read_ptr32_reg(out, ptr, R10);
	if (add > 0) {
		emit_movimm(out, add, R9);
		emit_add(out, R10, R9, R10);
	}
	else {
		emit_movimm(out, add*-1, R9);
		emit_sub(out, R10, R9, R10);
	}
	emit_write_ptr32_reg(out, ptr, R10);
}

static void emit_sub_ptr_imm(t_bytes *out, uintptr_t ptr, int minus) {
	emit_read_ptr32_reg(out, ptr, R10);
	emit_movimm(out, minus, R9);
	emit_sub(out, R10, R9, R10);
	emit_write_ptr32_reg(out, ptr, R10);
}

static void emit_inc_ptr(t_bytes *out, uintptr_t ptr, uintptr_t ptr2) {
	emit_read_ptr32_reg(out, ptr, R2);
	emit_read_ptr32_reg(out, ptr2, R3);
	emit_add(out, R2, R3, R2);
	emit_write_ptr32_reg(out, ptr, R2);
}

static void emit_inc_ptr_reg(t_bytes *out, uintptr_t ptr, uintptr_t ptr2, int reg) {
	emit_read_ptr32_reg(out, ptr, R7);
	emit_read_ptr32_reg(out, ptr2, R9);
	emit_add(out, reg, R9, R7);
}

static void emit_inc_reg(t_bytes *out, uintptr_t ptr, int reg) {
	emit_read_ptr32_reg(out, ptr, R9);
	emit_add(out, reg, R9, reg);
}

static void emit_mrs_cpsr(t_bytes *out, int reg) {
	output_w32(out, 0xE10F0000|rd_rn_rm(0,reg,0)|0);
}

static void branchn() {

}

static void emit_brk(t_bytes *out) {
  output_w32(out, 0xE1200070);
}

unsigned int genlabel();

static void emit_startfunc(t_bytes *out) {

  output_w32(out, 0xE92D4FF0); // push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
	//output_w32(out, 0xE28DB000); // add fp, sp, #0
	//output_w32(out, 0xE24DDC01); // sub sp, sp, #0x100


/*u_int skip;
	skip=genlabel();

   emit_movimm(out, 0x0, R0);
  // emit_label(out, (int)skip);
   emit_addimm(out, R0, 0x1, R0);
  emit_movimm(out, 0x1, R1);
  emit_cmpimm(out, R1, 0x1);
  emit_brk(out);
  emit_branch_label(out, skip, 0x0);

  emit_movimm(out, 0x2, R2);
  emit_movimm(out, 0x3, R3);
  emit_movimm(out, 0x4, R4);
  emit_movimm(out, 0x5, R5);

  emit_label(out, skip);

   emit_movimm(out, 0x6, R6);*/

   //thread 12 break

   //emit_brk(out);
}

static void emit_endfunc(t_bytes *out) {
  //output_w32(out, 0xE24BD000); // sub sp, fp, #0
	output_w32(out, 0xE8BD8FF0); // pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}
}

static void callR3(t_bytes *out) {
  output_w32(out, 0xE92D4FFE); // push {r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp, lr}
	output_w32(out, 0xE12FFF33); // blx r3
  output_w32(out, 0xE8BD4FFE); // pop {r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp, lr}
}
static void callR9(t_bytes *out) {
  output_w32(out, 0xE92D4FFE); // push {r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp, lr}
	output_w32(out, 0xE12FFF39); // blx sb
  output_w32(out, 0xE8BD4FFE); // pop {r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp, lr}
}
static void callR7(t_bytes *out) {
  output_w32(out, 0xE92D4FFE); // push {r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp, lr}
	output_w32(out, 0xE12FFF37); // blx r7
  output_w32(out, 0xE8BD4FFE); // pop {r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp, lr}
}
static void callR6(t_bytes *out) {
  output_w32(out, 0xE92D4FFE); // push {r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp, lr}
	output_w32(out, 0xE12FFF36); // blx r6
  output_w32(out, 0xE8BD4FFE); // pop {r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp, lr}
}
static void callR8(t_bytes *out) {
  output_w32(out, 0xE92D4FFE); // push {r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp, lr}
	output_w32(out, 0xE12FFF38); // blx r8
  output_w32(out, 0xE8BD4FFE); // pop {r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp, lr}
}

static  void save(t_bytes *out) {
	output_w32(out, 0xE92D4FFF); // push {r0, r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp, lr}
}

static  void restore(t_bytes *out) {
	output_w32(out, 0xE8BD4FFF); // pop {r0, r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp, lr}
}


//-----------------------------------------------------------------------------
//   Shifting macros
//-----------------------------------------------------------------------------
#define GET_CARRY(invert) { \
	flags_ptr(R2, VALUE); \
	/*printreg3(1, R2);*/ \
	int setb=genlabel(); \
	int done=genlabel(); \
		output_w32(g_out, 0xE2122020); /* ands r2, r2, #0x20 */ \
		/*emit_andimm(g_out, R2, 0x20, R2); \
		emit_cmpimm(g_out, R2, 0x20);*/ \
		emit_branch_label(g_out, setb, 0x1); \
		set_carry_flag(g_out); \
		emit_branch_label(g_out, done, 0xE); \
		emit_label(g_out, (int)setb); \
		clear_carry_flag(g_out); \
		emit_label(g_out, (int)done); \
	/*c.bt(flags_ptr, 5); \
	if (invert) c.cmc();*/ \
}

//-----------------------------------------------------------------------------
//   Shifting macros
//-----------------------------------------------------------------------------
#define SET_NZCV(sign) { \
	JIT_COMMENT("SET_NZCV"); \
	output_w32(g_out, 0xE10F1000); /* mrs r1, apsr */ \
	flags_ptr(y_reg, VALUE); \
	output_w32(g_out, 0xE1A01C21); /* lsr r1, r1, #0x18 */\
	output_w32(g_out, 0xE20110F0); /* and r1, r1, #0xf0 */ \
	output_w32(g_out, 0xE209900F); /* and sb, sb, #0xf */ \
	emit_or(g_out, R1, y_reg, R1); \
	emit_writeBYTE_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(flags_ptr(R0, ADDRESS)), R1); \
}

#define SET_NZC { \
	JIT_COMMENT("SET_NZC"); \
	output_w32(g_out, 0xE10F1000); /* mrs r1, apsr */ \
	flags_ptr(y_reg, VALUE); \
	output_w32(g_out, 0xE1A01C21); /* lsr r1, r1, #0x18 */\
	output_w32(g_out, 0xE20110C0); /* and r1, r1, #0xc0 */ \
	if (cf_change) { output_w32(g_out, 0xE1811285); /* orr r1, r1, r5, lsl #5 */ }   \
	output_w32(g_out, cf_change ? 0xE209901F /* and sb, sb, #0x1f */ : 0xE209903F /* and sb, sb, #0x3f */ ); \
	emit_or(g_out, R1, y_reg, R1); \
	emit_writeBYTE_ptr32_regptrTO_regFROM(g_out, CACHED_PTR(flags_ptr(R0, ADDRESS)), R1); \
}

#define SET_NZC_SHIFTS_ZERO(cf) { \
	JIT_COMMENT("SET_NZC_SHIFTS_ZERO"); \
	flags_ptr(R3, VALUE); \
	emit_and_ptr32_regptr(g_out, CACHED_PTR(flags_ptr(R1, ADDRESS)), 0x1F); \
	if(cf) \
	{ \
		emit_lsl(g_out, rcf_reg, 5); \
		emit_orimm(g_out,rcf_reg, (1<<6), rcf_reg); \
		emit_or_ptr32_regptr_reg(g_out, CACHED_PTR(flags_ptr(R1, ADDRESS)), rcf_reg); \
	} \
	else \
		emit_or_ptr32_regptr(g_out, CACHED_PTR(flags_ptr(R1, ADDRESS)), (1<<6)); \
	JIT_COMMENT("end SET_NZC_SHIFTS_ZERO"); \
}

#define SET_NZ(clear_cv) { \
	output_w32(g_out, 0xE10F1000); /* mrs r1, apsr */ \
	flags_ptr(y_reg, VALUE); \
	output_w32(g_out, 0xE1A01C21); /* lsr r1, r1, #0x18 */ \
	output_w32(g_out, 0xE20110C0); /* and r1, r1, #0xc0 */ \
	/*if (cf_change) { output_w32(g_out, 0xE1811085);*/  /* ORR	R1, R5, LSL #1 *//*}*/   \
	output_w32(g_out, clear_cv ? 0xE209900F /* and sb, sb, #0xf */  : 0xE209903F /* and sb, sb, #0x3f */ );\
	emit_or(g_out, R1, y_reg, R1); \
	emit_writeBYTE_ptr32_regptrTO_regFROM(g_out, PTR_STORE_REG/*flags_ptr(R0, ADDRESS)*/, R1); \
}

#define SET_N { \
	output_w32(g_out, 0xE10F1000); /* mrs r1, apsr */ \
	flags_ptr(y_reg, VALUE); \
	output_w32(g_out, 0xE1A01C21); /* lsr r1, r1, #0x18 */\
	output_w32(g_out, 0xE2011080); /* and r1, r1, #0x80 */ \
	/*if (cf_change) { output_w32(g_out, 0xE1811085);*/  /* ORR	R1, R5, LSL #1 *//*}*/   \
	/*output_w32(g_out, 0xE209907F);*/ /* and R9, #0x7F */ \
	output_w32(g_out, 0xE209907F); /* and sb, sb, #0x7f */ \
	emit_or(g_out, R1, y_reg, R1); \
	emit_writeBYTE_ptr32_regptrTO_regFROM(g_out, PTR_STORE_REG/*flags_ptr(R0, ADDRESS)*/, R1); \
}

#define SET_Z { \
	output_w32(g_out, 0xE10F1000); /* mrs r1, apsr */ \
	flags_ptr(y_reg, VALUE); \
	output_w32(g_out, 0xE1A01C21); /* lsr r1, r1, #0x18 */\
	output_w32(g_out, 0xE2011040); /* and r1, r1, #0x40 */  \
	output_w32(g_out, 0xE20990BF); /* and sb, sb, #0xbf */ \
	emit_or(g_out, R1, y_reg, R1); \
	emit_writeBYTE_ptr32_regptrTO_regFROM(g_out, PTR_STORE_REG/*flags_ptr(R0, ADDRESS)*/, R1); \
}

#define SET_Q { \
	output_w32(g_out, 0xE10F1000); /* mrs r1, apsr */ \
	flags_ptr(y_reg, VALUE); \
	output_w32(g_out, 0xE1A01C21); /*lsr R1, #24 */\
	output_w32(g_out, 0xE2011010); /* lsr r1, r1, #0x18 */ \
	output_w32(g_out, 0xE20990EF); /* and sb, sb, #0xef */ \
	emit_or(g_out, R1, y_reg, R1); \
	emit_writeBYTE_ptr32_regptrTO_regFROM(g_out, PTR_STORE_REG/*flags_ptr(R0, ADDRESS)*/, R1); \
}
