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

/* IDC_STATIC is documented in winuser.h, but not defined. */
#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

#ifndef WC_LISTVIEW
#define WC_LISTVIEW "SysListView32"
#endif

#ifndef UPDOWN_CLASS
#define UPDOWN_CLASS "msctls_updown32"
#endif

#ifdef __cplusplus
}
#endif
#endif
