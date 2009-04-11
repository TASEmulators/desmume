#ifndef _AFXRES_H
#define _AFXRES_H
#if __GNUC__ >= 3
#pragma GCC system_header
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WINDOWS_H
#include <windows.h>
#endif

#ifndef _INC_COMMCTRL
#include <commctrl.h>
#endif

#undef SUBLANG_DANISH_DENMARK
#define SUBLANG_DANISH_DENMARK SUBLANG_DEFAULT

#undef SUBLANG_ENGLISH_US
#define SUBLANG_ENGLISH_US SUBLANG_DEFAULT

#undef SUBLANG_FRENCH
#define SUBLANG_FRENCH SUBLANG_DEFAULT

/* IDC_STATIC is documented in winuser.h, but not defined. */
#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

#ifdef __cplusplus
}
#endif
#endif
