#ifndef DEBUG_H
#define DEBUG_H

#include "types.h"
#include <stdio.h>

typedef enum { DEBUG_STRING, DEBUG_STREAM , DEBUG_STDOUT, DEBUG_STDERR } DebugOutType;

typedef struct {
	DebugOutType output_type;
	union {
		FILE * stream;
		char * string;
	} output;
	char * name;
} Debug;

Debug * DebugInit(const char *, DebugOutType, char *);
void DebugDeInit(Debug *);

void DebugChangeOutput(Debug *, DebugOutType, char *);

void DebugPrintf(Debug *, const char *, u32, const char *, ...);

extern Debug * MainLog;

void LogStart(void);
void LogStop(void);

#ifdef DEBUG
#define LOG(f, ...) DebugPrintf(MainLog, __FILE__, __LINE__, f, __VA_ARGS__)
#else
#define LOG(f, ...)
#endif

#ifdef GPUDEBUG
#define GPULOG(f, ...) DebugPrintf(MainLog, __FILE__, __LINE__, f, __VA_ARGS__)
#else
#define GPULOG(f, ...)
#endif

#ifdef DIVDEBUG
#define DIVLOG(f, ...) DebugPrintf(MainLog, __FILE__, __LINE__, f, __VA_ARGS__)
#else
#define DIVLOG(f, ...)
#endif

#ifdef SQRTDEBUG
#define SQRTLOG(f, ...) DebugPrintf(MainLog, __FILE__, __LINE__, f, __VA_ARGS__)
#else
#define SQRTLOG(f, ...)
#endif

#ifdef CARDDEBUG
#define CARDLOG(f, ...) DebugPrintf(MainLog, __FILE__, __LINE__, f, __VA_ARGS__)
#else
#define CARDLOG(f, ...)
#endif

#ifdef DMADEBUG
#define DMALOG(f, ...) DebugPrintf(MainLog, __FILE__, __LINE__, f, __VA_ARGS__)
#else
#define DMALOG(f, ...)
#endif

#endif
