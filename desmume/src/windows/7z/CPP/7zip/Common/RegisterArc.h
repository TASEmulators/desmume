// RegisterArc.h

#ifndef __REGISTERARC_H
#define __REGISTERARC_H

#include "../Archive/IArchive.h"
#include "DeclareArcs.h"

typedef IInArchive * (*CreateInArchiveP)();
typedef IOutArchive * (*CreateOutArchiveP)();

struct CArcInfo
{
  const wchar_t *Name;
  const wchar_t *Ext;
  const wchar_t *AddExt;
  Byte ClassId;
  Byte Signature[16];
  int SignatureSize;
  bool KeepName;
  CreateInArchiveP CreateInArchive;
  CreateOutArchiveP CreateOutArchive;
};

void RegisterArc(const CArcInfo *arcInfo);

#define REGISTER_ARC(x) CRegister##x::CRegister##x() { RegisterArc(&g_ArcInfo); } \
    CRegister##x g_RegisterArc##x;

#define REGISTER_ARCN(x,n) CRegister##x##n::CRegister##x##n() { RegisterArc(&g_ArcInfo##n); } \
	CRegister##x##n g_RegisterArc##n##x;

#endif
