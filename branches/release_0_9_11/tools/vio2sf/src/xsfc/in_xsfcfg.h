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

void addInstrument(unsigned long addr, int type);
BOOL isInstrumentMuted(unsigned long addr);
unsigned long getInstrumentVolume(unsigned long addr);
BOOL isInstrumentSelectionActive();
void openSoundView(void* callback);

#ifdef __cplusplus
}
#endif

