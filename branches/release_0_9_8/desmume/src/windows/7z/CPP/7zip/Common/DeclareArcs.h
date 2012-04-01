// DeclareArcs.h

#ifndef __DECLAREARCS_H
#define __DECLAREARCS_H

#define DECLARE_ARC(x) struct CRegister##x { CRegister##x(); }; \
	FORCE_REF(CRegister##x, g_RegisterArc##x)

#define DECLARE_ARCN(x,n) struct CRegister##x##n { CRegister##x##n(); }; \
	FORCE_REF(CRegister##x##n, g_RegisterArc##n##x)

#ifndef FORCE_REF
	#define FORCE_REF(...)
#endif

DECLARE_ARC(7z)
DECLARE_ARC(BZip2)
DECLARE_ARC(GZip)
DECLARE_ARC(Lzh)
DECLARE_ARC(Lzma)
DECLARE_ARC(Rar)
DECLARE_ARC(Split)
DECLARE_ARC(Tar)
DECLARE_ARC(Zip)
DECLARE_ARCN(Zip,2)
DECLARE_ARCN(Zip,3)

#endif
