#ifndef __DTOOL_H__ 
#define __DTOOL_H__

#include <stdio.h>
#define DTOOL_LOG(...) fprintf(stderr, __VA_ARGS__);

#include "../types.h"
#include "../nds/interrupts.h"

typedef void (*dTool_openFn)();
typedef void (*dTool_updateFn)();
typedef void (*dTool_closeFn)();

typedef struct
{
	const char name[64];
	dTool_openFn open;
	dTool_updateFn update;
	dTool_closeFn close;
	
} dTool_t;

#endif /*__DTOOL_H__*/
