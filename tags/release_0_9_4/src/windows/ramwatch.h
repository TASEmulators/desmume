//RamWatch dialog was copied and adapted from GENS11: http://code.google.com/p/gens-rerecording/
//Authors: Upthorn, Nitsuja, adelikat

#ifndef RAMWATCH_H
#define RAMWATCH_H
#include "windows.h"
bool ResetWatches();
void OpenRWRecentFile(int memwRFileNumber);
extern bool AutoRWLoad;
extern bool RWSaveWindowPos;
#define MAX_RECENT_WATCHES 5
extern char rw_recent_files[MAX_RECENT_WATCHES][1024];
extern bool AskSave();
extern int ramw_x;
extern int ramw_y;
extern bool RWfileChanged;

// AddressWatcher is self-contained now
struct AddressWatcher
{
	unsigned int Address; // hardware address
	char Size;
	char Type;
	char* comment; // NULL means no comment, non-NULL means allocated comment
	bool WrongEndian;
	unsigned int CurValue;
};
#define MAX_WATCH_COUNT 256
extern AddressWatcher rswatches[MAX_WATCH_COUNT];
extern int WatchCount; // number of valid items in rswatches

extern char Watch_Dir[1024];

bool InsertWatch(const AddressWatcher& Watch, char *Comment);
bool InsertWatch(const AddressWatcher& Watch, HWND parent=NULL); // asks user for comment
void Update_RAM_Watch();
bool Load_Watches(bool clear, const char* filename);

LRESULT CALLBACK RamWatchProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern HWND RamWatchHWnd;

#endif
