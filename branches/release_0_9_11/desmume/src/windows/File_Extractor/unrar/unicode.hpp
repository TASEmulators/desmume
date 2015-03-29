#ifndef _RAR_UNICODE_
#define _RAR_UNICODE_

bool WideToChar(const wchar *Src,char *Dest,int DestSize=0x1000000);
void UtfToWide(const char *Src,wchar *Dest,int DestSize);

// strfn.cpp
void ExtToInt(const char *Src,char *Dest);

#endif
