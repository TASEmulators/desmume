// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

// [Guard]
#ifndef _ASMJIT_X86_X86ASSEMBLER_H
#define _ASMJIT_X86_X86ASSEMBLER_H

// [Dependencies - AsmJit]
#include "../core/assembler.h"

#include "../x86/x86defs.h"
#include "../x86/x86operand.h"
#include "../x86/x86util.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

//! @addtogroup AsmJit_X86
//! @{

// ============================================================================
// [AsmJit::X86Assembler]
// ============================================================================

//! @brief X86Assembler - low level x86/x64 code generation.
//!
//! @ref X86Assembler is the main class in AsmJit for generating low level
//! x86/x64 binary stream. It creates internal buffer where opcodes are stored
//! and contains methods that mimics x86/x64 assembler instructions. Code 
//! generation should be safe, because basic type-checks are done by the C++
//! compiler. It's nearly impossible to create invalid instruction (for example
//! <code>mov [eax], [eax]</code> that will not be detected at compile time by
//! C++ compiler.
//!
//! Each call to assembler intrinsics directly emits instruction to internal
//! binary stream. Instruction emitting also contains runtime checks so it
//! should be impossible to create an instruction that is not valid (except
//! there is bug in AsmJit).
//!
//! @ref X86Assembler contains internal buffer where all emitted instructions
//! are stored. Look at @ref Buffer for an implementation. To generate and
//! allocate memory for function use @ref X86Assembler::make() method that will
//! allocate memory using the provided memory manager (see @ref MemoryManager)
//! and relocates the output code to the provided address. If you want to create
//! your function manually, you should look at @ref VirtualMemory interface and
//! use @ref X86Assembler::relocCode() method to relocate emitted code into
//! provided memory location. You can also take the emitted buffer by @ref
//! X86Assembler::take() to do something else with it. If you take buffer, you
//! must free it manually by using @ref ASMJIT_FREE() macro.
//!
//! @section AsmJit_Assembler_CodeGeneration Code Generation
//!
//! To generate code is only needed to create instance of @c AsmJit::Assembler
//! and to use intrinsics. See example how to do that:
//!
//! @code
//! // Use AsmJit namespace.
//! using namespace AsmJit;
//!
//! // Create Assembler instance.
//! Assembler a;
//!
//! // Prolog.
//! a.push(ebp);
//! a.mov(ebp, esp);
//!
//! // Mov 1024 to EAX, EAX is also return value.
//! a.mov(eax, imm(1024));
//!
//! // Epilog.
//! a.mov(esp, ebp);
//! a.pop(ebp);
//!
//! // Return.
//! a.ret();
//! @endcode
//!
//! You can see that syntax is very close to Intel one. Only difference is that
//! you are calling functions that emits the binary code for you. All registers
//! are in @c AsmJit namespace, so it's very comfortable to use it (look at
//! first line). There is also used method @c AsmJit::imm() to create an
//! immediate value. Use @c AsmJit::uimm() to create unsigned immediate value.
//!
//! There is also possibility to use memory addresses and immediates. To build
//! memory address use @c ptr(), @c byte_ptr(), @c word_ptr(), @c dword_ptr()
//! or other friend methods. In most cases you needs only @c ptr() method, but
//! there are instructions where you must specify address size,
//!
//! for example (a is @c AsmJit::Assembler instance):
//!
//! @code
//! a.mov(ptr(eax), imm(0));                   // mov ptr [eax], 0
//! a.mov(ptr(eax), edx);                      // mov ptr [eax], edx
//! @endcode
//!
//! But it's also possible to create complex addresses:
//!
//! @code
//! // eax + ecx*x addresses
//! a.mov(ptr(eax, ecx, kScaleNone), imm(0));     // mov ptr [eax + ecx], 0
//! a.mov(ptr(eax, ecx, kScale2Times), imm(0));     // mov ptr [eax + ecx * 2], 0
//! a.mov(ptr(eax, ecx, kScale4Times), imm(0));     // mov ptr [eax + ecx * 4], 0
//! a.mov(ptr(eax, ecx, kScale8Times), imm(0));     // mov ptr [eax + ecx * 8], 0
//! // eax + ecx*x + disp addresses
//! a.mov(ptr(eax, ecx, kScaleNone,  4), imm(0)); // mov ptr [eax + ecx     +  4], 0
//! a.mov(ptr(eax, ecx, kScale2Times,  8), imm(0)); // mov ptr [eax + ecx * 2 +  8], 0
//! a.mov(ptr(eax, ecx, kScale4Times, 12), imm(0)); // mov ptr [eax + ecx * 4 + 12], 0
//! a.mov(ptr(eax, ecx, kScale8Times, 16), imm(0)); // mov ptr [eax + ecx * 8 + 16], 0
//! @endcode
//!
//! All addresses shown are using @c AsmJit::ptr() to make memory operand.
//! Some assembler instructions (single operand ones) needs to specify memory
//! operand size. For example calling <code>a.inc(ptr(eax))</code> can't be
//! used. @c AsmJit::Assembler::inc(), @c AsmJit::Assembler::dec() and similar
//! instructions can't be serialized without specifying how bytes they are
//! operating on. See next code how assembler works:
//!
//! @code
//! // [byte] address
//! a.inc(byte_ptr(eax));                      // inc byte ptr [eax]
//! a.dec(byte_ptr(eax));                      // dec byte ptr [eax]
//! // [word] address
//! a.inc(word_ptr(eax));                      // inc word ptr [eax]
//! a.dec(word_ptr(eax));                      // dec word ptr [eax]
//! // [dword] address
//! a.inc(dword_ptr(eax));                     // inc dword ptr [eax]
//! a.dec(dword_ptr(eax));                     // dec dword ptr [eax]
//! @endcode
//!
//! @section AsmJit_Assembler_CallingJitCode Calling JIT Code
//!
//! While you are over from emitting instructions, you can make your function
//! using @c AsmJit::Assembler::make() method. This method will use memory
//! manager to allocate virtual memory and relocates generated code to it. For
//! memory allocation is used global memory manager by default and memory is
//! freeable, but of course this default behavior can be overridden specifying
//! your memory manager and allocation type. If you want to do with code
//! something else you can always override make() method and do what you want.
//!
//! You can get size of generated code by @c getCodeSize() or @c getOffset()
//! methods. These methods returns you code size (or more precisely current code 
//! offset) in bytes. Use takeCode() to take internal buffer (all pointers in
//! @c AsmJit::Assembler instance will be zeroed and current buffer returned)
//! to use it. If you don't take it,  @c AsmJit::Assembler destructor will
//! free it automatically. To alloc and run code manually don't use
//! @c malloc()'ed memory, but instead use @c AsmJit::VirtualMemory::alloc()
//! to get memory for executing (specify @c canExecute to @c true) or
//! @c AsmJit::MemoryManager that provides more effective and comfortable way
//! to allocate virtual memory.
//!
//! See next example how to allocate memory where you can execute code created
//! by @c AsmJit::Assembler:
//!
//! @code
//! using namespace AsmJit;
//!
//! Assembler a;
//!
//! // ... your code generation
//!
//! // your function prototype
//! typedef void (*MyFn)();
//!
//! // make your function
//! MyFn fn = asmjit_cast<MyFn>(a.make());
//!
//! // call your function
//! fn();
//!
//! // If you don't need your function again, free it.
//! MemoryManager::getGlobal()->free(fn);
//! @endcode
//!
//! There is also low level alternative how to allocate virtual memory and
//! relocate code to it:
//!
//! @code
//! using namespace AsmJit;
//!
//! Assembler a;
//! // Your code generation ...
//!
//! // Your function prototype.
//! typedef void (*MyFn)();
//!
//! // Alloc memory for your function.
//! MyFn fn = asmjit_cast<MyFn>(
//!   MemoryManager::getGlobal()->alloc(a.getCodeSize());
//!
//! // Relocate the code (will make the function).
//! a.relocCode(fn);
//!
//! // Call the generated function.
//! fn();
//!
//! // If you don't need your function anymore, it should be freed.
//! MemoryManager::getGlobal()->free(fn);
//! @endcode
//!
//! @c note This was very primitive example how to call generated code.
//! In real production code you will never alloc and free code for one run,
//! you will usually use generated code many times.
//!
//! @section AsmJit_Assembler_Labels Labels
//!
//! While generating assembler code, you will usually need to create complex
//! code with labels. Labels are fully supported and you can call @c jmp or
//! @c je (and similar) instructions to initialized or yet uninitialized label.
//! Each label expects to be bound into offset. To bind label to specific
//! offset, use @c bind() method.
//!
//! See next example that contains complete code that creates simple memory
//! copy function (in DWORD entities).
//!
//! @code
//! // Example: Usage of Label (32-bit code).
//! //
//! // Create simple DWORD memory copy function:
//! // ASMJIT_STDCALL void copy32(uint32_t* dst, const uint32_t* src, size_t count);
//! using namespace AsmJit;
//!
//! // Assembler instance.
//! Assembler a;
//!
//! // Constants.
//! const int arg_offset = 8; // Arguments offset (STDCALL EBP).
//! const int arg_size = 12;  // Arguments size.
//!
//! // Labels.
//! Label L_Loop = a.newLabel();
//!
//! // Prolog.
//! a.push(ebp);
//! a.mov(ebp, esp);
//! a.push(esi);
//! a.push(edi);
//!
//! // Fetch arguments
//! a.mov(esi, dword_ptr(ebp, arg_offset + 0)); // Get dst.
//! a.mov(edi, dword_ptr(ebp, arg_offset + 4)); // Get src.
//! a.mov(ecx, dword_ptr(ebp, arg_offset + 8)); // Get count.
//!
//! // Bind L_Loop label to here.
//! a.bind(L_Loop);
//!
//! Copy 4 bytes.
//! a.mov(eax, dword_ptr(esi));
//! a.mov(dword_ptr(edi), eax);
//!
//! // Increment pointers.
//! a.add(esi, 4);
//! a.add(edi, 4);
//!
//! // Repeat loop until (--ecx != 0).
//! a.dec(ecx);
//! a.jz(L_Loop);
//!
//! // Epilog.
//! a.pop(edi);
//! a.pop(esi);
//! a.mov(esp, ebp);
//! a.pop(ebp);
//!
//! // Return: STDCALL convention is to pop stack in called function.
//! a.ret(arg_size);
//! @endcode
//!
//! If you need more abstraction for generating assembler code and you want
//! to hide calling conventions between 32-bit and 64-bit operating systems,
//! look at @c Compiler class that is designed for higher level code
//! generation.
//!
//! @section AsmJit_Assembler_AdvancedCodeGeneration Advanced Code Generation
//!
//! This section describes some advanced generation features of @c Assembler
//! class which can be simply overlooked. The first thing that is very likely
//! needed is generic register support. In previous example the named registers
//! were used. AsmJit contains functions which can convert register index into
//! operand and back.
//!
//! Let's define function which can be used to generate some abstract code:
//!
//! @code
//! // Simple function that generates dword copy.
//! void genCopyDWord(
//!   Assembler& a,
//!   const GpReg& dst, const GpReg& src, const GpReg& tmp)
//! {
//!   a.mov(tmp, dword_ptr(src));
//!   a.mov(dword_ptr(dst), tmp);
//! }
//! @endcode
//!
//! This function can be called like <code>genCopyDWord(a, edi, esi, ebx)</code>
//! or by using existing @ref GpReg instances. This abstraction allows to join
//! more code sections together without rewriting each to use specific registers.
//! You need to take care only about implicit registers which may be used by 
//! several instructions (like mul, imul, div, idiv, shifting, etc...).
//!
//! Next, more advanced, but often needed technique is that you can build your
//! own registers allocator. X86 architecture contains 8 general purpose registers,
//! 8 MMX (MM) registers and 8 SSE (XMM) registers. The X64 (AMD64) architecture
//! extends count of general purpose registers and SSE2 registers to 16. Use the
//! @c kX86RegNumBase constant to get count of GP or XMM registers or @c kX86RegNumGp,
//! @c kX86RegNumMm and @c kX86RegNumXmm constants individually.
//!
//! To build register from index (value from 0 inclusive to kRegNumXXX
//! exclusive) use @ref gpd(), @ref gpq() or @ref gpz() functions. To create
//! a 8 or 16-bit register use @ref gpw(), @ref gpb_lo() or @ref gpb_hi(). 
//! To create other registers there are similar methods like @ref mm(), @ref xmm()
//! and @ref st().
//!
//! So our function call to genCopyDWord can be also used like this:
//!
//! @code
//! genCopyDWord(a, gpd(kX86RegIndexEdi), gpd(kX86RegIndexEsi), gpd(kX86RegIndexEbx));
//! @endcode
//!
//! kX86RegIndexXXX are constants defined by @ref kX86RegIndex enum. You can use your
//! own register allocator (or register slot manager) to alloc / free registers
//! so kX86RegIndexXXX values can be replaced by your variables (0 to kRegNumXXX-1).
//!
//! @sa @ref X86Compiler.
struct X86Assembler : public Assembler
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  ASMJIT_API X86Assembler(Context* context = JitContext::getGlobal());
  ASMJIT_API virtual ~X86Assembler();

  // --------------------------------------------------------------------------
  // [Buffer - Setters (X86-Extensions)]
  // --------------------------------------------------------------------------

  //! @brief Set custom variable @a i at position @a pos.
  //!
  //! @note This function is used to patch existing code.
  ASMJIT_API void setVarAt(size_t pos, sysint_t i, uint8_t isUnsigned, uint32_t size);

  // --------------------------------------------------------------------------
  // [Emit]
  //
  // These functions are not protected against buffer overrun. Each place of
  // code which calls these functions ensures that there is some space using
  // canEmit() method. Emitters are internally protected in AsmJit::Buffer,
  // but only in debug builds.
  // --------------------------------------------------------------------------

  //! @brief Emit single @a opCode without operands.
  inline void _emitOpCode(uint32_t opCode)
  {
    // Instruction prefix.
    if (opCode & 0xFF000000) _emitByte(static_cast<uint8_t>((opCode >> 24) & 0xFF));

    // Instruction opcodes.
    if (opCode & 0x00FF0000) _emitByte(static_cast<uint8_t>((opCode >> 16) & 0xFF));
    if (opCode & 0x0000FF00) _emitByte(static_cast<uint8_t>((opCode >>  8) & 0xFF));

    // Last opcode is always emitted (can be also 0x00).
    _emitByte(static_cast<uint8_t>(opCode & 0xFF));
  }

  //! @brief Emit MODR/M byte.
  inline void _emitMod(uint8_t m, uint8_t o, uint8_t r)
  { _emitByte(((m & 0x03) << 6) | ((o & 0x07) << 3) | (r & 0x07)); }

  //! @brief Emit SIB byte.
  inline void _emitSib(uint8_t s, uint8_t i, uint8_t b)
  { _emitByte(((s & 0x03) << 6) | ((i & 0x07) << 3) | (b & 0x07)); }

  //! @brief Emit REX prefix (64-bit mode only).
  inline void _emitRexR(uint8_t w, uint8_t opReg, uint8_t regCode, bool forceRexPrefix)
  {
#if defined(ASMJIT_X64)
    uint32_t rex;

    // w - Default operand size(0=Default, 1=64-bit).
    // r - Register field (1=high bit extension of the ModR/M REG field).
    // x - Index field not used in RexR
    // b - Base field (1=high bit extension of the ModR/M or SIB Base field).
    rex  = (static_cast<uint32_t>(forceRexPrefix != 0)) << 6; // Rex prefix code.
    rex += (static_cast<uint32_t>(w      )            ) << 3; // Rex.W (w << 3).  
    rex += (static_cast<uint32_t>(opReg  ) & 0x08     ) >> 1; // Rex.R (r << 2).
    rex += (static_cast<uint32_t>(regCode) & 0x08     ) >> 3; // Rex.B (b << 0).

    if (rex) _emitByte(static_cast<uint8_t>(rex | 0x40));
#else
    ASMJIT_UNUSED(w);
    ASMJIT_UNUSED(opReg);
    ASMJIT_UNUSED(regCode);
    ASMJIT_UNUSED(forceRexPrefix);
#endif // ASMJIT_X64
  }

  //! @brief Emit REX prefix (64-bit mode only).
  inline void _emitRexRM(uint8_t w, uint8_t opReg, const Operand& rm, bool forceRexPrefix)
  {
#if defined(ASMJIT_X64)
    uint32_t rex;
    
    // w - Default operand size(0=Default, 1=64-bit).
    // r - Register field (1=high bit extension of the ModR/M REG field).
    // x - Index field (1=high bit extension of the SIB Index field).
    // b - Base field (1=high bit extension of the ModR/M or SIB Base field).

    rex  = (static_cast<uint32_t>(forceRexPrefix != 0)) << 6; // Rex prefix code.
    rex += (static_cast<uint32_t>(w      )            ) << 3; // Rex.W (w << 3).  
    rex += (static_cast<uint32_t>(opReg  ) & 0x08     ) >> 1; // Rex.R (r << 2).

    uint32_t b = 0;
    uint32_t x = 0;

    if (rm.isReg())
    {
      b = (static_cast<const Reg&>(rm).getRegCode() & 0x08) != 0;
    }
    else if (rm.isMem())
    {
      b = ((static_cast<const Mem&>(rm).getBase()  & 0x8) != 0) & (static_cast<const Mem&>(rm).getBase()  != kInvalidValue);
      x = ((static_cast<const Mem&>(rm).getIndex() & 0x8) != 0) & (static_cast<const Mem&>(rm).getIndex() != kInvalidValue);
    }

    rex += static_cast<uint32_t>(x) << 1; // Rex.R (x << 1).
    rex += static_cast<uint32_t>(b)     ; // Rex.B (b << 0).

    if (rex) _emitByte(static_cast<uint8_t>(rex | 0x40));
#else
    ASMJIT_UNUSED(w);
    ASMJIT_UNUSED(opReg);
    ASMJIT_UNUSED(rm);
#endif // ASMJIT_X64
  }

  //! @brief Emit Register / Register - calls _emitMod(3, opReg, r)
  inline void _emitModR(uint8_t opReg, uint8_t r)
  { _emitMod(3, opReg, r); }

  //! @brief Emit Register / Register - calls _emitMod(3, opReg, r.code())
  inline void _emitModR(uint8_t opReg, const Reg& r)
  { _emitMod(3, opReg, r.getRegCode()); }

  //! @brief Emit register / memory address combination to buffer.
  //!
  //! This method can hangle addresses from simple to complex ones with
  //! index and displacement.
  ASMJIT_API void _emitModM(uint8_t opReg, const Mem& mem, sysint_t immSize);

  //! @brief Emit Reg<-Reg or Reg<-Reg|Mem ModRM (can be followed by SIB 
  //! and displacement) to buffer.
  //!
  //! This function internally calls @c _emitModM() or _emitModR() that depends
  //! to @a op type.
  //!
  //! @note @a opReg is usually real register ID (see @c R) but some instructions
  //! have specific format and in that cases @a opReg is part of opcode.
  ASMJIT_API void _emitModRM(uint8_t opReg, const Operand& op, sysint_t immSize);

  //! @brief Emit CS (code segmend) prefix.
  //!
  //! Behavior of this function is to emit code prefix only if memory operand
  //! address uses code segment. Code segment is used through memory operand
  //! with attached @c AsmJit::Label.
  ASMJIT_API void _emitSegmentPrefix(const Operand& rm);

  //! @brief Emit instruction where register is inlined to opcode.
  ASMJIT_API void _emitX86Inl(uint32_t opCode, uint8_t i16bit, uint8_t rexw, uint8_t reg, bool forceRexPrefix);

  //! @brief Emit instruction with reg/memory operand.
  ASMJIT_API void _emitX86RM(uint32_t opCode, uint8_t i16bit, uint8_t rexw, uint8_t o,
    const Operand& op, sysint_t immSize, bool forceRexPrefix);

  //! @brief Emit FPU instruction with no operands.
  ASMJIT_API void _emitFpu(uint32_t opCode);

  //! @brief Emit FPU instruction with one operand @a sti (index of FPU register).
  ASMJIT_API void _emitFpuSTI(uint32_t opCode, uint32_t sti);

  //! @brief Emit FPU instruction with one operand @a opReg and memory operand @a mem.
  ASMJIT_API void _emitFpuMEM(uint32_t opCode, uint8_t opReg, const Mem& mem);

  //! @brief Emit MMX/SSE instruction.
  ASMJIT_API void _emitMmu(uint32_t opCode, uint8_t rexw, uint8_t opReg, const Operand& src, sysint_t immSize);

  //! @brief Emit displacement.
  ASMJIT_API LabelLink* _emitDisplacement(LabelData& l_data, sysint_t inlinedDisplacement, int size);

  //! @brief Emit relative relocation to absolute pointer @a target. It's needed
  //! to add what instruction is emitting this, because in x64 mode the relative
  //! displacement can be impossible to calculate and in this case the trampoline
  //! is used.
  ASMJIT_API void _emitJmpOrCallReloc(uint32_t instruction, void* target);

  // Helpers to decrease binary code size. These four emit methods are just
  // helpers thats used by assembler. They call emitX86() adding NULLs
  // to first, second and third operand, if needed.

  //! @brief Emit X86/FPU or MM/XMM instruction.
  ASMJIT_API void _emitInstruction(uint32_t code);

  //! @brief Emit X86/FPU or MM/XMM instruction.
  ASMJIT_API void _emitInstruction(uint32_t code, const Operand* o0);

  //! @brief Emit X86/FPU or MM/XMM instruction.
  ASMJIT_API void _emitInstruction(uint32_t code, const Operand* o0, const Operand* o1);

  //! @brief Emit X86/FPU or MM/XMM instruction.
  //!
  //! Operands @a o1, @a o2 or @a o3 can be @c NULL if they are not used.
  //!
  //! Hint: Use @c emitX86() helpers to emit instructions.
  ASMJIT_API void _emitInstruction(uint32_t code, const Operand* o0, const Operand* o1, const Operand* o2);

  //! @brief Private method for emitting jcc.
  ASMJIT_API void _emitJcc(uint32_t code, const Label* label, uint32_t hint);

  //! @brief Private method for emitting short jcc.
  inline void _emitShortJcc(uint32_t code, const Label* label, uint32_t hint)
  {
    _emitOptions |= kX86EmitOptionShortJump;
    _emitJcc(code, label, hint);
  }

  // --------------------------------------------------------------------------
  // [EmbedLabel]
  // --------------------------------------------------------------------------

  //! @brief Embed absolute label pointer (4 or 8 bytes).
  ASMJIT_API void embedLabel(const Label& label);

  // --------------------------------------------------------------------------
  // [Align]
  // --------------------------------------------------------------------------

  //! @brief Align target buffer to @a m bytes.
  //!
  //! Typical usage of this is to align labels at start of the inner loops.
  //!
  //! Inserts @c nop() instructions or CPU optimized NOPs.
  ASMJIT_API void align(uint32_t m);

  // --------------------------------------------------------------------------
  // [Label]
  // --------------------------------------------------------------------------

  //! @brief Create and return new label.
  ASMJIT_API Label newLabel();

  //! @brief Register labels (used by @c Compiler).
  ASMJIT_API void registerLabels(size_t count);

  //! @brief Bind label to the current offset.
  //!
  //! @note Label can be bound only once!
  ASMJIT_API void bind(const Label& label);

  // --------------------------------------------------------------------------
  // [Reloc]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual size_t relocCode(void* dst, sysuint_t addressBase) const;

  // --------------------------------------------------------------------------
  // [Make]
  // --------------------------------------------------------------------------

  ASMJIT_API virtual void* make();

  // --------------------------------------------------------------------------
  // [Embed]
  // --------------------------------------------------------------------------

  //! @brief Add 8-bit integer data to the instuction stream.
  inline void db(uint8_t  x) { embed(&x, 1); }
  //! @brief Add 16-bit integer data to the instuction stream.
  inline void dw(uint16_t x) { embed(&x, 2); }
  //! @brief Add 32-bit integer data to the instuction stream.
  inline void dd(uint32_t x) { embed(&x, 4); }
  //! @brief Add 64-bit integer data to the instuction stream.
  inline void dq(uint64_t x) { embed(&x, 8); }

  //! @brief Add 8-bit integer data to the instuction stream.
  inline void dint8(int8_t x) { embed(&x, sizeof(int8_t)); }
  //! @brief Add 8-bit integer data to the instuction stream.
  inline void duint8(uint8_t x) { embed(&x, sizeof(uint8_t)); }

  //! @brief Add 16-bit integer data to the instuction stream.
  inline void dint16(int16_t x) { embed(&x, sizeof(int16_t)); }
  //! @brief Add 16-bit integer data to the instuction stream.
  inline void duint16(uint16_t x) { embed(&x, sizeof(uint16_t)); }

  //! @brief Add 32-bit integer data to the instuction stream.
  inline void dint32(int32_t x) { embed(&x, sizeof(int32_t)); }
  //! @brief Add 32-bit integer data to the instuction stream.
  inline void duint32(uint32_t x) { embed(&x, sizeof(uint32_t)); }

  //! @brief Add 64-bit integer data to the instuction stream.
  inline void dint64(int64_t x) { embed(&x, sizeof(int64_t)); }
  //! @brief Add 64-bit integer data to the instuction stream.
  inline void duint64(uint64_t x) { embed(&x, sizeof(uint64_t)); }

  //! @brief Add system-integer data to the instuction stream.
  inline void dintptr(intptr_t x) { embed(&x, sizeof(intptr_t)); }
  //! @brief Add system-integer data to the instuction stream.
  inline void duintptr(uintptr_t x) { embed(&x, sizeof(uintptr_t)); }

  //! @brief Add float data to the instuction stream.
  inline void dfloat(float x) { embed(&x, sizeof(float)); }
  //! @brief Add double data to the instuction stream.
  inline void ddouble(double x) { embed(&x, sizeof(double)); }

  //! @brief Add pointer data to the instuction stream.
  inline void dptr(void* x) { embed(&x, sizeof(void*)); }

  //! @brief Add MM data to the instuction stream.
  inline void dmm(const MmData& x) { embed(&x, sizeof(MmData)); }
  //! @brief Add XMM data to the instuction stream.
  inline void dxmm(const XmmData& x) { embed(&x, sizeof(XmmData)); }

  //! @brief Add data to the instuction stream.
  inline void data(const void* data, size_t size) { embed(data, size); }

  //! @brief Add data in a given structure instance to the instuction stream.
  template<typename T>
  inline void dstruct(const T& x) { embed(&x, sizeof(T)); }

  // --------------------------------------------------------------------------
  // [X86 Instructions]
  // --------------------------------------------------------------------------

  //! @brief Add with Carry.
  inline void adc(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstAdc, &dst, &src); }
  //! @brief Add with Carry.
  inline void adc(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstAdc, &dst, &src); }
  //! @brief Add with Carry.
  inline void adc(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstAdc, &dst, &src); }
  //! @brief Add with Carry.
  inline void adc(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstAdc, &dst, &src); }
  //! @brief Add with Carry.
  inline void adc(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstAdc, &dst, &src); }

  //! @brief Add.
  inline void add(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstAdd, &dst, &src); }
  //! @brief Add.
  inline void add(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstAdd, &dst, &src); }
  //! @brief Add.
  inline void add(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstAdd, &dst, &src); }
  //! @brief Add.
  inline void add(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstAdd, &dst, &src); }
  //! @brief Add.
  inline void add(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstAdd, &dst, &src); }

  //! @brief Logical And.
  inline void and_(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstAnd, &dst, &src); }
  //! @brief Logical And.
  inline void and_(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstAnd, &dst, &src); }
  //! @brief Logical And.
  inline void and_(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstAnd, &dst, &src); }
  //! @brief Logical And.
  inline void and_(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstAnd, &dst, &src); }
  //! @brief Logical And.
  inline void and_(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstAnd, &dst, &src); }

  //! @brief Bit Scan Forward.
  inline void bsf(const GpReg& dst, const GpReg& src)
  {
    ASMJIT_ASSERT(!dst.isGpb());
    _emitInstruction(kX86InstBsf, &dst, &src);
  }
  //! @brief Bit Scan Forward.
  inline void bsf(const GpReg& dst, const Mem& src)
  {
    ASMJIT_ASSERT(!dst.isGpb());
    _emitInstruction(kX86InstBsf, &dst, &src);
  }

  //! @brief Bit Scan Reverse.
  inline void bsr(const GpReg& dst, const GpReg& src)
  {
    ASMJIT_ASSERT(!dst.isGpb());
    _emitInstruction(kX86InstBsr, &dst, &src);
  }
  //! @brief Bit Scan Reverse.
  inline void bsr(const GpReg& dst, const Mem& src)
  {
    ASMJIT_ASSERT(!dst.isGpb());
    _emitInstruction(kX86InstBsr, &dst, &src);
  }

  //! @brief Byte swap (32-bit or 64-bit registers only) (i486).
  inline void bswap(const GpReg& dst)
  {
    ASMJIT_ASSERT(dst.getRegType() == kX86RegTypeGpd || dst.getRegType() == kX86RegTypeGpq);
    _emitInstruction(kX86InstBSwap, &dst);
  }

  //! @brief Bit test.
  inline void bt(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstBt, &dst, &src); }
  //! @brief Bit test.
  inline void bt(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstBt, &dst, &src); }
  //! @brief Bit test.
  inline void bt(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstBt, &dst, &src); }
  //! @brief Bit test.
  inline void bt(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstBt, &dst, &src); }

  //! @brief Bit test and complement.
  inline void btc(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstBtc, &dst, &src); }
  //! @brief Bit test and complement.
  inline void btc(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstBtc, &dst, &src); }
  //! @brief Bit test and complement.
  inline void btc(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstBtc, &dst, &src); }
  //! @brief Bit test and complement.
  inline void btc(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstBtc, &dst, &src); }

  //! @brief Bit test and reset.
  inline void btr(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstBtr, &dst, &src); }
  //! @brief Bit test and reset.
  inline void btr(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstBtr, &dst, &src); }
  //! @brief Bit test and reset.
  inline void btr(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstBtr, &dst, &src); }
  //! @brief Bit test and reset.
  inline void btr(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstBtr, &dst, &src); }

  //! @brief Bit test and set.
  inline void bts(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstBts, &dst, &src); }
  //! @brief Bit test and set.
  inline void bts(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstBts, &dst, &src); }
  //! @brief Bit test and set.
  inline void bts(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstBts, &dst, &src); }
  //! @brief Bit test and set.
  inline void bts(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstBts, &dst, &src); }

  //! @brief Call Procedure.
  inline void call(const GpReg& dst)
  {
    ASMJIT_ASSERT(dst.isRegType(kX86RegTypeGpz));
    _emitInstruction(kX86InstCall, &dst);
  }
  //! @brief Call Procedure.
  inline void call(const Mem& dst)
  { _emitInstruction(kX86InstCall, &dst); }
  //! @brief Call Procedure.
  inline void call(const Imm& dst)
  { _emitInstruction(kX86InstCall, &dst); }
  //! @brief Call Procedure.
  //! @overload
  inline void call(void* dst)
  {
    Imm imm((sysint_t)dst);
    _emitInstruction(kX86InstCall, &imm);
  }

  //! @brief Call Procedure.
  inline void call(const Label& label)
  { _emitInstruction(kX86InstCall, &label); }

  //! @brief Convert Byte to Word (Sign Extend).
  //!
  //! AX <- Sign Extend AL
  inline void cbw()
  { _emitInstruction(kX86InstCbw); }

  //! @brief Convert Word to DWord (Sign Extend).
  //!
  //! DX:AX <- Sign Extend AX
  inline void cwd()
  { _emitInstruction(kX86InstCwd); }

  //! @brief Convert Word to DWord (Sign Extend).
  //!
  //! EAX <- Sign Extend AX
  inline void cwde()
  { _emitInstruction(kX86InstCwde); }

  //! @brief Convert DWord to QWord (Sign Extend).
  //!
  //! EDX:EAX <- Sign Extend EAX
  inline void cdq()
  { _emitInstruction(kX86InstCdq); }

#if defined(ASMJIT_X64)
  //! @brief Convert DWord to QWord (Sign Extend).
  //!
  //! RAX <- Sign Extend EAX
  inline void cdqe()
  { _emitInstruction(kX86InstCdqe); }
#endif // ASMJIT_X64

  //! @brief Clear Carry flag
  //!
  //! This instruction clears the CF flag in the EFLAGS register.
  inline void clc()
  { _emitInstruction(kX86InstClc); }

  //! @brief Clear Direction flag
  //!
  //! This instruction clears the DF flag in the EFLAGS register.
  inline void cld()
  { _emitInstruction(kX86InstCld); }

  //! @brief Complement Carry Flag.
  //!
  //! This instruction complements the CF flag in the EFLAGS register.
  //! (CF = NOT CF)
  inline void cmc()
  { _emitInstruction(kX86InstCmc); }

  //! @brief Conditional Move.
  inline void cmov(kX86Cond cc, const GpReg& dst, const GpReg& src)
  { _emitInstruction(X86Util::getCMovccInstFromCond(cc), &dst, &src); }

  //! @brief Conditional Move.
  inline void cmov(kX86Cond cc, const GpReg& dst, const Mem& src)
  { _emitInstruction(X86Util::getCMovccInstFromCond(cc), &dst, &src); }

  //! @brief Conditional Move.
  inline void cmova  (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovA  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmova  (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovA  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovae (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovAE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovae (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovAE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovb  (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovB  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovb  (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovB  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovbe (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovBE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovbe (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovBE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovc  (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovC  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovc  (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovC  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmove  (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovE  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmove  (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovE  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovg  (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovG  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovg  (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovG  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovge (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovGE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovge (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovGE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovl  (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovL  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovl  (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovL  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovle (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovLE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovle (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovLE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovna (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovNA , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovna (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNA , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnae(const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovNAE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnae(const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNAE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnb (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovNB , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnb (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNB , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnbe(const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovNBE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnbe(const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNBE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnc (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovNC , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnc (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNC , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovne (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovNE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovne (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovng (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovNG , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovng (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNG , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnge(const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovNGE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnge(const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNGE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnl (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovNL , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnl (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNL , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnle(const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovNLE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnle(const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNLE, &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovno (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovNO , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovno (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNO , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnp (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovNP , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnp (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNP , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovns (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovNS , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovns (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNS , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnz (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovNZ , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovnz (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovNZ , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovo  (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovO  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovo  (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovO  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovp  (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovP  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovp  (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovP  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovpe (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovPE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovpe (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovPE , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovpo (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovPO , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovpo (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovPO , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovs  (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovS  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovs  (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovS  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovz  (const GpReg& dst, const GpReg& src) { _emitInstruction(kX86InstCMovZ  , &dst, &src); }
  //! @brief Conditional Move.
  inline void cmovz  (const GpReg& dst, const Mem& src)   { _emitInstruction(kX86InstCMovZ  , &dst, &src); }

  //! @brief Compare Two Operands.
  inline void cmp(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstCmp, &dst, &src); }
  //! @brief Compare Two Operands.
  inline void cmp(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCmp, &dst, &src); }
  //! @brief Compare Two Operands.
  inline void cmp(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstCmp, &dst, &src); }
  //! @brief Compare Two Operands.
  inline void cmp(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstCmp, &dst, &src); }
  //! @brief Compare Two Operands.
  inline void cmp(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstCmp, &dst, &src); }

  //! @brief Compare and Exchange (i486).
  inline void cmpxchg(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstCmpXCHG, &dst, &src); }
  //! @brief Compare and Exchange (i486).
  inline void cmpxchg(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstCmpXCHG, &dst, &src); }

  //! @brief Compares the 64-bit value in EDX:EAX with the memory operand (Pentium).
  //!
  //! If the values are equal, then this instruction stores the 64-bit value
  //! in ECX:EBX into the memory operand and sets the zero flag. Otherwise,
  //! this instruction copies the 64-bit memory operand into the EDX:EAX
  //! registers and clears the zero flag.
  inline void cmpxchg8b(const Mem& dst)
  { _emitInstruction(kX86InstCmpXCHG8B, &dst); }

#if defined(ASMJIT_X64)
  //! @brief Compares the 128-bit value in RDX:RAX with the memory operand (X64).
  //!
  //! If the values are equal, then this instruction stores the 128-bit value
  //! in RCX:RBX into the memory operand and sets the zero flag. Otherwise,
  //! this instruction copies the 128-bit memory operand into the RDX:RAX
  //! registers and clears the zero flag.
  inline void cmpxchg16b(const Mem& dst)
  { _emitInstruction(kX86InstCmpXCHG16B, &dst); }
#endif // ASMJIT_X64

  //! @brief CPU Identification (i486).
  inline void cpuid()
  { _emitInstruction(kX86InstCpuId); }

#if defined(ASMJIT_X64)
  //! @brief Convert QWord to DQWord (Sign Extend).
  //!
  //! RDX:RAX <- Sign Extend RAX
  inline void cqo()
  { _emitInstruction(kX86InstCqo); }
#endif // ASMJIT_X64

#if defined(ASMJIT_X86)
  //! @brief Decimal adjust AL after addition
  //!
  //! This instruction adjusts the sum of two packed BCD values to create
  //! a packed BCD result.
  //!
  //! @note This instruction is only available in 32-bit mode.
  inline void daa()
  { _emitInstruction(kX86InstDaa); }
#endif // ASMJIT_X86

#if defined(ASMJIT_X86)
  //! @brief Decimal adjust AL after subtraction
  //!
  //! This instruction adjusts the result of the subtraction of two packed
  //! BCD values to create a packed BCD result.
  //!
  //! @note This instruction is only available in 32-bit mode.
  inline void das()
  { _emitInstruction(kX86InstDas); }
#endif // ASMJIT_X86

  //! @brief Decrement by 1.
  //! @note This instruction can be slower than sub(dst, 1)
  inline void dec(const GpReg& dst)
  { _emitInstruction(kX86InstDec, &dst); }
  //! @brief Decrement by 1.
  //! @note This instruction can be slower than sub(dst, 1)
  inline void dec(const Mem& dst)
  { _emitInstruction(kX86InstDec, &dst); }

  //! @brief Unsigned divide.
  //!
  //! This instruction divides (unsigned) the value in the AL, AX, or EAX
  //! register by the source operand and stores the result in the AX,
  //! DX:AX, or EDX:EAX registers.
  inline void div(const GpReg& src)
  { _emitInstruction(kX86InstDiv, &src); }
  //! @brief Unsigned divide.
  //! @overload
  inline void div(const Mem& src)
  { _emitInstruction(kX86InstDiv, &src); }

  //! @brief Make Stack Frame for Procedure Parameters.
  inline void enter(const Imm& imm16, const Imm& imm8)
  { _emitInstruction(kX86InstEnter, &imm16, &imm8); }

  //! @brief Signed divide.
  //!
  //! This instruction divides (signed) the value in the AL, AX, or EAX
  //! register by the source operand and stores the result in the AX,
  //! DX:AX, or EDX:EAX registers.
  inline void idiv(const GpReg& src)
  { _emitInstruction(kX86InstIDiv, &src); }
  //! @brief Signed divide.
  //! @overload
  inline void idiv(const Mem& src)
  { _emitInstruction(kX86InstIDiv, &src); }

  //! @brief Signed multiply.
  //!
  //! Source operand (in a general-purpose register or memory location)
  //! is multiplied by the value in the AL, AX, or EAX register (depending
  //! on the operand size) and the product is stored in the AX, DX:AX, or
  //! EDX:EAX registers, respectively.
  inline void imul(const GpReg& src)
  { _emitInstruction(kX86InstIMul, &src); }
  //! @overload
  inline void imul(const Mem& src)
  { _emitInstruction(kX86InstIMul, &src); }

  //! @brief Signed multiply.
  //!
  //! Destination operand (the first operand) is multiplied by the source
  //! operand (second operand). The destination operand is a general-purpose
  //! register and the source operand is an immediate value, a general-purpose
  //! register, or a memory location. The product is then stored in the
  //! destination operand location.
  inline void imul(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstIMul, &dst, &src); }
  //! @brief Signed multiply.
  //! @overload
  inline void imul(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstIMul, &dst, &src); }
  //! @brief Signed multiply.
  //! @overload
  inline void imul(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstIMul, &dst, &src); }

  //! @brief Signed multiply.
  //!
  //! source operand (which can be a general-purpose register or a memory
  //! location) is multiplied by the second source operand (an immediate
  //! value). The product is then stored in the destination operand
  //! (a general-purpose register).
  inline void imul(const GpReg& dst, const GpReg& src, const Imm& imm)
  { _emitInstruction(kX86InstIMul, &dst, &src, &imm); }
  //! @overload
  inline void imul(const GpReg& dst, const Mem& src, const Imm& imm)
  { _emitInstruction(kX86InstIMul, &dst, &src, &imm); }

  //! @brief Increment by 1.
  //! @note This instruction can be slower than add(dst, 1)
  inline void inc(const GpReg& dst)
  { _emitInstruction(kX86InstInc, &dst); }
  //! @brief Increment by 1.
  //! @note This instruction can be slower than add(dst, 1)
  inline void inc(const Mem& dst)
  { _emitInstruction(kX86InstInc, &dst); }

  //! @brief Interrupt 3 - trap to debugger.
  inline void int3()
  { _emitInstruction(kX86InstInt3); }

  //! @brief Jump to label @a label if condition @a cc is met.
  //!
  //! This instruction checks the state of one or more of the status flags in
  //! the EFLAGS register (CF, OF, PF, SF, and ZF) and, if the flags are in the
  //! specified state (condition), performs a jump to the target instruction
  //! specified by the destination operand. A condition code (cc) is associated
  //! with each instruction to indicate the condition being tested for. If the
  //! condition is not satisfied, the jump is not performed and execution
  //! continues with the instruction following the Jcc instruction.
  inline void j(kX86Cond cc, const Label& label, uint32_t hint = kCondHintNone)
  {
    _emitJcc(X86Util::getJccInstFromCond(cc), &label, hint);
  }

  //! @brief Jump to label @a label if condition is met.
  inline void ja  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJA  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jae (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJAE , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jb  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJB  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jbe (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJBE , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jc  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJC  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void je  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJE  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jg  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJG  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jge (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJGE , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jl  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJL  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jle (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJLE , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jna (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNA , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnae(const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNAE, &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnb (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNB , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnbe(const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNBE, &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnc (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNC , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jne (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNE , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jng (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNG , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnge(const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNGE, &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnl (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNL , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnle(const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNLE, &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jno (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNO , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnp (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNP , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jns (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNS , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jnz (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJNZ , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jo  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJO  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jp  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJP  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jpe (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJPE , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jpo (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJPO , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void js  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJS  , &label, hint); }
  //! @brief Jump to label @a label if condition is met.
  inline void jz  (const Label& label, uint32_t hint = kCondHintNone) { _emitJcc(kX86InstJZ  , &label, hint); }

  //! @brief Short jump to label @a label if condition @a cc is met.
  //! @sa j()
  inline void short_j(kX86Cond cc, const Label& label, uint32_t hint = kCondHintNone)
  {
    _emitOptions |= kX86EmitOptionShortJump;
    j(cc, label, hint);
  }

  //! @brief Short jump to label @a label if condition is met.
  inline void short_ja  (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJA  , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jae (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJAE , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jb  (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJB  , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jbe (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJBE , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jc  (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJC  , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_je  (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJE  , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jg  (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJG  , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jge (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJGE , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jl  (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJL  , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jle (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJLE , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jna (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJNA , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jnae(const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJNAE, &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jnb (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJNB , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jnbe(const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJNBE, &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jnc (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJNC , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jne (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJNE , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jng (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJNG , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jnge(const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJNGE, &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jnl (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJNL , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jnle(const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJNLE, &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jno (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJNO , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jnp (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJNP , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jns (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJNS , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jnz (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJNZ , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jo  (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJO  , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jp  (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJP  , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jpe (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJPE , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jpo (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJPO , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_js  (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJS  , &label, hint); }
  //! @brief Short jump to label @a label if condition is met.
  inline void short_jz  (const Label& label, uint32_t hint = kCondHintNone) { _emitShortJcc(kX86InstJZ  , &label, hint); }

  //! @brief Jump.
  //! @overload
  inline void jmp(const GpReg& dst)
  { _emitInstruction(kX86InstJmp, &dst); }
  //! @brief Jump.
  //! @overload
  inline void jmp(const Mem& dst)
  { _emitInstruction(kX86InstJmp, &dst); }
  //! @brief Jump.
  //! @overload
  inline void jmp(const Imm& dst)
  { _emitInstruction(kX86InstJmp, &dst); }

  //! @brief Jump.
  //! @overload
  inline void jmp(void* dst)
  {
    Imm imm((sysint_t)dst);
    _emitInstruction(kX86InstJmp, &imm);
  }

  //! @brief Jump.
  //!
  //! This instruction transfers program control to a different point
  //! in the instruction stream without recording return information.
  //! The destination (target) operand specifies the label of the
  //! instruction being jumped to.
  inline void jmp(const Label& label)
  { _emitInstruction(kX86InstJmp, &label); }

  //! @brief Short jump.
  //! @sa jmp()
  inline void short_jmp(const Label& label)
  {
    _emitOptions |= kX86EmitOptionShortJump;
    _emitInstruction(kX86InstJmp, &label);
  }

  //! @brief Load Effective Address
  //!
  //! This instruction computes the effective address of the second
  //! operand (the source operand) and stores it in the first operand
  //! (destination operand). The source operand is a memory address
  //! (offset part) specified with one of the processors addressing modes.
  //! The destination operand is a general-purpose register.
  inline void lea(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstLea, &dst, &src); }

  //! @brief High Level Procedure Exit.
  inline void leave()
  { _emitInstruction(kX86InstLeave); }

  //! @brief Move.
  //!
  //! This instruction copies the second operand (source operand) to the first
  //! operand (destination operand). The source operand can be an immediate
  //! value, general-purpose register, segment register, or memory location.
  //! The destination register can be a general-purpose register, segment
  //! register, or memory location. Both operands must be the same size, which
  //! can be a byte, a word, or a DWORD.
  //!
  //! @note To move MMX or SSE registers to/from GP registers or memory, use
  //! corresponding functions: @c movd(), @c movq(), etc. Passing MMX or SSE
  //! registers to @c mov() is illegal.
  inline void mov(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }
  //! @brief Move.
  //! @overload
  inline void mov(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }
  //! @brief Move.
  //! @overload
  inline void mov(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }
  //! @brief Move.
  //! @overload
  inline void mov(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }
  //! @brief Move.
  //! @overload
  inline void mov(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }

  //! @brief Move from segment register.
  //! @overload.
  inline void mov(const GpReg& dst, const SegmentReg& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }

  //! @brief Move from segment register.
  //! @overload.
  inline void mov(const Mem& dst, const SegmentReg& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }

  //! @brief Move to segment register.
  //! @overload.
  inline void mov(const SegmentReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }

  //! @brief Move to segment register.
  //! @overload.
  inline void mov(const SegmentReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMov, &dst, &src); }

  //! @brief Move byte, word, dword or qword from absolute address @a src to
  //! AL, AX, EAX or RAX register.
  inline void mov_ptr(const GpReg& dst, void* src)
  {
    ASMJIT_ASSERT(dst.getRegIndex() == 0);
    Imm imm((sysint_t)src);
    _emitInstruction(kX86InstMovPtr, &dst, &imm);
  }

  //! @brief Move byte, word, dword or qword from AL, AX, EAX or RAX register
  //! to absolute address @a dst.
  inline void mov_ptr(void* dst, const GpReg& src)
  {
    ASMJIT_ASSERT(src.getRegIndex() == 0);
    Imm imm((sysint_t)dst);
    _emitInstruction(kX86InstMovPtr, &imm, &src);
  }

  //! @brief Move with Sign-Extension.
  //!
  //! This instruction copies the contents of the source operand (register
  //! or memory location) to the destination operand (register) and sign
  //! extends the value to 16, 32 or 64-bits.
  //!
  //! @sa movsxd().
  void movsx(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstMovSX, &dst, &src); }
  //! @brief Move with Sign-Extension.
  //! @overload
  void movsx(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovSX, &dst, &src); }

#if defined(ASMJIT_X64)
  //! @brief Move DWord to QWord with sign-extension.
  inline void movsxd(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstMovSXD, &dst, &src); }
  //! @brief Move DWord to QWord with sign-extension.
  //! @overload
  inline void movsxd(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovSXD, &dst, &src); }
#endif // ASMJIT_X64

  //! @brief Move with Zero-Extend.
  //!
  //! This instruction copies the contents of the source operand (register
  //! or memory location) to the destination operand (register) and zero
  //! extends the value to 16 or 32-bits. The size of the converted value
  //! depends on the operand-size attribute.
  inline void movzx(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstMovZX, &dst, &src); }
  //! @brief Move with Zero-Extend.
  //! @brief Overload
  inline void movzx(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovZX, &dst, &src); }

  //! @brief Unsigned multiply.
  //!
  //! Source operand (in a general-purpose register or memory location)
  //! is multiplied by the value in the AL, AX, or EAX register (depending
  //! on the operand size) and the product is stored in the AX, DX:AX, or
  //! EDX:EAX registers, respectively.
  inline void mul(const GpReg& src)
  { _emitInstruction(kX86InstMul, &src); }
  //! @brief Unsigned multiply.
  //! @overload
  inline void mul(const Mem& src)
  { _emitInstruction(kX86InstMul, &src); }

  //! @brief Two's Complement Negation.
  inline void neg(const GpReg& dst)
  { _emitInstruction(kX86InstNeg, &dst); }
  //! @brief Two's Complement Negation.
  inline void neg(const Mem& dst)
  { _emitInstruction(kX86InstNeg, &dst); }

  //! @brief No Operation.
  //!
  //! This instruction performs no operation. This instruction is a one-byte
  //! instruction that takes up space in the instruction stream but does not
  //! affect the machine context, except the EIP register. The NOP instruction
  //! is an alias mnemonic for the XCHG (E)AX, (E)AX instruction.
  inline void nop()
  { _emitInstruction(kX86InstNop); }

  //! @brief One's Complement Negation.
  inline void not_(const GpReg& dst)
  { _emitInstruction(kX86InstNot, &dst); }
  //! @brief One's Complement Negation.
  inline void not_(const Mem& dst)
  { _emitInstruction(kX86InstNot, &dst); }

  //! @brief Logical Inclusive OR.
  inline void or_(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstOr, &dst, &src); }
  //! @brief Logical Inclusive OR.
  inline void or_(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstOr, &dst, &src); }
  //! @brief Logical Inclusive OR.
  inline void or_(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstOr, &dst, &src); }
  //! @brief Logical Inclusive OR.
  inline void or_(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstOr, &dst, &src); }
  //! @brief Logical Inclusive OR.
  inline void or_(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstOr, &dst, &src); }

  //! @brief Pop a Value from the Stack.
  //!
  //! This instruction loads the value from the top of the stack to the location
  //! specified with the destination operand and then increments the stack pointer.
  //! The destination operand can be a general purpose register, memory location,
  //! or segment register.
  inline void pop(const GpReg& dst)
  {
    ASMJIT_ASSERT(dst.isRegType(kX86RegTypeGpw) || dst.isRegType(kX86RegTypeGpz));
    _emitInstruction(kX86InstPop, &dst);
  }
  //! @brief Pop a Segment Register from the Stack.
  //!
  //! @note There is no instruction to pop a cs segment register.
  inline void pop(const SegmentReg& dst)
  {
    ASMJIT_ASSERT(dst.getRegIndex() != kX86SegCs);
    _emitInstruction(kX86InstPop, &dst);
  }

  inline void pop(const Mem& dst)
  {
    ASMJIT_ASSERT(dst.getSize() == 2 || dst.getSize() == sizeof(sysint_t));
    _emitInstruction(kX86InstPop, &dst);
  }

#if defined(ASMJIT_X86)
  //! @brief Pop All General-Purpose Registers.
  //!
  //! Pop EDI, ESI, EBP, EBX, EDX, ECX, and EAX.
  inline void popad()
  { _emitInstruction(kX86InstPopAD); }
#endif // ASMJIT_X86

  //! @brief Pop Stack into EFLAGS Register (32-bit or 64-bit).
  inline void popf()
  {
#if defined(ASMJIT_X86)
    popfd();
#else
    popfq();
#endif
  }

#if defined(ASMJIT_X86)
  //! @brief Pop Stack into EFLAGS Register (32-bit).
  inline void popfd() { _emitInstruction(kX86InstPopFD); }
#else
  //! @brief Pop Stack into EFLAGS Register (64-bit).
  inline void popfq() { _emitInstruction(kX86InstPopFQ); }
#endif

  //! @brief Push WORD/DWORD/QWORD Onto the Stack.
  //!
  //! @note 32-bit architecture pushed DWORD while 64-bit
  //! pushes QWORD. 64-bit mode not provides instruction to
  //! push 32-bit register/memory.
  inline void push(const GpReg& src)
  {
    ASMJIT_ASSERT(src.isRegType(kX86RegTypeGpw) || src.isRegType(kX86RegTypeGpz));
    _emitInstruction(kX86InstPush, &src);
  }
  //! @brief Push Segment Register Onto the Stack.
  inline void push(const SegmentReg& src)
  { _emitInstruction(kX86InstPush, &src); }
  //! @brief Push WORD/DWORD/QWORD Onto the Stack.
  inline void push(const Mem& src)
  {
    ASMJIT_ASSERT(src.getSize() == 2 || src.getSize() == sizeof(sysint_t));
    _emitInstruction(kX86InstPush, &src);
  }
  //! @brief Push WORD/DWORD/QWORD Onto the Stack.
  inline void push(const Imm& src)
  { _emitInstruction(kX86InstPush, &src); }

#if defined(ASMJIT_X86)
  //! @brief Push All General-Purpose Registers.
  //!
  //! Push EAX, ECX, EDX, EBX, original ESP, EBP, ESI, and EDI.
  inline void pushad()
  { _emitInstruction(kX86InstPushAD); }
#endif // ASMJIT_X86

  //! @brief Push EFLAGS Register (32-bit or 64-bit) onto the Stack.
  inline void pushf()
  {
#if defined(ASMJIT_X86)
    pushfd();
#else
    pushfq();
#endif
  }

#if defined(ASMJIT_X86)
  //! @brief Push EFLAGS Register (32-bit) onto the Stack.
  inline void pushfd() { _emitInstruction(kX86InstPushFD); }
#else
  //! @brief Push EFLAGS Register (64-bit) onto the Stack.
  inline void pushfq() { _emitInstruction(kX86InstPushFQ); }
#endif // ASMJIT_X86

  //! @brief Rotate Bits Left.
  //! @note @a src register can be only @c cl.
  inline void rcl(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstRcl, &dst, &src); }
  //! @brief Rotate Bits Left.
  inline void rcl(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstRcl, &dst, &src); }
  //! @brief Rotate Bits Left.
  //! @note @a src register can be only @c cl.
  inline void rcl(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstRcl, &dst, &src); }
  //! @brief Rotate Bits Left.
  inline void rcl(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstRcl, &dst, &src); }

  //! @brief Rotate Bits Right.
  //! @note @a src register can be only @c cl.
  inline void rcr(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstRcr, &dst, &src); }
  //! @brief Rotate Bits Right.
  inline void rcr(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstRcr, &dst, &src); }
  //! @brief Rotate Bits Right.
  //! @note @a src register can be only @c cl.
  inline void rcr(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstRcr, &dst, &src); }
  //! @brief Rotate Bits Right.
  inline void rcr(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstRcr, &dst, &src); }

  //! @brief Read Time-Stamp Counter (Pentium).
  inline void rdtsc()
  { _emitInstruction(kX86InstRdtsc); }

  //! @brief Read Time-Stamp Counter and Processor ID (New).
  inline void rdtscp()
  { _emitInstruction(kX86InstRdtscP); }

  //! @brief Load ECX/RCX BYTEs from DS:[ESI/RSI] to AL.
  inline void rep_lodsb()
  { _emitInstruction(kX86InstRepLodSB); }

  //! @brief Load ECX/RCX DWORDs from DS:[ESI/RSI] to EAX.
  inline void rep_lodsd()
  { _emitInstruction(kX86InstRepLodSD); }

#if defined(ASMJIT_X64)
  //! @brief Load ECX/RCX QWORDs from DS:[ESI/RSI] to RAX.
  inline void rep_lodsq()
  { _emitInstruction(kX86InstRepLodSQ); }
#endif // ASMJIT_X64

  //! @brief Load ECX/RCX WORDs from DS:[ESI/RSI] to AX.
  inline void rep_lodsw()
  { _emitInstruction(kX86InstRepLodSW); }

  //! @brief Move ECX/RCX BYTEs from DS:[ESI/RSI] to ES:[EDI/RDI].
  inline void rep_movsb()
  { _emitInstruction(kX86InstRepMovSB); }

  //! @brief Move ECX/RCX DWORDs from DS:[ESI/RSI] to ES:[EDI/RDI].
  inline void rep_movsd()
  { _emitInstruction(kX86InstRepMovSD); }

#if defined(ASMJIT_X64)
  //! @brief Move ECX/RCX QWORDs from DS:[ESI/RSI] to ES:[EDI/RDI].
  inline void rep_movsq()
  { _emitInstruction(kX86InstRepMovSQ); }
#endif // ASMJIT_X64

  //! @brief Move ECX/RCX WORDs from DS:[ESI/RSI] to ES:[EDI/RDI].
  inline void rep_movsw()
  { _emitInstruction(kX86InstRepMovSW); }

  //! @brief Fill ECX/RCX BYTEs at ES:[EDI/RDI] with AL.
  inline void rep_stosb()
  { _emitInstruction(kX86InstRepStoSB); }

  //! @brief Fill ECX/RCX DWORDs at ES:[EDI/RDI] with EAX.
  inline void rep_stosd()
  { _emitInstruction(kX86InstRepStoSD); }

#if defined(ASMJIT_X64)
  //! @brief Fill ECX/RCX QWORDs at ES:[EDI/RDI] with RAX.
  inline void rep_stosq()
  { _emitInstruction(kX86InstRepStoSQ); }
#endif // ASMJIT_X64

  //! @brief Fill ECX/RCX WORDs at ES:[EDI/RDI] with AX.
  inline void rep_stosw()
  { _emitInstruction(kX86InstRepStoSW); }

  //! @brief Repeated find nonmatching BYTEs in ES:[EDI/RDI] and DS:[ESI/RDI].
  inline void repe_cmpsb()
  { _emitInstruction(kX86InstRepECmpSB); }
  
  //! @brief Repeated find nonmatching DWORDs in ES:[EDI/RDI] and DS:[ESI/RDI].
  inline void repe_cmpsd()
  { _emitInstruction(kX86InstRepECmpSD); }

#if defined(ASMJIT_X64)
  //! @brief Repeated find nonmatching QWORDs in ES:[EDI/RDI] and DS:[ESI/RDI].
  inline void repe_cmpsq()
  { _emitInstruction(kX86InstRepECmpSQ); }
#endif // ASMJIT_X64
  
  //! @brief Repeated find nonmatching WORDs in ES:[EDI/RDI] and DS:[ESI/RDI].
  inline void repe_cmpsw()
  { _emitInstruction(kX86InstRepECmpSW); }

  //! @brief Find non-AL BYTE starting at ES:[EDI/RDI].
  inline void repe_scasb()
  { _emitInstruction(kX86InstRepEScaSB); }
  
  //! @brief Find non-EAX DWORD starting at ES:[EDI/RDI].
  inline void repe_scasd()
  { _emitInstruction(kX86InstRepEScaSD); }

#if defined(ASMJIT_X64)
  //! @brief Find non-RAX QWORD starting at ES:[EDI/RDI].
  inline void repe_scasq()
  { _emitInstruction(kX86InstRepEScaSQ); }
#endif // ASMJIT_X64

  //! @brief Find non-AX WORD starting at ES:[EDI/RDI].
  inline void repe_scasw()
  { _emitInstruction(kX86InstRepEScaSW); }

  //! @brief Repeated find nonmatching BYTEs in ES:[EDI/RDI] and DS:[ESI/RDI].
  inline void repne_cmpsb()
  { _emitInstruction(kX86InstRepNECmpSB); }

  //! @brief Repeated find nonmatching DWORDs in ES:[EDI/RDI] and DS:[ESI/RDI].
  inline void repne_cmpsd()
  { _emitInstruction(kX86InstRepNECmpSD); }

#if defined(ASMJIT_X64)
  //! @brief Repeated find nonmatching QWORDs in ES:[EDI/RDI] and DS:[ESI/RDI].
  inline void repne_cmpsq()
  { _emitInstruction(kX86InstRepNECmpSQ); }
#endif // ASMJIT_X64

  //! @brief Repeated find nonmatching WORDs in ES:[EDI/RDI] and DS:[ESI/RDI].
  inline void repne_cmpsw()
  { _emitInstruction(kX86InstRepNECmpSW); }

  //! @brief Find AL, starting at ES:[EDI/RDI].
  inline void repne_scasb()
  { _emitInstruction(kX86InstRepNEScaSB); }

  //! @brief Find EAX, starting at ES:[EDI/RDI].
  inline void repne_scasd()
  { _emitInstruction(kX86InstRepNEScaSD); }

#if defined(ASMJIT_X64)
  //! @brief Find RAX, starting at ES:[EDI/RDI].
  inline void repne_scasq()
  { _emitInstruction(kX86InstRepNEScaSQ); }
#endif // ASMJIT_X64

  //! @brief Find AX, starting at ES:[EDI/RDI].
  inline void repne_scasw()
  { _emitInstruction(kX86InstRepNEScaSW); }

  //! @brief Return from Procedure.
  inline void ret()
  { _emitInstruction(kX86InstRet); }

  //! @brief Return from Procedure.
  inline void ret(const Imm& imm16)
  { _emitInstruction(kX86InstRet, &imm16); }

  //! @brief Rotate Bits Left.
  //! @note @a src register can be only @c cl.
  inline void rol(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstRol, &dst, &src); }
  //! @brief Rotate Bits Left.
  inline void rol(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstRol, &dst, &src); }
  //! @brief Rotate Bits Left.
  //! @note @a src register can be only @c cl.
  inline void rol(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstRol, &dst, &src); }
  //! @brief Rotate Bits Left.
  inline void rol(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstRol, &dst, &src); }

  //! @brief Rotate Bits Right.
  //! @note @a src register can be only @c cl.
  inline void ror(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstRor, &dst, &src); }
  //! @brief Rotate Bits Right.
  inline void ror(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstRor, &dst, &src); }
  //! @brief Rotate Bits Right.
  //! @note @a src register can be only @c cl.
  inline void ror(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstRor, &dst, &src); }
  //! @brief Rotate Bits Right.
  inline void ror(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstRor, &dst, &src); }

#if defined(ASMJIT_X86)
  //! @brief Store AH into Flags.
  inline void sahf()
  { _emitInstruction(kX86InstSahf); }
#endif // ASMJIT_X86

  //! @brief Integer subtraction with borrow.
  inline void sbb(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstSbb, &dst, &src); }
  //! @brief Integer subtraction with borrow.
  inline void sbb(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstSbb, &dst, &src); }
  //! @brief Integer subtraction with borrow.
  inline void sbb(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstSbb, &dst, &src); }
  //! @brief Integer subtraction with borrow.
  inline void sbb(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstSbb, &dst, &src); }
  //! @brief Integer subtraction with borrow.
  inline void sbb(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstSbb, &dst, &src); }

  //! @brief Shift Bits Left.
  //! @note @a src register can be only @c cl.
  inline void sal(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstSal, &dst, &src); }
  //! @brief Shift Bits Left.
  inline void sal(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstSal, &dst, &src); }
  //! @brief Shift Bits Left.
  //! @note @a src register can be only @c cl.
  inline void sal(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstSal, &dst, &src); }
  //! @brief Shift Bits Left.
  inline void sal(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstSal, &dst, &src); }

  //! @brief Shift Bits Right.
  //! @note @a src register can be only @c cl.
  inline void sar(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstSar, &dst, &src); }
  //! @brief Shift Bits Right.
  inline void sar(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstSar, &dst, &src); }
  //! @brief Shift Bits Right.
  //! @note @a src register can be only @c cl.
  inline void sar(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstSar, &dst, &src); }
  //! @brief Shift Bits Right.
  inline void sar(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstSar, &dst, &src); }

  //! @brief Set Byte on Condition.
  inline void set(kX86Cond cc, const GpReg& dst)
  {
    ASMJIT_ASSERT(dst.getSize() == 1);
    _emitInstruction(X86Util::getSetccInstFromCond(cc), &dst);
  }

  //! @brief Set Byte on Condition.
  inline void set(kX86Cond cc, const Mem& dst)
  {
    ASMJIT_ASSERT(dst.getSize() <= 1);
    _emitInstruction(X86Util::getSetccInstFromCond(cc), &dst);
  }

  //! @brief Set Byte on Condition.
  inline void seta  (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetA  , &dst); }
  //! @brief Set Byte on Condition.
  inline void seta  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetA  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setae (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetAE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setae (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetAE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setb  (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetB  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setb  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetB  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setbe (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetBE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setbe (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetBE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setc  (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetC  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setc  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetC  , &dst); }
  //! @brief Set Byte on Condition.
  inline void sete  (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetE  , &dst); }
  //! @brief Set Byte on Condition.
  inline void sete  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetE  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setg  (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetG  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setg  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetG  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setge (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetGE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setge (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetGE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setl  (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetL  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setl  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetL  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setle (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetLE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setle (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetLE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setna (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNA , &dst); }
  //! @brief Set Byte on Condition.
  inline void setna (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNA , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnae(const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNAE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setnae(const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNAE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setnb (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNB , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnb (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNB , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnbe(const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNBE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setnbe(const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNBE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setnc (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNC , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnc (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNC , &dst); }
  //! @brief Set Byte on Condition.
  inline void setne (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setne (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setng (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNG , &dst); }
  //! @brief Set Byte on Condition.
  inline void setng (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNG , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnge(const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNGE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setnge(const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNGE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setnl (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNL , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnl (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNL , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnle(const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNLE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setnle(const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNLE, &dst); }
  //! @brief Set Byte on Condition.
  inline void setno (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNO , &dst); }
  //! @brief Set Byte on Condition.
  inline void setno (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNO , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnp (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNP , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnp (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNP , &dst); }
  //! @brief Set Byte on Condition.
  inline void setns (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNS , &dst); }
  //! @brief Set Byte on Condition.
  inline void setns (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNS , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnz (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetNZ , &dst); }
  //! @brief Set Byte on Condition.
  inline void setnz (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetNZ , &dst); }
  //! @brief Set Byte on Condition.
  inline void seto  (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetO  , &dst); }
  //! @brief Set Byte on Condition.
  inline void seto  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetO  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setp  (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetP  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setp  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetP  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setpe (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetPE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setpe (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetPE , &dst); }
  //! @brief Set Byte on Condition.
  inline void setpo (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetPO , &dst); }
  //! @brief Set Byte on Condition.
  inline void setpo (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetPO , &dst); }
  //! @brief Set Byte on Condition.
  inline void sets  (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetS  , &dst); }
  //! @brief Set Byte on Condition.
  inline void sets  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetS  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setz  (const GpReg& dst) { ASMJIT_ASSERT(dst.getSize() == 1); _emitInstruction(kX86InstSetZ  , &dst); }
  //! @brief Set Byte on Condition.
  inline void setz  (const Mem& dst)   { ASMJIT_ASSERT(dst.getSize() <= 1); _emitInstruction(kX86InstSetZ  , &dst); }

  //! @brief Shift Bits Left.
  //! @note @a src register can be only @c cl.
  inline void shl(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstShl, &dst, &src); }
  //! @brief Shift Bits Left.
  inline void shl(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstShl, &dst, &src); }
  //! @brief Shift Bits Left.
  //! @note @a src register can be only @c cl.
  inline void shl(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstShl, &dst, &src); }
  //! @brief Shift Bits Left.
  inline void shl(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstShl, &dst, &src); }

  //! @brief Shift Bits Right.
  //! @note @a src register can be only @c cl.
  inline void shr(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstShr, &dst, &src); }
  //! @brief Shift Bits Right.
  inline void shr(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstShr, &dst, &src); }
  //! @brief Shift Bits Right.
  //! @note @a src register can be only @c cl.
  inline void shr(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstShr, &dst, &src); }
  //! @brief Shift Bits Right.
  inline void shr(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstShr, &dst, &src); }

  //! @brief Double Precision Shift Left.
  //! @note src2 register can be only @c cl register.
  inline void shld(const GpReg& dst, const GpReg& src1, const GpReg& src2)
  { _emitInstruction(kX86InstShld, &dst, &src1, &src2); }
  //! @brief Double Precision Shift Left.
  inline void shld(const GpReg& dst, const GpReg& src1, const Imm& src2)
  { _emitInstruction(kX86InstShld, &dst, &src1, &src2); }
  //! @brief Double Precision Shift Left.
  //! @note src2 register can be only @c cl register.
  inline void shld(const Mem& dst, const GpReg& src1, const GpReg& src2)
  { _emitInstruction(kX86InstShld, &dst, &src1, &src2); }
  //! @brief Double Precision Shift Left.
  inline void shld(const Mem& dst, const GpReg& src1, const Imm& src2)
  { _emitInstruction(kX86InstShld, &dst, &src1, &src2); }

  //! @brief Double Precision Shift Right.
  //! @note src2 register can be only @c cl register.
  inline void shrd(const GpReg& dst, const GpReg& src1, const GpReg& src2)
  { _emitInstruction(kX86InstShrd, &dst, &src1, &src2); }
  //! @brief Double Precision Shift Right.
  inline void shrd(const GpReg& dst, const GpReg& src1, const Imm& src2)
  { _emitInstruction(kX86InstShrd, &dst, &src1, &src2); }
  //! @brief Double Precision Shift Right.
  //! @note src2 register can be only @c cl register.
  inline void shrd(const Mem& dst, const GpReg& src1, const GpReg& src2)
  { _emitInstruction(kX86InstShrd, &dst, &src1, &src2); }
  //! @brief Double Precision Shift Right.
  inline void shrd(const Mem& dst, const GpReg& src1, const Imm& src2)
  { _emitInstruction(kX86InstShrd, &dst, &src1, &src2); }

  //! @brief Set Carry Flag to 1.
  inline void stc()
  { _emitInstruction(kX86InstStc); }

  //! @brief Set Direction Flag to 1.
  inline void std()
  { _emitInstruction(kX86InstStd); }

  //! @brief Subtract.
  inline void sub(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstSub, &dst, &src); }
  //! @brief Subtract.
  inline void sub(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstSub, &dst, &src); }
  //! @brief Subtract.
  inline void sub(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstSub, &dst, &src); }
  //! @brief Subtract.
  inline void sub(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstSub, &dst, &src); }
  //! @brief Subtract.
  inline void sub(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstSub, &dst, &src); }

  //! @brief Logical Compare.
  inline void test(const GpReg& op1, const GpReg& op2)
  { _emitInstruction(kX86InstTest, &op1, &op2); }
  //! @brief Logical Compare.
  inline void test(const GpReg& op1, const Imm& op2)
  { _emitInstruction(kX86InstTest, &op1, &op2); }
  //! @brief Logical Compare.
  inline void test(const Mem& op1, const GpReg& op2)
  { _emitInstruction(kX86InstTest, &op1, &op2); }
  //! @brief Logical Compare.
  inline void test(const Mem& op1, const Imm& op2)
  { _emitInstruction(kX86InstTest, &op1, &op2); }

  //! @brief Undefined instruction - Raise invalid opcode exception.
  inline void ud2()
  { _emitInstruction(kX86InstUd2); }

  //! @brief Exchange and Add.
  inline void xadd(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstXadd, &dst, &src); }
  //! @brief Exchange and Add.
  inline void xadd(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstXadd, &dst, &src); }

  //! @brief Exchange Register/Memory with Register.
  inline void xchg(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstXchg, &dst, &src); }
  //! @brief Exchange Register/Memory with Register.
  inline void xchg(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstXchg, &dst, &src); }
  //! @brief Exchange Register/Memory with Register.
  inline void xchg(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstXchg, &src, &dst); }

  //! @brief Exchange Register/Memory with Register.
  inline void xor_(const GpReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstXor, &dst, &src); }
  //! @brief Exchange Register/Memory with Register.
  inline void xor_(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstXor, &dst, &src); }
  //! @brief Exchange Register/Memory with Register.
  inline void xor_(const GpReg& dst, const Imm& src)
  { _emitInstruction(kX86InstXor, &dst, &src); }
  //! @brief Exchange Register/Memory with Register.
  inline void xor_(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstXor, &dst, &src); }
  //! @brief Exchange Register/Memory with Register.
  inline void xor_(const Mem& dst, const Imm& src)
  { _emitInstruction(kX86InstXor, &dst, &src); }

  // --------------------------------------------------------------------------
  // [X87 Instructions (FPU)]
  // --------------------------------------------------------------------------

  //! @brief Compute 2^x - 1 (FPU).
  inline void f2xm1()
  { _emitInstruction(kX86InstF2XM1); }

  //! @brief Absolute Value of st(0) (FPU).
  inline void fabs()
  { _emitInstruction(kX86InstFAbs); }

  //! @brief Add @a src to @a dst and store result in @a dst (FPU).
  //!
  //! @note One of dst or src must be st(0).
  inline void fadd(const X87Reg& dst, const X87Reg& src)
  {
    ASMJIT_ASSERT(dst.getRegIndex() == 0 || src.getRegIndex() == 0);
    _emitInstruction(kX86InstFAdd, &dst, &src);
  }

  //! @brief Add @a src to st(0) and store result in st(0) (FPU).
  //!
  //! @note SP-FP or DP-FP determined by @a adr size.
  inline void fadd(const Mem& src)
  { _emitInstruction(kX86InstFAdd, &src); }

  //! @brief Add st(0) to @a dst and POP register stack (FPU).
  inline void faddp(const X87Reg& dst = st(1))
  { _emitInstruction(kX86InstFAddP, &dst); }

  //! @brief Load Binary Coded Decimal (FPU).
  inline void fbld(const Mem& src)
  { _emitInstruction(kX86InstFBLd, &src); }

  //! @brief Store BCD Integer and Pop (FPU).
  inline void fbstp(const Mem& dst)
  { _emitInstruction(kX86InstFBStP, &dst); }

  //! @brief Change st(0) Sign (FPU).
  inline void fchs()
  { _emitInstruction(kX86InstFCHS); }

  //! @brief Clear Exceptions (FPU).
  //!
  //! Clear floating-point exception flags after checking for pending unmasked
  //! floating-point exceptions.
  //!
  //! Clears the floating-point exception flags (PE, UE, OE, ZE, DE, and IE),
  //! the exception summary status flag (ES), the stack fault flag (SF), and
  //! the busy flag (B) in the FPU status word. The FCLEX instruction checks
  //! for and handles any pending unmasked floating-point exceptions before
  //! clearing the exception flags.
  inline void fclex()
  { _emitInstruction(kX86InstFClex); }

  //! @brief FP Conditional Move (FPU).
  inline void fcmovb(const X87Reg& src)
  { _emitInstruction(kX86InstFCMovB, &src); }
  //! @brief FP Conditional Move (FPU).
  inline void fcmovbe(const X87Reg& src)
  { _emitInstruction(kX86InstFCMovBE, &src); }
  //! @brief FP Conditional Move (FPU).
  inline void fcmove(const X87Reg& src)
  { _emitInstruction(kX86InstFCMovE, &src); }
  //! @brief FP Conditional Move (FPU).
  inline void fcmovnb(const X87Reg& src)
  { _emitInstruction(kX86InstFCMovNB, &src); }
  //! @brief FP Conditional Move (FPU).
  inline void fcmovnbe(const X87Reg& src)
  { _emitInstruction(kX86InstFCMovNBE, &src); }
  //! @brief FP Conditional Move (FPU).
  inline void fcmovne(const X87Reg& src)
  { _emitInstruction(kX86InstFCMovNE, &src); }
  //! @brief FP Conditional Move (FPU).
  inline void fcmovnu(const X87Reg& src)
  { _emitInstruction(kX86InstFCMovNU, &src); }
  //! @brief FP Conditional Move (FPU).
  inline void fcmovu(const X87Reg& src)
  { _emitInstruction(kX86InstFCMovU, &src); }

  //! @brief Compare st(0) with @a reg (FPU).
  inline void fcom(const X87Reg& reg = st(1))
  { _emitInstruction(kX86InstFCom, &reg); }
  //! @brief Compare st(0) with 4-byte or 8-byte FP at @a src (FPU).
  inline void fcom(const Mem& src)
  { _emitInstruction(kX86InstFCom, &src); }

  //! @brief Compare st(0) with @a reg and pop the stack (FPU).
  inline void fcomp(const X87Reg& reg = st(1))
  { _emitInstruction(kX86InstFComP, &reg); }
  //! @brief Compare st(0) with 4-byte or 8-byte FP at @a adr and pop the
  //! stack (FPU).
  inline void fcomp(const Mem& mem)
  { _emitInstruction(kX86InstFComP, &mem); }

  //! @brief Compare st(0) with st(1) and pop register stack twice (FPU).
  inline void fcompp()
  { _emitInstruction(kX86InstFComPP); }

  //! @brief Compare st(0) and @a reg and Set EFLAGS (FPU).
  inline void fcomi(const X87Reg& reg)
  { _emitInstruction(kX86InstFComI, &reg); }

  //! @brief Compare st(0) and @a reg and Set EFLAGS and pop the stack (FPU).
  inline void fcomip(const X87Reg& reg)
  { _emitInstruction(kX86InstFComIP, &reg); }

  //! @brief Cosine (FPU).
  //!
  //! This instruction calculates the cosine of the source operand in
  //! register st(0) and stores the result in st(0).
  inline void fcos()
  { _emitInstruction(kX86InstFCos); }

  //! @brief Decrement Stack-Top Pointer (FPU).
  //!
  //! Subtracts one from the TOP field of the FPU status word (decrements
  //! the top-ofstack pointer). If the TOP field contains a 0, it is set
  //! to 7. The effect of this instruction is to rotate the stack by one
  //! position. The contents of the FPU data registers and tag register
  //! are not affected.
  inline void fdecstp()
  { _emitInstruction(kX86InstFDecStP); }

  //! @brief Divide @a dst by @a src (FPU).
  //!
  //! @note One of @a dst or @a src register must be st(0).
  inline void fdiv(const X87Reg& dst, const X87Reg& src)
  {
    ASMJIT_ASSERT(dst.getRegIndex() == 0 || src.getRegIndex() == 0);
    _emitInstruction(kX86InstFDiv, &dst, &src);
  }
  //! @brief Divide st(0) by 32-bit or 64-bit FP value (FPU).
  inline void fdiv(const Mem& src)
  { _emitInstruction(kX86InstFDiv, &src); }

  //! @brief Divide @a reg by st(0) (FPU).
  inline void fdivp(const X87Reg& reg = st(1))
  { _emitInstruction(kX86InstFDivP, &reg); }

  //! @brief Reverse Divide @a dst by @a src (FPU).
  //!
  //! @note One of @a dst or @a src register must be st(0).
  inline void fdivr(const X87Reg& dst, const X87Reg& src)
  {
    ASMJIT_ASSERT(dst.getRegIndex() == 0 || src.getRegIndex() == 0);
    _emitInstruction(kX86InstFDivR, &dst, &src);
  }
  //! @brief Reverse Divide st(0) by 32-bit or 64-bit FP value (FPU).
  inline void fdivr(const Mem& src)
  { _emitInstruction(kX86InstFDivR, &src); }

  //! @brief Reverse Divide @a reg by st(0) (FPU).
  inline void fdivrp(const X87Reg& reg = st(1))
  { _emitInstruction(kX86InstFDivRP, &reg); }

  //! @brief Free Floating-Point Register (FPU).
  //!
  //! Sets the tag in the FPU tag register associated with register @a reg
  //! to empty (11B). The contents of @a reg and the FPU stack-top pointer
  //! (TOP) are not affected.
  inline void ffree(const X87Reg& reg)
  { _emitInstruction(kX86InstFFree, &reg); }

  //! @brief Add 16-bit or 32-bit integer to st(0) (FPU).
  inline void fiadd(const Mem& src)
  {
    ASMJIT_ASSERT(src.getSize() == 2 || src.getSize() == 4);
    _emitInstruction(kX86InstFIAdd, &src);
  }

  //! @brief Compare st(0) with 16-bit or 32-bit Integer (FPU).
  inline void ficom(const Mem& src)
  {
    ASMJIT_ASSERT(src.getSize() == 2 || src.getSize() == 4);
    _emitInstruction(kX86InstFICom, &src);
  }

  //! @brief Compare st(0) with 16-bit or 32-bit Integer and pop the stack (FPU).
  inline void ficomp(const Mem& src)
  {
    ASMJIT_ASSERT(src.getSize() == 2 || src.getSize() == 4);
    _emitInstruction(kX86InstFIComP, &src);
  }

  //! @brief Divide st(0) by 32-bit or 16-bit integer (@a src) (FPU).
  inline void fidiv(const Mem& src)
  {
    ASMJIT_ASSERT(src.getSize() == 2 || src.getSize() == 4);
    _emitInstruction(kX86InstFIDiv, &src);
  }

  //! @brief Reverse Divide st(0) by 32-bit or 16-bit integer (@a src) (FPU).
  inline void fidivr(const Mem& src)
  {
    ASMJIT_ASSERT(src.getSize() == 2 || src.getSize() == 4);
    _emitInstruction(kX86InstFIDivR, &src);
  }

  //! @brief Load 16-bit, 32-bit or 64-bit Integer and push it to the stack (FPU).
  //!
  //! Converts the signed-integer source operand into double extended-precision
  //! floating point format and pushes the value onto the FPU register stack.
  //! The source operand can be a word, doubleword, or quadword integer. It is
  //! loaded without rounding errors. The sign of the source operand is
  //! preserved.
  inline void fild(const Mem& src)
  {
    ASMJIT_ASSERT(src.getSize() == 2 || src.getSize() == 4 || src.getSize() == 8);
    _emitInstruction(kX86InstFILd, &src);
  }

  //! @brief Multiply st(0) by 16-bit or 32-bit integer and store it
  //! to st(0) (FPU).
  inline void fimul(const Mem& src)
  {
    ASMJIT_ASSERT(src.getSize() == 2 || src.getSize() == 4);
    _emitInstruction(kX86InstFIMul, &src);
  }

  //! @brief Increment Stack-Top Pointer (FPU).
  //!
  //! Adds one to the TOP field of the FPU status word (increments the
  //! top-of-stack pointer). If the TOP field contains a 7, it is set to 0.
  //! The effect of this instruction is to rotate the stack by one position.
  //! The contents of the FPU data registers and tag register are not affected.
  //! This operation is not equivalent to popping the stack, because the tag
  //! for the previous top-of-stack register is not marked empty.
  inline void fincstp()
  { _emitInstruction(kX86InstFIncStP); }

  //! @brief Initialize Floating-Point Unit (FPU).
  //!
  //! Initialize FPU after checking for pending unmasked floating-point
  //! exceptions.
  inline void finit()
  { _emitInstruction(kX86InstFInit); }

  //! @brief Subtract 16-bit or 32-bit integer from st(0) and store result to
  //! st(0) (FPU).
  inline void fisub(const Mem& src)
  {
    ASMJIT_ASSERT(src.getSize() == 2 || src.getSize() == 4);
    _emitInstruction(kX86InstFISub, &src);
  }

  //! @brief Reverse Subtract 16-bit or 32-bit integer from st(0) and
  //! store result to  st(0) (FPU).
  inline void fisubr(const Mem& src)
  {
    ASMJIT_ASSERT(src.getSize() == 2 || src.getSize() == 4);
    _emitInstruction(kX86InstFISubR, &src);
  }

  //! @brief Initialize Floating-Point Unit (FPU).
  //!
  //! Initialize FPU without checking for pending unmasked floating-point
  //! exceptions.
  inline void fninit()
  { _emitInstruction(kX86InstFNInit); }

  //! @brief Store st(0) as 16-bit or 32-bit Integer to @a dst (FPU).
  inline void fist(const Mem& dst)
  {
    ASMJIT_ASSERT(dst.getSize() == 2 || dst.getSize() == 4);
    _emitInstruction(kX86InstFISt, &dst);
  }

  //! @brief Store st(0) as 16-bit, 32-bit or 64-bit Integer to @a dst and pop
  //! stack (FPU).
  inline void fistp(const Mem& dst)
  {
    ASMJIT_ASSERT(dst.getSize() == 2 || dst.getSize() == 4 || dst.getSize() == 8);
    _emitInstruction(kX86InstFIStP, &dst);
  }

  //! @brief Push 32-bit, 64-bit or 80-bit Floating Point Value onto the FPU
  //! register stack (FPU).
  inline void fld(const Mem& src)
  {
    ASMJIT_ASSERT(src.getSize() == 4 || src.getSize() == 8 || src.getSize() == 10);
    _emitInstruction(kX86InstFLd, &src);
  }

  //! @brief Push @a reg onto the FPU register stack (FPU).
  inline void fld(const X87Reg& reg)
  { _emitInstruction(kX86InstFLd, &reg); }

  //! @brief Push +1.0 onto the FPU register stack (FPU).
  inline void fld1()
  { _emitInstruction(kX86InstFLd1); }

  //! @brief Push log2(10) onto the FPU register stack (FPU).
  inline void fldl2t()
  { _emitInstruction(kX86InstFLdL2T); }

  //! @brief Push log2(e) onto the FPU register stack (FPU).
  inline void fldl2e()
  { _emitInstruction(kX86InstFLdL2E); }

  //! @brief Push pi onto the FPU register stack (FPU).
  inline void fldpi()
  { _emitInstruction(kX86InstFLdPi); }

  //! @brief Push log10(2) onto the FPU register stack (FPU).
  inline void fldlg2()
  { _emitInstruction(kX86InstFLdLg2); }

  //! @brief Push ln(2) onto the FPU register stack (FPU).
  inline void fldln2()
  { _emitInstruction(kX86InstFLdLn2); }

  //! @brief Push +0.0 onto the FPU register stack (FPU).
  inline void fldz()
  { _emitInstruction(kX86InstFLdZ); }

  //! @brief Load x87 FPU Control Word (2 bytes) (FPU).
  inline void fldcw(const Mem& src)
  { _emitInstruction(kX86InstFLdCw, &src); }

  //! @brief Load x87 FPU Environment (14 or 28 bytes) (FPU).
  inline void fldenv(const Mem& src)
  { _emitInstruction(kX86InstFLdEnv, &src); }

  //! @brief Multiply @a dst by @a src and store result in @a dst (FPU).
  //!
  //! @note One of dst or src must be st(0).
  inline void fmul(const X87Reg& dst, const X87Reg& src)
  {
    ASMJIT_ASSERT(dst.getRegIndex() == 0 || src.getRegIndex() == 0);
    _emitInstruction(kX86InstFMul, &dst, &src);
  }
  //! @brief Multiply st(0) by @a src and store result in st(0) (FPU).
  //!
  //! @note SP-FP or DP-FP determined by @a adr size.
  inline void fmul(const Mem& src)
  { _emitInstruction(kX86InstFMul, &src); }

  //! @brief Multiply st(0) by @a dst and POP register stack (FPU).
  inline void fmulp(const X87Reg& dst = st(1))
  { _emitInstruction(kX86InstFMulP, &dst); }

  //! @brief Clear Exceptions (FPU).
  //!
  //! Clear floating-point exception flags without checking for pending
  //! unmasked floating-point exceptions.
  //!
  //! Clears the floating-point exception flags (PE, UE, OE, ZE, DE, and IE),
  //! the exception summary status flag (ES), the stack fault flag (SF), and
  //! the busy flag (B) in the FPU status word. The FCLEX instruction does
  //! not checks for and handles any pending unmasked floating-point exceptions
  //! before clearing the exception flags.
  inline void fnclex()
  { _emitInstruction(kX86InstFNClex); }

  //! @brief No Operation (FPU).
  inline void fnop()
  { _emitInstruction(kX86InstFNop); }

  //! @brief Save FPU State (FPU).
  //!
  //! Store FPU environment to m94byte or m108byte without
  //! checking for pending unmasked FP exceptions.
  //! Then re-initialize the FPU.
  inline void fnsave(const Mem& dst)
  { _emitInstruction(kX86InstFNSave, &dst); }

  //! @brief Store x87 FPU Environment (FPU).
  //!
  //! Store FPU environment to @a dst (14 or 28 Bytes) without checking for
  //! pending unmasked floating-point exceptions. Then mask all floating
  //! point exceptions.
  inline void fnstenv(const Mem& dst)
  { _emitInstruction(kX86InstFNStEnv, &dst); }

  //! @brief Store x87 FPU Control Word (FPU).
  //!
  //! Store FPU control word to @a dst (2 Bytes) without checking for pending
  //! unmasked floating-point exceptions.
  inline void fnstcw(const Mem& dst)
  { _emitInstruction(kX86InstFNStCw, &dst); }

  //! @brief Store x87 FPU Status Word (2 Bytes) (FPU).
  inline void fnstsw(const GpReg& dst)
  {
    ASMJIT_ASSERT(dst.isRegCode(kX86RegAx));
    _emitInstruction(kX86InstFNStSw, &dst);
  }
  //! @brief Store x87 FPU Status Word (2 Bytes) (FPU).
  inline void fnstsw(const Mem& dst)
  { _emitInstruction(kX86InstFNStSw, &dst); }

  //! @brief Partial Arctangent (FPU).
  //!
  //! Replace st(1) with arctan(st(1)/st(0)) and pop the register stack.
  inline void fpatan()
  { _emitInstruction(kX86InstFPAtan); }

  //! @brief Partial Remainder (FPU).
  //!
  //! Replace st(0) with the remainder obtained from dividing st(0) by st(1).
  inline void fprem()
  { _emitInstruction(kX86InstFPRem); }

  //! @brief Partial Remainder (FPU).
  //!
  //! Replace st(0) with the IEEE remainder obtained from dividing st(0) by
  //! st(1).
  inline void fprem1()
  { _emitInstruction(kX86InstFPRem1); }

  //! @brief Partial Tangent (FPU).
  //!
  //! Replace st(0) with its tangent and push 1 onto the FPU stack.
  inline void fptan()
  { _emitInstruction(kX86InstFPTan); }

  //! @brief Round to Integer (FPU).
  //!
  //! Rount st(0) to an Integer.
  inline void frndint()
  { _emitInstruction(kX86InstFRndInt); }

  //! @brief Restore FPU State (FPU).
  //!
  //! Load FPU state from src (94 or 108 bytes).
  inline void frstor(const Mem& src)
  { _emitInstruction(kX86InstFRstor, &src); }

  //! @brief Save FPU State (FPU).
  //!
  //! Store FPU state to 94 or 108-bytes after checking for
  //! pending unmasked FP exceptions. Then reinitialize
  //! the FPU.
  inline void fsave(const Mem& dst)
  { _emitInstruction(kX86InstFSave, &dst); }

  //! @brief Scale (FPU).
  //!
  //! Scale st(0) by st(1).
  inline void fscale()
  { _emitInstruction(kX86InstFScale); }

  //! @brief Sine (FPU).
  //!
  //! This instruction calculates the sine of the source operand in
  //! register st(0) and stores the result in st(0).
  inline void fsin()
  { _emitInstruction(kX86InstFSin); }

  //! @brief Sine and Cosine (FPU).
  //!
  //! Compute the sine and cosine of st(0); replace st(0) with
  //! the sine, and push the cosine onto the register stack.
  inline void fsincos()
  { _emitInstruction(kX86InstFSinCos); }

  //! @brief Square Root (FPU).
  //!
  //! Calculates square root of st(0) and stores the result in st(0).
  inline void fsqrt()
  { _emitInstruction(kX86InstFSqrt); }

  //! @brief Store Floating Point Value (FPU).
  //!
  //! Store st(0) as 32-bit or 64-bit floating point value to @a dst.
  inline void fst(const Mem& dst)
  {
    ASMJIT_ASSERT(dst.getSize() == 4 || dst.getSize() == 8);
    _emitInstruction(kX86InstFSt, &dst);
  }

  //! @brief Store Floating Point Value (FPU).
  //!
  //! Store st(0) to @a reg.
  inline void fst(const X87Reg& reg)
  { _emitInstruction(kX86InstFSt, &reg); }

  //! @brief Store Floating Point Value and Pop Register Stack (FPU).
  //!
  //! Store st(0) as 32-bit or 64-bit floating point value to @a dst
  //! and pop register stack.
  inline void fstp(const Mem& dst)
  {
    ASMJIT_ASSERT(dst.getSize() == 4 || dst.getSize() == 8 || dst.getSize() == 10);
    _emitInstruction(kX86InstFStP, &dst);
  }

  //! @brief Store Floating Point Value and Pop Register Stack (FPU).
  //!
  //! Store st(0) to @a reg and pop register stack.
  inline void fstp(const X87Reg& reg)
  { _emitInstruction(kX86InstFStP, &reg); }

  //! @brief Store x87 FPU Control Word (FPU).
  //!
  //! Store FPU control word to @a dst (2 Bytes) after checking for pending
  //! unmasked floating-point exceptions.
  inline void fstcw(const Mem& dst)
  { _emitInstruction(kX86InstFStCw, &dst); }

  //! @brief Store x87 FPU Environment (FPU).
  //!
  //! Store FPU environment to @a dst (14 or 28 Bytes) after checking for
  //! pending unmasked floating-point exceptions. Then mask all floating
  //! point exceptions.
  inline void fstenv(const Mem& dst)
  { _emitInstruction(kX86InstFStEnv, &dst); }

  //! @brief Store x87 FPU Status Word (2 Bytes) (FPU).
  inline void fstsw(const GpReg& dst)
  {
    ASMJIT_ASSERT(dst.isRegCode(kX86RegAx));
    _emitInstruction(kX86InstFStSw, &dst);
  }
  //! @brief Store x87 FPU Status Word (2 Bytes) (FPU).
  inline void fstsw(const Mem& dst)
  { _emitInstruction(kX86InstFStSw, &dst); }

  //! @brief Subtract @a src from @a dst and store result in @a dst (FPU).
  //!
  //! @note One of dst or src must be st(0).
  inline void fsub(const X87Reg& dst, const X87Reg& src)
  {
    ASMJIT_ASSERT(dst.getRegIndex() == 0 || src.getRegIndex() == 0);
    _emitInstruction(kX86InstFSub, &dst, &src);
  }
  //! @brief Subtract @a src from st(0) and store result in st(0) (FPU).
  //!
  //! @note SP-FP or DP-FP determined by @a adr size.
  inline void fsub(const Mem& src)
  {
    ASMJIT_ASSERT(src.getSize() == 4 || src.getSize() == 8);
    _emitInstruction(kX86InstFSub, &src);
  }

  //! @brief Subtract st(0) from @a dst and POP register stack (FPU).
  inline void fsubp(const X87Reg& dst = st(1))
  { _emitInstruction(kX86InstFSubP, &dst); }

  //! @brief Reverse Subtract @a src from @a dst and store result in @a dst (FPU).
  //!
  //! @note One of dst or src must be st(0).
  inline void fsubr(const X87Reg& dst, const X87Reg& src)
  {
    ASMJIT_ASSERT(dst.getRegIndex() == 0 || src.getRegIndex() == 0);
    _emitInstruction(kX86InstFSubR, &dst, &src);
  }

  //! @brief Reverse Subtract @a src from st(0) and store result in st(0) (FPU).
  //!
  //! @note SP-FP or DP-FP determined by @a adr size.
  inline void fsubr(const Mem& src)
  {
    ASMJIT_ASSERT(src.getSize() == 4 || src.getSize() == 8);
    _emitInstruction(kX86InstFSubR, &src);
  }

  //! @brief Reverse Subtract st(0) from @a dst and POP register stack (FPU).
  inline void fsubrp(const X87Reg& dst = st(1))
  { _emitInstruction(kX86InstFSubRP, &dst); }

  //! @brief Floating point test - Compare st(0) with 0.0. (FPU).
  inline void ftst()
  { _emitInstruction(kX86InstFTst); }

  //! @brief Unordered Compare st(0) with @a reg (FPU).
  inline void fucom(const X87Reg& reg = st(1))
  { _emitInstruction(kX86InstFUCom, &reg); }

  //! @brief Unordered Compare st(0) and @a reg, check for ordered values
  //! and Set EFLAGS (FPU).
  inline void fucomi(const X87Reg& reg)
  { _emitInstruction(kX86InstFUComI, &reg); }

  //! @brief UnorderedCompare st(0) and @a reg, Check for ordered values
  //! and Set EFLAGS and pop the stack (FPU).
  inline void fucomip(const X87Reg& reg = st(1))
  { _emitInstruction(kX86InstFUComIP, &reg); }

  //! @brief Unordered Compare st(0) with @a reg and pop register stack (FPU).
  inline void fucomp(const X87Reg& reg = st(1))
  { _emitInstruction(kX86InstFUComP, &reg); }

  //! @brief Unordered compare st(0) with st(1) and pop register stack twice
  //! (FPU).
  inline void fucompp()
  { _emitInstruction(kX86InstFUComPP); }

  inline void fwait()
  { _emitInstruction(kX86InstFWait); }

  //! @brief Examine st(0) (FPU).
  //!
  //! Examines the contents of the ST(0) register and sets the condition code
  //! flags C0, C2, and C3 in the FPU status word to indicate the class of
  //! value or number in the register.
  inline void fxam()
  { _emitInstruction(kX86InstFXam); }

  //! @brief Exchange Register Contents (FPU).
  //!
  //! Exchange content of st(0) with @a reg.
  inline void fxch(const X87Reg& reg = st(1))
  { _emitInstruction(kX86InstFXch, &reg); }

  //! @brief Restore FP And MMX(tm) State And Streaming SIMD Extension State
  //! (FPU, MMX, SSE).
  //!
  //! Load FP and MMX(tm) technology and Streaming SIMD Extension state from
  //! src (512 bytes).
  inline void fxrstor(const Mem& src)
  { _emitInstruction(kX86InstFXRstor, &src); }

  //! @brief Store FP and MMX(tm) State and Streaming SIMD Extension State
  //! (FPU, MMX, SSE).
  //!
  //! Store FP and MMX(tm) technology state and Streaming SIMD Extension state
  //! to dst (512 bytes).
  inline void fxsave(const Mem& dst)
  { _emitInstruction(kX86InstFXSave, &dst); }

  //! @brief Extract Exponent and Significand (FPU).
  //!
  //! Separate value in st(0) into exponent and significand, store exponent
  //! in st(0), and push the significand onto the register stack.
  inline void fxtract()
  { _emitInstruction(kX86InstFXtract); }

  //! @brief Compute y * log2(x).
  //!
  //! Replace st(1) with (st(1) * log2st(0)) and pop the register stack.
  inline void fyl2x()
  { _emitInstruction(kX86InstFYL2X); }

  //! @brief Compute y * log_2(x+1).
  //!
  //! Replace st(1) with (st(1) * (log2st(0) + 1.0)) and pop the register stack.
  inline void fyl2xp1()
  { _emitInstruction(kX86InstFYL2XP1); }

  // --------------------------------------------------------------------------
  // [MMX]
  // --------------------------------------------------------------------------

  //! @brief Empty MMX state.
  inline void emms()
  { _emitInstruction(kX86InstEmms); }

  //! @brief Move DWord (MMX).
  inline void movd(const Mem& dst, const MmReg& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }
  //! @brief Move DWord (MMX).
  inline void movd(const GpReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }
  //! @brief Move DWord (MMX).
  inline void movd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }
  //! @brief Move DWord (MMX).
  inline void movd(const MmReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }

  //! @brief Move QWord (MMX).
  inline void movq(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
  //! @brief Move QWord (MMX).
  inline void movq(const Mem& dst, const MmReg& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
#if defined(ASMJIT_X64)
  //! @brief Move QWord (MMX).
  inline void movq(const GpReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
#endif
  //! @brief Move QWord (MMX).
  inline void movq(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
#if defined(ASMJIT_X64)
  //! @brief Move QWord (MMX).
  inline void movq(const MmReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
#endif

  //! @brief Pack with Signed Saturation (MMX).
  inline void packsswb(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPackSSWB, &dst, &src); }
  //! @brief Pack with Signed Saturation (MMX).
  inline void packsswb(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPackSSWB, &dst, &src); }

  //! @brief Pack with Signed Saturation (MMX).
  inline void packssdw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPackSSDW, &dst, &src); }
  //! @brief Pack with Signed Saturation (MMX).
  inline void packssdw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPackSSDW, &dst, &src); }

  //! @brief Pack with Unsigned Saturation (MMX).
  inline void packuswb(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPackUSWB, &dst, &src); }
  //! @brief Pack with Unsigned Saturation (MMX).
  inline void packuswb(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPackUSWB, &dst, &src); }

  //! @brief Packed BYTE Add (MMX).
  inline void paddb(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAddB, &dst, &src); }
  //! @brief Packed BYTE Add (MMX).
  inline void paddb(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddB, &dst, &src); }

  //! @brief Packed WORD Add (MMX).
  inline void paddw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAddW, &dst, &src); }
  //! @brief Packed WORD Add (MMX).
  inline void paddw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddW, &dst, &src); }

  //! @brief Packed DWORD Add (MMX).
  inline void paddd(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAddD, &dst, &src); }
  //! @brief Packed DWORD Add (MMX).
  inline void paddd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddD, &dst, &src); }

  //! @brief Packed Add with Saturation (MMX).
  inline void paddsb(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAddSB, &dst, &src); }
  //! @brief Packed Add with Saturation (MMX).
  inline void paddsb(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddSB, &dst, &src); }

  //! @brief Packed Add with Saturation (MMX).
  inline void paddsw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAddSW, &dst, &src); }
  //! @brief Packed Add with Saturation (MMX).
  inline void paddsw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddSW, &dst, &src); }

  //! @brief Packed Add Unsigned with Saturation (MMX).
  inline void paddusb(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAddUSB, &dst, &src); }
  //! @brief Packed Add Unsigned with Saturation (MMX).
  inline void paddusb(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddUSB, &dst, &src); }

  //! @brief Packed Add Unsigned with Saturation (MMX).
  inline void paddusw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAddUSW, &dst, &src); }
  //! @brief Packed Add Unsigned with Saturation (MMX).
  inline void paddusw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddUSW, &dst, &src); }

  //! @brief Logical AND (MMX).
  inline void pand(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAnd, &dst, &src); }
  //! @brief Logical AND (MMX).
  inline void pand(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAnd, &dst, &src); }

  //! @brief Logical AND Not (MMX).
  inline void pandn(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAndN, &dst, &src); }
  //! @brief Logical AND Not (MMX).
  inline void pandn(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAndN, &dst, &src); }

  //! @brief Packed Compare for Equal (BYTES) (MMX).
  inline void pcmpeqb(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPCmpEqB, &dst, &src); }
  //! @brief Packed Compare for Equal (BYTES) (MMX).
  inline void pcmpeqb(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpEqB, &dst, &src); }

  //! @brief Packed Compare for Equal (WORDS) (MMX).
  inline void pcmpeqw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPCmpEqW, &dst, &src); }
  //! @brief Packed Compare for Equal (WORDS) (MMX).
  inline void pcmpeqw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpEqW, &dst, &src); }

  //! @brief Packed Compare for Equal (DWORDS) (MMX).
  inline void pcmpeqd(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPCmpEqD, &dst, &src); }
  //! @brief Packed Compare for Equal (DWORDS) (MMX).
  inline void pcmpeqd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpEqD, &dst, &src); }

  //! @brief Packed Compare for Greater Than (BYTES) (MMX).
  inline void pcmpgtb(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPCmpGtB, &dst, &src); }
  //! @brief Packed Compare for Greater Than (BYTES) (MMX).
  inline void pcmpgtb(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpGtB, &dst, &src); }

  //! @brief Packed Compare for Greater Than (WORDS) (MMX).
  inline void pcmpgtw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPCmpGtW, &dst, &src); }
  //! @brief Packed Compare for Greater Than (WORDS) (MMX).
  inline void pcmpgtw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpGtW, &dst, &src); }

  //! @brief Packed Compare for Greater Than (DWORDS) (MMX).
  inline void pcmpgtd(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPCmpGtD, &dst, &src); }
  //! @brief Packed Compare for Greater Than (DWORDS) (MMX).
  inline void pcmpgtd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpGtD, &dst, &src); }

  //! @brief Packed Multiply High (MMX).
  inline void pmulhw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPMulHW, &dst, &src); }
  //! @brief Packed Multiply High (MMX).
  inline void pmulhw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulHW, &dst, &src); }

  //! @brief Packed Multiply Low (MMX).
  inline void pmullw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPMulLW, &dst, &src); }
  //! @brief Packed Multiply Low (MMX).
  inline void pmullw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulLW, &dst, &src); }

  //! @brief Bitwise Logical OR (MMX).
  inline void por(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPOr, &dst, &src); }
  //! @brief Bitwise Logical OR (MMX).
  inline void por(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPOr, &dst, &src); }

  //! @brief Packed Multiply and Add (MMX).
  inline void pmaddwd(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPMAddWD, &dst, &src); }
  //! @brief Packed Multiply and Add (MMX).
  inline void pmaddwd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMAddWD, &dst, &src); }

  //! @brief Packed Shift Left Logical (MMX).
  inline void pslld(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSllD, &dst, &src); }
  //! @brief Packed Shift Left Logical (MMX).
  inline void pslld(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSllD, &dst, &src); }
  //! @brief Packed Shift Left Logical (MMX).
  inline void pslld(const MmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSllD, &dst, &src); }

  //! @brief Packed Shift Left Logical (MMX).
  inline void psllq(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSllQ, &dst, &src); }
  //! @brief Packed Shift Left Logical (MMX).
  inline void psllq(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSllQ, &dst, &src); }
  //! @brief Packed Shift Left Logical (MMX).
  inline void psllq(const MmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSllQ, &dst, &src); }

  //! @brief Packed Shift Left Logical (MMX).
  inline void psllw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSllW, &dst, &src); }
  //! @brief Packed Shift Left Logical (MMX).
  inline void psllw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSllW, &dst, &src); }
  //! @brief Packed Shift Left Logical (MMX).
  inline void psllw(const MmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSllW, &dst, &src); }

  //! @brief Packed Shift Right Arithmetic (MMX).
  inline void psrad(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSraD, &dst, &src); }
  //! @brief Packed Shift Right Arithmetic (MMX).
  inline void psrad(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSraD, &dst, &src); }
  //! @brief Packed Shift Right Arithmetic (MMX).
  inline void psrad(const MmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSraD, &dst, &src); }

  //! @brief Packed Shift Right Arithmetic (MMX).
  inline void psraw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSraW, &dst, &src); }
  //! @brief Packed Shift Right Arithmetic (MMX).
  inline void psraw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSraW, &dst, &src); }
  //! @brief Packed Shift Right Arithmetic (MMX).
  inline void psraw(const MmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSraW, &dst, &src); }

  //! @brief Packed Shift Right Logical (MMX).
  inline void psrld(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSrlD, &dst, &src); }
  //! @brief Packed Shift Right Logical (MMX).
  inline void psrld(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSrlD, &dst, &src); }
  //! @brief Packed Shift Right Logical (MMX).
  inline void psrld(const MmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSrlD, &dst, &src); }

  //! @brief Packed Shift Right Logical (MMX).
  inline void psrlq(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSrlQ, &dst, &src); }
  //! @brief Packed Shift Right Logical (MMX).
  inline void psrlq(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSrlQ, &dst, &src); }
  //! @brief Packed Shift Right Logical (MMX).
  inline void psrlq(const MmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSrlQ, &dst, &src); }

  //! @brief Packed Shift Right Logical (MMX).
  inline void psrlw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSrlW, &dst, &src); }
  //! @brief Packed Shift Right Logical (MMX).
  inline void psrlw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSrlW, &dst, &src); }
  //! @brief Packed Shift Right Logical (MMX).
  inline void psrlw(const MmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSrlW, &dst, &src); }

  //! @brief Packed Subtract (MMX).
  inline void psubb(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSubB, &dst, &src); }
  //! @brief Packed Subtract (MMX).
  inline void psubb(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubB, &dst, &src); }

  //! @brief Packed Subtract (MMX).
  inline void psubw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSubW, &dst, &src); }
  //! @brief Packed Subtract (MMX).
  inline void psubw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubW, &dst, &src); }

  //! @brief Packed Subtract (MMX).
  inline void psubd(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSubD, &dst, &src); }
  //! @brief Packed Subtract (MMX).
  inline void psubd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubD, &dst, &src); }

  //! @brief Packed Subtract with Saturation (MMX).
  inline void psubsb(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSubSB, &dst, &src); }
  //! @brief Packed Subtract with Saturation (MMX).
  inline void psubsb(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubSB, &dst, &src); }

  //! @brief Packed Subtract with Saturation (MMX).
  inline void psubsw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSubSW, &dst, &src); }
  //! @brief Packed Subtract with Saturation (MMX).
  inline void psubsw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubSW, &dst, &src); }

  //! @brief Packed Subtract with Unsigned Saturation (MMX).
  inline void psubusb(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSubUSB, &dst, &src); }
  //! @brief Packed Subtract with Unsigned Saturation (MMX).
  inline void psubusb(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubUSB, &dst, &src); }

  //! @brief Packed Subtract with Unsigned Saturation (MMX).
  inline void psubusw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSubUSW, &dst, &src); }
  //! @brief Packed Subtract with Unsigned Saturation (MMX).
  inline void psubusw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubUSW, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpckhbw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPunpckHBW, &dst, &src); }
  //! @brief Unpack High Packed Data (MMX).
  inline void punpckhbw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckHBW, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpckhwd(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPunpckHWD, &dst, &src); }
  //! @brief Unpack High Packed Data (MMX).
  inline void punpckhwd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckHWD, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpckhdq(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPunpckHDQ, &dst, &src); }
  //! @brief Unpack High Packed Data (MMX).
  inline void punpckhdq(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckHDQ, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpcklbw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPunpckLBW, &dst, &src); }
  //! @brief Unpack High Packed Data (MMX).
  inline void punpcklbw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckLBW, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpcklwd(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPunpckLWD, &dst, &src); }
  //! @brief Unpack High Packed Data (MMX).
  inline void punpcklwd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckLWD, &dst, &src); }

  //! @brief Unpack High Packed Data (MMX).
  inline void punpckldq(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPunpckLDQ, &dst, &src); }
  //! @brief Unpack High Packed Data (MMX).
  inline void punpckldq(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckLDQ, &dst, &src); }

  //! @brief Bitwise Exclusive OR (MMX).
  inline void pxor(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPXor, &dst, &src); }
  //! @brief Bitwise Exclusive OR (MMX).
  inline void pxor(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPXor, &dst, &src); }

  // -------------------------------------------------------------------------
  // [3dNow]
  // -------------------------------------------------------------------------

  //! @brief Faster EMMS (3dNow!).
  //!
  //! @note Use only for early AMD processors where is only 3dNow! or SSE. If
  //! CPU contains SSE2, it's better to use @c emms() ( @c femms() is mapped
  //! to @c emms() ).
  inline void femms()
  { _emitInstruction(kX86InstFEmms); }

  //! @brief Packed SP-FP to Integer Convert (3dNow!).
  inline void pf2id(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPF2ID, &dst, &src); }
  //! @brief Packed SP-FP to Integer Convert (3dNow!).
  inline void pf2id(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPF2ID, &dst, &src); }

  //! @brief  Packed SP-FP to Integer Word Convert (3dNow!).
  inline void pf2iw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPF2IW, &dst, &src); }
  //! @brief  Packed SP-FP to Integer Word Convert (3dNow!).
  inline void pf2iw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPF2IW, &dst, &src); }

  //! @brief Packed SP-FP Accumulate (3dNow!).
  inline void pfacc(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFAcc, &dst, &src); }
  //! @brief Packed SP-FP Accumulate (3dNow!).
  inline void pfacc(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFAcc, &dst, &src); }

  //! @brief Packed SP-FP Addition (3dNow!).
  inline void pfadd(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFAdd, &dst, &src); }
  //! @brief Packed SP-FP Addition (3dNow!).
  inline void pfadd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFAdd, &dst, &src); }

  //! @brief Packed SP-FP Compare - dst == src (3dNow!).
  inline void pfcmpeq(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFCmpEQ, &dst, &src); }
  //! @brief Packed SP-FP Compare - dst == src (3dNow!).
  inline void pfcmpeq(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFCmpEQ, &dst, &src); }

  //! @brief Packed SP-FP Compare - dst >= src (3dNow!).
  inline void pfcmpge(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFCmpGE, &dst, &src); }
  //! @brief Packed SP-FP Compare - dst >= src (3dNow!).
  inline void pfcmpge(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFCmpGE, &dst, &src); }

  //! @brief Packed SP-FP Compare - dst > src (3dNow!).
  inline void pfcmpgt(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFCmpGT, &dst, &src); }
  //! @brief Packed SP-FP Compare - dst > src (3dNow!).
  inline void pfcmpgt(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFCmpGT, &dst, &src); }

  //! @brief Packed SP-FP Maximum (3dNow!).
  inline void pfmax(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFMax, &dst, &src); }
  //! @brief Packed SP-FP Maximum (3dNow!).
  inline void pfmax(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFMax, &dst, &src); }

  //! @brief Packed SP-FP Minimum (3dNow!).
  inline void pfmin(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFMin, &dst, &src); }
  //! @brief Packed SP-FP Minimum (3dNow!).
  inline void pfmin(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFMin, &dst, &src); }

  //! @brief Packed SP-FP Multiply (3dNow!).
  inline void pfmul(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFMul, &dst, &src); }
  //! @brief Packed SP-FP Multiply (3dNow!).
  inline void pfmul(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFMul, &dst, &src); }

  //! @brief Packed SP-FP Negative Accumulate (3dNow!).
  inline void pfnacc(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFNAcc, &dst, &src); }
  //! @brief Packed SP-FP Negative Accumulate (3dNow!).
  inline void pfnacc(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFNAcc, &dst, &src); }

  //! @brief Packed SP-FP Mixed Accumulate (3dNow!).
  inline void pfpnacc(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFPNAcc, &dst, &src); }
  //! @brief Packed SP-FP Mixed Accumulate (3dNow!).
  inline void pfpnacc(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFPNAcc, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal Approximation (3dNow!).
  inline void pfrcp(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFRcp, &dst, &src); }
  //! @brief Packed SP-FP Reciprocal Approximation (3dNow!).
  inline void pfrcp(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFRcp, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal, First Iteration Step (3dNow!).
  inline void pfrcpit1(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFRcpIt1, &dst, &src); }
  //! @brief Packed SP-FP Reciprocal, First Iteration Step (3dNow!).
  inline void pfrcpit1(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFRcpIt1, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal, Second Iteration Step (3dNow!).
  inline void pfrcpit2(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFRcpIt2, &dst, &src); }
  //! @brief Packed SP-FP Reciprocal, Second Iteration Step (3dNow!).
  inline void pfrcpit2(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFRcpIt2, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal Square Root, First Iteration Step (3dNow!).
  inline void pfrsqit1(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFRSqIt1, &dst, &src); }
  //! @brief Packed SP-FP Reciprocal Square Root, First Iteration Step (3dNow!).
  inline void pfrsqit1(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFRSqIt1, &dst, &src); }

  //! @brief Packed SP-FP Reciprocal Square Root Approximation (3dNow!).
  inline void pfrsqrt(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFRSqrt, &dst, &src); }
  //! @brief Packed SP-FP Reciprocal Square Root Approximation (3dNow!).
  inline void pfrsqrt(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFRSqrt, &dst, &src); }

  //! @brief Packed SP-FP Subtract (3dNow!).
  inline void pfsub(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFSub, &dst, &src); }
  //! @brief Packed SP-FP Subtract (3dNow!).
  inline void pfsub(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFSub, &dst, &src); }

  //! @brief Packed SP-FP Reverse Subtract (3dNow!).
  inline void pfsubr(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPFSubR, &dst, &src); }
  //! @brief Packed SP-FP Reverse Subtract (3dNow!).
  inline void pfsubr(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPFSubR, &dst, &src); }

  //! @brief Packed DWords to SP-FP (3dNow!).
  inline void pi2fd(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPI2FD, &dst, &src); }
  //! @brief Packed DWords to SP-FP (3dNow!).
  inline void pi2fd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPI2FD, &dst, &src); }

  //! @brief Packed Words to SP-FP (3dNow!).
  inline void pi2fw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPI2FW, &dst, &src); }
  //! @brief Packed Words to SP-FP (3dNow!).
  inline void pi2fw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPI2FW, &dst, &src); }

  //! @brief Packed swap DWord (3dNow!)
  inline void pswapd(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSwapD, &dst, &src); }
  //! @brief Packed swap DWord (3dNow!)
  inline void pswapd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSwapD, &dst, &src); }

  // --------------------------------------------------------------------------
  // [SSE]
  // --------------------------------------------------------------------------

  //! @brief Packed SP-FP Add (SSE).
  inline void addps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstAddPS, &dst, &src); }
  //! @brief Packed SP-FP Add (SSE).
  inline void addps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstAddPS, &dst, &src); }

  //! @brief Scalar SP-FP Add (SSE).
  inline void addss(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstAddSS, &dst, &src); }
  //! @brief Scalar SP-FP Add (SSE).
  inline void addss(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstAddSS, &dst, &src); }

  //! @brief Bit-wise Logical And Not For SP-FP (SSE).
  inline void andnps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstAndnPS, &dst, &src); }
  //! @brief Bit-wise Logical And Not For SP-FP (SSE).
  inline void andnps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstAndnPS, &dst, &src); }

  //! @brief Bit-wise Logical And For SP-FP (SSE).
  inline void andps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstAndPS, &dst, &src); }
  //! @brief Bit-wise Logical And For SP-FP (SSE).
  inline void andps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstAndPS, &dst, &src); }

  //! @brief Packed SP-FP Compare (SSE).
  inline void cmpps(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpPS, &dst, &src, &imm8); }
  //! @brief Packed SP-FP Compare (SSE).
  inline void cmpps(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpPS, &dst, &src, &imm8); }

  //! @brief Compare Scalar SP-FP Values (SSE).
  inline void cmpss(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpSS, &dst, &src, &imm8); }
  //! @brief Compare Scalar SP-FP Values (SSE).
  inline void cmpss(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpSS, &dst, &src, &imm8); }

  //! @brief Scalar Ordered SP-FP Compare and Set EFLAGS (SSE).
  inline void comiss(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstComISS, &dst, &src); }
  //! @brief Scalar Ordered SP-FP Compare and Set EFLAGS (SSE).
  inline void comiss(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstComISS, &dst, &src); }

  //! @brief Packed Signed INT32 to Packed SP-FP Conversion (SSE).
  inline void cvtpi2ps(const XmmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstCvtPI2PS, &dst, &src); }
  //! @brief Packed Signed INT32 to Packed SP-FP Conversion (SSE).
  inline void cvtpi2ps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPI2PS, &dst, &src); }

  //! @brief Packed SP-FP to Packed INT32 Conversion (SSE).
  inline void cvtps2pi(const MmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvtPS2PI, &dst, &src); }
  //! @brief Packed SP-FP to Packed INT32 Conversion (SSE).
  inline void cvtps2pi(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPS2PI, &dst, &src); }

  //! @brief Scalar Signed INT32 to SP-FP Conversion (SSE).
  inline void cvtsi2ss(const XmmReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstCvtSI2SS, &dst, &src); }
  //! @brief Scalar Signed INT32 to SP-FP Conversion (SSE).
  inline void cvtsi2ss(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtSI2SS, &dst, &src); }

  //! @brief Scalar SP-FP to Signed INT32 Conversion (SSE).
  inline void cvtss2si(const GpReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvtSS2SI, &dst, &src); }
  //! @brief Scalar SP-FP to Signed INT32 Conversion (SSE).
  inline void cvtss2si(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtSS2SI, &dst, &src); }

  //! @brief Packed SP-FP to Packed INT32 Conversion (truncate) (SSE).
  inline void cvttps2pi(const MmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvttPS2PI, &dst, &src); }
  //! @brief Packed SP-FP to Packed INT32 Conversion (truncate) (SSE).
  inline void cvttps2pi(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvttPS2PI, &dst, &src); }

  //! @brief Scalar SP-FP to Signed INT32 Conversion (truncate) (SSE).
  inline void cvttss2si(const GpReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvttSS2SI, &dst, &src); }
  //! @brief Scalar SP-FP to Signed INT32 Conversion (truncate) (SSE).
  inline void cvttss2si(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvttSS2SI, &dst, &src); }

  //! @brief Packed SP-FP Divide (SSE).
  inline void divps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstDivPS, &dst, &src); }
  //! @brief Packed SP-FP Divide (SSE).
  inline void divps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstDivPS, &dst, &src); }

  //! @brief Scalar SP-FP Divide (SSE).
  inline void divss(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstDivSS, &dst, &src); }
  //! @brief Scalar SP-FP Divide (SSE).
  inline void divss(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstDivSS, &dst, &src); }

  //! @brief Load Streaming SIMD Extension Control/Status (SSE).
  inline void ldmxcsr(const Mem& src)
  { _emitInstruction(kX86InstLdMXCSR, &src); }

  //! @brief Byte Mask Write (SSE).
  //!
  //! @note The default memory location is specified by DS:EDI.
  inline void maskmovq(const MmReg& data, const MmReg& mask)
  { _emitInstruction(kX86InstMaskMovQ, &data, &mask); }

  //! @brief Packed SP-FP Maximum (SSE).
  inline void maxps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMaxPS, &dst, &src); }
  //! @brief Packed SP-FP Maximum (SSE).
  inline void maxps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMaxPS, &dst, &src); }

  //! @brief Scalar SP-FP Maximum (SSE).
  inline void maxss(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMaxSS, &dst, &src); }
  //! @brief Scalar SP-FP Maximum (SSE).
  inline void maxss(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMaxSS, &dst, &src); }

  //! @brief Packed SP-FP Minimum (SSE).
  inline void minps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMinPS, &dst, &src); }
  //! @brief Packed SP-FP Minimum (SSE).
  inline void minps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMinPS, &dst, &src); }

  //! @brief Scalar SP-FP Minimum (SSE).
  inline void minss(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMinSS, &dst, &src); }
  //! @brief Scalar SP-FP Minimum (SSE).
  inline void minss(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMinSS, &dst, &src); }

  //! @brief Move Aligned Packed SP-FP Values (SSE).
  inline void movaps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovAPS, &dst, &src); }
  //! @brief Move Aligned Packed SP-FP Values (SSE).
  inline void movaps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovAPS, &dst, &src); }

  //! @brief Move Aligned Packed SP-FP Values (SSE).
  inline void movaps(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovAPS, &dst, &src); }

  //! @brief Move DWord.
  inline void movd(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }
  //! @brief Move DWord.
  inline void movd(const GpReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }
  //! @brief Move DWord.
  inline void movd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }
  //! @brief Move DWord.
  inline void movd(const XmmReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstMovD, &dst, &src); }

  //! @brief Move QWord (SSE).
  inline void movq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
  //! @brief Move QWord (SSE).
  inline void movq(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
#if defined(ASMJIT_X64)
  //! @brief Move QWord (SSE).
  inline void movq(const GpReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
#endif // ASMJIT_X64
  //! @brief Move QWord (SSE).
  inline void movq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
#if defined(ASMJIT_X64)
  //! @brief Move QWord (SSE).
  inline void movq(const XmmReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstMovQ, &dst, &src); }
#endif // ASMJIT_X64

  //! @brief Move 64 Bits Non Temporal (SSE).
  inline void movntq(const Mem& dst, const MmReg& src)
  { _emitInstruction(kX86InstMovNTQ, &dst, &src); }

  //! @brief High to Low Packed SP-FP (SSE).
  inline void movhlps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovHLPS, &dst, &src); }

  //! @brief Move High Packed SP-FP (SSE).
  inline void movhps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovHPS, &dst, &src); }

  //! @brief Move High Packed SP-FP (SSE).
  inline void movhps(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovHPS, &dst, &src); }

  //! @brief Move Low to High Packed SP-FP (SSE).
  inline void movlhps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovLHPS, &dst, &src); }

  //! @brief Move Low Packed SP-FP (SSE).
  inline void movlps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovLPS, &dst, &src); }

  //! @brief Move Low Packed SP-FP (SSE).
  inline void movlps(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovLPS, &dst, &src); }

  //! @brief Move Aligned Four Packed SP-FP Non Temporal (SSE).
  inline void movntps(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovNTPS, &dst, &src); }

  //! @brief Move Scalar SP-FP (SSE).
  inline void movss(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovSS, &dst, &src); }

  //! @brief Move Scalar SP-FP (SSE).
  inline void movss(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovSS, &dst, &src); }

  //! @brief Move Scalar SP-FP (SSE).
  inline void movss(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovSS, &dst, &src); }

  //! @brief Move Unaligned Packed SP-FP Values (SSE).
  inline void movups(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovUPS, &dst, &src); }
  //! @brief Move Unaligned Packed SP-FP Values (SSE).
  inline void movups(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovUPS, &dst, &src); }

  //! @brief Move Unaligned Packed SP-FP Values (SSE).
  inline void movups(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovUPS, &dst, &src); }

  //! @brief Packed SP-FP Multiply (SSE).
  inline void mulps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMulPS, &dst, &src); }
  //! @brief Packed SP-FP Multiply (SSE).
  inline void mulps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMulPS, &dst, &src); }

  //! @brief Scalar SP-FP Multiply (SSE).
  inline void mulss(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMulSS, &dst, &src); }
  //! @brief Scalar SP-FP Multiply (SSE).
  inline void mulss(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMulSS, &dst, &src); }

  //! @brief Bit-wise Logical OR for SP-FP Data (SSE).
  inline void orps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstOrPS, &dst, &src); }
  //! @brief Bit-wise Logical OR for SP-FP Data (SSE).
  inline void orps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstOrPS, &dst, &src); }

  //! @brief Packed Average (SSE).
  inline void pavgb(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAvgB, &dst, &src); }
  //! @brief Packed Average (SSE).
  inline void pavgb(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAvgB, &dst, &src); }

  //! @brief Packed Average (SSE).
  inline void pavgw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAvgW, &dst, &src); }
  //! @brief Packed Average (SSE).
  inline void pavgw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAvgW, &dst, &src); }

  //! @brief Extract Word (SSE).
  inline void pextrw(const GpReg& dst, const MmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrW, &dst, &src, &imm8); }

  //! @brief Insert Word (SSE).
  inline void pinsrw(const MmReg& dst, const GpReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRW, &dst, &src, &imm8); }
  //! @brief Insert Word (SSE).
  inline void pinsrw(const MmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRW, &dst, &src, &imm8); }

  //! @brief Packed Signed Integer Word Maximum (SSE).
  inline void pmaxsw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPMaxSW, &dst, &src); }
  //! @brief Packed Signed Integer Word Maximum (SSE).
  inline void pmaxsw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxSW, &dst, &src); }

  //! @brief Packed Unsigned Integer Byte Maximum (SSE).
  inline void pmaxub(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPMaxUB, &dst, &src); }
  //! @brief Packed Unsigned Integer Byte Maximum (SSE).
  inline void pmaxub(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxUB, &dst, &src); }

  //! @brief Packed Signed Integer Word Minimum (SSE).
  inline void pminsw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPMinSW, &dst, &src); }
  //! @brief Packed Signed Integer Word Minimum (SSE).
  inline void pminsw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinSW, &dst, &src); }

  //! @brief Packed Unsigned Integer Byte Minimum (SSE).
  inline void pminub(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPMinUB, &dst, &src); }
  //! @brief Packed Unsigned Integer Byte Minimum (SSE).
  inline void pminub(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinUB, &dst, &src); }

  //! @brief Move Byte Mask To Integer (SSE).
  inline void pmovmskb(const GpReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPMovMskB, &dst, &src); }

  //! @brief Packed Multiply High Unsigned (SSE).
  inline void pmulhuw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPMulHUW, &dst, &src); }
  //! @brief Packed Multiply High Unsigned (SSE).
  inline void pmulhuw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulHUW, &dst, &src); }

  //! @brief Packed Sum of Absolute Differences (SSE).
  inline void psadbw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSADBW, &dst, &src); }
  //! @brief Packed Sum of Absolute Differences (SSE).
  inline void psadbw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSADBW, &dst, &src); }

  //! @brief Packed Shuffle word (SSE).
  inline void pshufw(const MmReg& dst, const MmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufW, &dst, &src, &imm8); }
  //! @brief Packed Shuffle word (SSE).
  inline void pshufw(const MmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufW, &dst, &src, &imm8); }

  //! @brief Packed SP-FP Reciprocal (SSE).
  inline void rcpps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstRcpPS, &dst, &src); }
  //! @brief Packed SP-FP Reciprocal (SSE).
  inline void rcpps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstRcpPS, &dst, &src); }

  //! @brief Scalar SP-FP Reciprocal (SSE).
  inline void rcpss(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstRcpSS, &dst, &src); }
  //! @brief Scalar SP-FP Reciprocal (SSE).
  inline void rcpss(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstRcpSS, &dst, &src); }

  //! @brief Prefetch (SSE).
  inline void prefetch(const Mem& mem, const Imm& hint)
  { _emitInstruction(kX86InstPrefetch, &mem, &hint); }

  //! @brief Compute Sum of Absolute Differences (SSE).
  inline void psadbw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSADBW, &dst, &src); }
  //! @brief Compute Sum of Absolute Differences (SSE).
  inline void psadbw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSADBW, &dst, &src); }

  //! @brief Packed SP-FP Square Root Reciprocal (SSE).
  inline void rsqrtps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstSqrtPS, &dst, &src); }
  //! @brief Packed SP-FP Square Root Reciprocal (SSE).
  inline void rsqrtps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstSqrtPS, &dst, &src); }

  //! @brief Scalar SP-FP Square Root Reciprocal (SSE).
  inline void rsqrtss(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstSqrtSS, &dst, &src); }
  //! @brief Scalar SP-FP Square Root Reciprocal (SSE).
  inline void rsqrtss(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstSqrtSS, &dst, &src); }

  //! @brief Store fence (SSE).
  inline void sfence()
  { _emitInstruction(kX86InstSFence); }

  //! @brief Shuffle SP-FP (SSE).
  inline void shufps(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstShufPS, &dst, &src, &imm8); }
  //! @brief Shuffle SP-FP (SSE).
  inline void shufps(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstShufPS, &dst, &src, &imm8); }

  //! @brief Packed SP-FP Square Root (SSE).
  inline void sqrtps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstSqrtPS, &dst, &src); }
  //! @brief Packed SP-FP Square Root (SSE).
  inline void sqrtps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstSqrtPS, &dst, &src); }

  //! @brief Scalar SP-FP Square Root (SSE).
  inline void sqrtss(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstSqrtSS, &dst, &src); }
  //! @brief Scalar SP-FP Square Root (SSE).
  inline void sqrtss(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstSqrtSS, &dst, &src); }

  //! @brief Store Streaming SIMD Extension Control/Status (SSE).
  inline void stmxcsr(const Mem& dst)
  { _emitInstruction(kX86InstStMXCSR, &dst); }

  //! @brief Packed SP-FP Subtract (SSE).
  inline void subps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstSubPS, &dst, &src); }
  //! @brief Packed SP-FP Subtract (SSE).
  inline void subps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstSubPS, &dst, &src); }

  //! @brief Scalar SP-FP Subtract (SSE).
  inline void subss(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstSubSS, &dst, &src); }
  //! @brief Scalar SP-FP Subtract (SSE).
  inline void subss(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstSubSS, &dst, &src); }

  //! @brief Unordered Scalar SP-FP compare and set EFLAGS (SSE).
  inline void ucomiss(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstUComISS, &dst, &src); }
  //! @brief Unordered Scalar SP-FP compare and set EFLAGS (SSE).
  inline void ucomiss(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstUComISS, &dst, &src); }

  //! @brief Unpack High Packed SP-FP Data (SSE).
  inline void unpckhps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstUnpckHPS, &dst, &src); }
  //! @brief Unpack High Packed SP-FP Data (SSE).
  inline void unpckhps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstUnpckHPS, &dst, &src); }

  //! @brief Unpack Low Packed SP-FP Data (SSE).
  inline void unpcklps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstUnpckLPS, &dst, &src); }
  //! @brief Unpack Low Packed SP-FP Data (SSE).
  inline void unpcklps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstUnpckLPS, &dst, &src); }

  //! @brief Bit-wise Logical Xor for SP-FP Data (SSE).
  inline void xorps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstXorPS, &dst, &src); }
  //! @brief Bit-wise Logical Xor for SP-FP Data (SSE).
  inline void xorps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstXorPS, &dst, &src); }

  // --------------------------------------------------------------------------
  // [SSE2]
  // --------------------------------------------------------------------------

  //! @brief Packed DP-FP Add (SSE2).
  inline void addpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstAddPD, &dst, &src); }
  //! @brief Packed DP-FP Add (SSE2).
  inline void addpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstAddPD, &dst, &src); }

  //! @brief Scalar DP-FP Add (SSE2).
  inline void addsd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstAddSD, &dst, &src); }
  //! @brief Scalar DP-FP Add (SSE2).
  inline void addsd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstAddSD, &dst, &src); }

  //! @brief Bit-wise Logical And Not For DP-FP (SSE2).
  inline void andnpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstAndnPD, &dst, &src); }
  //! @brief Bit-wise Logical And Not For DP-FP (SSE2).
  inline void andnpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstAndnPD, &dst, &src); }

  //! @brief Bit-wise Logical And For DP-FP (SSE2).
  inline void andpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstAndPD, &dst, &src); }
  //! @brief Bit-wise Logical And For DP-FP (SSE2).
  inline void andpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstAndPD, &dst, &src); }

  //! @brief Flush Cache Line (SSE2).
  inline void clflush(const Mem& mem)
  { _emitInstruction(kX86InstClFlush, &mem); }

  //! @brief Packed DP-FP Compare (SSE2).
  inline void cmppd(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpPD, &dst, &src, &imm8); }
  //! @brief Packed DP-FP Compare (SSE2).
  inline void cmppd(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpPD, &dst, &src, &imm8); }

  //! @brief Compare Scalar SP-FP Values (SSE2).
  inline void cmpsd(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpSD, &dst, &src, &imm8); }
  //! @brief Compare Scalar SP-FP Values (SSE2).
  inline void cmpsd(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstCmpSD, &dst, &src, &imm8); }

  //! @brief Scalar Ordered DP-FP Compare and Set EFLAGS (SSE2).
  inline void comisd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstComISD, &dst, &src); }
  //! @brief Scalar Ordered DP-FP Compare and Set EFLAGS (SSE2).
  inline void comisd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstComISD, &dst, &src); }

  //! @brief Convert Packed Dword Integers to Packed DP-FP Values (SSE2).
  inline void cvtdq2pd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvtDQ2PD, &dst, &src); }
  //! @brief Convert Packed Dword Integers to Packed DP-FP Values (SSE2).
  inline void cvtdq2pd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtDQ2PD, &dst, &src); }

  //! @brief Convert Packed Dword Integers to Packed SP-FP Values (SSE2).
  inline void cvtdq2ps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvtDQ2PS, &dst, &src); }
  //! @brief Convert Packed Dword Integers to Packed SP-FP Values (SSE2).
  inline void cvtdq2ps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtDQ2PS, &dst, &src); }

  //! @brief Convert Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvtpd2dq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvtPD2DQ, &dst, &src); }
  //! @brief Convert Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvtpd2dq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPD2DQ, &dst, &src); }

  //! @brief Convert Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvtpd2pi(const MmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvtPD2PI, &dst, &src); }
  //! @brief Convert Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvtpd2pi(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPD2PI, &dst, &src); }

  //! @brief Convert Packed DP-FP Values to Packed SP-FP Values (SSE2).
  inline void cvtpd2ps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvtPD2PS, &dst, &src); }
  //! @brief Convert Packed DP-FP Values to Packed SP-FP Values (SSE2).
  inline void cvtpd2ps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPD2PS, &dst, &src); }

  //! @brief Convert Packed Dword Integers to Packed DP-FP Values (SSE2).
  inline void cvtpi2pd(const XmmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstCvtPI2PD, &dst, &src); }
  //! @brief Convert Packed Dword Integers to Packed DP-FP Values (SSE2).
  inline void cvtpi2pd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPI2PD, &dst, &src); }

  //! @brief Convert Packed SP-FP Values to Packed Dword Integers (SSE2).
  inline void cvtps2dq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvtPS2DQ, &dst, &src); }
  //! @brief Convert Packed SP-FP Values to Packed Dword Integers (SSE2).
  inline void cvtps2dq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPS2DQ, &dst, &src); }

  //! @brief Convert Packed SP-FP Values to Packed DP-FP Values (SSE2).
  inline void cvtps2pd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvtPS2PD, &dst, &src); }
  //! @brief Convert Packed SP-FP Values to Packed DP-FP Values (SSE2).
  inline void cvtps2pd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtPS2PD, &dst, &src); }

  //! @brief Convert Scalar DP-FP Value to Dword Integer (SSE2).
  inline void cvtsd2si(const GpReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvtSD2SI, &dst, &src); }
  //! @brief Convert Scalar DP-FP Value to Dword Integer (SSE2).
  inline void cvtsd2si(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtSD2SI, &dst, &src); }

  //! @brief Convert Scalar DP-FP Value to Scalar SP-FP Value (SSE2).
  inline void cvtsd2ss(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvtSD2SS, &dst, &src); }
  //! @brief Convert Scalar DP-FP Value to Scalar SP-FP Value (SSE2).
  inline void cvtsd2ss(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtSD2SS, &dst, &src); }

  //! @brief Convert Dword Integer to Scalar DP-FP Value (SSE2).
  inline void cvtsi2sd(const XmmReg& dst, const GpReg& src)
  { _emitInstruction(kX86InstCvtSI2SD, &dst, &src); }
  //! @brief Convert Dword Integer to Scalar DP-FP Value (SSE2).
  inline void cvtsi2sd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtSI2SD, &dst, &src); }

  //! @brief Convert Scalar SP-FP Value to Scalar DP-FP Value (SSE2).
  inline void cvtss2sd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvtSS2SD, &dst, &src); }
  //! @brief Convert Scalar SP-FP Value to Scalar DP-FP Value (SSE2).
  inline void cvtss2sd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvtSS2SD, &dst, &src); }

  //! @brief Convert with Truncation Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvttpd2pi(const MmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvttPD2PI, &dst, &src); }
  //! @brief Convert with Truncation Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvttpd2pi(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvttPD2PI, &dst, &src); }

  //! @brief Convert with Truncation Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvttpd2dq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvttPD2DQ, &dst, &src); }
  //! @brief Convert with Truncation Packed DP-FP Values to Packed Dword Integers (SSE2).
  inline void cvttpd2dq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvttPD2DQ, &dst, &src); }

  //! @brief Convert with Truncation Packed SP-FP Values to Packed Dword Integers (SSE2).
  inline void cvttps2dq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvttPS2DQ, &dst, &src); }
  //! @brief Convert with Truncation Packed SP-FP Values to Packed Dword Integers (SSE2).
  inline void cvttps2dq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvttPS2DQ, &dst, &src); }

  //! @brief Convert with Truncation Scalar DP-FP Value to Signed Dword Integer (SSE2).
  inline void cvttsd2si(const GpReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstCvttSD2SI, &dst, &src); }
  //! @brief Convert with Truncation Scalar DP-FP Value to Signed Dword Integer (SSE2).
  inline void cvttsd2si(const GpReg& dst, const Mem& src)
  { _emitInstruction(kX86InstCvttSD2SI, &dst, &src); }

  //! @brief Packed DP-FP Divide (SSE2).
  inline void divpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstDivPD, &dst, &src); }
  //! @brief Packed DP-FP Divide (SSE2).
  inline void divpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstDivPD, &dst, &src); }

  //! @brief Scalar DP-FP Divide (SSE2).
  inline void divsd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstDivSD, &dst, &src); }
  //! @brief Scalar DP-FP Divide (SSE2).
  inline void divsd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstDivSD, &dst, &src); }

  //! @brief Load Fence (SSE2).
  inline void lfence()
  { _emitInstruction(kX86InstLFence); }

  //! @brief Store Selected Bytes of Double Quadword (SSE2).
  //!
  //! @note Target is DS:EDI.
  inline void maskmovdqu(const XmmReg& src, const XmmReg& mask)
  { _emitInstruction(kX86InstMaskMovDQU, &src, &mask); }

  //! @brief Return Maximum Packed Double-Precision FP Values (SSE2).
  inline void maxpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMaxPD, &dst, &src); }
  //! @brief Return Maximum Packed Double-Precision FP Values (SSE2).
  inline void maxpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMaxPD, &dst, &src); }

  //! @brief Return Maximum Scalar Double-Precision FP Value (SSE2).
  inline void maxsd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMaxSD, &dst, &src); }
  //! @brief Return Maximum Scalar Double-Precision FP Value (SSE2).
  inline void maxsd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMaxSD, &dst, &src); }

  //! @brief Memory Fence (SSE2).
  inline void mfence()
  { _emitInstruction(kX86InstMFence); }

  //! @brief Return Minimum Packed DP-FP Values (SSE2).
  inline void minpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMinPD, &dst, &src); }
  //! @brief Return Minimum Packed DP-FP Values (SSE2).
  inline void minpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMinPD, &dst, &src); }

  //! @brief Return Minimum Scalar DP-FP Value (SSE2).
  inline void minsd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMinSD, &dst, &src); }
  //! @brief Return Minimum Scalar DP-FP Value (SSE2).
  inline void minsd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMinSD, &dst, &src); }

  //! @brief Move Aligned DQWord (SSE2).
  inline void movdqa(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovDQA, &dst, &src); }
  //! @brief Move Aligned DQWord (SSE2).
  inline void movdqa(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovDQA, &dst, &src); }

  //! @brief Move Aligned DQWord (SSE2).
  inline void movdqa(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovDQA, &dst, &src); }

  //! @brief Move Unaligned Double Quadword (SSE2).
  inline void movdqu(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovDQU, &dst, &src); }
  //! @brief Move Unaligned Double Quadword (SSE2).
  inline void movdqu(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovDQU, &dst, &src); }

  //! @brief Move Unaligned Double Quadword (SSE2).
  inline void movdqu(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovDQU, &dst, &src); }

  //! @brief Extract Packed SP-FP Sign Mask (SSE2).
  inline void movmskps(const GpReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovMskPS, &dst, &src); }

  //! @brief Extract Packed DP-FP Sign Mask (SSE2).
  inline void movmskpd(const GpReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovMskPD, &dst, &src); }

  //! @brief Move Scalar Double-Precision FP Value (SSE2).
  inline void movsd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovSD, &dst, &src); }
  //! @brief Move Scalar Double-Precision FP Value (SSE2).
  inline void movsd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovSD, &dst, &src); }

  //! @brief Move Scalar Double-Precision FP Value (SSE2).
  inline void movsd(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovSD, &dst, &src); }

  //! @brief Move Aligned Packed Double-Precision FP Values (SSE2).
  inline void movapd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovAPD, &dst, &src); }

  //! @brief Move Aligned Packed Double-Precision FP Values (SSE2).
  inline void movapd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovAPD, &dst, &src); }

  //! @brief Move Aligned Packed Double-Precision FP Values (SSE2).
  inline void movapd(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovAPD, &dst, &src); }

  //! @brief Move Quadword from XMM to MMX Technology Register (SSE2).
  inline void movdq2q(const MmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovDQ2Q, &dst, &src); }

  //! @brief Move Quadword from MMX Technology to XMM Register (SSE2).
  inline void movq2dq(const XmmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstMovQ2DQ, &dst, &src); }

  //! @brief Move High Packed Double-Precision FP Value (SSE2).
  inline void movhpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovHPD, &dst, &src); }

  //! @brief Move High Packed Double-Precision FP Value (SSE2).
  inline void movhpd(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovHPD, &dst, &src); }

  //! @brief Move Low Packed Double-Precision FP Value (SSE2).
  inline void movlpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovLPD, &dst, &src); }

  //! @brief Move Low Packed Double-Precision FP Value (SSE2).
  inline void movlpd(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovLPD, &dst, &src); }

  //! @brief Store Double Quadword Using Non-Temporal Hint (SSE2).
  inline void movntdq(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovNTDQ, &dst, &src); }

  //! @brief Store Store DWORD Using Non-Temporal Hint (SSE2).
  inline void movnti(const Mem& dst, const GpReg& src)
  { _emitInstruction(kX86InstMovNTI, &dst, &src); }

  //! @brief Store Packed Double-Precision FP Values Using Non-Temporal Hint (SSE2).
  inline void movntpd(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovNTPD, &dst, &src); }

  //! @brief Move Unaligned Packed Double-Precision FP Values (SSE2).
  inline void movupd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovUPD, &dst, &src); }

  //! @brief Move Unaligned Packed Double-Precision FP Values (SSE2).
  inline void movupd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovUPD, &dst, &src); }

  //! @brief Move Unaligned Packed Double-Precision FP Values (SSE2).
  inline void movupd(const Mem& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovUPD, &dst, &src); }

  //! @brief Packed DP-FP Multiply (SSE2).
  inline void mulpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMulPD, &dst, &src); }
  //! @brief Packed DP-FP Multiply (SSE2).
  inline void mulpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMulPD, &dst, &src); }

  //! @brief Scalar DP-FP Multiply (SSE2).
  inline void mulsd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMulSD, &dst, &src); }
  //! @brief Scalar DP-FP Multiply (SSE2).
  inline void mulsd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMulSD, &dst, &src); }

  //! @brief Bit-wise Logical OR for DP-FP Data (SSE2).
  inline void orpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstOrPD, &dst, &src); }
  //! @brief Bit-wise Logical OR for DP-FP Data (SSE2).
  inline void orpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstOrPD, &dst, &src); }

  //! @brief Pack with Signed Saturation (SSE2).
  inline void packsswb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPackSSWB, &dst, &src); }
  //! @brief Pack with Signed Saturation (SSE2).
  inline void packsswb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPackSSWB, &dst, &src); }

  //! @brief Pack with Signed Saturation (SSE2).
  inline void packssdw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPackSSDW, &dst, &src); }
  //! @brief Pack with Signed Saturation (SSE2).
  inline void packssdw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPackSSDW, &dst, &src); }

  //! @brief Pack with Unsigned Saturation (SSE2).
  inline void packuswb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPackUSWB, &dst, &src); }
  //! @brief Pack with Unsigned Saturation (SSE2).
  inline void packuswb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPackUSWB, &dst, &src); }

  //! @brief Packed BYTE Add (SSE2).
  inline void paddb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAddB, &dst, &src); }
  //! @brief Packed BYTE Add (SSE2).
  inline void paddb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddB, &dst, &src); }

  //! @brief Packed WORD Add (SSE2).
  inline void paddw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAddW, &dst, &src); }
  //! @brief Packed WORD Add (SSE2).
  inline void paddw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddW, &dst, &src); }

  //! @brief Packed DWORD Add (SSE2).
  inline void paddd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAddD, &dst, &src); }
  //! @brief Packed DWORD Add (SSE2).
  inline void paddd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddD, &dst, &src); }

  //! @brief Packed QWORD Add (SSE2).
  inline void paddq(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAddQ, &dst, &src); }
  //! @brief Packed QWORD Add (SSE2).
  inline void paddq(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddQ, &dst, &src); }

  //! @brief Packed QWORD Add (SSE2).
  inline void paddq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAddQ, &dst, &src); }
  //! @brief Packed QWORD Add (SSE2).
  inline void paddq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddQ, &dst, &src); }

  //! @brief Packed Add with Saturation (SSE2).
  inline void paddsb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAddSB, &dst, &src); }
  //! @brief Packed Add with Saturation (SSE2).
  inline void paddsb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddSB, &dst, &src); }

  //! @brief Packed Add with Saturation (SSE2).
  inline void paddsw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAddSW, &dst, &src); }
  //! @brief Packed Add with Saturation (SSE2).
  inline void paddsw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddSW, &dst, &src); }

  //! @brief Packed Add Unsigned with Saturation (SSE2).
  inline void paddusb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAddUSB, &dst, &src); }
  //! @brief Packed Add Unsigned with Saturation (SSE2).
  inline void paddusb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddUSB, &dst, &src); }

  //! @brief Packed Add Unsigned with Saturation (SSE2).
  inline void paddusw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAddUSW, &dst, &src); }
  //! @brief Packed Add Unsigned with Saturation (SSE2).
  inline void paddusw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAddUSW, &dst, &src); }

  //! @brief Logical AND (SSE2).
  inline void pand(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAnd, &dst, &src); }
  //! @brief Logical AND (SSE2).
  inline void pand(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAnd, &dst, &src); }

  //! @brief Logical AND Not (SSE2).
  inline void pandn(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAndN, &dst, &src); }
  //! @brief Logical AND Not (SSE2).
  inline void pandn(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAndN, &dst, &src); }

  //! @brief Spin Loop Hint (SSE2).
  inline void pause()
  { _emitInstruction(kX86InstPause); }

  //! @brief Packed Average (SSE2).
  inline void pavgb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAvgB, &dst, &src); }
  //! @brief Packed Average (SSE2).
  inline void pavgb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAvgB, &dst, &src); }

  //! @brief Packed Average (SSE2).
  inline void pavgw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAvgW, &dst, &src); }
  //! @brief Packed Average (SSE2).
  inline void pavgw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAvgW, &dst, &src); }

  //! @brief Packed Compare for Equal (BYTES) (SSE2).
  inline void pcmpeqb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPCmpEqB, &dst, &src); }
  //! @brief Packed Compare for Equal (BYTES) (SSE2).
  inline void pcmpeqb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpEqB, &dst, &src); }

  //! @brief Packed Compare for Equal (WORDS) (SSE2).
  inline void pcmpeqw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPCmpEqW, &dst, &src); }
  //! @brief Packed Compare for Equal (WORDS) (SSE2).
  inline void pcmpeqw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpEqW, &dst, &src); }

  //! @brief Packed Compare for Equal (DWORDS) (SSE2).
  inline void pcmpeqd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPCmpEqD, &dst, &src); }
  //! @brief Packed Compare for Equal (DWORDS) (SSE2).
  inline void pcmpeqd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpEqD, &dst, &src); }

  //! @brief Packed Compare for Greater Than (BYTES) (SSE2).
  inline void pcmpgtb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPCmpGtB, &dst, &src); }
  //! @brief Packed Compare for Greater Than (BYTES) (SSE2).
  inline void pcmpgtb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpGtB, &dst, &src); }

  //! @brief Packed Compare for Greater Than (WORDS) (SSE2).
  inline void pcmpgtw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPCmpGtW, &dst, &src); }
  //! @brief Packed Compare for Greater Than (WORDS) (SSE2).
  inline void pcmpgtw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpGtW, &dst, &src); }

  //! @brief Packed Compare for Greater Than (DWORDS) (SSE2).
  inline void pcmpgtd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPCmpGtD, &dst, &src); }
  //! @brief Packed Compare for Greater Than (DWORDS) (SSE2).
  inline void pcmpgtd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpGtD, &dst, &src); }

  //! @brief Packed Signed Integer Word Maximum (SSE2).
  inline void pmaxsw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMaxSW, &dst, &src); }
  //! @brief Packed Signed Integer Word Maximum (SSE2).
  inline void pmaxsw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxSW, &dst, &src); }

  //! @brief Packed Unsigned Integer Byte Maximum (SSE2).
  inline void pmaxub(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMaxUB, &dst, &src); }
  //! @brief Packed Unsigned Integer Byte Maximum (SSE2).
  inline void pmaxub(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxUB, &dst, &src); }

  //! @brief Packed Signed Integer Word Minimum (SSE2).
  inline void pminsw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMinSW, &dst, &src); }
  //! @brief Packed Signed Integer Word Minimum (SSE2).
  inline void pminsw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinSW, &dst, &src); }

  //! @brief Packed Unsigned Integer Byte Minimum (SSE2).
  inline void pminub(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMinUB, &dst, &src); }
  //! @brief Packed Unsigned Integer Byte Minimum (SSE2).
  inline void pminub(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinUB, &dst, &src); }

  //! @brief Move Byte Mask (SSE2).
  inline void pmovmskb(const GpReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMovMskB, &dst, &src); }

  //! @brief Packed Multiply High (SSE2).
  inline void pmulhw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMulHW, &dst, &src); }
  //! @brief Packed Multiply High (SSE2).
  inline void pmulhw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulHW, &dst, &src); }

  //! @brief Packed Multiply High Unsigned (SSE2).
  inline void pmulhuw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMulHUW, &dst, &src); }
  //! @brief Packed Multiply High Unsigned (SSE2).
  inline void pmulhuw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulHUW, &dst, &src); }

  //! @brief Packed Multiply Low (SSE2).
  inline void pmullw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMulLW, &dst, &src); }
  //! @brief Packed Multiply Low (SSE2).
  inline void pmullw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulLW, &dst, &src); }

  //! @brief Packed Multiply to QWORD (SSE2).
  inline void pmuludq(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPMulUDQ, &dst, &src); }
  //! @brief Packed Multiply to QWORD (SSE2).
  inline void pmuludq(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulUDQ, &dst, &src); }

  //! @brief Packed Multiply to QWORD (SSE2).
  inline void pmuludq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMulUDQ, &dst, &src); }
  //! @brief Packed Multiply to QWORD (SSE2).
  inline void pmuludq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulUDQ, &dst, &src); }

  //! @brief Bitwise Logical OR (SSE2).
  inline void por(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPOr, &dst, &src); }
  //! @brief Bitwise Logical OR (SSE2).
  inline void por(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPOr, &dst, &src); }

  //! @brief Packed Shift Left Logical (SSE2).
  inline void pslld(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSllD, &dst, &src); }
  //! @brief Packed Shift Left Logical (SSE2).
  inline void pslld(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSllD, &dst, &src); }
  //! @brief Packed Shift Left Logical (SSE2).
  inline void pslld(const XmmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSllD, &dst, &src); }

  //! @brief Packed Shift Left Logical (SSE2).
  inline void psllq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSllQ, &dst, &src); }
  //! @brief Packed Shift Left Logical (SSE2).
  inline void psllq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSllQ, &dst, &src); }
  //! @brief Packed Shift Left Logical (SSE2).
  inline void psllq(const XmmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSllQ, &dst, &src); }

  //! @brief Packed Shift Left Logical (SSE2).
  inline void psllw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSllW, &dst, &src); }
  //! @brief Packed Shift Left Logical (SSE2).
  inline void psllw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSllW, &dst, &src); }
  //! @brief Packed Shift Left Logical (SSE2).
  inline void psllw(const XmmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSllW, &dst, &src); }

  //! @brief Packed Shift Left Logical (SSE2).
  inline void pslldq(const XmmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSllDQ, &dst, &src); }

  //! @brief Packed Shift Right Arithmetic (SSE2).
  inline void psrad(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSraD, &dst, &src); }
  //! @brief Packed Shift Right Arithmetic (SSE2).
  inline void psrad(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSraD, &dst, &src); }
  //! @brief Packed Shift Right Arithmetic (SSE2).
  inline void psrad(const XmmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSraD, &dst, &src); }

  //! @brief Packed Shift Right Arithmetic (SSE2).
  inline void psraw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSraW, &dst, &src); }
  //! @brief Packed Shift Right Arithmetic (SSE2).
  inline void psraw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSraW, &dst, &src); }
  //! @brief Packed Shift Right Arithmetic (SSE2).
  inline void psraw(const XmmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSraW, &dst, &src); }

  //! @brief Packed Subtract (SSE2).
  inline void psubb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSubB, &dst, &src); }
  //! @brief Packed Subtract (SSE2).
  inline void psubb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubB, &dst, &src); }

  //! @brief Packed Subtract (SSE2).
  inline void psubw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSubW, &dst, &src); }
  //! @brief Packed Subtract (SSE2).
  inline void psubw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubW, &dst, &src); }

  //! @brief Packed Subtract (SSE2).
  inline void psubd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSubD, &dst, &src); }
  //! @brief Packed Subtract (SSE2).
  inline void psubd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubD, &dst, &src); }

  //! @brief Packed Subtract (SSE2).
  inline void psubq(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSubQ, &dst, &src); }
  //! @brief Packed Subtract (SSE2).
  inline void psubq(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubQ, &dst, &src); }

  //! @brief Packed Subtract (SSE2).
  inline void psubq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSubQ, &dst, &src); }
  //! @brief Packed Subtract (SSE2).
  inline void psubq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubQ, &dst, &src); }

  //! @brief Packed Multiply and Add (SSE2).
  inline void pmaddwd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMAddWD, &dst, &src); }
  //! @brief Packed Multiply and Add (SSE2).
  inline void pmaddwd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMAddWD, &dst, &src); }

  //! @brief Shuffle Packed DWORDs (SSE2).
  inline void pshufd(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufD, &dst, &src, &imm8); }
  //! @brief Shuffle Packed DWORDs (SSE2).
  inline void pshufd(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufD, &dst, &src, &imm8); }

  //! @brief Shuffle Packed High Words (SSE2).
  inline void pshufhw(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufHW, &dst, &src, &imm8); }
  //! @brief Shuffle Packed High Words (SSE2).
  inline void pshufhw(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufHW, &dst, &src, &imm8); }

  //! @brief Shuffle Packed Low Words (SSE2).
  inline void pshuflw(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufLW, &dst, &src, &imm8); }
  //! @brief Shuffle Packed Low Words (SSE2).
  inline void pshuflw(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPShufLW, &dst, &src, &imm8); }

  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrld(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSrlD, &dst, &src); }
  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrld(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSrlD, &dst, &src); }
  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrld(const XmmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSrlD, &dst, &src); }

  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrlq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSrlQ, &dst, &src); }
  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrlq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSrlQ, &dst, &src); }
  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrlq(const XmmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSrlQ, &dst, &src); }

  //! @brief DQWord Shift Right Logical (MMX).
  inline void psrldq(const XmmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSrlDQ, &dst, &src); }

  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrlw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSrlW, &dst, &src); }
  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrlw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSrlW, &dst, &src); }
  //! @brief Packed Shift Right Logical (SSE2).
  inline void psrlw(const XmmReg& dst, const Imm& src)
  { _emitInstruction(kX86InstPSrlW, &dst, &src); }

  //! @brief Packed Subtract with Saturation (SSE2).
  inline void psubsb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSubSB, &dst, &src); }
  //! @brief Packed Subtract with Saturation (SSE2).
  inline void psubsb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubSB, &dst, &src); }

  //! @brief Packed Subtract with Saturation (SSE2).
  inline void psubsw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSubSW, &dst, &src); }
  //! @brief Packed Subtract with Saturation (SSE2).
  inline void psubsw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubSW, &dst, &src); }

  //! @brief Packed Subtract with Unsigned Saturation (SSE2).
  inline void psubusb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSubUSB, &dst, &src); }
  //! @brief Packed Subtract with Unsigned Saturation (SSE2).
  inline void psubusb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubUSB, &dst, &src); }

  //! @brief Packed Subtract with Unsigned Saturation (SSE2).
  inline void psubusw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSubUSW, &dst, &src); }
  //! @brief Packed Subtract with Unsigned Saturation (SSE2).
  inline void psubusw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSubUSW, &dst, &src); }

  //! @brief Unpack High Data (SSE2).
  inline void punpckhbw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPunpckHBW, &dst, &src); }
  //! @brief Unpack High Data (SSE2).
  inline void punpckhbw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckHBW, &dst, &src); }

  //! @brief Unpack High Data (SSE2).
  inline void punpckhwd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPunpckHWD, &dst, &src); }
  //! @brief Unpack High Data (SSE2).
  inline void punpckhwd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckHWD, &dst, &src); }

  //! @brief Unpack High Data (SSE2).
  inline void punpckhdq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPunpckHDQ, &dst, &src); }
  //! @brief Unpack High Data (SSE2).
  inline void punpckhdq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckHDQ, &dst, &src); }

  //! @brief Unpack High Data (SSE2).
  inline void punpckhqdq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPunpckHQDQ, &dst, &src); }
  //! @brief Unpack High Data (SSE2).
  inline void punpckhqdq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckHQDQ, &dst, &src); }

  //! @brief Unpack Low Data (SSE2).
  inline void punpcklbw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPunpckLBW, &dst, &src); }
  //! @brief Unpack Low Data (SSE2).
  inline void punpcklbw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckLBW, &dst, &src); }

  //! @brief Unpack Low Data (SSE2).
  inline void punpcklwd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPunpckLWD, &dst, &src); }
  //! @brief Unpack Low Data (SSE2).
  inline void punpcklwd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckLWD, &dst, &src); }

  //! @brief Unpack Low Data (SSE2).
  inline void punpckldq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPunpckLDQ, &dst, &src); }
  //! @brief Unpack Low Data (SSE2).
  inline void punpckldq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckLDQ, &dst, &src); }

  //! @brief Unpack Low Data (SSE2).
  inline void punpcklqdq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPunpckLQDQ, &dst, &src); }
  //! @brief Unpack Low Data (SSE2).
  inline void punpcklqdq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPunpckLQDQ, &dst, &src); }

  //! @brief Bitwise Exclusive OR (SSE2).
  inline void pxor(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPXor, &dst, &src); }
  //! @brief Bitwise Exclusive OR (SSE2).
  inline void pxor(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPXor, &dst, &src); }

  //! @brief Shuffle DP-FP (SSE2).
  inline void shufpd(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstShufPD, &dst, &src, &imm8); }
  //! @brief Shuffle DP-FP (SSE2).
  inline void shufpd(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstShufPD, &dst, &src, &imm8); }

  //! @brief Compute Square Roots of Packed DP-FP Values (SSE2).
  inline void sqrtpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstSqrtPD, &dst, &src); }
  //! @brief Compute Square Roots of Packed DP-FP Values (SSE2).
  inline void sqrtpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstSqrtPD, &dst, &src); }

  //! @brief Compute Square Root of Scalar DP-FP Value (SSE2).
  inline void sqrtsd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstSqrtSD, &dst, &src); }
  //! @brief Compute Square Root of Scalar DP-FP Value (SSE2).
  inline void sqrtsd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstSqrtSD, &dst, &src); }

  //! @brief Packed DP-FP Subtract (SSE2).
  inline void subpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstSubPD, &dst, &src); }
  //! @brief Packed DP-FP Subtract (SSE2).
  inline void subpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstSubPD, &dst, &src); }

  //! @brief Scalar DP-FP Subtract (SSE2).
  inline void subsd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstSubSD, &dst, &src); }
  //! @brief Scalar DP-FP Subtract (SSE2).
  inline void subsd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstSubSD, &dst, &src); }

  //! @brief Scalar Unordered DP-FP Compare and Set EFLAGS (SSE2).
  inline void ucomisd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstUComISD, &dst, &src); }
  //! @brief Scalar Unordered DP-FP Compare and Set EFLAGS (SSE2).
  inline void ucomisd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstUComISD, &dst, &src); }

  //! @brief Unpack and Interleave High Packed Double-Precision FP Values (SSE2).
  inline void unpckhpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstUnpckHPD, &dst, &src); }
  //! @brief Unpack and Interleave High Packed Double-Precision FP Values (SSE2).
  inline void unpckhpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstUnpckHPD, &dst, &src); }

  //! @brief Unpack and Interleave Low Packed Double-Precision FP Values (SSE2).
  inline void unpcklpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstUnpckLPD, &dst, &src); }
  //! @brief Unpack and Interleave Low Packed Double-Precision FP Values (SSE2).
  inline void unpcklpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstUnpckLPD, &dst, &src); }

  //! @brief Bit-wise Logical OR for DP-FP Data (SSE2).
  inline void xorpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstXorPD, &dst, &src); }
  //! @brief Bit-wise Logical OR for DP-FP Data (SSE2).
  inline void xorpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstXorPD, &dst, &src); }

  // --------------------------------------------------------------------------
  // [SSE3]
  // --------------------------------------------------------------------------

  //! @brief Packed DP-FP Add/Subtract (SSE3).
  inline void addsubpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstAddSubPD, &dst, &src); }
  //! @brief Packed DP-FP Add/Subtract (SSE3).
  inline void addsubpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstAddSubPD, &dst, &src); }

  //! @brief Packed SP-FP Add/Subtract (SSE3).
  inline void addsubps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstAddSubPS, &dst, &src); }
  //! @brief Packed SP-FP Add/Subtract (SSE3).
  inline void addsubps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstAddSubPS, &dst, &src); }

  //! @brief Store Integer with Truncation (SSE3).
  inline void fisttp(const Mem& dst)
  { _emitInstruction(kX86InstFISttP, &dst); }

  //! @brief Packed DP-FP Horizontal Add (SSE3).
  inline void haddpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstHAddPD, &dst, &src); }
  //! @brief Packed DP-FP Horizontal Add (SSE3).
  inline void haddpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstHAddPD, &dst, &src); }

  //! @brief Packed SP-FP Horizontal Add (SSE3).
  inline void haddps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstHAddPS, &dst, &src); }
  //! @brief Packed SP-FP Horizontal Add (SSE3).
  inline void haddps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstHAddPS, &dst, &src); }

  //! @brief Packed DP-FP Horizontal Subtract (SSE3).
  inline void hsubpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstHSubPD, &dst, &src); }
  //! @brief Packed DP-FP Horizontal Subtract (SSE3).
  inline void hsubpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstHSubPD, &dst, &src); }

  //! @brief Packed SP-FP Horizontal Subtract (SSE3).
  inline void hsubps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstHSubPS, &dst, &src); }
  //! @brief Packed SP-FP Horizontal Subtract (SSE3).
  inline void hsubps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstHSubPS, &dst, &src); }

  //! @brief Load Unaligned Integer 128 Bits (SSE3).
  inline void lddqu(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstLdDQU, &dst, &src); }

  //! @brief Set Up Monitor Address (SSE3).
  inline void monitor()
  { _emitInstruction(kX86InstMonitor); }

  //! @brief Move One DP-FP and Duplicate (SSE3).
  inline void movddup(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovDDup, &dst, &src); }
  //! @brief Move One DP-FP and Duplicate (SSE3).
  inline void movddup(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovDDup, &dst, &src); }

  //! @brief Move Packed SP-FP High and Duplicate (SSE3).
  inline void movshdup(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovSHDup, &dst, &src); }
  //! @brief Move Packed SP-FP High and Duplicate (SSE3).
  inline void movshdup(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovSHDup, &dst, &src); }

  //! @brief Move Packed SP-FP Low and Duplicate (SSE3).
  inline void movsldup(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstMovSLDup, &dst, &src); }
  //! @brief Move Packed SP-FP Low and Duplicate (SSE3).
  inline void movsldup(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovSLDup, &dst, &src); }

  //! @brief Monitor Wait (SSE3).
  inline void mwait()
  { _emitInstruction(kX86InstMWait); }

  // --------------------------------------------------------------------------
  // [SSSE3]
  // --------------------------------------------------------------------------

  //! @brief Packed SIGN (SSSE3).
  inline void psignb(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSignB, &dst, &src); }
  //! @brief Packed SIGN (SSSE3).
  inline void psignb(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSignB, &dst, &src); }

  //! @brief Packed SIGN (SSSE3).
  inline void psignb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSignB, &dst, &src); }
  //! @brief Packed SIGN (SSSE3).
  inline void psignb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSignB, &dst, &src); }

  //! @brief Packed SIGN (SSSE3).
  inline void psignw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSignW, &dst, &src); }
  //! @brief Packed SIGN (SSSE3).
  inline void psignw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSignW, &dst, &src); }

  //! @brief Packed SIGN (SSSE3).
  inline void psignw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSignW, &dst, &src); }
  //! @brief Packed SIGN (SSSE3).
  inline void psignw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSignW, &dst, &src); }

  //! @brief Packed SIGN (SSSE3).
  inline void psignd(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPSignD, &dst, &src); }
  //! @brief Packed SIGN (SSSE3).
  inline void psignd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSignD, &dst, &src); }

  //! @brief Packed SIGN (SSSE3).
  inline void psignd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPSignD, &dst, &src); }
  //! @brief Packed SIGN (SSSE3).
  inline void psignd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPSignD, &dst, &src); }

  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPHAddW, &dst, &src); }
  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPHAddW, &dst, &src); }

  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPHAddW, &dst, &src); }
  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPHAddW, &dst, &src); }

  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddd(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPHAddD, &dst, &src); }
  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPHAddD, &dst, &src); }

  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPHAddD, &dst, &src); }
  //! @brief Packed Horizontal Add (SSSE3).
  inline void phaddd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPHAddD, &dst, &src); }

  //! @brief Packed Horizontal Add and Saturate (SSSE3).
  inline void phaddsw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPHAddSW, &dst, &src); }
  //! @brief Packed Horizontal Add and Saturate (SSSE3).
  inline void phaddsw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPHAddSW, &dst, &src); }

  //! @brief Packed Horizontal Add and Saturate (SSSE3).
  inline void phaddsw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPHAddSW, &dst, &src); }
  //! @brief Packed Horizontal Add and Saturate (SSSE3).
  inline void phaddsw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPHAddSW, &dst, &src); }

  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPHSubW, &dst, &src); }
  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPHSubW, &dst, &src); }

  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPHSubW, &dst, &src); }
  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPHSubW, &dst, &src); }

  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubd(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPHSubD, &dst, &src); }
  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPHSubD, &dst, &src); }

  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPHSubD, &dst, &src); }
  //! @brief Packed Horizontal Subtract (SSSE3).
  inline void phsubd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPHSubD, &dst, &src); }

  //! @brief Packed Horizontal Subtract and Saturate (SSSE3).
  inline void phsubsw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPHSubSW, &dst, &src); }
  //! @brief Packed Horizontal Subtract and Saturate (SSSE3).
  inline void phsubsw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPHSubSW, &dst, &src); }

  //! @brief Packed Horizontal Subtract and Saturate (SSSE3).
  inline void phsubsw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPHSubSW, &dst, &src); }
  //! @brief Packed Horizontal Subtract and Saturate (SSSE3).
  inline void phsubsw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPHSubSW, &dst, &src); }

  //! @brief Multiply and Add Packed Signed and Unsigned Bytes (SSSE3).
  inline void pmaddubsw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPMAddUBSW, &dst, &src); }
  //! @brief Multiply and Add Packed Signed and Unsigned Bytes (SSSE3).
  inline void pmaddubsw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMAddUBSW, &dst, &src); }

  //! @brief Multiply and Add Packed Signed and Unsigned Bytes (SSSE3).
  inline void pmaddubsw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMAddUBSW, &dst, &src); }
  //! @brief Multiply and Add Packed Signed and Unsigned Bytes (SSSE3).
  inline void pmaddubsw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMAddUBSW, &dst, &src); }

  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsb(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAbsB, &dst, &src); }
  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsb(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAbsB, &dst, &src); }

  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAbsB, &dst, &src); }
  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAbsB, &dst, &src); }

  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAbsW, &dst, &src); }
  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAbsW, &dst, &src); }

  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAbsW, &dst, &src); }
  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAbsW, &dst, &src); }

  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsd(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPAbsD, &dst, &src); }
  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsd(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAbsD, &dst, &src); }

  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPAbsD, &dst, &src); }
  //! @brief Packed Absolute Value (SSSE3).
  inline void pabsd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPAbsD, &dst, &src); }

  //! @brief Packed Multiply High with Round and Scale (SSSE3).
  inline void pmulhrsw(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPMulHRSW, &dst, &src); }
  //! @brief Packed Multiply High with Round and Scale (SSSE3).
  inline void pmulhrsw(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulHRSW, &dst, &src); }

  //! @brief Packed Multiply High with Round and Scale (SSSE3).
  inline void pmulhrsw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMulHRSW, &dst, &src); }
  //! @brief Packed Multiply High with Round and Scale (SSSE3).
  inline void pmulhrsw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulHRSW, &dst, &src); }

  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void pshufb(const MmReg& dst, const MmReg& src)
  { _emitInstruction(kX86InstPShufB, &dst, &src); }
  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void pshufb(const MmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPShufB, &dst, &src); }

  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void pshufb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPShufB, &dst, &src); }
  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void pshufb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPShufB, &dst, &src); }

  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void palignr(const MmReg& dst, const MmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPAlignR, &dst, &src, &imm8); }
  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void palignr(const MmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPAlignR, &dst, &src, &imm8); }

  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void palignr(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPAlignR, &dst, &src, &imm8); }
  //! @brief Packed Shuffle Bytes (SSSE3).
  inline void palignr(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPAlignR, &dst, &src, &imm8); }

  // --------------------------------------------------------------------------
  // [SSE4.1]
  // --------------------------------------------------------------------------

  //! @brief Blend Packed DP-FP Values (SSE4.1).
  inline void blendpd(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstBlendPD, &dst, &src, &imm8); }
  //! @brief Blend Packed DP-FP Values (SSE4.1).
  inline void blendpd(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstBlendPD, &dst, &src, &imm8); }

  //! @brief Blend Packed SP-FP Values (SSE4.1).
  inline void blendps(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstBlendPS, &dst, &src, &imm8); }
  //! @brief Blend Packed SP-FP Values (SSE4.1).
  inline void blendps(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstBlendPS, &dst, &src, &imm8); }

  //! @brief Variable Blend Packed DP-FP Values (SSE4.1).
  inline void blendvpd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstBlendVPD, &dst, &src); }
  //! @brief Variable Blend Packed DP-FP Values (SSE4.1).
  inline void blendvpd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstBlendVPD, &dst, &src); }

  //! @brief Variable Blend Packed SP-FP Values (SSE4.1).
  inline void blendvps(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstBlendVPS, &dst, &src); }
  //! @brief Variable Blend Packed SP-FP Values (SSE4.1).
  inline void blendvps(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstBlendVPS, &dst, &src); }

  //! @brief Dot Product of Packed DP-FP Values (SSE4.1).
  inline void dppd(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstDpPD, &dst, &src, &imm8); }
  //! @brief Dot Product of Packed DP-FP Values (SSE4.1).
  inline void dppd(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstDpPD, &dst, &src, &imm8); }

  //! @brief Dot Product of Packed SP-FP Values (SSE4.1).
  inline void dpps(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstDpPS, &dst, &src, &imm8); }
  //! @brief Dot Product of Packed SP-FP Values (SSE4.1).
  inline void dpps(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstDpPS, &dst, &src, &imm8); }

  //! @brief Extract Packed SP-FP Value (SSE4.1).
  inline void extractps(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstExtractPS, &dst, &src, &imm8); }
  //! @brief Extract Packed SP-FP Value (SSE4.1).
  inline void extractps(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstExtractPS, &dst, &src, &imm8); }

  //! @brief Load Double Quadword Non-Temporal Aligned Hint (SSE4.1).
  inline void movntdqa(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstMovNTDQA, &dst, &src); }

  //! @brief Compute Multiple Packed Sums of Absolute Difference (SSE4.1).
  inline void mpsadbw(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstMPSADBW, &dst, &src, &imm8); }
  //! @brief Compute Multiple Packed Sums of Absolute Difference (SSE4.1).
  inline void mpsadbw(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstMPSADBW, &dst, &src, &imm8); }

  //! @brief Pack with Unsigned Saturation (SSE4.1).
  inline void packusdw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPackUSDW, &dst, &src); }
  //! @brief Pack with Unsigned Saturation (SSE4.1).
  inline void packusdw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPackUSDW, &dst, &src); }

  //! @brief Variable Blend Packed Bytes (SSE4.1).
  inline void pblendvb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPBlendVB, &dst, &src); }
  //! @brief Variable Blend Packed Bytes (SSE4.1).
  inline void pblendvb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPBlendVB, &dst, &src); }

  //! @brief Blend Packed Words (SSE4.1).
  inline void pblendw(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPBlendW, &dst, &src, &imm8); }
  //! @brief Blend Packed Words (SSE4.1).
  inline void pblendw(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPBlendW, &dst, &src, &imm8); }

  //! @brief Compare Packed Qword Data for Equal (SSE4.1).
  inline void pcmpeqq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPCmpEqQ, &dst, &src); }
  //! @brief Compare Packed Qword Data for Equal (SSE4.1).
  inline void pcmpeqq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpEqQ, &dst, &src); }

  //! @brief Extract Byte (SSE4.1).
  inline void pextrb(const GpReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrB, &dst, &src, &imm8); }
  //! @brief Extract Byte (SSE4.1).
  inline void pextrb(const Mem& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrB, &dst, &src, &imm8); }

  //! @brief Extract Dword (SSE4.1).
  inline void pextrd(const GpReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrD, &dst, &src, &imm8); }
  //! @brief Extract Dword (SSE4.1).
  inline void pextrd(const Mem& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrD, &dst, &src, &imm8); }

  //! @brief Extract Dword (SSE4.1).
  inline void pextrq(const GpReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrQ, &dst, &src, &imm8); }
  //! @brief Extract Dword (SSE4.1).
  inline void pextrq(const Mem& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrQ, &dst, &src, &imm8); }

  //! @brief Extract Word (SSE4.1).
  inline void pextrw(const GpReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrW, &dst, &src, &imm8); }
  //! @brief Extract Word (SSE4.1).
  inline void pextrw(const Mem& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPExtrW, &dst, &src, &imm8); }

  //! @brief Packed Horizontal Word Minimum (SSE4.1).
  inline void phminposuw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPHMinPOSUW, &dst, &src); }
  //! @brief Packed Horizontal Word Minimum (SSE4.1).
  inline void phminposuw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPHMinPOSUW, &dst, &src); }

  //! @brief Insert Byte (SSE4.1).
  inline void pinsrb(const XmmReg& dst, const GpReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRB, &dst, &src, &imm8); }
  //! @brief Insert Byte (SSE4.1).
  inline void pinsrb(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRB, &dst, &src, &imm8); }

  //! @brief Insert Dword (SSE4.1).
  inline void pinsrd(const XmmReg& dst, const GpReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRD, &dst, &src, &imm8); }
  //! @brief Insert Dword (SSE4.1).
  inline void pinsrd(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRD, &dst, &src, &imm8); }

  //! @brief Insert Dword (SSE4.1).
  inline void pinsrq(const XmmReg& dst, const GpReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRQ, &dst, &src, &imm8); }
  //! @brief Insert Dword (SSE4.1).
  inline void pinsrq(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRQ, &dst, &src, &imm8); }

  //! @brief Insert Word (SSE2).
  inline void pinsrw(const XmmReg& dst, const GpReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRW, &dst, &src, &imm8); }
  //! @brief Insert Word (SSE2).
  inline void pinsrw(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPInsRW, &dst, &src, &imm8); }

  //! @brief Maximum of Packed Word Integers (SSE4.1).
  inline void pmaxuw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMaxUW, &dst, &src); }
  //! @brief Maximum of Packed Word Integers (SSE4.1).
  inline void pmaxuw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxUW, &dst, &src); }

  //! @brief Maximum of Packed Signed Byte Integers (SSE4.1).
  inline void pmaxsb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMaxSB, &dst, &src); }
  //! @brief Maximum of Packed Signed Byte Integers (SSE4.1).
  inline void pmaxsb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxSB, &dst, &src); }

  //! @brief Maximum of Packed Signed Dword Integers (SSE4.1).
  inline void pmaxsd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMaxSD, &dst, &src); }
  //! @brief Maximum of Packed Signed Dword Integers (SSE4.1).
  inline void pmaxsd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxSD, &dst, &src); }

  //! @brief Maximum of Packed Unsigned Dword Integers (SSE4.1).
  inline void pmaxud(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMaxUD, &dst, &src); }
  //! @brief Maximum of Packed Unsigned Dword Integers (SSE4.1).
  inline void pmaxud(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMaxUD, &dst, &src); }

  //! @brief Minimum of Packed Signed Byte Integers (SSE4.1).
  inline void pminsb(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMinSB, &dst, &src); }
  //! @brief Minimum of Packed Signed Byte Integers (SSE4.1).
  inline void pminsb(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinSB, &dst, &src); }

  //! @brief Minimum of Packed Word Integers (SSE4.1).
  inline void pminuw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMinUW, &dst, &src); }
  //! @brief Minimum of Packed Word Integers (SSE4.1).
  inline void pminuw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinUW, &dst, &src); }

  //! @brief Minimum of Packed Dword Integers (SSE4.1).
  inline void pminud(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMinUD, &dst, &src); }
  //! @brief Minimum of Packed Dword Integers (SSE4.1).
  inline void pminud(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinUD, &dst, &src); }

  //! @brief Minimum of Packed Dword Integers (SSE4.1).
  inline void pminsd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMinSD, &dst, &src); }
  //! @brief Minimum of Packed Dword Integers (SSE4.1).
  inline void pminsd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMinSD, &dst, &src); }

  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxbw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMovSXBW, &dst, &src); }
  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxbw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovSXBW, &dst, &src); }

  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxbd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMovSXBD, &dst, &src); }
  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxbd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovSXBD, &dst, &src); }

  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxbq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMovSXBQ, &dst, &src); }
  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxbq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovSXBQ, &dst, &src); }

  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxwd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMovSXWD, &dst, &src); }
  //! @brief Packed Move with Sign Extend (SSE4.1).
  inline void pmovsxwd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovSXWD, &dst, &src); }

  //! @brief (SSE4.1).
  inline void pmovsxwq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMovSXWQ, &dst, &src); }
  //! @brief (SSE4.1).
  inline void pmovsxwq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovSXWQ, &dst, &src); }

  //! @brief (SSE4.1).
  inline void pmovsxdq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMovSXDQ, &dst, &src); }
  //! @brief (SSE4.1).
  inline void pmovsxdq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovSXDQ, &dst, &src); }

  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxbw(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMovZXBW, &dst, &src); }
  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxbw(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovZXBW, &dst, &src); }

  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxbd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMovZXBD, &dst, &src); }
  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxbd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovZXBD, &dst, &src); }

  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxbq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMovZXBQ, &dst, &src); }
  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxbq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovZXBQ, &dst, &src); }

  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxwd(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMovZXWD, &dst, &src); }
  //! @brief Packed Move with Zero Extend (SSE4.1).
  inline void pmovzxwd(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovZXWD, &dst, &src); }

  //! @brief (SSE4.1).
  inline void pmovzxwq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMovZXWQ, &dst, &src); }
  //! @brief (SSE4.1).
  inline void pmovzxwq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovZXWQ, &dst, &src); }

  //! @brief (SSE4.1).
  inline void pmovzxdq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMovZXDQ, &dst, &src); }
  //! @brief (SSE4.1).
  inline void pmovzxdq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMovZXDQ, &dst, &src); }

  //! @brief Multiply Packed Signed Dword Integers (SSE4.1).
  inline void pmuldq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMulDQ, &dst, &src); }
  //! @brief Multiply Packed Signed Dword Integers (SSE4.1).
  inline void pmuldq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulDQ, &dst, &src); }

  //! @brief Multiply Packed Signed Integers and Store Low Result (SSE4.1).
  inline void pmulld(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPMulLD, &dst, &src); }
  //! @brief Multiply Packed Signed Integers and Store Low Result (SSE4.1).
  inline void pmulld(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPMulLD, &dst, &src); }

  //! @brief Logical Compare (SSE4.1).
  inline void ptest(const XmmReg& op1, const XmmReg& op2)
  { _emitInstruction(kX86InstPTest, &op1, &op2); }
  //! @brief Logical Compare (SSE4.1).
  inline void ptest(const XmmReg& op1, const Mem& op2)
  { _emitInstruction(kX86InstPTest, &op1, &op2); }

  //! Round Packed SP-FP Values @brief (SSE4.1).
  inline void roundps(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundPS, &dst, &src, &imm8); }
  //! Round Packed SP-FP Values @brief (SSE4.1).
  inline void roundps(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundPS, &dst, &src, &imm8); }

  //! @brief Round Scalar SP-FP Values (SSE4.1).
  inline void roundss(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundSS, &dst, &src, &imm8); }
  //! @brief Round Scalar SP-FP Values (SSE4.1).
  inline void roundss(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundSS, &dst, &src, &imm8); }

  //! @brief Round Packed DP-FP Values (SSE4.1).
  inline void roundpd(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundPD, &dst, &src, &imm8); }
  //! @brief Round Packed DP-FP Values (SSE4.1).
  inline void roundpd(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundPD, &dst, &src, &imm8); }

  //! @brief Round Scalar DP-FP Values (SSE4.1).
  inline void roundsd(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundSD, &dst, &src, &imm8); }
  //! @brief Round Scalar DP-FP Values (SSE4.1).
  inline void roundsd(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstRoundSD, &dst, &src, &imm8); }

  // --------------------------------------------------------------------------
  // [SSE4.2]
  // --------------------------------------------------------------------------

  //! @brief Accumulate CRC32 Value (polynomial 0x11EDC6F41) (SSE4.2).
  inline void crc32(const GpReg& dst, const GpReg& src)
  {
    ASMJIT_ASSERT(dst.isRegType(kX86RegTypeGpd) || dst.isRegType(kX86RegTypeGpq));
    _emitInstruction(kX86InstCrc32, &dst, &src);
  }
  //! @brief Accumulate CRC32 Value (polynomial 0x11EDC6F41) (SSE4.2).
  inline void crc32(const GpReg& dst, const Mem& src)
  {
    ASMJIT_ASSERT(dst.isRegType(kX86RegTypeGpd) || dst.isRegType(kX86RegTypeGpq));
    _emitInstruction(kX86InstCrc32, &dst, &src);
  }

  //! @brief Packed Compare Explicit Length Strings, Return Index (SSE4.2).
  inline void pcmpestri(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpEStrI, &dst, &src, &imm8); }
  //! @brief Packed Compare Explicit Length Strings, Return Index (SSE4.2).
  inline void pcmpestri(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpEStrI, &dst, &src, &imm8); }

  //! @brief Packed Compare Explicit Length Strings, Return Mask (SSE4.2).
  inline void pcmpestrm(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpEStrM, &dst, &src, &imm8); }
  //! @brief Packed Compare Explicit Length Strings, Return Mask (SSE4.2).
  inline void pcmpestrm(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpEStrM, &dst, &src, &imm8); }

  //! @brief Packed Compare Implicit Length Strings, Return Index (SSE4.2).
  inline void pcmpistri(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpIStrI, &dst, &src, &imm8); }
  //! @brief Packed Compare Implicit Length Strings, Return Index (SSE4.2).
  inline void pcmpistri(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpIStrI, &dst, &src, &imm8); }

  //! @brief Packed Compare Implicit Length Strings, Return Mask (SSE4.2).
  inline void pcmpistrm(const XmmReg& dst, const XmmReg& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpIStrM, &dst, &src, &imm8); }
  //! @brief Packed Compare Implicit Length Strings, Return Mask (SSE4.2).
  inline void pcmpistrm(const XmmReg& dst, const Mem& src, const Imm& imm8)
  { _emitInstruction(kX86InstPCmpIStrM, &dst, &src, &imm8); }

  //! @brief Compare Packed Data for Greater Than (SSE4.2).
  inline void pcmpgtq(const XmmReg& dst, const XmmReg& src)
  { _emitInstruction(kX86InstPCmpGtQ, &dst, &src); }
  //! @brief Compare Packed Data for Greater Than (SSE4.2).
  inline void pcmpgtq(const XmmReg& dst, const Mem& src)
  { _emitInstruction(kX86InstPCmpGtQ, &dst, &src); }

  //! @brief Return the Count of Number of Bits Set to 1 (SSE4.2).
  inline void popcnt(const GpReg& dst, const GpReg& src)
  {
    ASMJIT_ASSERT(!dst.isGpb());
    ASMJIT_ASSERT(src.getRegType() == dst.getRegType());
    _emitInstruction(kX86InstPopCnt, &dst, &src);
  }
  //! @brief Return the Count of Number of Bits Set to 1 (SSE4.2).
  inline void popcnt(const GpReg& dst, const Mem& src)
  {
    ASMJIT_ASSERT(!dst.isGpb());
    _emitInstruction(kX86InstPopCnt, &dst, &src);
  }

  // -------------------------------------------------------------------------
  // [AMD only]
  // -------------------------------------------------------------------------

  //! @brief Prefetch (3dNow - Amd).
  //!
  //! Loads the entire 64-byte aligned memory sequence containing the
  //! specified memory address into the L1 data cache. The position of
  //! the specified memory address within the 64-byte cache line is
  //! irrelevant. If a cache hit occurs, or if a memory fault is detected,
  //! no bus cycle is initiated and the instruction is treated as a NOP.
  inline void amd_prefetch(const Mem& mem)
  { _emitInstruction(kX86InstAmdPrefetch, &mem); }

  //! @brief Prefetch and set cache to modified (3dNow - Amd).
  //!
  //! The PREFETCHW instruction loads the prefetched line and sets the
  //! cache-line state to Modified, in anticipation of subsequent data
  //! writes to the line. The PREFETCH instruction, by contrast, typically
  //! sets the cache-line state to Exclusive (depending on the hardware
  //! implementation).
  inline void amd_prefetchw(const Mem& mem)
  { _emitInstruction(kX86InstAmdPrefetchW, &mem); }

  // -------------------------------------------------------------------------
  // [Intel only]
  // -------------------------------------------------------------------------

  //! @brief Move Data After Swapping Bytes (SSE3 - Intel Atom).
  inline void movbe(const GpReg& dst, const Mem& src)
  {
    ASMJIT_ASSERT(!dst.isGpb());
    _emitInstruction(kX86InstMovBE, &dst, &src);
  }

  //! @brief Move Data After Swapping Bytes (SSE3 - Intel Atom).
  inline void movbe(const Mem& dst, const GpReg& src)
  {
    ASMJIT_ASSERT(!src.isGpb());
    _emitInstruction(kX86InstMovBE, &dst, &src);
  }

  // -------------------------------------------------------------------------
  // [Emit Options]
  // -------------------------------------------------------------------------

  //! @brief Assert LOCK# Signal Prefix.
  //!
  //! This instruction causes the processor's LOCK# signal to be asserted
  //! during execution of the accompanying instruction (turns the
  //! instruction into an atomic instruction). In a multiprocessor environment,
  //! the LOCK# signal insures that the processor has exclusive use of any shared
  //! memory while the signal is asserted.
  //!
  //! The LOCK prefix can be prepended only to the following instructions and
  //! to those forms of the instructions that use a memory operand: ADD, ADC,
  //! AND, BTC, BTR, BTS, CMPXCHG, DEC, INC, NEG, NOT, OR, SBB, SUB, XOR, XADD,
  //! and XCHG. An undefined opcode exception will be generated if the LOCK
  //! prefix is used with any other instruction. The XCHG instruction always
  //! asserts the LOCK# signal regardless of the presence or absence of the LOCK
  //! prefix.
  //!
  //! @sa @c kX86EmitOptionLock.
  inline void lock()
  { _emitOptions |= kX86EmitOptionLock; }

  //! @brief Force REX prefix to be emitted.
  //!
  //! This option should be used carefully, because there are unencodable
  //! combinations. If you want to access ah, bh, ch or dh registers then you
  //! can't emit REX prefix and it will cause an illegal instruction error.
  //!
  //! @note REX prefix is only valid for X64/AMD64 platform.
  //!
  //! @sa @c kX86EmitOptionRex.
  inline void rex()
  { _emitOptions |= kX86EmitOptionRex; }
};

//! @}

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

// [Guard]
#endif // _ASMJIT_X86_X86ASSEMBLER_H
