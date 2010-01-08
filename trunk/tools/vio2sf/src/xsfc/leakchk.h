#ifndef LEAKCHK_H
#define LEAKCHK_H

#if defined(_MSC_VER) && defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
/*
#define new  ::new(_CLIENT_BLOCK, __FILE__, __LINE__)
*/
#endif

#endif
