/*
	Copyright (C) 2026 DeSmuME team

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

/*
	This file is responsible for accounting for the different build lists
	used for each architecture in Universal build targets.
*/

#if defined(HAVE_JIT)
	#if defined(__i386__) || defined(__x86_64__)
		#include "../../utils/AsmJit/core/assembler.cpp"
		#include "../../utils/AsmJit/core/assert.cpp"
		#include "../../utils/AsmJit/core/buffer.cpp"
		#include "../../utils/AsmJit/core/compiler.cpp"
		#include "../../utils/AsmJit/core/compilercontext.cpp"
		#include "../../utils/AsmJit/core/compilerfunc.cpp"
		#include "../../utils/AsmJit/core/compileritem.cpp"
		#include "../../utils/AsmJit/core/context.cpp"
		#include "../../utils/AsmJit/core/cpuinfo.cpp"
		#include "../../utils/AsmJit/core/defs.cpp"
		#include "../../utils/AsmJit/core/func.cpp"
		#include "../../utils/AsmJit/core/logger.cpp"
		#include "../../utils/AsmJit/core/memorymanager.cpp"
		#include "../../utils/AsmJit/core/memorymarker.cpp"
		#include "../../utils/AsmJit/core/operand.cpp"
		#include "../../utils/AsmJit/core/stringbuilder.cpp"
		#include "../../utils/AsmJit/core/stringutil.cpp"
		#include "../../utils/AsmJit/core/virtualmemory.cpp"
		#include "../../utils/AsmJit/core/zonememory.cpp"
		#include "../../utils/AsmJit/x86/x86assembler.cpp"
		#include "../../utils/AsmJit/x86/x86compiler.cpp"
		#include "../../utils/AsmJit/x86/x86compilercontext.cpp"
		#include "../../utils/AsmJit/x86/x86compilerfunc.cpp"
		#include "../../utils/AsmJit/x86/x86compileritem.cpp"
		#include "../../utils/AsmJit/x86/x86cpuinfo.cpp"
		#include "../../utils/AsmJit/x86/x86defs.cpp"
		#include "../../utils/AsmJit/x86/x86func.cpp"
		#include "../../utils/AsmJit/x86/x86operand.cpp"
		#include "../../utils/AsmJit/x86/x86util.cpp"
		#include "../../arm_jit.cpp"
	#elif defined(__aarch64__)
		#include "../../utils/arm_jit/arm_jit_arm.cpp"
	#endif
#endif
