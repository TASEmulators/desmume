#ifndef DEBUG_H
#define DEBUG_H

#include "core.h"
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
#define LOG(f, r...) DebugPrintf(MainLog, __FILE__, __LINE__, f, ## r)
#else
#define LOG(f, r...)
#endif

#endif
