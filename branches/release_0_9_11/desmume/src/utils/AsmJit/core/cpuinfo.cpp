// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

#define ASMJIT_EXPORTS

// [Dependencies - AsmJit]
#include "../core/cpuinfo.h"

#if defined(ASMJIT_X86) || defined(ASMJIT_X64)
#include "../x86/x86cpuinfo.h"
#else
// ?
#endif // ASMJIT_X86 || ASMJIT_X64

// [Dependencies - Windows]
#if defined(ASMJIT_WINDOWS)
# include <windows.h>
#endif // ASMJIT_WINDOWS

// [Dependencies - Posix]
#if defined(ASMJIT_POSIX)
# include <errno.h>
# include <sys/statvfs.h>
# include <sys/utsname.h>
# include <unistd.h>
#endif // ASMJIT_POSIX

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit {

// ============================================================================
// [AsmJit::CpuInfo - DetectNumberOfProcessors]
// ============================================================================

uint32_t CpuInfo::detectNumberOfProcessors()
{
#if defined(ASMJIT_WINDOWS)
  SYSTEM_INFO info;
  ::GetSystemInfo(&info);
  return info.dwNumberOfProcessors;
#elif defined(ASMJIT_POSIX) && defined(_SC_NPROCESSORS_ONLN)
  // It seems that sysconf returns the number of "logical" processors on both
  // mac and linux.  So we get the number of "online logical" processors.
  long res = ::sysconf(_SC_NPROCESSORS_ONLN);
  if (res == -1) return 1;

  return static_cast<uint32_t>(res);
#else
  return 1;
#endif
}

// ============================================================================
// [AsmJit::CpuInfo - GetGlobal]
// ============================================================================

#if defined(ASMJIT_X86) || defined(ASMJIT_X64)
struct InitializedCpuInfo : public X86CpuInfo
{
  inline InitializedCpuInfo() :
    X86CpuInfo()
  {
    x86CpuDetect(this);
  }
};
#else
#error "AsmJit::CpuInfo - Unsupported CPU or compiler."
#endif // ASMJIT_X86 || ASMJIT_X64

const CpuInfo* CpuInfo::getGlobal()
{
#if defined(ASMJIT_X86) || defined(ASMJIT_X64)
  static InitializedCpuInfo cpuInfo;
#else
#error "AsmJit::CpuInfo - Unsupported CPU or compiler."
#endif // ASMJIT_X86 || ASMJIT_X64
  return &cpuInfo;
}

} // AsmJit

// [Api-End]
#include "../core/apiend.h"
