#ifndef DESMUME_DXERR8_COMPAT_H
#define DESMUME_DXERR8_COMPAT_H

// Local declarations for the DXGetErrorString8 family. Modern Windows SDKs
// no longer ship dxerr8.h; dxerr8_compat.cpp supplies the implementation.

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

const char* WINAPI DXGetErrorString8A(HRESULT hr);
const WCHAR* WINAPI DXGetErrorString8W(HRESULT hr);
const char* WINAPI DXGetErrorDescription8A(HRESULT hr);
const WCHAR* WINAPI DXGetErrorDescription8W(HRESULT hr);
HRESULT WINAPI DXTraceA(const char* strFile, DWORD dwLine, HRESULT hr, const char* strMsg, BOOL bPopMsgBox);
HRESULT WINAPI DXTraceW(const char* strFile, DWORD dwLine, HRESULT hr, const WCHAR* strMsg, BOOL bPopMsgBox);

#ifdef UNICODE
#define DXGetErrorString8 DXGetErrorString8W
#define DXGetErrorDescription8 DXGetErrorDescription8W
#define DXTrace DXTraceW
#else
#define DXGetErrorString8 DXGetErrorString8A
#define DXGetErrorDescription8 DXGetErrorDescription8A
#define DXTrace DXTraceA
#endif

#ifdef __cplusplus
}
#endif

#endif
