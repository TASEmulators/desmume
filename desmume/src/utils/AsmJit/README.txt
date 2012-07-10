AsmJit - Complete x86/x64 JIT Assembler for C++ Language - Version 1.0
======================================================================

http://code.google.com/p/asmjit/

Introduction
============

AsmJit is complete x86/x64 JIT Assembler for C++ language. It supports FPU,
MMX, 3dNow, SSE, SSE2, SSE3 and SSE4 intrinsics, powerful compiler that helps
to write portable functions for 32-bit (x86) and 64-bit (x64) architectures.
AsmJit can be used to create functions at runtime that can be called from
existing (but also generated) C/C++ code.

AsmJit is crossplatform library that supports various compilers and operating
systems. Currently only limitation is x86 (32-bit) or x64 (64-bit) processor.
Currently tested operating systems are Windows (32 bit and 64 bit), Linux (32
bit and 64 bit) and MacOSX (32 bit). 

Assembler / Compiler
====================

AsmJit library contains two main classes for code generation with different
goals. First main code generation class is called Assembler and contains low
level API that can be used to generate JIT binary code. It directly emits
binary stream that represents encoded x86/x64 assembler opcodes. Together
with operands and labels it can be used to generate complete code.

There is also class named Compiler that allows to develop crossplatform
assembler code without worring about function calling conventions and
registers allocation. It can be also used to write 32 bit and 64 bit portable
code. Compiler is recommended class to use for high level code generation.

Building & Configuring
======================

You have two choices when using AsmJit library:

1. Embed files into your project - This is simplest way how to use AsmJit
   library. If you want to do this, edit AsmJit/Config.h file and undefine
   "// #define ASMJIT_API" macro or define ASMJIT_API using your IDE or
   makefiles (for example gcc can accept -DASMJIT_API to define it).

   You will bypass shared library (dll) support by defining ASMJIT_API .

2. Build shared library and link your project to it. This is now default way
   for using AsmJit library, but can be easily changed. To build dynamically
   linked library use cmake (http://www.cmake.org/) that can generate 
   makefiles and project files for your IDE (included is Visual Studio,
   Code::Blocks, Eclipse, KDevelop and more...).
   
   To build shared library use this cmake command (or use provided scripts):
     cmake {Replace this with AsmJit directory} DASMJIT_BUILD_LIBRARY=1

Directory structure
===================

AsmJit - Directory where are sources needed to compile AsmJit. This directory
is designed to be embeddable to your application as easy as possible. There is
also AsmJit/Config.h header where you can configure platform (if autodetection 
not works for you) and application specific features. Look at platform macros 
to change some backends to your preferences.

Test - Directory with cmake project to test AsmJit library. It generates simple
command line applications for testing AsmJit functionality. It's only here as a
demonstration how easy this can be done. These applications are also examples 
how to use AsmJit API. For example look at testjit for simple code generation, 
testcpu for cpuid() and cpuInfo() demonstration, testcompiler for compiler 
example, etc...

Supported compilers
===================

AsmJit is successfully tested by following compilers:
- MSVC (VC6.0, VC7.1, VC8.0)
- GCC (3.4.X+ including MinGW and 4.1.X+, 4.3.X+, 4.4.X+)

If you are using different compiler and you have troubles, please use AsmJit
mailing list or create an Issue (see project home page).

Supported platforms
===================

Fully supported platforms at this time are X86 (32-bit) and X86_64/X64 (64-bit).
Other platforms need volunteers. Also note that AsmJit is designed to generate
assembler binary only for host CPU, don't try to generate 64-bit assembler in
32 bit mode and vica versa - this is not designed to work and will not work.

Examples
========

Examples and tests can be found in these places:
- AsmJit home page <http://code.google.com/p/asmjit/>
- AsmJit wiki <http://code.google.com/p/asmjit/w/list>
- AsmJit Test directory (in this package)

License
=======

AsmJit can be distributed under zlib license:
<http://www.opensource.org/licenses/zlib-license.php>

Google Groups and Mailing Lists
===============================

AsmJit google group:
  * http://groups.google.com/group/asmjit-dev 

AsmJit mailing list:
  * asmjit-dev@googlegroups.com 

Contact Author/Maintainer
=========================

Petr Kobalicek <kobalicek.petr@gmail.com>
