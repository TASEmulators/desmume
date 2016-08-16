#ifndef ARM_GEN_H_LR
#define ARM_GEN_H_LR

#include <assert.h>
#include <stdio.h>

#if defined(VITA)
# include <psp2/kernel/sysmem.h>
#endif

namespace arm_gen
{

template<uint32_t MAX>
struct Constraint
{
   public:
      Constraint(uint32_t val) : value(val) { assert(val < MAX); }
      operator uint32_t() const { return value; }

   private:
      const uint32_t value;
};

struct reg_t : public Constraint<16>
{
   public:
      reg_t(uint32_t num) : Constraint<16>(num) { }
};

// Do NOT reorder these enums
enum AG_COND
{
   EQ, NE, CS, CC, MI, PL, VS, VC,
   HI, LS, GE, LT, GT, LE, AL, EGG,
   CONDINVALID
};

enum AG_ALU_OP
{
   AND, ANDS, EOR, EORS, SUB, SUBS, RSB, RSBS,
   ADD, ADDS, ADC, ADCS, SBC, SBCS, RSC, RSCS,
   XX1, TST , XX2, TEQ , XX3, CMP , XX4, CMN ,
   ORR, ORRS, MOV, MOVS, BIC, BICS, MVN, MVNS,
   OPINVALID
};

enum AG_MEM_OP
{
   STR, LDR, STRB, LDRB, MEMINVALID
};

enum AG_MEM_FLAGS
{
   POST_INDEX = 1 << 24,
   NEGATE_OFFSET = 1 << 23,
   WRITE_BACK = 1 << 21,
   MEM_NONE = 0
};

enum AG_ALU_SHIFT
{
   LSL, LSR, ASR, ROR, SHIFTINVALID
};

struct alu2
{
   private:
      alu2(uint32_t val) : encoding(val) { }

   public:
      static alu2 reg_shift_reg(reg_t rm, AG_ALU_SHIFT type, reg_t rs)     { return alu2(rm | (type << 5) | 0x10 | (rs << 8)); }
      static alu2 reg_shift_imm(reg_t rm, AG_ALU_SHIFT type, uint32_t imm) { return alu2(rm | (type << 5) | (imm << 7)); }
      static alu2 imm_ror(uint32_t val, uint32_t ror)                      { return alu2((1 << 25) | ((ror / 2) << 8) | val); }
      static alu2 imm_rol(uint32_t val, uint32_t rol)                      { return imm_ror(val, (32 - rol) & 0x1F); }


      static alu2 reg(reg_t rm)                                            { return reg_shift_imm(rm, LSL, 0); }
      static alu2 imm(uint8_t val)                                         { return imm_ror(val, 0); }

      const uint32_t encoding;
};

struct mem2
{
   private:
      mem2(uint32_t val) : encoding(val) { }

   public:
      static mem2 reg_shift_imm(reg_t rm, AG_ALU_SHIFT type, uint32_t imm) { return mem2((1 << 25) | rm | (type << 5) | (imm << 7)); }

      static mem2 reg(reg_t rm)                                            { return reg_shift_imm(rm, LSL, 0); }
      static mem2 imm(uint32_t val)                                        { return mem2(val); }

      const uint32_t encoding;
};

// 80 Columns be damned
class code_pool
{
   public:
      code_pool(uint32_t instruction_count);
      ~code_pool();

      uint32_t instructions_remaining() const { return instruction_count - next_instruction; }

      void* fn_pointer();

      // Relocs
      void set_label(const char* name);
      void resolve_label(const char* name);

      // Code Gen: Generic
      void insert_instruction(uint32_t op, AG_COND cond = AL);
      void insert_raw_instruction(uint32_t op);

      // Code Gen: ALU
      void alu_op(AG_ALU_OP op, reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL);
      void and_(reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(AND , rd, rn, arg, cond); }
      void and_(reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(AND , rd, rd, arg, cond); }
      void ands(reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(ANDS, rd, rn, arg, cond); }
      void ands(reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(ANDS, rd, rd, arg, cond); }
      void eor (reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(EOR , rd, rn, arg, cond); }
      void eor (reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(EOR , rd, rd, arg, cond); }
      void eors(reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(EORS, rd, rn, arg, cond); }
      void eors(reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(EORS, rd, rd, arg, cond); }
      void sub (reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(SUB , rd, rn, arg, cond); }
      void sub (reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(SUB , rd, rd, arg, cond); }
      void subs(reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(SUBS, rd, rn, arg, cond); }
      void subs(reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(SUBS, rd, rd, arg, cond); }
      void rsb (reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(RSB , rd, rn, arg, cond); }
      void rsb (reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(RSB , rd, rd, arg, cond); }
      void rsbs(reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(RSBS, rd, rn, arg, cond); }
      void rsbs(reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(RSBS, rd, rd, arg, cond); }
      void add (reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(ADD , rd, rn, arg, cond); }
      void add (reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(ADD , rd, rd, arg, cond); }
      void adds(reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(ADDS, rd, rn, arg, cond); }
      void adds(reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(ADDS, rd, rd, arg, cond); }
      void adc (reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(ADC , rd, rn, arg, cond); }
      void adc (reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(ADC , rd, rd, arg, cond); }
      void adcs(reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(ADCS, rd, rn, arg, cond); }
      void adcs(reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(ADCS, rd, rd, arg, cond); }
      void sbc (reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(SBC , rd, rn, arg, cond); }
      void sbc (reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(SBC , rd, rd, arg, cond); }
      void sbcs(reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(SBCS, rd, rn, arg, cond); }
      void sbcs(reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(SBCS, rd, rd, arg, cond); }
      void rsc (reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(RSC , rd, rn, arg, cond); }
      void rsc (reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(RSC , rd, rd, arg, cond); }
      void rscs(reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(RSCS, rd, rn, arg, cond); }
      void rscs(reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(RSCS, rd, rd, arg, cond); }
      void tst (          reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(TST , rn, rn, arg, cond); } // 1
      void teq (          reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(TEQ , rn, rn, arg, cond); } // 1
      void cmp (          reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(CMP , rn, rn, arg, cond); } // 1
      void cmn (          reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(CMN , rn, rn, arg, cond); } // 1
      void orr (reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(ORR , rd, rn, arg, cond); }
      void orr (reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(ORR , rd, rd, arg, cond); }
      void orrs(reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(ORRS, rd, rn, arg, cond); }
      void orrs(reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(ORRS, rd, rd, arg, cond); }
      void mov (reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(MOV , rd, rd, arg, cond); } // 2
      void movs(reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(MOVS, rd, rd, arg, cond); } // 2
      void bic (reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(BIC , rd, rn, arg, cond); }
      void bic (reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(BIC , rd, rd, arg, cond); }
      void bics(reg_t rd, reg_t rn, const alu2& arg, AG_COND cond = AL) { alu_op(BICS, rd, rn, arg, cond); }
      void bics(reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(BICS, rd, rd, arg, cond); }
      void mvn (reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(MVN , rd, rd, arg, cond); } // 2
      void mvns(reg_t rd,           const alu2& arg, AG_COND cond = AL) { alu_op(MVNS, rd, rd, arg, cond); } // 2

      // Code Gen: Memory
      void mem_op(AG_MEM_OP op, reg_t rd, reg_t rn, const mem2& arg, AG_MEM_FLAGS flags = MEM_NONE, AG_COND cond = AL);
      void ldr (reg_t rd, reg_t base, const mem2& arg = mem2::imm(0), AG_MEM_FLAGS flags = MEM_NONE, AG_COND cond = AL) { mem_op(LDR , rd, base, arg, flags, cond); }
      void str (reg_t rd, reg_t base, const mem2& arg = mem2::imm(0), AG_MEM_FLAGS flags = MEM_NONE, AG_COND cond = AL) { mem_op(STR , rd, base, arg, flags, cond); }
      void ldrb(reg_t rd, reg_t base, const mem2& arg = mem2::imm(0), AG_MEM_FLAGS flags = MEM_NONE, AG_COND cond = AL) { mem_op(LDRB, rd, base, arg, flags, cond); }
      void strb(reg_t rd, reg_t base, const mem2& arg = mem2::imm(0), AG_MEM_FLAGS flags = MEM_NONE, AG_COND cond = AL) { mem_op(STRB, rd, base, arg, flags, cond); }

      // Code Gen: Sign Extend
      void sxtb(reg_t rd, reg_t rm, AG_COND cond = AL)           { insert_instruction( 0x06AF0070 | (rd << 12) | rm, cond ); }
      void sxth(reg_t rd, reg_t rm, AG_COND cond = AL)           { insert_instruction( 0x06BF0070 | (rd << 12) | rm, cond ); }
      void uxtb(reg_t rd, reg_t rm, AG_COND cond = AL)           { insert_instruction( 0x06EF0070 | (rd << 12) | rm, cond ); }
      void uxth(reg_t rd, reg_t rm, AG_COND cond = AL)           { insert_instruction( 0x06FF0070 | (rd << 12) | rm, cond ); }

      // Code Gen: Other
      void set_status(reg_t source_reg, AG_COND cond = AL)  { insert_instruction( 0x0128F000 | source_reg, cond ); }
      void get_status(reg_t dest_reg, AG_COND cond = AL)    { insert_instruction( 0x010F0000 | (dest_reg << 12), cond ); }
      void bx(reg_t target_reg, AG_COND cond = AL)          { insert_instruction( 0x012FFF10 | target_reg, cond ); }
      void blx(reg_t target_reg, AG_COND cond = AL)         { insert_instruction( 0x012FFF30 | target_reg, cond ); }
      void push(uint16_t regs, AG_COND cond = AL)           { insert_instruction( 0x092D0000 | regs, cond ); }
      void pop(uint16_t regs, AG_COND cond = AL)            { insert_instruction( 0x08BD0000 | regs, cond ); }

      void b(const char* target, AG_COND cond = AL);

      // Inserts a movw; movt pair to load the constant, omits movt is constant fits in 16 bits.
      void load_constant(reg_t target_reg, uint32_t constant, AG_COND cond = AL);
      void insert_constants();

      void jmp(uint32_t offset);
      void resolve_jmp(uint32_t instruction, uint32_t offset);

      uint32_t get_next_instruction() { return next_instruction; };

   private:
      const uint32_t instruction_count;
      uint32_t* instructions;

      uint32_t next_instruction;
      uint32_t flush_start;

      uint32_t literals[128][2];
      uint32_t literal_count;

      static const uint32_t TARGET_COUNT = 16;

      struct target
      {
         const char* name;
         uint32_t position;
      };

      target labels[TARGET_COUNT];
      target branches[TARGET_COUNT];
#if defined(VITA)
      SceUID block;
#endif
};
} // namespace arm_gen

#endif
