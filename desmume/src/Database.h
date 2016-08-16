#ifndef DESMUME_DATABASE_H_
#define DESMUME_DATABASE_H_

namespace Database
{
	const char* RegionXXXForCode(char code, bool unknownAsString);
	const char* MakerNameForMakerCode(u16 id, bool unknownAsString);
};

#endif
