
#ifndef CONFIGKEYS_H
#define CONFIGKEYS_H

extern unsigned long keytab[12];
extern const DWORD tabkey[48];
extern DWORD ds_up;
extern DWORD ds_down;
extern DWORD ds_left;
extern DWORD ds_right;
extern DWORD ds_a;
extern DWORD ds_b;
extern DWORD ds_x;
extern DWORD ds_y;
extern DWORD ds_l;
extern DWORD ds_r;
extern DWORD ds_select;
extern DWORD ds_start;
extern DWORD ds_debug;

void GetINIPath(char *initpath);
void  ReadConfig(void);

BOOL CALLBACK ConfigView_Proc(HWND dialog,UINT komunikat,WPARAM wparam,LPARAM lparam);

#endif
