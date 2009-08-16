// RegisterCodec.h

#ifndef __REGISTERCODEC_H
#define __REGISTERCODEC_H

#include "../Common/MethodId.h"
#include "DeclareCodecs.h"

typedef void * (*CreateCodecP)();
struct CCodecInfo
{
  CreateCodecP CreateDecoder;
  CreateCodecP CreateEncoder;
  CMethodId Id;
  const wchar_t *Name;
  UInt32 NumInStreams;
  bool IsFilter;
};

void RegisterCodec(const CCodecInfo *codecInfo);

#define REGISTER_CODEC(x) CRegisterCodec##x::CRegisterCodec##x() { RegisterCodec(&g_CodecInfo); } \
    CRegisterCodec##x g_RegisterCodec##x;

#define REGISTER_CODECS(x) CRegisterCodecs##x::CRegisterCodecs##x() { \
	for(int i=0;i<sizeof(g_CodecsInfo)/sizeof(*g_CodecsInfo);i++) \
		RegisterCodec(&g_CodecsInfo[i]); } \
	CRegisterCodecs##x g_RegisterCodecs##x;

#endif
