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

// AArch64 primitive layer.
//
// Every function here used to hand-encode its instruction words (`output_w32`
// with a magic constant and the operand fields shifted into place). They are
// now emitted through asmjit, with the instruction selection kept *byte for
// byte* identical to the old encoder: this step is a migration, not a rewrite,
// so it must be possible to show that a block compiles to the same instructions
// as before. tools/jit_matrix/check_a64.py enforces that.
//
// Register numbering follows the backend convention: `R0..R15` are the ARM
// register indices the handlers pass around, `X0_REG..` are host registers.
// Argument order follows the encodings, so (rs1 = Rn, rs2 = Rm, rt = Rd).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) && ! defined(__ANDROID__)
typedef uint32_t u_int;
#else
#include <sys/mman.h>
#endif

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

#define X0_REG      0
#define X1_REG      1
#define X2_REG      2
#define X3_REG      3
#define X4_REG      4
#define X5_REG      5
#define X6_REG      6
#define X7_REG      7
#define X8_REG      8
#define X9_REG      9
#define X10_REG     10
#define X11_REG     11
#define X12_REG     12
#define X13_REG     13
#define X14_REG     14
#define X15_REG     15
#define X16_REG     16
#define X17_REG     17
#if (defined(_WIN32) || defined(__APPLE__) || defined(__ANDROID__))
#define X18_REG     23 // x18 is reserved on windows on arm, macos, and android, so we use x23 instead
#else
#define X18_REG     18
#endif
#define X19_REG     19
#define X20_REG     20
#define X21_REG     21
#define X22_REG     22
#define X23_REG     23
#define X24_REG     24
#define X25_REG     25
#define X26_REG     26
#define X27_REG     27
#define X28_REG     28
#define x29_REG     29
#define X30_REG     30
#define X31_REG     31

#define SHIFT_REG R0

#define WZR 31
#define XZR WZR

#include <map>
#include <vector>

#include <asmjit/a64.h>

// The encoder used to be a linked list of raw byte chunks (`struct s_bytes`);
// asmjit now owns it. Keeping the name `t_bytes` means the ~570 call sites in
// arm_jit_arm.cpp (`emit_add(g_out, ...)`) did not have to change.
typedef asmjit::a64::Compiler t_bytes;

// Register operand helpers: the backend works in register *numbers* everywhere.
static inline asmjit::a64::Gp gw(int n) { return asmjit::a64::w(n); }
static inline asmjit::a64::Gp gx(int n) { return asmjit::a64::x(n); }
static inline asmjit::a64::Mem gmem(int base) { return asmjit::a64::ptr(gx(base)); }

typedef unsigned int (*JittedFunc)();

uint64_t genlabel();

#include "emit_core.cpp"

//---------------------------------------------------------------------------
// Forward declarations
//
// The primitives are mutually recursive (a shift helper calls emit_andimm,
// which calls emit_movimm...). The old encoder leaned on the same thing; the
// declarations are kept in one place instead of being scattered mid-file.
//---------------------------------------------------------------------------

static void emit_movz(t_bytes *out, u_int imm, u_int rt);
static void emit_movz_lsl16(t_bytes *out, u_int imm, u_int rt);
static void emit_movz64(t_bytes *out, u_int imm, u_int rt);
static void emit_movn(t_bytes *out, u_int imm, u_int rt);
static void emit_movn_lsl16(t_bytes *out, u_int imm, u_int rt);
static void emit_movk(t_bytes *out, u_int imm, u_int rt);
static void emit_movk_lslnum(t_bytes *out, u_int imm, u_int rt, int lsl);
static void emit_movk_lslnum64(t_bytes *out, u_int imm, u_int rt, int lsl);
static void emit_mov(t_bytes *out, int rs, int rt);
static void emit_movimm(t_bytes *out, uintptr_t imm, u_int rt);
static void emit_ptr(t_bytes *out, uintptr_t ptr, u_int rt);
static void emit_zeroreg(t_bytes *out, int rt);

static void emit_add(t_bytes *out, int rs1, int rs2, int rt);
static void emit_adds(t_bytes *out, int rs1, int rs2, int rt);
static void emit_add64(t_bytes *out, int rs1, int rs2, int rt);
static void emit_sub(t_bytes *out, int rs1, int rs2, int rt);
static void emit_subs(t_bytes *out, int rs1, int rs2, int rt);
static void emit_and(t_bytes *out, u_int rs1, u_int rs2, u_int rt);
static void emit_or(t_bytes *out, u_int rs1, u_int rs2, u_int rt);
static void emit_or64(t_bytes *out, u_int rs1, u_int rs2, u_int rt);
static void emit_xor(t_bytes *out, u_int rs1, u_int rs2, u_int rt);
static void emit_ands(t_bytes *out, u_int rs1, u_int rs2, u_int rt);
static void emit_test(t_bytes *out, int rs, int rt);
static void emit_cmp(t_bytes *out, int rs, int rt);
static void emit_lsl(t_bytes *out, int reg, int n);
static void emit_lsr(t_bytes *out, int reg, int n);
static void emit_asr(t_bytes *out, int reg, int n);
static void emit_ror(t_bytes *out, int reg, int n);

static void emit_lsl_reg(t_bytes *out, int reg, int nreg);
static void emit_lsr_reg(t_bytes *out, int reg, int nreg);
static void emit_asr_reg(t_bytes *out, int reg, int nreg);
static void emit_ror_reg(t_bytes *out, int reg, int nreg);
static void emit_lsl_reg_quick(t_bytes *out, int reg, int nreg);
static void emit_lsrs_reg(t_bytes *out, int reg, int nreg);
static void emit_asrs_reg(t_bytes *out, int reg, int nreg);
static void emit_rors_reg(t_bytes *out, int reg, int nreg);
static void emit_lsls_reg(t_bytes *out, int reg, int nreg);
static void emit_lsls(t_bytes *out, int reg, int n);
static void emit_lsrs(t_bytes *out, int reg, int n);
static void emit_asrs(t_bytes *out, int reg, int n);
static void emit_rors(t_bytes *out, int reg, int n);

static void emit_andimm(t_bytes *out, int rs, int imm, int rt);
static void emit_andsimm(t_bytes *out, int rs, int imm, int rt);
static void emit_orimm(t_bytes *out, int rs, int imm, int rt);
static void emit_cmpimm(t_bytes *out, int rs, int imm);
static void emit_testimm(t_bytes *out, int rs, int imm);
static void emit_setc(t_bytes *out, int reg);
static void emit_setnz(t_bytes *out, int reg);
static void get_carry_flag(t_bytes *out, int reg);
static void set_carry_flag(t_bytes *out);
static void clear_carry_flag(t_bytes *out);

//---------------------------------------------------------------------------
// Immediates
//---------------------------------------------------------------------------

// Returns true when `imm` can be encoded as the immediate of a logical
// instruction for a register of `regsize` bits, with `encoded` set to N:immr:imms.
// Still needed: it decides *which* form emit_movimm and emit_andsimm pick, and
// that choice has to stay identical to keep the generated blocks identical.
static uint32_t is_mask(uint64_t value) {
	return value && ((value + 1) & value) == 0;
}

static uint32_t is_shifted_mask(uint64_t Value) {
	return Value && is_mask((Value - 1) | Value);
}

uint32_t count_leading_zeros(uint64_t value)
{
#ifdef _MSC_VER
	// _BitScanReverse takes `unsigned long*`, which is 32-bit on every MSVC
	// target but a distinct type from uint32_t. Passing a uint32_t* here is
	// rejected with C2664 whenever this file is built by MSVC (Windows on ARM);
	// Apple and Android take the __builtin_clzll path and never noticed.
	unsigned long leading_zero_low = 0;
	unsigned long leading_zero_high = 0;
	if(!_BitScanReverse(&leading_zero_low, (uint32_t)value))
		leading_zero_low = 32;
	else
		leading_zero_low = 31 - leading_zero_low;

	if(!_BitScanReverse(&leading_zero_high, (uint32_t)(value>>32)))
		leading_zero_high = 32;
	else
		leading_zero_high = 31 - leading_zero_high;

	if(leading_zero_high == 32)
		return (uint32_t)(leading_zero_low + leading_zero_high);
	else
		return (uint32_t)leading_zero_high;
#else /* ARM64 */
	return __builtin_clzll(value);
#endif
}

uint32_t count_trailing_zeros(uint64_t value)
{
#ifdef _MSC_VER
	unsigned long trailing_zero_low = 0;
	unsigned long trailing_zero_high = 0;
	if(!_BitScanForward(&trailing_zero_low, (uint32_t)value))
		trailing_zero_low = 32;

	if(!_BitScanForward(&trailing_zero_high, (uint32_t)(value>>32)))
		trailing_zero_high = 32;

	if(trailing_zero_low == 32)
		return (uint32_t)(trailing_zero_low + trailing_zero_high);
	else
		return (uint32_t)trailing_zero_low;
#else /* ARM64 */
	return __builtin_ctzll(value);
#endif
}

static uint32_t genimm2(uint64_t imm, uint32_t regsize, uint32_t * encoded) {
	// First, determine the element size.
	uint32_t size = regsize;
	do {
		size /= 2;
		uint64_t mask = (1ULL << size) - 1;

		if ((imm & mask) != ((imm >> size) & mask)) {
			size *= 2;
			break;
		}
	} while (size > 2);

	// Second, determine the rotation to make the element be: 0^m 1^n.
	uint32_t trailing_one, trailing_zero;
	uint64_t mask = ((uint64_t)-1LL) >> (64 - size);
	imm &= mask;

	if (is_shifted_mask(imm)) {
		trailing_zero = count_trailing_zeros(imm);
		assert(trailing_zero < 64);
		trailing_one = count_trailing_zeros(~(imm >> trailing_zero));
	} else {
		imm |= ~mask;
		if (!is_shifted_mask(~imm))
			return 0;

		uint32_t leading_one = count_leading_zeros(~imm);
		trailing_zero = 64 - leading_one;
		trailing_one = leading_one + count_trailing_zeros(~imm) - (64 - size);
	}

	// Encode in immr the number of RORs it would take to get *from* 0^m 1^n
	// to our target value, where trailing_zero is the number of RORs to go the opposite
	// direction.
	assert(size > trailing_zero);
	uint32_t immr = (size - trailing_zero) & (size - 1);

	// If size has a 1 in the n'th bit, create a value that has zeroes in
	// bits [0, n] and ones above that.
	uint64_t Nimms = ~(size-1) << 1;

	// Or the trailing_one value into the low bits, which must be below the Nth bit
	// bit mentioned above.
	Nimms |= (trailing_one-1);

	// Extract the seventh bit and toggle it to create the N field.
	uint32_t N = ((Nimms >> 6) & 1) ^ 1;

	*encoded = (N << 12) | (immr << 6) | (Nimms & 0x3f);
	return 1;
}

//---------------------------------------------------------------------------
// Register moves and shifts
//---------------------------------------------------------------------------

static void emit_movz(t_bytes *out, u_int imm, u_int rt) {
	// The old encoder OR'd the whole value into the instruction word, so anything
	// wider than 16 bits spilled into the shift field. Callers rely on it:
	// emit_andsimm passes a 17-bit value and expects movz ..., lsl #16.
	ASMJIT_ASSUME_OK(out->movz(gw(rt), asmjit::Imm(imm & 0xffff), asmjit::Imm(((imm >> 16) & 3) * 16)));
}

static void emit_movz_lsl16(t_bytes *out, u_int imm, u_int rt) {
	ASMJIT_ASSUME_OK(out->movz(gw(rt), asmjit::Imm(imm), asmjit::Imm(16)));
}

static void emit_movz64(t_bytes *out, u_int imm, u_int rt) {
	ASMJIT_ASSUME_OK(out->movz(gx(rt), asmjit::Imm(imm)));
}

static void emit_movn(t_bytes *out, u_int imm, u_int rt) {
	ASMJIT_ASSUME_OK(out->movn(gw(rt), asmjit::Imm(imm & 0xffff), asmjit::Imm(((imm >> 16) & 3) * 16)));
}

static void emit_movn_lsl16(t_bytes *out, u_int imm, u_int rt) {
	ASMJIT_ASSUME_OK(out->movn(gw(rt), asmjit::Imm(imm), asmjit::Imm(16)));
}

static void emit_movk(t_bytes *out, u_int imm, u_int rt) {
	ASMJIT_ASSUME_OK(out->movk(gw(rt), asmjit::Imm(imm & 0xffff), asmjit::Imm(((imm >> 16) & 3) * 16)));
}

static void emit_movk_lsl16(t_bytes *out, u_int imm, u_int rt) {
	ASMJIT_ASSUME_OK(out->movk(gw(rt), asmjit::Imm(imm), asmjit::Imm(16)));
}

static void emit_movk_lslnum(t_bytes *out, u_int imm, u_int rt, int lsl) {
	ASMJIT_ASSUME_OK(out->movk(gw(rt), asmjit::Imm(imm), asmjit::Imm(lsl)));
}

static void emit_movk_lslnum64(t_bytes *out, u_int imm, u_int rt, int lsl) {
	ASMJIT_ASSUME_OK(out->movk(gx(rt), asmjit::Imm(imm), asmjit::Imm(lsl)));
}

static void emit_zeroreg(t_bytes *out, int rt) {
	ASMJIT_ASSUME_OK(out->movz(gw(rt), asmjit::Imm(0)));
}

// Builds a 64-bit address in `rt`. Deliberately keeps the movz + movk chain the
// hand-written version produced instead of using `mov(reg, imm)`, whose form
// selection differs (it would happily emit a single movn/movz).
static void emit_ptr(t_bytes *out, uintptr_t ptr, u_int rt) {
	emit_movz64(out, ptr&0xffff,rt);
	if ((ptr>>16)&0xffff) emit_movk_lslnum64(out, (ptr>>16)&0xffff,rt, 16);
	if ((ptr>>32)&0xffff) emit_movk_lslnum64(out, (ptr>>32)&0xffff,rt, 32);
	if ((ptr>>48)&0xffff) emit_movk_lslnum64(out, (ptr>>48)&0xffff,rt, 48);
}

// asmjit's validator rejects WZR as the first source of a logical instruction,
// even though `orr wD, wzr, #imm` is the canonical way to materialise a bitmask
// immediate (it is what `mov wD, #imm` assembles to). Emit that one word
// directly; see documentation/asmjit-jit-migration-plan.md, known issues.
static void emit_orr_imm_wzr(t_bytes *out, int rt, u_int armval) {
	uint32_t word = 0x32000000u | (armval << 10) | (31u << 5) | (uint32_t)rt;
	ASMJIT_ASSUME_OK(out->embed(&word, 4));
}

// Materialises a 32-bit value in `rt`. The branch order matters: it is what the
// handlers have always produced, and a "better" choice would change the block.
static void emit_movimm(t_bytes *out, uintptr_t imm, u_int rt) {
	uint32_t armval=0;
	if(imm<65536) {
		emit_movz(out, imm,rt);
	}else if((~imm)<65536) {
		emit_movn(out, ~imm,rt);
	}else if((imm&0xffff)==0) {
		emit_movz_lsl16(out, (imm>>16)&0xffff,rt);
	}else if(((~imm)&0xffff)==0) {
		emit_movn_lsl16(out, (~imm>>16)&0xffff,rt);
	}else if(genimm2((uint64_t)imm,32,&armval)) {
		emit_orr_imm_wzr(out, rt, armval);
	}else{
		emit_movz_lsl16(out, (imm>>16)&0xffff,rt);
		emit_movk(out, imm&0xffff,rt);
	}
}

static void emit_mov(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->mov(gw(rt), gw(rs)));
}

static void emit_mov64(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->mov(gx(rt), gx(rs)));
}

static void emit_movs(t_bytes *out, int rs, int rt) {
	// Historical quirk: the old instruction constant already carried Rn = wzr, so
	// `rs` was OR'd into a field that was already full and never reached it. This
	// has always been "mov w<rt>, w0", whatever `rs` was. No handler uses it
	// today; kept byte-identical so the migration stays a no-op.
	(void)rs;
	uint32_t word = 0x2A0003E0u | (uint32_t)rt;
	ASMJIT_ASSUME_OK(out->embed(&word, 4));
	ASMJIT_ASSUME_OK(out->cmp(gw(rt), gw(rt)));
}

static void emit_lsl(t_bytes *out, int reg, int n) {
	emit_movimm(out, n, SHIFT_REG);
	ASMJIT_ASSUME_OK(out->lsl(gw(reg), gw(reg), gw(SHIFT_REG)));
}

static void emit_lsr(t_bytes *out, int reg, int n) {
	if (n < 32) {
		ASMJIT_ASSUME_OK(out->lsr(gw(reg), gw(reg), asmjit::Imm(n)));
	}
	else {
		emit_movimm(out, n, SHIFT_REG);
		ASMJIT_ASSUME_OK(out->lsr(gw(reg), gw(reg), gw(SHIFT_REG)));
	}
}

static void emit_lsr64(t_bytes *out, int reg, int n) {
	if (n < 32) {
		ASMJIT_ASSUME_OK(out->lsr(gx(reg), gx(reg), asmjit::Imm(n)));
	}
	else {
		emit_movimm(out, n, SHIFT_REG);
		ASMJIT_ASSUME_OK(out->lsr(gx(reg), gx(reg), gx(SHIFT_REG)));
	}
}

static void emit_asr(t_bytes *out, int reg, int num) {
	if (num < 32) {
		ASMJIT_ASSUME_OK(out->asr(gw(reg), gw(reg), asmjit::Imm(num)));
	}
	else {
		emit_movimm(out, num, SHIFT_REG);
		ASMJIT_ASSUME_OK(out->asr(gw(reg), gw(reg), gw(SHIFT_REG)));
	}
}

static void emit_asr64(t_bytes *out, int reg, int num) {
	if (num < 32) {
		ASMJIT_ASSUME_OK(out->asr(gx(reg), gx(reg), asmjit::Imm(num)));
	}
	else {
		emit_movimm(out, num, SHIFT_REG);
		ASMJIT_ASSUME_OK(out->asr(gx(reg), gx(reg), gx(SHIFT_REG)));
	}
}

static void emit_ror(t_bytes *out, int reg, int n) {
	if (n < 32) {
		ASMJIT_ASSUME_OK(out->ror(gw(reg), gw(reg), asmjit::Imm(n)));
	}
	else {
		emit_movimm(out, n, SHIFT_REG);
		emit_ror_reg(out, reg, SHIFT_REG);
	}
}

static void emit_lsl_reg_quick(t_bytes *out, int reg, int nreg) {
	ASMJIT_ASSUME_OK(out->lsl(gw(reg), gw(reg), gw(nreg)));
}

static void emit_lsr_reg_quick(t_bytes *out, int reg, int nreg) {
	ASMJIT_ASSUME_OK(out->lsr(gw(reg), gw(reg), gw(nreg)));
}

static void emit_lsl_reg(t_bytes *out, int reg, int nreg) {
	uint64_t setb=genlabel();
	uint64_t done=genlabel();

	emit_andimm(out, nreg, 0xff, nreg);

	emit_cmpimm(out, nreg, 32);
	emit_branch_label(out, setb, 10); // greater than or equal to 32

	ASMJIT_ASSUME_OK(out->lsl(gw(reg), gw(reg), gw(nreg)));
	emit_branch_label(out, done, 0xE);

	emit_label(out, setb);
	emit_movimm(out, 0, reg);

	emit_label(out, done);
}

static void emit_lsr_reg(t_bytes *out, int reg, int nreg) {
	uint64_t setb=genlabel();
	uint64_t done=genlabel();

	emit_andimm(out, nreg, 0xff, nreg);

	emit_cmpimm(out, nreg, 32);
	emit_branch_label(out, setb, 10); // greater than or equal to 32

	ASMJIT_ASSUME_OK(out->lsr(gw(reg), gw(reg), gw(nreg)));
	emit_branch_label(out, done, 0xE);

	emit_label(out, setb);
	emit_movimm(out, 0, reg);

	emit_label(out, done);
}

static void emit_asr_reg(t_bytes *out, int reg, int nreg) {
	uint64_t done=genlabel();
	uint64_t setb=genlabel();
	emit_andimm(out, nreg, 0xff, nreg);

	emit_cmpimm(out, nreg, 0);
	emit_branch_label(out, done, 0); // eq 0
	emit_cmpimm(out, nreg, 32);
	emit_branch_label(out, setb, 10); // greater than or equal to 32

	ASMJIT_ASSUME_OK(out->asr(gw(reg), gw(reg), gw(nreg)));

	emit_branch_label(out, done, 0xE);

	emit_label(out, setb);
	emit_movimm(out, 1<<31, X22_REG);
	emit_test(out, reg, X22_REG); // test BIT31(reg 22) of original reg 20
	emit_setnz(out, reg);
	emit_movimm(out, 0xFFFFFFFF, X22_REG);
	ASMJIT_ASSUME_OK(out->mul(gw(reg), gw(reg), gw(X22_REG)));

	emit_label(out, done);
}

static void emit_ror_reg(t_bytes *out, int reg, int nreg) {
	uint64_t done=genlabel();

	emit_andimm(out, nreg, 0xff, nreg);

	emit_cmpimm(out, nreg, 0);
	emit_branch_label(out, done, 0); // 0
	emit_andsimm(out, nreg, 0x1F, nreg);
	emit_branch_label(out, done, 0); // 0

	ASMJIT_ASSUME_OK(out->ror(gw(reg), gw(reg), gw(nreg)));

	emit_label(out, done);
}

static void emit_rrx(t_bytes *out, int reg, int n) {
	emit_setc(out, 13);
	ASMJIT_ASSUME_OK(out->lsl(gw(13), gw(13), asmjit::Imm(31))); // lsl w13, w13, #31
	emit_andimm(out, reg, 1, X5_REG);

	emit_movimm(out, n, SHIFT_REG);
	emit_ror_reg(out, reg, SHIFT_REG);
	emit_andimm(out, reg, 0x7fffffff, reg);
	emit_or(out, reg, 13, reg);
}

static void emit_lsls(t_bytes *out, int reg, int n) {
	emit_movimm(out, n, SHIFT_REG);
	emit_lsls_reg(out, reg, SHIFT_REG);
}

static void emit_lsrs(t_bytes *out, int reg, int n) {
	emit_movimm(out, n, SHIFT_REG);
	emit_lsrs_reg(out, reg, SHIFT_REG);
}

static void emit_asrs(t_bytes *out, int reg, int n) {
	emit_movimm(out, n, SHIFT_REG);
	emit_asrs_reg(out, reg, SHIFT_REG);
}

static void emit_rors(t_bytes *out, int reg, int n) {
	emit_movimm(out, n, SHIFT_REG);
	emit_rors_reg(out, reg, SHIFT_REG);
}

static void emit_rrxs(t_bytes *out, int reg, int n) {
	emit_setc(out, 13);
	ASMJIT_ASSUME_OK(out->lsl(gw(13), gw(13), asmjit::Imm(31)));
	emit_andimm(out, reg, 1, X5_REG);

	emit_movimm(out, n, SHIFT_REG);
	emit_ror_reg(out, reg, SHIFT_REG);
	emit_andimm(out, reg, 0x7fffffff, reg);
	emit_or(out, reg, 13, reg);
}

//---------------------------------------------------------------------------
// Shifting with flags
//
// These keep the hand-written version's structure exactly, including the
// register choices (x18..x22 are used as scratch, x20 holds the pre-shift
// value). Reworking them belongs to the register-cache step, where the changes
// can be measured; here the only goal is "same instructions".
//---------------------------------------------------------------------------

static void emit_lsls_reg(t_bytes *out, int reg, int nreg) {
	uint64_t nonzero=genlabel();
	uint64_t done=genlabel();
	uint64_t eq32=genlabel();
	uint64_t gt32=genlabel();

	emit_andimm(out, nreg, 0xff, nreg);

	emit_cmpimm(out, nreg, 0);
	emit_branch_label(out, nonzero, 0x1);

	/* eq 0 */

	get_carry_flag(out, 5);
	emit_branch_label(out, done, 0xE);

	/* end eq 0 */

	emit_label(out, nonzero);

	emit_mov(out, nreg, X19_REG);
	emit_mov(out, reg, X20_REG);

	/* non zero */
	emit_cmpimm(out, nreg, 32);
	emit_branch_label(out, eq32, 0x0); // nreg == 32
	emit_branch_label(out, gt32, 12); // gt 32

	// get last shifted bit
	emit_movimm(out, 32, X18_REG);
	ASMJIT_ASSUME_OK(out->sub(gw(X18_REG), gw(X18_REG), gw(X19_REG)));

	// perform shifft
	ASMJIT_ASSUME_OK(out->lsl(gw(reg), gw(reg), gw(nreg)));

	emit_movimm(out, 1, X22_REG);
	emit_lsl_reg_quick(out, X22_REG, X18_REG);
	emit_test(out, X20_REG, X22_REG);

	emit_setnz(out, X5_REG);
	emit_branch_label(out, done, 0xE);

	/* end non zero */

	emit_label(out, eq32);

	/* equal 32 */

	emit_movimm(out, 1, X22_REG);
	emit_test(out, X20_REG, X22_REG); // test BIT0(reg 22) of original reg 20

	emit_setnz(out, X5_REG);
	emit_movimm(out, 0, reg);
	emit_branch_label(out, done, 0xE);

	/* end equal 32 */

	emit_label(out, gt32);

	/* gt 32 */

	emit_movimm(out, 0, reg);
	emit_movimm(out, 0, X5_REG);

	/* end gt 32 */

	emit_label(out, done);
	emit_cmpimm(out, reg, 0);
}

static void emit_lsrs_reg(t_bytes *out, int reg, int nreg) {
	uint64_t nonzero=genlabel();
	uint64_t done=genlabel();
	uint64_t eq32=genlabel();
	uint64_t gt32=genlabel();

	emit_andimm(out, nreg, 0xff, nreg);

	emit_cmpimm(out, nreg, 0);
	emit_branch_label(out, nonzero, 0x1);

	/* eq 0 */

	get_carry_flag(out, X5_REG);
	emit_branch_label(out, done, 0xE);

	/* end eq 0 */

	emit_label(out, nonzero);

	emit_mov(out, nreg, X19_REG);
	emit_mov(out, reg, X20_REG);

	/* non zero */
	emit_cmpimm(out, nreg, 32);
	emit_branch_label(out, eq32, 0x0); // nreg == 32
	emit_branch_label(out, gt32, 12); // gt 32

	// get last shifted bit
	emit_movimm(out, 1, X18_REG);
	ASMJIT_ASSUME_OK(out->sub(gw(X18_REG), gw(X19_REG), gw(X18_REG)));

	// perform shifft
	ASMJIT_ASSUME_OK(out->lsr(gw(reg), gw(reg), gw(nreg)));

	emit_movimm(out, 1, X22_REG);
	emit_lsl_reg_quick(out, X22_REG, X18_REG);
	emit_test(out, X20_REG, X22_REG);

	emit_setnz(out, X5_REG);
	emit_branch_label(out, done, 0xE);

	/* end non zero */

	emit_label(out, eq32);

	/* equal 32 */

	emit_movimm(out, 1<<31, X22_REG);
	emit_test(out, X20_REG, X22_REG); // test BIT31(reg 22) of original reg 20

	emit_setnz(out, X5_REG);
	emit_movimm(out, 0, reg);

	emit_branch_label(out, done, 0xE);

	/* end equal 32 */

	emit_label(out, gt32);

	/* gt 32 */

	emit_movimm(out, 0, reg);
	emit_movimm(out, 0, X5_REG);
	// fall into clrb

	/* end gt 32 */

	emit_label(out, done);

	emit_cmpimm(out, reg, 0);
}

static void emit_asrs_reg(t_bytes *out, int reg, int nreg) {
	uint64_t nonzero=genlabel();
	uint64_t done=genlabel();
	uint64_t gteq32=genlabel();

	emit_andimm(out, nreg, 0xff, nreg);

	emit_cmpimm(out, nreg, 0);
	emit_branch_label(out, nonzero, 0x1);

	/* eq 0 */

	get_carry_flag(out, X5_REG);
	emit_branch_label(out, done, 0xE);

	/* end eq 0 */

	emit_label(out, nonzero);

	emit_mov(out, nreg, X19_REG);
	emit_mov(out, reg, X20_REG);

	/* non zero */
	emit_cmpimm(out, nreg, 32);
	emit_branch_label(out, gteq32, 10); // gteq 32

	// get last shifted bit
	emit_movimm(out, 1, X18_REG);
	ASMJIT_ASSUME_OK(out->sub(gw(X18_REG), gw(X19_REG), gw(X18_REG)));

	// perform shifft
	ASMJIT_ASSUME_OK(out->asr(gw(reg), gw(reg), gw(nreg)));

	emit_movimm(out, 1, X22_REG);
	emit_lsl_reg_quick(out, X22_REG, X18_REG);
	emit_test(out, X20_REG, X22_REG);

	emit_setnz(out, X5_REG);
	emit_branch_label(out, done, 0xE);

	/* end non zero */

	emit_label(out, gteq32);

	/* equal 32 */

	// perform shifft
	ASMJIT_ASSUME_OK(out->asr(gw(reg), gw(reg), gw(nreg)));

	emit_movimm(out, 1<<31, X22_REG);
	emit_test(out, X20_REG, X22_REG); // test BIT31(reg 22) of original reg 20

	emit_setnz(out, X5_REG);

	emit_mov(out, X5_REG, reg);
	emit_movimm(out, 0xFFFFFFFF, X22_REG);
	ASMJIT_ASSUME_OK(out->mul(gw(reg), gw(reg), gw(X22_REG)));

	emit_label(out, done);

	emit_cmpimm(out, reg, 0);
}

static void emit_rors_reg(t_bytes *out, int reg, int nreg) {
	uint64_t nonzero=genlabel();
	uint64_t done=genlabel();
	uint64_t and0=genlabel();

	emit_mov(out, reg, X20_REG);

	emit_andimm(out, nreg, 0xff, nreg);

	emit_cmpimm(out, nreg, 0);
	emit_branch_label(out, nonzero, 0x1);

	get_carry_flag(out, 5);
	emit_branch_label(out, done, 0xE);

	/* eq 0 */

	/* end eq 0 */

	emit_label(out, nonzero);

	/* non zero */
	emit_andimm(out, nreg, 0x1F, nreg);
	emit_mov(out, nreg, X19_REG);
	emit_cmpimm(out, X19_REG, 0);
	emit_branch_label(out, and0, 0x0); // eq 0

	// get last shifted bit
	emit_movimm(out, 1, X18_REG);
	ASMJIT_ASSUME_OK(out->sub(gw(X18_REG), gw(X19_REG), gw(X18_REG)));

	// perform shifft
	ASMJIT_ASSUME_OK(out->ror(gw(reg), gw(reg), gw(nreg)));

	emit_movimm(out, 1, X22_REG);
	emit_lsl_reg_quick(out, X22_REG, X18_REG);
	emit_test(out, X20_REG, X22_REG);

	emit_setnz(out, X5_REG);
	emit_branch_label(out, done, 0xE);

	/* end non zero */

	emit_label(out, and0);

	/* equal 32 */

	// perform shifft
	emit_mov(out, X20_REG, reg);

	emit_movimm(out, 1<<31, X22_REG);
	emit_test(out, X20_REG, X22_REG); // test BIT31(reg 22) of original reg 20

	emit_setnz(out, X5_REG);
	emit_branch_label(out, done, 0xE);

	emit_label(out, done);
	emit_cmpimm(out, reg, 0);
}

static void emit_rrxs_reg(t_bytes *out, int reg, int nreg) {
	emit_rors_reg(out, reg, nreg);
}

//---------------------------------------------------------------------------
// Data processing
//---------------------------------------------------------------------------

static void emit_add(t_bytes *out, int rs1, int rs2, int rt) {
	ASMJIT_ASSUME_OK(out->add(gw(rt), gw(rs1), gw(rs2)));
}

static void emit_adds(t_bytes *out, int rs1, int rs2, int rt) {
	ASMJIT_ASSUME_OK(out->adds(gw(rt), gw(rs1), gw(rs2)));
}

static void emit_add64(t_bytes *out, int rs1, int rs2, int rt) {
	ASMJIT_ASSUME_OK(out->add(gx(rt), gx(rs1), gx(rs2)));
}

static void emit_adc(t_bytes *out, int rs1, int rs2, int rt) {
	ASMJIT_ASSUME_OK(out->adc(gw(rt), gw(rs1), gw(rs2)));
}

static void emit_adcs(t_bytes *out, int rs1, int rs2, int rt) {
	ASMJIT_ASSUME_OK(out->adcs(gw(rt), gw(rs1), gw(rs2)));
}

static void emit_sub(t_bytes *out, int rs1, int rs2, int rt) {
	ASMJIT_ASSUME_OK(out->sub(gw(rt), gw(rs1), gw(rs2)));
}

static void emit_subs(t_bytes *out, int rs1, int rs2, int rt) {
	ASMJIT_ASSUME_OK(out->subs(gw(rt), gw(rs1), gw(rs2)));
}

static void emit_sbc(t_bytes *out, int rs1, int rs2, int rt) {
	ASMJIT_ASSUME_OK(out->sbc(gw(rt), gw(rs1), gw(rs2)));
}

static void emit_sbcs(t_bytes *out, int rs1, int rs2, int rt) {
	ASMJIT_ASSUME_OK(out->sbcs(gw(rt), gw(rs1), gw(rs2)));
}

static void emit_neg(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->neg(gw(rt), gw(rs)));
}

static void emit_negs(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->negs(gw(rt), gw(rs)));
}

static void emit_mvns(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->mvn(gw(rt), gw(rs)));
	emit_cmpimm(out, rt, 0);
}

static void emit_movimms(t_bytes *out, u_int imm, u_int rt) {
	emit_movimm(out, imm, rt);
	emit_cmpimm(out, rt, 0);
}

static void emit_mov_cond(t_bytes *out, int rs, int rt, int cond) {
	uint64_t setb=genlabel();
	uint64_t done=genlabel();

	emit_branch_label(out, setb, cond);
	emit_branch_label(out, done, 0xE);
	emit_label(out, setb);
	emit_mov(out, rs, rt);
	emit_label(out, done);
}

static void emit_test(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->tst(gw(rs), gw(rt)));
}

static void emit_not(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->mvn(gw(rt), gw(rs)));
}

static void emit_and(t_bytes *out, u_int rs1, u_int rs2, u_int rt) {
	ASMJIT_ASSUME_OK(out->and_(gw(rt), gw(rs1), gw(rs2)));
}

static void emit_or(t_bytes *out, u_int rs1, u_int rs2, u_int rt) {
	ASMJIT_ASSUME_OK(out->orr(gw(rt), gw(rs1), gw(rs2)));
}

static void emit_or64(t_bytes *out, u_int rs1, u_int rs2, u_int rt) {
	ASMJIT_ASSUME_OK(out->orr(gx(rt), gx(rs1), gx(rs2)));
}

static void emit_or_and_set_flags(t_bytes *out, int rs1, int rs2, int rt) {
	ASMJIT_ASSUME_OK(out->orr(gw(rt), gw(rs1), gw(rs2)));
	emit_cmpimm(out, rt, 0);
}

static void emit_xor(t_bytes *out, u_int rs1, u_int rs2, u_int rt) {
	ASMJIT_ASSUME_OK(out->eor(gw(rt), gw(rs1), gw(rs2)));
}

static void emit_ands(t_bytes *out, u_int rs1, u_int rs2, u_int rt) {
	ASMJIT_ASSUME_OK(out->ands(gw(rt), gw(rs1), gw(rs2)));
}

static void emit_ors(t_bytes *out, u_int rs1, u_int rs2, u_int rt) {
	emit_or_and_set_flags(out, rs1, rs2, rt);
}

static void emit_xors(t_bytes *out, u_int rs1, u_int rs2, u_int rt) {
	ASMJIT_ASSUME_OK(out->eor(gw(rt), gw(rs1), gw(rs2)));
	emit_cmpimm(out, rt, 0);
}

static void emit_cmp(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->cmp(gw(rs), gw(rt)));
}

static void emit_cmpx64(t_bytes *out, int rs) {
	ASMJIT_ASSUME_OK(out->cmp(gx(rs), asmjit::Imm(0)));
}

static void emit_clz(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->clz(gw(rt), gw(rs)));
}

static void emit_teq(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->eor(gw(16), gw(rs), gw(rt)));
	emit_cmpimm(out, X16_REG, 0);
}

//---------------------------------------------------------------------------
// Add / compare with immediate
//---------------------------------------------------------------------------

static void emit_addimm64(t_bytes *out, u_int rs, int imm, u_int rt) {
	emit_movimm(out, imm, 16);
	emit_add64(out, rs, 16, rt);
}

static void emit_addimm(t_bytes *out, u_int rs, int imm, u_int rt) {
	if(imm!=0) {
		if(imm<0&&imm>-4096) {
			ASMJIT_ASSUME_OK(out->sub(gw(rt), gw(rs), asmjit::Imm((-imm)&0xfff)));
		}else if(imm>0&&imm<4096) {
			ASMJIT_ASSUME_OK(out->add(gw(rt), gw(rs), asmjit::Imm(imm&0xfff)));
		}else if(imm<0) {
			ASMJIT_ASSUME_OK(out->sub(gw(rt), gw(rs), asmjit::Imm(((-imm)>>12&0xfff)<<12)));
			if((-imm&0xfff)!=0) {
				ASMJIT_ASSUME_OK(out->sub(gw(rt), gw(rt), asmjit::Imm((-imm)&0xfff)));
			}
		}else {
			ASMJIT_ASSUME_OK(out->add(gw(rt), gw(rs), asmjit::Imm(((imm>>12)&0xfff)<<12)));
			if((imm&0xfff)!=0) {
				ASMJIT_ASSUME_OK(out->add(gw(rt), gw(rt), asmjit::Imm(imm&0xfff)));
			}
		}
	}
	else if(rs!=rt) emit_mov(out, rs,rt);
}

static void emit_addsimm(t_bytes *out, u_int rs, int imm, u_int rt) {
	emit_movimm(out, imm, X16_REG);
	emit_adds(out, rs, X16_REG, rt);
}

static void emit_subimm(t_bytes *out, int reg, int val, int rt) {
	emit_movimm(out, val, X10_REG);
	emit_sub(out, reg, X10_REG, rt);
}

static void emit_subsimm(t_bytes *out, int reg, int val, int rt) {
	emit_movimm(out, val, X10_REG);
	emit_subs(out, reg, X10_REG, rt);
}

static void emit_cmpimm(t_bytes *out, int rs, int imm) {
	if (imm == 0) {
		ASMJIT_ASSUME_OK(out->cmp(gw(rs), asmjit::Imm(0)));
	}
	else {
		emit_movimm(out, imm, X16_REG);
		emit_cmp(out, rs, X16_REG);
	}
}

static void emit_testimm(t_bytes *out, int rs, int imm) {
	ASMJIT_ASSUME_OK(out->tst(gw(rs), asmjit::Imm(imm)));
}

static void emit_testimm64(t_bytes *out, int rs, int64_t imm) {
	ASMJIT_ASSUME_OK(out->tst(gx(rs), asmjit::Imm((uint64_t)imm)));
}

static void emit_orimm(t_bytes *out, int rs, int imm, int rt) {
	if(imm==0) {
		if(rs!=rt) emit_mov(out, rs,rt);
	}else{
		// The bitmask-immediate form right here has always been disabled: the
		// hand-written encoder got it wrong, so it was forced down the
		// load-then-orr path. Re-enabling it is a behavioural change and belongs
		// with the register-cache work, not with the migration.
		emit_movimm(out, imm,X9_REG);
		ASMJIT_ASSUME_OK(out->orr(gw(rt), gw(rs), gw(X9_REG)));
	}
}

static void emit_xorimm(t_bytes *out, int reg, int val, int rt) {
	emit_movimm(out, val, X10_REG);
	emit_xor(out, reg, X10_REG, rt);
}

static void emit_andimm(t_bytes *out, int rs, int imm, int rt) {
	if(imm==0) {
		emit_zeroreg(out, rt);
	}else{
		// See emit_orimm: the bitmask-immediate form stays disabled on purpose.
		emit_movimm(out, imm,X10_REG);
		ASMJIT_ASSUME_OK(out->and_(gw(rt), gw(rs), gw(X10_REG)));
	}
}

static void emit_andsimm(t_bytes *out, int rs, int imm, int rt) {
	u_int armval;
	if(imm==0) {
		emit_zeroreg(out, rt);
		emit_cmp(out, rt, rt);
	}else if(genimm2((uint64_t)imm,32,&armval)) {
		ASMJIT_ASSUME_OK(out->ands(gw(rt), gw(rs), asmjit::Imm(imm)));
	}else{
		emit_movz(out, imm,X10_REG);
		ASMJIT_ASSUME_OK(out->ands(gw(rt), gw(rs), gw(X10_REG)));
	}
}

//---------------------------------------------------------------------------
// Multiplication and multiply-accumulate
//---------------------------------------------------------------------------

static void emit_mul(t_bytes *out, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->mul(gw(rs1), gw(rs1), gw(rs2)));
}

static void emit_muls(t_bytes *out, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->mul(gw(rs1), gw(rs1), gw(rs2)));
	emit_cmpimm(out, rs1, 0);
}

static void emit_mla(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->madd(gw(hi), gw(rs1), gw(rs2), gw(lo)));
}

static void emit_mlas(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->madd(gw(hi), gw(rs1), gw(rs2), gw(lo)));
	emit_cmpimm(out, hi, 0);
}

static void emit_smulbb(t_bytes *out, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->sbfx(gx(X17_REG), gx(rs1), asmjit::Imm(0), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->sbfx(gx(X18_REG), gx(rs2), asmjit::Imm(0), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->smull(gx(rs1), gw(X17_REG), gw(X18_REG)));
}

static void emit_smulbt(t_bytes *out, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->sbfx(gx(X17_REG), gx(rs1), asmjit::Imm(0), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->sbfx(gx(X18_REG), gx(rs2), asmjit::Imm(16), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->smull(gx(rs1), gw(X17_REG), gw(X18_REG)));
}

static void emit_smultb(t_bytes *out, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->sbfx(gx(X17_REG), gx(rs1), asmjit::Imm(16), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->sbfx(gx(X18_REG), gx(rs2), asmjit::Imm(0), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->smull(gx(rs1), gw(X17_REG), gw(X18_REG)));
}

static void emit_smultt(t_bytes *out, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->sbfx(gx(X17_REG), gx(rs1), asmjit::Imm(16), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->sbfx(gx(X18_REG), gx(rs2), asmjit::Imm(16), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->smull(gx(rs1), gw(X17_REG), gw(X18_REG)));
}

static void emit_smulwt(t_bytes *out, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->sbfx(gx(X18_REG), gx(rs1), asmjit::Imm(16), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->smull(gx(rs1), gw(rs2), gw(X18_REG)));
	emit_asr64(out, rs1, 16);
}

static void emit_smulwb(t_bytes *out, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->sbfx(gx(X18_REG), gx(rs1), asmjit::Imm(0), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->smull(gx(rs1), gw(rs2), gw(X18_REG)));
	emit_asr64(out, rs1, 16);
}

static void emit_smlawb(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->sbfx(gx(X18_REG), gx(rs1), asmjit::Imm(0), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->smull(gx(14), gw(rs2), gw(X18_REG)));
	emit_asr64(out, X14_REG, 16);
	emit_adds(out, X14_REG, lo, hi); // sets carry flag - need to use in SET_Q
}

static void emit_smlawt(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->sbfx(gx(X18_REG), gx(rs1), asmjit::Imm(0), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->smull(gx(14), gw(rs2), gw(X18_REG)));
	emit_asr64(out, X14_REG, 16);
	emit_adds(out, X14_REG, lo, hi); // sets carry flag - need to use in SET_Q
}

static void emit_smlabb(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->sbfx(gx(X17_REG), gx(rs1), asmjit::Imm(0), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->sbfx(gx(X18_REG), gx(rs2), asmjit::Imm(0), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->smull(gx(hi), gw(X17_REG), gw(X18_REG)));
	emit_adds(out, hi, lo, hi); // sets carry flag - need to use in SET_Q
}

static void emit_smlabt(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->sbfx(gx(X17_REG), gx(rs1), asmjit::Imm(0), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->sbfx(gx(X18_REG), gx(rs2), asmjit::Imm(16), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->smull(gx(hi), gw(X17_REG), gw(X18_REG)));
	emit_adds(out, hi, lo, hi);
}

static void emit_smlatb(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->sbfx(gx(X17_REG), gx(rs1), asmjit::Imm(16), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->sbfx(gx(X18_REG), gx(rs2), asmjit::Imm(0), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->smull(gx(hi), gw(X17_REG), gw(X18_REG)));
	emit_adds(out, hi, lo, hi);
}

static void emit_smlatt(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->sbfx(gx(X17_REG), gx(rs1), asmjit::Imm(16), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->sbfx(gx(X18_REG), gx(rs2), asmjit::Imm(16), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->smull(gx(hi), gw(X17_REG), gw(X18_REG)));
	emit_adds(out, hi, lo, hi);
}

// The long multiplies reassemble the 64-bit accumulator from the hi/lo pair,
// multiply-accumulate, then split it again. The sequence (and the scratch
// registers it uses) is exactly the hand-written one.
static void emit_smlal_pair(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2,
                            int rs1_shift, int rs2_shift) {
	emit_mov(out, hi, X17_REG);
	ASMJIT_ASSUME_OK(out->bfi(gx(X17_REG), gx(lo), asmjit::Imm(32), asmjit::Imm(32)));
	ASMJIT_ASSUME_OK(out->sbfx(gx(X19_REG), gx(rs1), asmjit::Imm(rs1_shift), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->sbfx(gx(X20_REG), gx(rs2), asmjit::Imm(rs2_shift), asmjit::Imm(16)));
	ASMJIT_ASSUME_OK(out->smaddl(gx(X17_REG), gw(X19_REG), gw(X20_REG), gx(X17_REG)));
	ASMJIT_ASSUME_OK(out->lsr(gx(lo), gx(X17_REG), asmjit::Imm(32)));
	emit_mov(out, X17_REG, hi);
}

static void emit_smlalbb(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	emit_smlal_pair(out, hi, lo, rs1, rs2, 0, 0);
}
static void emit_smlalbt(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	emit_smlal_pair(out, hi, lo, rs1, rs2, 0, 16);
}
static void emit_smlaltb(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	emit_smlal_pair(out, hi, lo, rs1, rs2, 16, 0);
}
static void emit_smlaltt(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	emit_smlal_pair(out, hi, lo, rs1, rs2, 16, 16);
}

static void emit_umull(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->umull(gx(X17_REG), gw(rs1), gw(rs2)));
	ASMJIT_ASSUME_OK(out->lsr(gx(lo), gx(X17_REG), asmjit::Imm(32)));
	emit_mov(out, X17_REG, hi);
}

static void emit_umulls(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->umull(gx(X17_REG), gw(rs1), gw(rs2)));
	emit_cmpx64(out, X17_REG);
	ASMJIT_ASSUME_OK(out->lsr(gx(lo), gx(X17_REG), asmjit::Imm(32)));
	emit_mov(out, X17_REG, hi);
}

// UMADDL shares the shape with SMADDL; only the instruction differs.
static void emit_umlal_common(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2, int with_flags) {
	emit_mov(out, hi, X17_REG);
	ASMJIT_ASSUME_OK(out->bfi(gx(X17_REG), gx(lo), asmjit::Imm(32), asmjit::Imm(32)));
	ASMJIT_ASSUME_OK(out->smaddl(gx(X17_REG), gw(rs1), gw(rs2), gx(X17_REG)));
	if (with_flags) emit_cmpx64(out, X17_REG);
	ASMJIT_ASSUME_OK(out->lsr(gx(lo), gx(X17_REG), asmjit::Imm(32)));
	emit_mov(out, X17_REG, hi);
}

static void emit_umlal(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	emit_umlal_common(out, hi, lo, rs1, rs2, 0);
}

static void emit_umlals(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	emit_umlal_common(out, hi, lo, rs1, rs2, 1);
}

static void emit_smull(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->smull(gx(X17_REG), gw(rs1), gw(rs2)));
	ASMJIT_ASSUME_OK(out->lsr(gx(lo), gx(X17_REG), asmjit::Imm(32)));
	emit_mov(out, X17_REG, hi);
}

static void emit_smulls(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	ASMJIT_ASSUME_OK(out->smull(gx(X17_REG), gw(rs1), gw(rs2)));
	emit_cmpx64(out, X17_REG);
	ASMJIT_ASSUME_OK(out->lsr(gx(lo), gx(X17_REG), asmjit::Imm(32)));
	emit_mov(out, X17_REG, hi);
}

static void emit_smlal_common(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2, int with_flags) {
	emit_mov(out, hi, X17_REG);
	ASMJIT_ASSUME_OK(out->bfi(gx(X17_REG), gx(lo), asmjit::Imm(32), asmjit::Imm(32)));
	ASMJIT_ASSUME_OK(out->smaddl(gx(X17_REG), gw(rs1), gw(rs2), gx(X17_REG)));
	if (with_flags) emit_cmpx64(out, X17_REG);
	ASMJIT_ASSUME_OK(out->lsr(gx(lo), gx(X17_REG), asmjit::Imm(32)));
	emit_mov(out, X17_REG, hi);
}

static void emit_smlal(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	emit_smlal_common(out, hi, lo, rs1, rs2, 0);
}

static void emit_smlals(t_bytes *out, u_int hi, u_int lo, u_int rs1, u_int rs2) {
	emit_smlal_common(out, hi, lo, rs1, rs2, 1);
}

//---------------------------------------------------------------------------
// Loads and stores
//---------------------------------------------------------------------------

static void emit_ldr(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->ldr(gw(rt), gmem(rs)));
}

// Field access through a block-resident base pointer.
//
// `off` is always an offsetof() value, i.e. a compile-time constant at every
// call site, and it fits the scaled 12-bit immediate that LD/ST allow -- the
// armcpu_t fields this back end touches all live in the first kilobyte. The
// callers fall back to materialising the address when that ever stops being
// true, so an out-of-range offset cannot silently encode a wrong address.
static void emit_ldr_off(t_bytes *out, int rt, int base, int off) {
	ASMJIT_ASSUME_OK(out->ldr(gw(rt), asmjit::a64::ptr(gx(base), off)));
}

static void emit_str_off(t_bytes *out, int rt, int base, int off) {
	ASMJIT_ASSUME_OK(out->str(gw(rt), asmjit::a64::ptr(gx(base), off)));
}

static void emit_ldrb_off(t_bytes *out, int rt, int base, int off) {
	ASMJIT_ASSUME_OK(out->ldrb(gw(rt), asmjit::a64::ptr(gx(base), off)));
}

static void emit_strb_off(t_bytes *out, int rt, int base, int off) {
	ASMJIT_ASSUME_OK(out->strb(gw(rt), asmjit::a64::ptr(gx(base), off)));
}

static void emit_ldrh_off(t_bytes *out, int rt, int base, int off) {
	ASMJIT_ASSUME_OK(out->ldrh(gw(rt), asmjit::a64::ptr(gx(base), off)));
}

static void emit_strh_off(t_bytes *out, int rt, int base, int off) {
	ASMJIT_ASSUME_OK(out->strh(gw(rt), asmjit::a64::ptr(gx(base), off)));
}

static void emit_ldrsh_off(t_bytes *out, int rt, int base, int off) {
	ASMJIT_ASSUME_OK(out->ldrsh(gw(rt), asmjit::a64::ptr(gx(base), off)));
}

static void emit_ldrsb_off(t_bytes *out, int rt, int base, int off) {
	ASMJIT_ASSUME_OK(out->ldrsb(gw(rt), asmjit::a64::ptr(gx(base), off)));
}

// Address of a field of the object `base` points at, in a 64-bit register.
static void emit_add_off64(t_bytes *out, int rt, int base, int off) {
	ASMJIT_ASSUME_OK(out->add(gx(rt), gx(base), asmjit::Imm(off)));
}

static void emit_ldr64(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->ldr(gx(rt), gmem(rs)));
}

static void emit_str(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->str(gw(rt), gmem(rs)));
}

static void emit_str64(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->str(gx(rt), gmem(rs)));
}

static void emit_ldrb(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->ldrb(gw(rt), gmem(rs)));
}

static void emit_ldrsb(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->ldrsb(gw(rt), gmem(rs)));
}

static void emit_strb(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->strb(gw(rt), gmem(rs)));
}

static void emit_ldrsw(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->ldrsh(gw(rt), gmem(rs)));
}

static void emit_ldrs(t_bytes *out, int rs, int rt) {
	emit_ldr(out, rs, rt);
}

static void emit_ldrw(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->ldrh(gw(rt), gmem(rs)));
}

static void emit_strw(t_bytes *out, int rs, int rt) {
	ASMJIT_ASSUME_OK(out->strh(gw(rt), gmem(rs)));
}

static void emit_str_cond(t_bytes *out, int rs, int rt, int cond) {
	uint64_t setb=genlabel();
	uint64_t done=genlabel();

	emit_branch_label(out, setb, cond);
	emit_branch_label(out, done, 0xE);

	emit_label(out, setb);
	emit_str(out, rs, rt);

	emit_label(out, done);
}

static void emit_str_ptr_cond(t_bytes *out, uintptr_t ptr, int reg, int cond) {
	emit_ptr(out, (uintptr_t)ptr, X10_REG);
	emit_str_cond(out, X10_REG, reg, cond);
}

static void emit_write_ptr32(t_bytes *out, uintptr_t ptr, u_int val) {
	emit_ptr(out, (uintptr_t)ptr, X2_REG);
	emit_movimm(out, val, X4_REG);
	emit_str(out, X2_REG, X4_REG);
}

static void emit_write_ptr(t_bytes *out, uintptr_t ptr, uintptr_t val) {
	emit_ptr(out, ptr, X2_REG);
	emit_ptr(out, val, X4_REG);
	emit_str64(out, X2_REG, X4_REG);
}

static void emit_write_ptr32_regptrTO_immFROM(t_bytes *out, int regptr, int val) { /* reg contains ptr */
	emit_movimm(out, (uintptr_t)val, X10_REG);
	emit_str(out, regptr, X10_REG);
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
	emit_ptr(out, (uintptr_t)ptr, X8_REG);
	emit_str(out, X8_REG, reg);
}

static void emit_writeBYTE_ptr32_regptrTO_immFROM(t_bytes *out, int regptr, int val) { /* reg contains ptr */
	emit_movimm(out, (uintptr_t)val, X9_REG);
	emit_strb(out, regptr, X9_REG);
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
	emit_movimm(out, (uintptr_t)val, X9_REG);
	emit_strw(out, regptr, X9_REG);
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
	emit_ldr(out, regptr, X10_REG);
	emit_add(out, X10_REG, reg, X10_REG);
	emit_str(out, regptr, X10_REG);
}

static void emit_adds_ptr32_regptrTO_regFROM(t_bytes *out, int regptr, int reg) { /* reg contains ptr */
	emit_ldr(out, regptr, X10_REG);
	emit_adds(out, X10_REG, reg, X10_REG);
	emit_str(out, regptr, X10_REG);
}

static void emit_add_ptr32_regptrTO_immFROM(t_bytes *out, int regptr, int val) { /* reg contains ptr */
	emit_movimm(out, (uintptr_t)val, X9_REG);
	emit_ldr(out, regptr, X10_REG);
	emit_add(out, X10_REG, X9_REG, X10_REG);
	emit_str(out, regptr, X10_REG);
}

static void emit_adds_ptr32_regptrTO_immFROM(t_bytes *out, int regptr, int val) { /* reg contains ptr */
	emit_movimm(out, (uintptr_t)val, X9_REG);
	emit_ldr(out, regptr, X10_REG);
	emit_adds(out, X10_REG, X9_REG, X10_REG);
	emit_str(out, regptr, X10_REG);
}

static void emit_sub_ptr32_regptrTO_regFROM(t_bytes *out, int regptr, int reg) { /* reg contains ptr */
	emit_ldr(out, regptr, X10_REG);
	emit_sub(out, X10_REG, reg, X10_REG);
	emit_str(out, regptr, X10_REG);
}

static void emit_subs_ptr32_regptrTO_regFROM(t_bytes *out, int regptr, int reg) { /* reg contains ptr */
	emit_ldr(out, regptr, X10_REG);
	emit_subs(out, X10_REG, reg, X10_REG);
	emit_str(out, regptr, X10_REG);
}

static void emit_sub_ptr32_regptrTO_immFROM(t_bytes *out, int regptr, int val) { /* reg contains ptr */
	emit_movimm(out, (uintptr_t)val, X9_REG);
	emit_ldr(out, regptr, X10_REG);
	emit_sub(out, X10_REG, X9_REG, X10_REG);
	emit_str(out, regptr, X10_REG);
}

static void emit_subs_ptr32_regptrTO_immFROM(t_bytes *out, int regptr, int val) { /* reg contains ptr */
	emit_movimm(out, (uintptr_t)val, X9_REG);
	emit_ldr(out, regptr, X10_REG);
	emit_subs(out, X10_REG, X9_REG, X10_REG);
	emit_str(out, regptr, X10_REG);
}

static void emit_neg_ptr32_regptr(t_bytes *out, int regptr) { /* reg contains ptr */
	emit_ldr(out, regptr, X10_REG);
	emit_neg(out, X10_REG, X10_REG);
	emit_str(out, regptr, X10_REG);
}

static void emit_negs_ptr32_regptr(t_bytes *out, int regptr) { /* reg contains ptr */
	emit_ldr(out, regptr, X10_REG);
	emit_negs(out, X10_REG, X10_REG);
	emit_str(out, regptr, X10_REG);
}

static void emit_ror_ptr32_regptr(t_bytes *out, int regptr, int shift) {
	emit_ldr(out, regptr, X10_REG);
	emit_ror(out, X10_REG, shift);
	emit_str(out, regptr, X10_REG);
}

static void emit_rors_ptr32_regptr(t_bytes *out, int regptr, int shift) {
	emit_ldr(out, regptr, X10_REG);
	emit_rors(out, X10_REG, shift);
	emit_str(out, regptr, X10_REG);
}

static void emit_rors_ptr32_regptr_reg(t_bytes *out, int regptr, int regimm) {
	emit_ldr(out, regptr, X10_REG);
	emit_rors_reg(out, X10_REG, regimm);
	emit_str(out, regptr, X10_REG);
}

static void emit_ror_ptr32_regptr_reg(t_bytes *out, int regptr, int regimm) {
	emit_ldr(out, regptr, X10_REG);
	emit_ror_reg(out, X10_REG, regimm);
	emit_str(out, regptr, X10_REG);
}

static void emit_and_ptr32_regptr(t_bytes *out, int regptr, int bits) {
	emit_ldr(out, regptr, X13_REG);
	emit_andimm(out, X13_REG, bits, X13_REG);
	emit_str(out, regptr, X13_REG);
}

static void emit_or_ptr32_regptr(t_bytes *out, int regptr, int bits) {
	emit_ldr(out, regptr, X13_REG);
	emit_orimm(out, X13_REG, bits, X13_REG);
	emit_str(out, regptr, X13_REG);
}

static void emit_or_ptr8_regptr_reg(t_bytes *out, int regptr, int reg) {
	emit_ldrb(out, regptr, X13_REG);
	emit_or(out, X13_REG, reg, X13_REG);
	emit_strb(out, regptr, X13_REG);
}

static void emit_or_ptr8_regptr_imm(t_bytes *out, int regptr, int imm) {
	emit_ldrb(out, regptr, X13_REG);
	emit_orimm(out, X13_REG, imm, X13_REG);
	emit_strb(out, regptr, X13_REG);
}

static void emit_or_ptr32_regptr_reg(t_bytes *out, int regptr, int reg) {
	emit_ldr(out, regptr, X13_REG);
	emit_or(out, X13_REG, reg, X13_REG);
	emit_str(out, regptr, X13_REG);
}

static void emit_and_ptr32_regptr_reg(t_bytes *out, int regptr, int reg) {
	emit_ldr(out, regptr, X13_REG);
	emit_and(out, X13_REG, reg, X13_REG);
	emit_str(out, regptr, X13_REG);
}

//---------------------------------------------------------------------------
// Condition codes into registers
//---------------------------------------------------------------------------

static void emit_setc(t_bytes *out, int reg) {
	ASMJIT_ASSUME_OK(out->cset(gw(reg), asmjit::arm::CondCode::kCS));
}

static void emit_sets(t_bytes *out, int reg) {
	ASMJIT_ASSUME_OK(out->cset(gw(reg), asmjit::arm::CondCode::kMI));
}

static void emit_setz(t_bytes *out, int reg) {
	ASMJIT_ASSUME_OK(out->cset(gw(reg), asmjit::arm::CondCode::kEQ));
}

static void emit_setnz(t_bytes *out, int reg) {
	ASMJIT_ASSUME_OK(out->cset(gw(reg), asmjit::arm::CondCode::kNE));
}

static void emit_setnc(t_bytes *out, int reg) {
	ASMJIT_ASSUME_OK(out->cset(gw(reg), asmjit::arm::CondCode::kCC));
}

static void emit_seto(t_bytes *out, int reg) {
	ASMJIT_ASSUME_OK(out->cset(gw(reg), asmjit::arm::CondCode::kVS));
}

static void emit_write_ptr32_reg_byte(t_bytes *out, uintptr_t ptr, int reg) {
	emit_movimm(out, (uintptr_t)ptr, X8_REG);
	emit_strb(out, X8_REG, reg);
}

//---------------------------------------------------------------------------
// Address computation
//---------------------------------------------------------------------------

static void emit_lea_reg_reg(t_bytes *out, int reg1, int reg, int scale, int outreg) {
	if (scale == 2)      emit_lsl(out, reg, 1); // MOV r10, r10, LSL #1 (multiple by 2)
	else if (scale == 4) emit_lsl(out, reg, 2); // MOV r10, r10, LSL #2 (multiple by 4)
	else if (scale == 8) emit_lsl(out, reg, 3); // MOV r10, r10, LSL #3 (multiple by 8)

	emit_add(out, reg, reg1, X10_REG);
	emit_mov(out, X10_REG, outreg);
}

static void emit_lea_ptr_reg(t_bytes *out, int ptr, int reg, int scale, int outreg) {
	emit_movimm(out, ptr, X10_REG);

	if (scale == 2)      emit_lsl(out, reg, 1);
	else if (scale == 4) emit_lsl(out, reg, 2);
	else if (scale == 8) emit_lsl(out, reg, 3);

	emit_add(out, reg, X10_REG, X10_REG);
	emit_mov(out, X10_REG, outreg);
}

static void emit_lea_ptr_val(t_bytes *out, int ptr, int val, int scale, int outreg) {
	emit_movimm(out, ptr, X10_REG);
	emit_movimm(out, val, X9_REG);

	// The shift amounts here are 2/4/8 rather than 1/2/3, so this scales by
	// 4/16/256. Left untouched: it is existing behaviour and changing it is a
	// semantic change, not a migration step.
	if (scale == 2)      emit_lsl(out, X9_REG, 2);
	else if (scale == 4) emit_lsl(out, X9_REG, 4);
	else if (scale == 8) emit_lsl(out, X9_REG, 8);

	emit_add(out, X9_REG, X10_REG, X10_REG);

	emit_mov(out, outreg, X10_REG);
}

//---------------------------------------------------------------------------
// NZCV helpers
//---------------------------------------------------------------------------

static void get_flags(int reg, int v);

static void clear_carry_flag(t_bytes *out) {
	ASMJIT_ASSUME_OK(out->mrs(gx(X19_REG), asmjit::Imm(asmjit::a64::Predicate::SysReg::kNZCV)));
	ASMJIT_ASSUME_OK(out->bic(gx(X19_REG), gx(X19_REG), asmjit::Imm(1u << 29)));
	ASMJIT_ASSUME_OK(out->msr(asmjit::Imm(asmjit::a64::Predicate::SysReg::kNZCV), gx(X19_REG)));
}

static void set_carry_flag(t_bytes *out) {
	ASMJIT_ASSUME_OK(out->mrs(gx(X19_REG), asmjit::Imm(asmjit::a64::Predicate::SysReg::kNZCV)));
	ASMJIT_ASSUME_OK(out->orr(gx(X19_REG), gx(X19_REG), asmjit::Imm(1u << 29)));
	ASMJIT_ASSUME_OK(out->msr(asmjit::Imm(asmjit::a64::Predicate::SysReg::kNZCV), gx(X19_REG)));
}

static void get_carry_flag(t_bytes *out, int reg) {
	get_flags(reg, VALUE_NO_CACHE);
	ASMJIT_ASSUME_OK(out->and_(gw(reg), gw(reg), asmjit::Imm(1 << 5)));
	ASMJIT_ASSUME_OK(out->lsr(gw(reg), gw(reg), asmjit::Imm(5)));
}

//---------------------------------------------------------------------------
// Pointers held in globals
//---------------------------------------------------------------------------

static void emit_read_ptr32_reg(t_bytes *out, uintptr_t ptr, int rs) {
	emit_ptr(out, (uintptr_t)ptr, X10_REG);
	emit_ldr(out, X10_REG, rs);
}

static void emit_read_ptr64_reg(t_bytes *out, uintptr_t ptr, int rs) {
	emit_ptr(out, (uintptr_t)ptr, X10_REG);
	emit_ldr64(out, X10_REG, rs);
}

static void emit_read_ptr32_reg_byte(t_bytes *out, uintptr_t ptr, int rs) {
	emit_ptr(out, (uintptr_t)ptr, X8_REG);
	emit_ldrb(out, X8_REG, rs);
}

static void emit_var_copy32(t_bytes *out, uintptr_t from, uintptr_t to) {
	emit_ptr(out, (uintptr_t)from, X7_REG);
	emit_ptr(out, (uintptr_t)to, X7_REG);
}

static void emit_var_copy32_reg(t_bytes *out, int from, int to) {
	emit_str(out, to, from);
}

static void emit_set_ptr_imm(t_bytes *out, uintptr_t ptr, int set) {
	emit_movimm(out, set, X10_REG);
	emit_write_ptr32_reg(out, ptr, X10_REG);
}

static void emit_inc_ptr_imm(t_bytes *out, uintptr_t ptr, int add) {
	emit_read_ptr32_reg(out, ptr, X10_REG);
	if (add > 0) {
		emit_movimm(out, add, X9_REG);
		emit_add(out, X10_REG, X9_REG, X10_REG);
	}
	else {
		emit_movimm(out, add*-1, X9_REG);
		emit_sub(out, X10_REG, X9_REG, X10_REG);
	}
	emit_write_ptr32_reg(out, ptr, X10_REG);
}

static void emit_sub_ptr_imm(t_bytes *out, uintptr_t ptr, int minus) {
	emit_read_ptr32_reg(out, ptr, X10_REG);
	emit_movimm(out, minus, X9_REG);
	emit_sub(out, X10_REG, X9_REG, X10_REG);
	emit_write_ptr32_reg(out, ptr, X10_REG);
}

static void emit_inc_ptr(t_bytes *out, uintptr_t ptr, uintptr_t ptr2) {
	emit_read_ptr32_reg(out, ptr, X2_REG);
	emit_read_ptr32_reg(out, ptr2, X3_REG);
	emit_add(out, X2_REG, X3_REG, X2_REG);
	emit_write_ptr32_reg(out, ptr, X2_REG);
}

static void emit_inc_ptr_reg(t_bytes *out, uintptr_t ptr, uintptr_t ptr2, int reg) {
	emit_read_ptr32_reg(out, ptr, X7_REG);
	emit_read_ptr32_reg(out, ptr2, X9_REG);
	emit_add(out, reg, X9_REG, X7_REG);
}

static void emit_inc_reg(t_bytes *out, uintptr_t ptr, int reg) {
	emit_read_ptr32_reg(out, ptr, X9_REG);
	emit_add(out, reg, X9_REG, reg);
}

static void emit_brk(t_bytes *out) {
	ASMJIT_ASSUME_OK(out->brk(asmjit::Imm(0)));
}

//---------------------------------------------------------------------------
// Function prolog, epilog and helper calls
//
// The frame is still managed by hand (save/restore below), not by asmjit's PEI:
// the block returns bb_total_cycles and the handlers rely on the exact register
// set that save() preserves. With an empty FuncFrame PEI emits nothing of its
// own, so the two do not fight.
//---------------------------------------------------------------------------

static void save(t_bytes *out) {
	ASMJIT_ASSUME_OK(out->stp(gx(19), gx(20), asmjit::a64::ptr_pre(asmjit::a64::sp, -16)));
	ASMJIT_ASSUME_OK(out->stp(gx(21), gx(22), asmjit::a64::ptr_pre(asmjit::a64::sp, -16)));
	ASMJIT_ASSUME_OK(out->stp(gx(23), gx(24), asmjit::a64::ptr_pre(asmjit::a64::sp, -16)));
	// x25/x26 hold the block-resident base pointers (see JIT_CPU_BASE_REG in
	// arm_jit_arm.cpp), so they must survive the return to the caller. Pushed
	// as a pair because that is one instruction.
	ASMJIT_ASSUME_OK(out->stp(gx(25), gx(26), asmjit::a64::ptr_pre(asmjit::a64::sp, -16)));
}

static void restore(t_bytes *out) {
	ASMJIT_ASSUME_OK(out->ldp(gx(25), gx(26), asmjit::a64::ptr_post(asmjit::a64::sp, 16)));
	ASMJIT_ASSUME_OK(out->ldp(gx(23), gx(24), asmjit::a64::ptr_post(asmjit::a64::sp, 16)));
	ASMJIT_ASSUME_OK(out->ldp(gx(21), gx(22), asmjit::a64::ptr_post(asmjit::a64::sp, 16)));
	ASMJIT_ASSUME_OK(out->ldp(gx(19), gx(20), asmjit::a64::ptr_post(asmjit::a64::sp, 16)));
}

static void emit_startfunc(t_bytes *out) {
	ASMJIT_ASSUME_OK(out->stp(gx(30), gx(29), asmjit::a64::ptr_pre(asmjit::a64::sp, -16)));
	save(out);
}

static void emit_endfunc(t_bytes *out) {
	restore(out);

	ASMJIT_ASSUME_OK(out->ldp(gx(30), gx(29), asmjit::a64::ptr_post(asmjit::a64::sp, 16)));
	ASMJIT_ASSUME_OK(out->ret(gx(30)));
}

static void save_call(t_bytes *out) {
	ASMJIT_ASSUME_OK(out->stp(gx(1), gx(2), asmjit::a64::ptr_pre(asmjit::a64::sp, -16)));
	ASMJIT_ASSUME_OK(out->stp(gx(3), gx(4), asmjit::a64::ptr_pre(asmjit::a64::sp, -16)));
	ASMJIT_ASSUME_OK(out->stp(gx(5), gx(6), asmjit::a64::ptr_pre(asmjit::a64::sp, -16)));
	ASMJIT_ASSUME_OK(out->stp(gx(7), gx(8), asmjit::a64::ptr_pre(asmjit::a64::sp, -16)));
	ASMJIT_ASSUME_OK(out->stp(gx(9), gx(10), asmjit::a64::ptr_pre(asmjit::a64::sp, -16)));
	ASMJIT_ASSUME_OK(out->stp(gx(11), gx(12), asmjit::a64::ptr_pre(asmjit::a64::sp, -16)));

	#if (defined(_WIN32) || defined(__APPLE__) || defined(__ANDROID__))
	ASMJIT_ASSUME_OK(out->str(gx(23), asmjit::a64::ptr_pre(asmjit::a64::sp, -16)));
	#endif
}

static void restore_call(t_bytes *out) {
	#if (defined(_WIN32) || defined(__APPLE__) || defined(__ANDROID__))
	ASMJIT_ASSUME_OK(out->ldr(gx(23), asmjit::a64::ptr_post(asmjit::a64::sp, 16)));
	#endif

	ASMJIT_ASSUME_OK(out->ldp(gx(11), gx(12), asmjit::a64::ptr_post(asmjit::a64::sp, 16)));
	ASMJIT_ASSUME_OK(out->ldp(gx(9), gx(10), asmjit::a64::ptr_post(asmjit::a64::sp, 16)));
	ASMJIT_ASSUME_OK(out->ldp(gx(7), gx(8), asmjit::a64::ptr_post(asmjit::a64::sp, 16)));
	ASMJIT_ASSUME_OK(out->ldp(gx(5), gx(6), asmjit::a64::ptr_post(asmjit::a64::sp, 16)));
	ASMJIT_ASSUME_OK(out->ldp(gx(3), gx(4), asmjit::a64::ptr_post(asmjit::a64::sp, 16)));
	ASMJIT_ASSUME_OK(out->ldp(gx(1), gx(2), asmjit::a64::ptr_post(asmjit::a64::sp, 16)));
}

static void callR3(t_bytes *out) {
	save_call(out);
	ASMJIT_ASSUME_OK(out->blr(gx(3)));
	restore_call(out);
}

static void callR9(t_bytes *out) {
	save_call(out);
	ASMJIT_ASSUME_OK(out->blr(gx(9)));
	restore_call(out);
}

static void callR7(t_bytes *out) {
	save_call(out);
	ASMJIT_ASSUME_OK(out->blr(gx(7)));
	restore_call(out);
}

static void callR6(t_bytes *out) {
	save_call(out);
	ASMJIT_ASSUME_OK(out->blr(gx(6)));
	restore_call(out);
}

static void callR8(t_bytes *out) {
	save_call(out);
	ASMJIT_ASSUME_OK(out->blr(gx(8)));
	restore_call(out);
}

//-----------------------------------------------------------------------------
//   Shifting macros
//
// Expanded inside arm_jit_arm.cpp, so they work on `g_out` and on that file's
// register macros (x/y/rcf...). Rewritten against asmjit for the same reason as
// the primitives above.
//-----------------------------------------------------------------------------
#define SET_NZCV(sign) { \
	JIT_COMMENT("SET_NZCV"); \
	ASMJIT_ASSUME_OK(g_out->mrs(gx(X1_REG), asmjit::Imm(asmjit::a64::Predicate::SysReg::kNZCV))); \
	flags_ptr(y_reg, VALUE_NO_CACHE); \
	ASMJIT_ASSUME_OK(g_out->lsr(gx(X1_REG), gx(X1_REG), asmjit::Imm(0x18))); \
	/* After the shift, x1[7:4] is host N/Z/C/V and x1[3:0] is zero. Replace the
	 * old `and x1,#0xf0 / and x9,#0xf / orr` with one bitfield insert: CPSR's
	 * low nibble is the mode, which must survive a flag update verbatim. */ \
	ASMJIT_ASSUME_OK(g_out->bfxil(gw(X1_REG), gw(y_reg), asmjit::Imm(0), asmjit::Imm(4))); \
	emit_strb_off(g_out, X1_REG, JIT_CPU_BASE_REG, RC_FLAGS_OFF); \
}

#define SET_NZC { \
	JIT_COMMENT("SET_NZC"); \
	ASMJIT_ASSUME_OK(g_out->mrs(gx(X1_REG), asmjit::Imm(asmjit::a64::Predicate::SysReg::kNZCV))); \
	flags_ptr(y_reg, VALUE_NO_CACHE); \
	ASMJIT_ASSUME_OK(g_out->lsr(gx(X1_REG), gx(X1_REG), asmjit::Imm(0x18))); \
	ASMJIT_ASSUME_OK(g_out->and_(gx(X1_REG), gx(X1_REG), asmjit::Imm(0xc0))); \
	if (cf_change) { ASMJIT_ASSUME_OK(g_out->orr(gx(X1_REG), gx(X1_REG), gx(X5_REG), asmjit::Imm(5))); /* x5 == rcf, already <<5 */ } \
	/* `and x9,#0x1f/0x3f` + `orr` collapse into one bitfield insert. cf_change
	 * keeps only the 5-bit mode (C comes from rcf); otherwise the 6 low bits
	 * (mode + old C) survive. */ \
	ASMJIT_ASSUME_OK(g_out->bfxil(gw(X1_REG), gw(y_reg), asmjit::Imm(0), asmjit::Imm(cf_change ? 5 : 6))); \
	emit_strb_off(g_out, X1_REG, JIT_CPU_BASE_REG, RC_FLAGS_OFF); \
}

#define SET_NZC_SHIFTS_ZERO(cf) { \
	JIT_COMMENT("SET_NZC_SHIFTS_ZERO"); \
	flags_ptr(R3, VALUE); \
	emit_and_ptr32_regptr(g_out, CACHED_PTR(flags_ptr(X1_REG, ADDRESS)), 0x1F); \
	if(cf) \
	{ \
		emit_lsl(g_out, rcf_reg, 5); \
		emit_orimm(g_out,rcf_reg, (1<<6), rcf_reg); \
		emit_or_ptr32_regptr_reg(g_out, CACHED_PTR(flags_ptr(X1_REG, ADDRESS)), rcf_reg); \
	} \
	else \
		emit_or_ptr32_regptr(g_out, CACHED_PTR(flags_ptr(X1_REG, ADDRESS)), (1<<6)); \
}

#define SET_NZ(clear_cv) { \
	ASMJIT_ASSUME_OK(g_out->mrs(gx(X1_REG), asmjit::Imm(asmjit::a64::Predicate::SysReg::kNZCV))); \
	flags_ptr(y_reg, VALUE_NO_CACHE); \
	ASMJIT_ASSUME_OK(g_out->lsr(gx(X1_REG), gx(X1_REG), asmjit::Imm(0x18))); \
	ASMJIT_ASSUME_OK(g_out->and_(gx(X1_REG), gx(X1_REG), asmjit::Imm(0xc0))); \
	ASMJIT_ASSUME_OK(g_out->bfxil(gw(X1_REG), gw(y_reg), asmjit::Imm(0), asmjit::Imm(clear_cv ? 4 : 6))); \
	emit_strb_off(g_out, X1_REG, JIT_CPU_BASE_REG, RC_FLAGS_OFF); \
}

#define SET_N { \
	ASMJIT_ASSUME_OK(g_out->mrs(gx(X1_REG), asmjit::Imm(asmjit::a64::Predicate::SysReg::kNZCV))); \
	flags_ptr(y_reg, VALUE_NO_CACHE); \
	ASMJIT_ASSUME_OK(g_out->lsr(gx(X1_REG), gx(X1_REG), asmjit::Imm(0x18))); \
	ASMJIT_ASSUME_OK(g_out->and_(gx(X1_REG), gx(X1_REG), asmjit::Imm(0x80))); \
	ASMJIT_ASSUME_OK(g_out->bfxil(gw(X1_REG), gw(y_reg), asmjit::Imm(0), asmjit::Imm(7))); \
	emit_strb_off(g_out, X1_REG, JIT_CPU_BASE_REG, RC_FLAGS_OFF); \
}

#define SET_Z { \
	ASMJIT_ASSUME_OK(g_out->mrs(gx(X1_REG), asmjit::Imm(asmjit::a64::Predicate::SysReg::kNZCV))); \
	flags_ptr(y_reg, VALUE_NO_CACHE); \
	ASMJIT_ASSUME_OK(g_out->lsr(gx(X1_REG), gx(X1_REG), asmjit::Imm(0x18))); \
	ASMJIT_ASSUME_OK(g_out->and_(gx(X1_REG), gx(X1_REG), asmjit::Imm(0x40))); \
	emit_andimm(g_out, X9_REG, 0xbf, X9_REG); /* and sb, sb, #0xbf */ \
	emit_or64(g_out, X1_REG, y_reg, X1_REG); \
	emit_strb_off(g_out, X1_REG, JIT_CPU_BASE_REG, RC_FLAGS_OFF); \
}

#define SET_Q { \
	flags_ptr(y_reg, VALUE); \
	emit_seto(g_out, y_reg); \
	emit_lsl(g_out, y_reg, 3); \
	emit_or_ptr32_regptr_reg(g_out, CACHED_PTR(flags_ptr(X1_REG, ADDRESS)), y_reg); \
}

// x86 uses `bt flags_ptr, 5` (+ `cmc` for SBC) to load the emulated carry into
// the host CF in one instruction. AArch64 has no memory-bit-to-NZCV instruction,
// but there is a branchless equivalent: isolate CPSR.C and `cmp` it against the
// same mask. `cmp x, #0x20` sets host C exactly when bit 5 was set, which is the
// polarity both `adcs` and `sbcs` want -- unlike x86's `sbb`, AArch64 `sbc`
// consumes C as "no borrow", the same convention ARM uses. That is also why the
// `invert` parameter was already a no-op in the old arm64 macro.
//
// The old body spent `ands` + a taken/not-taken branch + `mrs`/`bic|orr`/`msr`
// (3 + 3 instructions) on this, with a branch per ADC/SBC/RSC. Keep the
// `flags_ptr(..., VALUE)` access so the implicit "last field address in x11"
// contract stays intact for the handlers that store through it.
#define GET_CARRY(invert) { \
	flags_ptr(X2_REG, VALUE_NO_CACHE); \
	ASMJIT_ASSUME_OK(g_out->and_(gw(X2_REG), gw(X2_REG), asmjit::Imm(0x20))); \
	ASMJIT_ASSUME_OK(g_out->cmp(gw(X2_REG), asmjit::Imm(0x20))); \
}
