// DeclareCodecs.h

#ifndef __DECLARECODECS_H
#define __DECLARECODECS_H

#define DECLARE_CODEC(x) struct CRegisterCodec##x { CRegisterCodec##x(); }; \
	FORCE_REF(CRegisterCodec##x, g_RegisterCodec##x)

#define DECLARE_CODECS(x) struct CRegisterCodecs##x { CRegisterCodecs##x(); }; \
	FORCE_REF(CRegisterCodecs##x, g_RegisterCodecs##x)

#ifndef FORCE_REF
	#define FORCE_REF(...)
#endif

DECLARE_CODEC(7zAES)
DECLARE_CODEC(BCJ2)
DECLARE_CODEC(BCJ)
DECLARE_CODEC(BZip2)
DECLARE_CODEC(Copy)
DECLARE_CODEC(Deflate64)
DECLARE_CODEC(DeflateNsis)
DECLARE_CODEC(Deflate)
DECLARE_CODEC(LZMA)
DECLARE_CODEC(PPMD)

DECLARE_CODECS(Branch)
DECLARE_CODECS(ByteSwap)
DECLARE_CODECS(Rar)

#endif
