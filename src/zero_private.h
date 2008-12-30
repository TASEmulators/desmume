#ifndef _zero_private_h
#define _zero_private_h

//#define ZERO_MODE

#ifdef ZERO_MODE
#define ASSERT_UNALIGNED(x) assert(x)
#else
#define ASSERT_UNALIGNED(x)
#endif

#endif
