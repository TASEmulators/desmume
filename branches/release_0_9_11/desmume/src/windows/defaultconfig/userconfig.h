#ifndef _USERCONFIG_H
#define _USERCONFIG_H

//this is a default file. it should not be edited, or else you will mess up the defaults.
//to customize your build, place a customized copy in the userconfig directory
//(alongside this defaultconfig directory)

//disables SSE and SSE2 optimizations (better change it in the vc++ codegen options too)
//note that you may have to use this if your compiler doesn't support standard SSE intrinsics
//#define NOSSE
//#define NOSSE2 

//#define DEVELOPER //enables dev+ features
//#define GDB_STUB //enables the gdb stub. for some reason this is separate from dev+ for now. requires DEVELOPER.

//#define EXPERIMENTAL_WIFI_COMM //enables experimental wifi communication features which do not actually work yet
//basic wifi register emulation is still enabled, to at least make it seem like the wifi is working in an empty universe

#endif //_USERCONFIG_H
