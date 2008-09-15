#ifndef __GPU_OSD_
#define __GPU_OSD_

#include <stdlib.h>
#include "debug.h"

#define OSD_MAX_LINES	10

class OSDCLASS
{
private:
	u8		screen[256*192*2];
	u64		offset;
	u8		mode;
	//u64		*text_lines[10];

	void printChar(int x, int y, char c);
public:
	char	name[7];		// for debuging
	OSDCLASS(int core);
	~OSDCLASS();

	void	setOffset(int ofs);
	void	update();
	void	addLines(const char *fmt, ...);
	void	addFixed(int x, int y, const char *fmt, ...);
};

extern OSDCLASS	*osd;
extern OSDCLASS	*osdA;
extern OSDCLASS	*osdB;

#endif