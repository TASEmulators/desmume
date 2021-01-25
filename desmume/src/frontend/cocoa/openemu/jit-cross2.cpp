//
//  jit-cross2.cpp
//  DeSmuME
//
//  Created by C.W. Betts on 1/24/21.
//  Copyright © 2021 DeSmuME Team. All rights reserved.
//

// this file is used to make cross-compiling work a bit better for X86_64 and arm64 build targets.
// It just includes the .cpp files that would be compiled.
#ifdef __aarch64__
// nothing here yet.
#elif defined(__x86_64__)
#include "utils/AsmJit/x86/x86assembler.cpp"
#include "utils/AsmJit/x86/x86compiler.cpp"
#include "utils/AsmJit/x86/x86compilercontext.cpp"
#include "utils/AsmJit/x86/x86compilerfunc.cpp"
#include "utils/AsmJit/x86/x86compileritem.cpp"
#include "utils/AsmJit/x86/x86cpuinfo.cpp"
#include "utils/AsmJit/x86/x86defs.cpp"
#include "utils/AsmJit/x86/x86func.cpp"
#include "utils/AsmJit/x86/x86operand.cpp"
#include "utils/AsmJit/x86/x86util.cpp"
#else
#error unknown arch!
#endif
