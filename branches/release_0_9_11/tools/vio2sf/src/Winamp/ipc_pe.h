#ifndef __IPC_PE_H
#define __IPC_PE_H

#define IPC_PE_GETCURINDEX        100 // returns current idx
#define IPC_PE_GETINDEXTOTAL      101 // returns number of items 
#define IPC_PE_GETINDEXINFO       102 // (copydata) lpData is of type callbackinfo, callback is called with copydata/fileinfo structure and msg IPC_PE_GETINDEXINFORESULT
#define IPC_PE_GETINDEXINFORESULT 103 // callback message for IPC_PE_GETINDEXINFO
#define IPC_PE_DELETEINDEX        104 // lParam = index
#define IPC_PE_SWAPINDEX          105 // (lParam & 0xFFFF0000) >> 16 = from, (lParam & 0xFFFF) = to
#define IPC_PE_INSERTFILENAME     106 // (copydata) lpData is of type fileinfo
#define IPC_PE_GETDIRTY           107 // returns 1 if the playlist changed since the last IPC_PE_SETCLEAN
#define IPC_PE_SETCLEAN	          108 // resets the dirty flag until next modification
#define IPC_PE_GETIDXFROMPOINT    109 // pass a point parm, return a playlist index
#define IPC_PE_SAVEEND            110 // pass index to save from
#define IPC_PE_RESTOREEND         111 // no parm
#define IPC_PE_GETNEXTSELECTED    112 // same as IPC_PLAYLIST_GET_NEXT_SELECTED for the main window
#define IPC_PE_GETSELECTEDCOUNT   113
#define IPC_PE_INSERTFILENAMEW  114 // (copydata) lpData is of type fileinfoW
#define IPC_PE_GETINDEXINFO_TITLE 115 // like IPC_PE_GETINDEXINFO, but writes the title to char file[MAX_PATH] instead of filename
#define IPC_PE_GETINDEXINFORESULT_TITLE 116 // callback message for IPC_PE_GETINDEXINFO
typedef struct {
	char file[MAX_PATH];
	int index;
	} fileinfo;

typedef struct {
	wchar_t file[MAX_PATH];
	int index;
	} fileinfoW;

typedef struct {
	HWND callback;
	int index;
	} callbackinfo;

// the following messages are in_process ONLY

#define IPC_PE_GETINDEXTITLE      200 // lParam = pointer to fileinfo2 struct
#define IPC_PE_GETINDEXTITLEW      201 // lParam = pointer to fileinfo2W struct
#define IPC_PE_GETINDEXINFO_INPROC      202 // lParam = pointer to fileinfo struct
#define IPC_PE_GETINDEXINFOW_INPROC      203 // lParam = pointer to fileinfoW struct

typedef struct {
  int fileindex;
  char filetitle[256];
  char filelength[16];
  } fileinfo2;

typedef struct
{
  int fileindex;
  wchar_t filetitle[256];
  wchar_t filelength[16];
  } fileinfo2W;

#endif