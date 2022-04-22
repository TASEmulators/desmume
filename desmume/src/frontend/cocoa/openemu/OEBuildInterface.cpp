/*
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

/*
	Unlike the other build targets, the OpenEmu plug-in build system has
	the goal of maintaining a single build target for itself to keep
	things simple. This causes a number of things to happen:

	1. No PGO. But we can't do PGO anyways because we're building a plug-in,
	   and therefore can't run the plug-in through Xcode's debugger to even
	   generate the optimization profile for PGO.
	2. No separate build file lists for different binaries. While we don't
	   suppport PowerPC or i386 builds for the plug-in, we do support x86_64,
	   x86_64h, and arm64 builds. To make up for build list differences
	   between binaries, we use this file to account for them.
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
	#endif
#endif
