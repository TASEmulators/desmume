#ifndef _MAIN_H_
#define _MAIN_H_

#include "CWindow.h"
extern WINCLASS	*MainWindow;


enum HotkeyPage {
	HOTKEY_PAGE_TOOLS=0,
	HOTKEY_PAGE_PLACEHOLDER=1,
	NUM_HOTKEY_PAGE,
};

static LPCTSTR hotkeyPageTitle[] = {
	_T("Tools"),
	_T("Placeholder"),
	_T("NUM_HOTKEY_PAGE"),
};



struct SCustomKey {
	typedef void (*THandler) (void);
	WORD key;
	WORD modifiers;
	THandler handleKeyDown;
	THandler handleKeyUp;
	HotkeyPage page;
	LPCTSTR name;
	const char* code;
	//HotkeyTiming timing;
};

union SCustomKeys {
	struct {
		SCustomKey PrintScreen;
		SCustomKey LastItem; // dummy, must be last
	};
	SCustomKey key[];
};

extern SCustomKeys CustomKeys;
bool IsLastCustomKey (const SCustomKey *key);
void CopyCustomKeys (SCustomKeys *dst, const SCustomKeys *src);
void InitCustomKeys (SCustomKeys *keys);
int GetModifiers(int key);




#endif
