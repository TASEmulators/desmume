#include "xsfcfg.h"

#ifdef __cplusplus
extern "C"
{
#endif

void winamp_config_init(HINSTANCE hinstDLL);
void winamp_config_load(HWND hwndWinamp);
void winamp_config_add_prefs(HWND hwndWinamp);
void winamp_config_remove_prefs(HWND hwndWinamp);
void winamp_config_dialog(HWND hwndWinamp, HWND hwndParent);

#ifdef __cplusplus
}
#endif

