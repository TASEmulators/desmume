// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

#define ASMJIT_EXPORTS

// [Dependencies - AsmJit]
#include "../x86/x86cpuinfo.h"
#include "../x86/x86defs.h"

// 2009-02-05: Thanks to Mike Tajmajer for VC7.1 compiler support. It shouldn't
// affect x64 compilation, because x64 compiler starts with VS2005 (VC8.0).
#if defined(_MSC_VER)
# if _MSC_VER >= 1400
#  include <intrin.h>
# endif // _MSC_VER >= 1400 (>= VS2005)
#endif // _MSC_VER

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

// ============================================================================
// [AsmJit::X86CpuVendor]
// ============================================================================

struct X86CpuVendor
{
  uint32_t id;
  char text[12];
};

static const X86CpuVendor x86CpuVendor[] = 
{
  { kCpuIntel    , { 'G', 'e', 'n', 'u', 'i', 'n', 'e', 'I', 'n', 't', 'e', 'l' } },

  { kCpuAmd      , { 'A', 'u', 't', 'h', 'e', 'n', 't', 'i', 'c', 'A', 'M', 'D' } },
  { kCpuAmd      , { 'A', 'M', 'D', 'i', 's', 'b', 'e', 't', 't', 'e', 'r', '!' } },

  { kCpuNSM      , { 'G', 'e', 'o', 'd', 'e', ' ', 'b', 'y', ' ', 'N', 'S', 'C' } },
  { kCpuNSM      , { 'C', 'y', 'r', 'i', 'x', 'I', 'n', 's', 't', 'e', 'a', 'd' } },

  { kCpuTransmeta, { 'G', 'e', 'n', 'u', 'i', 'n', 'e', 'T', 'M', 'x', '8', '6' } },
  { kCpuTransmeta, { 'T', 'r', 'a', 'n', 's', 'm', 'e', 't', 'a', 'C', 'P', 'U' } },

  { kCpuVia      , { 'V', 'I', 'A',  0 , 'V', 'I', 'A',  0 , 'V', 'I', 'A',  0  } },
  { kCpuVia      , { 'C', 'e', 'n', 't', 'a', 'u', 'r', 'H', 'a', 'u', 'l', 's' } }
};

static inline bool x86CpuVendorEq(const X86CpuVendor& info, const char* vendorString)
{
  const uint32_t* a = reinterpret_cast<const uint32_t*>(info.text);
  const uint32_t* b = reinterpret_cast<const uint32_t*>(vendorString);

  return (a[0] == b[0]) &
         (a[1] == b[1]) &
         (a[2] == b[2]) ;
}

// ============================================================================
// [AsmJit::x86CpuSimplifyBrandString]
// ============================================================================

static inline void x86CpuSimplifyBrandString(char* s)
{
  // Always clear the current character in the buffer. This ensures that there
  // is no garbage after the string NULL terminator.
  char* d = s;

  char prev = 0;
  char curr = s[0];
  s[0] = '\0';

  for (;;)
  {
    if (curr == 0) break;

    if (curr == ' ')
    {
      if (prev == '@') goto _Skip;
      if (s[1] == ' ' || s[1] == '@') goto _Skip;
    }

    d[0] = curr;
    d++;
    prev = curr;

_Skip:
    curr = *++s;
    s[0] = '\0';
  }

  d[0] = '\0';
}

// ============================================================================
// [AsmJit::x86CpuId]
// ============================================================================

// This is messy, I know. cpuid is implemented as intrinsic in VS2005, but
// we should support other compilers as well. Main problem is that MS compilers
// in 64-bit mode not allows to use inline assembler, so we need intrinsic and
// we need also asm version.

// x86CpuId() and detectCpuInfo() for x86 and x64 platforms begins here.
#if defined(ASMJIT_X86) || defined(ASMJIT_X64)
void x86CpuId(uint32_t in, X86CpuId* out)
{
#if defined(_MSC_VER)

// 2009-02-05: Thanks to Mike Tajmajer for supporting VC7.1 compiler.
// ASMJIT_X64 is here only for readibility, only VS2005 can compile 64-bit code.
# if _MSC_VER >= 1400 || defined(ASMJIT_X64)
  // Done by intrinsics.
  __cpuid(reinterpret_cast<int*>(out->i), in);
# else // _MSC_VER < 1400
  uint32_t cpuid_in = in;
  uint32_t* cpuid_out = out->i;

  __asm
  {
    mov     eax, cpuid_in
    mov     edi, cpuid_out
    cpuid
    mov     dword ptr[edi +  0], eax
    mov     dword ptr[edi +  4], ebx
    mov     dword ptr[edi +  8], ecx
    mov     dword ptr[edi + 12], edx
  }
# endif // _MSC_VER < 1400

#elif defined(__GNUC__)

// Note, need to preserve ebx/rbx register!
# if defined(ASMJIT_X86)
#  define __myCpuId(a, b, c, d, inp) \
  asm ("mov %%ebx, %%edi\n"    \
       "cpuid\n"               \
       "xchg %%edi, %%ebx\n"   \
       : "=a" (a), "=D" (b), "=c" (c), "=d" (d) : "a" (inp))
# else
#  define __myCpuId(a, b, c, d, inp) \
  asm ("mov %%rbx, %%rdi\n"    \
       "cpuid\n"               \
       "xchg %%rdi, %%rbx\n"   \
       : "=a" (a), "=D" (b), "=c" (c), "=d" (d) : "a" (inp))
# endif
  __myCpuId(out->eax, out->ebx, out->ecx, out->edx, in);

#endif // Compiler #ifdef.
}

// ============================================================================
// [AsmJit::x86CpuDetect]
// ============================================================================

void x86CpuDetect(X86CpuInfo* out)
{
  uint32_t i;
  X86CpuId regs;

  // Clear everything except the '_size' member.
  memset(reinterpret_cast<uint8_t*>(out) + sizeof(uint32_t),
    0, sizeof(CpuInfo) - sizeof(uint32_t));

  // Fill safe defaults.
  memcpy(out->_vendorString, "Unknown", 8);
  out->_numberOfProcessors = CpuInfo::detectNumberOfProcessors();

  // Get vendor string/id.
  x86CpuId(0, &regs);

  memcpy(out->_vendorString, &regs.ebx, 4);
  memcpy(out->_vendorString + 4, &regs.edx, 4);
  memcpy(out->_vendorString + 8, &regs.ecx, 4);

  for (i = 0; i < 3; i++)
  {
    if (x86CpuVendorEq(x86CpuVendor[i], out->_vendorString))
    {
      out->_vendorId = x86CpuVendor[i].id;
      break;
    }
  }

  // Get feature flags in ecx/edx, and family/model in eax.
  x86CpuId(1, &regs);

  // Fill family and model fields.
  out->_family   = (regs.eax >> 8) & 0x0F;
  out->_model    = (regs.eax >> 4) & 0x0F;
  out->_stepping = (regs.eax     ) & 0x0F;

  // Use extended family and model fields.
  if (out->_family == 0x0F)
  {
    out->_family += ((regs.eax >> 20) & 0xFF);
    out->_model  += ((regs.eax >> 16) & 0x0F) << 4;
  }

  out->_processorType        = ((regs.eax >> 12) & 0x03);
  out->_brandIndex           = ((regs.ebx      ) & 0xFF);
  out->_flushCacheLineSize   = ((regs.ebx >>  8) & 0xFF) * 8;
  out->_maxLogicalProcessors = ((regs.ebx >> 16) & 0xFF);
  out->_apicPhysicalId       = ((regs.ebx >> 24) & 0xFF);

  if (regs.ecx & 0x00000001U) out->_features |= kX86FeatureSse3;
  if (regs.ecx & 0x00000002U) out->_features |= kX86FeaturePclMulDQ;
  if (regs.ecx & 0x00000008U) out->_features |= kX86FeatureMonitorMWait;
  if (regs.ecx & 0x00000200U) out->_features |= kX86FeatureSsse3;
  if (regs.ecx & 0x00002000U) out->_features |= kX86FeatureCmpXchg16B;
  if (regs.ecx & 0x00080000U) out->_features |= kX86FeatureSse41;
  if (regs.ecx & 0x00100000U) out->_features |= kX86FeatureSse42;
  if (regs.ecx & 0x00400000U) out->_features |= kX86FeatureMovBE;
  if (regs.ecx & 0x00800000U) out->_features |= kX86FeaturePopCnt;
  if (regs.ecx & 0x10000000U) out->_features |= kX86FeatureAvx;

  if (regs.edx & 0x00000010U) out->_features |= kX86FeatureRdtsc;
  if (regs.edx & 0x00000100U) out->_features |= kX86FeatureCmpXchg8B;
  if (regs.edx & 0x00008000U) out->_features |= kX86FeatureCMov;
  if (regs.edx & 0x00800000U) out->_features |= kX86FeatureMmx;
  if (regs.edx & 0x01000000U) out->_features |= kX86FeatureFXSR;
  if (regs.edx & 0x02000000U) out->_features |= kX86FeatureSse | kX86FeatureMmxExt;
  if (regs.edx & 0x04000000U) out->_features |= kX86FeatureSse | kX86FeatureSse2;
  if (regs.edx & 0x10000000U) out->_features |= kX86FeatureMultiThreading;

  if (out->_vendorId == kCpuAmd && (regs.edx & 0x10000000U))
  {
    // AMD sets Multithreading to ON if it has more cores.
    if (out->_numberOfProcessors == 1) out->_numberOfProcessors = 2;
  }

  // This comment comes from V8 and I think that its important:
  //
  // Opteron Rev E has i bug in which on very rare occasions i locked
  // instruction doesn't act as i read-acquire barrier if followed by i
  // non-locked read-modify-write instruction.  Rev F has this bug in 
  // pre-release versions, but not in versions released to customers,
  // so we test only for Rev E, which is family 15, model 32..63 inclusive.

  if (out->_vendorId == kCpuAmd && out->_family == 15 && out->_model >= 32 && out->_model <= 63) 
  {
    out->_bugs |= kX86BugAmdLockMB;
  }

  // Calling cpuid with 0x80000000 as the in argument
  // gets the number of valid extended IDs.

  x86CpuId(0x80000000, &regs);

  uint32_t exIds = regs.eax;
  if (exIds > 0x80000004) exIds = 0x80000004;

  uint32_t* brand = reinterpret_cast<uint32_t*>(out->_brandString);

  for (i = 0x80000001; i <= exIds; i++)
  {
    x86CpuId(i, &regs);

    switch (i)
    {
      case 0x80000001:
        if (regs.ecx & 0x00000001U) out->_features |= kX86FeatureLahfSahf;
        if (regs.ecx & 0x00000020U) out->_features |= kX86FeatureLzCnt;
        if (regs.ecx & 0x00000040U) out->_features |= kX86FeatureSse4A;
        if (regs.ecx & 0x00000080U) out->_features |= kX86FeatureMSse;
        if (regs.ecx & 0x00000100U) out->_features |= kX86FeaturePrefetch;

        if (regs.edx & 0x00100000U) out->_features |= kX86FeatureExecuteDisableBit;
        if (regs.edx & 0x00200000U) out->_features |= kX86FeatureFFXSR;
        if (regs.edx & 0x00400000U) out->_features |= kX86FeatureMmxExt;
        if (regs.edx & 0x08000000U) out->_features |= kX86FeatureRdtscP;
        if (regs.edx & 0x20000000U) out->_features |= kX86Feature64Bit;
        if (regs.edx & 0x40000000U) out->_features |= kX86Feature3dNowExt | kX86FeatureMmxExt;
        if (regs.edx & 0x80000000U) out->_features |= kX86Feature3dNow;
        break;

      case 0x80000002:
      case 0x80000003:
      case 0x80000004:
        *brand++ = regs.eax;
        *brand++ = regs.ebx;
        *brand++ = regs.ecx;
        *brand++ = regs.edx;
        break;

      default:
        // Additional features can be detected in the future.
        break;
    }
  }

  // Simplify the brand string (remove unnecessary spaces to make it printable).
  x86CpuSimplifyBrandString(out->_brandString);
}
#endif

} // AsmJit

// [Api-End]
#include "../core/apiend.h"
