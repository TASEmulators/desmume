#ifndef _WINDRIVER_H_
#define _WINDRIVER_H_

#define WIN32_LEAN_AND_MEAN
#include "../common.h"
#include "CWindow.h"

extern WINCLASS	*MainWindow;

class Lock {
public:
	Lock();
	~Lock();
};

#endif