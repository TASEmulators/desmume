#ifndef __DTOOL_H__ 
#define __DTOOL_H__

#include "../types.h"
#include "../registers.h"

typedef void (*dTool_openFn)(int id);
typedef void (*dTool_updateFn)();
typedef void (*dTool_closeFn)();

typedef struct
{
	/* this should be the same name of the action in the gui xml */
	const char shortname[16];
	const char name[32];
	dTool_openFn open;
	dTool_updateFn update;
	dTool_closeFn close;
} dTool_t;

extern void dTool_CloseCallback(int id);

#endif /*__DTOOL_H__*/
