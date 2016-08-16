#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "arm_gen.h"

#ifdef _3DS
# include <malloc.h>
# include "3ds/memory.h"
#elif defined(VITA)
# include <psp2/kernel/sysmem.h>
# define RW_INIT sceKernelOpenVMDomain
# define RW_END sceKernelCloseVMDomain
#else
# include <sys/mman.h>
#endif

// __clear_cache(start, end)
#ifdef __BLACKBERRY_QNX__
#undef __clear_cache
#define __clear_cache(start,end) msync(start, (size_t)((void*)end - (void*)start), MS_SYNC | MS_CACHE_ONLY | MS_INVALIDATE_ICACHE);
#elif defined(__MACH__)
#include <libkern/OSCacheControl.h>
#define __clear_cache mach_clear_cache
static void __clear_cache(void *start, void *end) {
  size_t len = (char *)end - (char *)start;
  sys_dcache_flush(start, len);
  sys_icache_invalidate(start, len);
}
#elif defined(_3DS)
#undef __clear_cache
#define __clear_cache(start,end)FlushInvalidateCache();
#elif defined(VITA)
#undef __clear_cache
#define __clear_cache(start,end)sceKernelSyncVMDomain(block, start, (char *)end - (char *)start)
#endif

namespace arm_gen
{

#ifdef _3DS
uint32_t* _instructions = 0;
#endif

code_pool::code_pool(uint32_t icount) :
   instruction_count(icount),
   instructions(0),
   next_instruction(0),
   flush_start(0)
{

   printf("\n\ncode_pool icount: %i\n\n", icount);
   literal_count = 0;
   memset(labels, 0, sizeof(labels));
   memset(branches, 0, sizeof(branches));

#if defined(_3DS)
   if(!_instructions)
   {
      _instructions = (uint32_t*)memalign(4096, instruction_count * 4);
      if (!_instructions)
      {
         fprintf(stderr, "memalign failed\n");
         abort();
      }
      ReprotectMemory((unsigned int*)_instructions, (instruction_count * 4) / 4096, 7);
   }
   instructions = _instructions;
#elif defined(VITA)
   block = sceKernelAllocMemBlockForVM("desmume_rwx_block", instruction_count * 4);
   if (block < 0)
   {
      fprintf(stderr, "sceKernelAllocMemBlockForVM failed\n");
      abort();
   }

   if (sceKernelGetMemBlockBase(block, (void **)&instructions) < 0)
   {
      fprintf(stderr, "sceKernelGetMemBlockBase failed\n");
      abort();
   }
#elif defined(USE_POSIX_MEMALIGN)
   if (posix_memalign((void**)&instructions, 4096, instruction_count * 4))
   {
      fprintf(stderr, "posix_memalign failed\n");
      abort();
   }

   if (mprotect(instructions, instruction_count * 4, PROT_READ | PROT_WRITE | PROT_EXEC))
   {
      fprintf(stderr, "mprotect failed\n");
      abort();
   }
#else
   instructions = (uint32_t*)memalign(4096, instruction_count * 4);
   if (!instructions)
   {
      fprintf(stderr, "memalign failed\n");
      abort();
   }

   if (mprotect(instructions, instruction_count * 4, PROT_READ | PROT_WRITE | PROT_EXEC))
   {
      fprintf(stderr, "mprotect failed\n");
      abort();
   }
#endif
}

code_pool::~code_pool()
{
#ifdef _3DS
   //ReprotectMemory((unsigned int*)instructions, (instruction_count * 4) / 4096, 3);
#elif defined(VITA)
   sceKernelFreeMemBlock(block);
#else
   mprotect(instructions, instruction_count * 4, PROT_READ | PROT_WRITE);
   free(instructions);
#endif
}

void* code_pool::fn_pointer()
{
   void* result = &instructions[flush_start];

   __clear_cache(&instructions[flush_start], &instructions[next_instruction]);
   flush_start = next_instruction;

   return result;
}

void code_pool::set_label(const char* name)
{
   for (int i = 0; i < TARGET_COUNT; i ++)
   {
      if (labels[i].name == name)
      {
         fprintf(stderr, "Duplicate label\n");
         abort();
      }
   }

   for (int i = 0; i < TARGET_COUNT; i ++)
   {
      if (labels[i].name == 0)
      {
         labels[i].name = name;
         labels[i].position = next_instruction;
         return;
      }
   }

   fprintf(stderr, "Label overflow\n");
   abort();
}

void code_pool::resolve_label(const char* name)
{
#ifdef VITA
   RW_INIT();
#endif
   for (int i = 0; i < TARGET_COUNT; i ++)
   {
      if (labels[i].name != name)
      {
         continue;
      }

      for (int j = 0; j < TARGET_COUNT; j ++)
      {
         if (branches[j].name != name)
         {
            continue;
         }

         const uint32_t source = branches[j].position;
         const uint32_t target = labels[i].position;
         instructions[source] |= ((target - source) - 2) & 0xFFFFFF;

         branches[j].name = 0;
      }

      labels[i].name = 0;
      break;
   }
#ifdef VITA
   RW_END();
#endif
}

// Code Gen: Generic
void code_pool::insert_instruction(uint32_t op, AG_COND cond)
{
   assert(cond < CONDINVALID);
   insert_raw_instruction((op & 0x0FFFFFFF) | (cond << 28));
}

void code_pool::insert_raw_instruction(uint32_t op)
{
   if (next_instruction >= instruction_count)
   {
      fprintf(stderr, "code_pool overflow\n");
      abort();
   }
#ifdef VITA
   RW_INIT();
#endif
   instructions[next_instruction ++] = op;
#ifdef VITA
   RW_END();
#endif
}

void code_pool::alu_op(AG_ALU_OP op, reg_t rd, reg_t rn,
                       const alu2& arg, AG_COND cond)
{
   assert(op < OPINVALID);
   insert_instruction( (op << 20) | (rn << 16) | (rd << 12) | arg.encoding, cond );
}

void code_pool::mem_op(AG_MEM_OP op, reg_t rd, reg_t rn, const mem2& arg,
                       AG_MEM_FLAGS flags, AG_COND cond)
{
   uint32_t instruction = 0x04000000;
   instruction |= (op & 1) ? 1 << 20 : 0;
   instruction |= (op & 2) ? 1 << 22 : 0;

   instruction |= arg.encoding;
   instruction |= rd << 12;
   instruction |= rn << 16;

   instruction |= flags ^ 0x1800000;

   insert_instruction( instruction, cond );
}

void code_pool::b(const char* target, AG_COND cond)
{
   assert(target);

   for (int i = 0; i < TARGET_COUNT; i ++)
   {
      if (branches[i].name == 0)
      {
         branches[i].name = target;
         branches[i].position = next_instruction;
         insert_instruction( 0x0A000000, cond );
         return;
      }
   }

   assert(false);
}

void code_pool::load_constant(reg_t target_reg, uint32_t constant, AG_COND cond)
{
   // TODO: Support another method for ARM procs that don't have movw|movt

   uint32_t instructions[2] = { 0x03000000, 0x03400000 };

   for (int i = 0; i < 2; i ++, constant >>= 16)
   {
      // If the upper 16-bits are zero the movt op is not needed
      if (i == 1 && constant == 0)
         break;

      instructions[i] |= target_reg << 12;
      instructions[i] |= constant & 0xFFF;
      instructions[i] |= (constant & 0xF000) << 4;
      insert_instruction( instructions[i], cond );
   }
}

} // namespace arm_gen
