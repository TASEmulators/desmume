#ifndef NULLSOFT_WINAMP_GEN_H
#define NULLSOFT_WINAMP_GEN_H

#include <windows.h>

#define GEN_INIT_SUCCESS 0

// return values from the winampUninstallPlugin(HINSTANCE hdll, HWND parent, int param)
// which determine if we can uninstall the plugin immediately or on winamp restart
//
// uninstall support was added from 5.0+ and uninstall now support from 5.5+
// it is down to you to ensure that if uninstall now is returned that it will not cause a crash
// (ie don't use if you've been subclassing the main window)
#define GEN_PLUGIN_UNINSTALL_NOW    0x1
#define GEN_PLUGIN_UNINSTALL_REBOOT 0x0

typedef struct {
	int version;
	char *description;
	int (*init)();
	void (*config)();
	void (*quit)();
	HWND hwndParent;
	HINSTANCE hDllInstance;
} winampGeneralPurposePlugin;

#define GPPHDR_VER 0x10
#ifdef __cplusplus
extern "C" {
#endif
//extern winampGeneralPurposePlugin *gen_plugins[256];
typedef winampGeneralPurposePlugin * (*winampGeneralPurposePluginGetter)();
#ifdef __cplusplus
}
#endif

#endif