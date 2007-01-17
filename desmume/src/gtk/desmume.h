#ifndef __DESMUME_H__
#define __DESMUME_H__

#include "globals.h"

extern void desmume_init();
extern void desmume_free();

extern void desmume_pause();
extern void desmume_resume();
extern void desmume_toggle();
extern BOOL desmume_running();

extern void desmume_cycle();

#endif /*__DESMUME_H__*/

