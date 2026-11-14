/*
	Copyright (C) 2006-2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

// dsmeditor.cpp : the recorded input of a .dsm as a frame/input grid.
//
// This is a port of VBA-RR's VbmEditor to the Nintendo DS: same contract (the
// window owns the movie's input timeline and nothing else), same shape of the
// window (virtual list, transport that forwards to the main window, marks,
// clipboard in a plain CSV), but the columns are the ones a .dsm record
// actually has: thirteen pad bits, the stylus, the microphone button, the lid
// and - when the movie contains one - the reset command.
//
// Two things differ from the VBA original on purpose:
//
//   * Restructuring the timeline - insert, delete, paste, cut - needs the
//     emulator paused.  The emulator reads records[currFrameCounter] once per
//     frame, so moving frames around while it runs would replay a different
//     timeline from one frame to the next.  Rewriting one record is always
//     allowed: the emulation runs on this same thread, between two message
//     pumps, so nothing is reading the record vector while a cell is edited.
//   * A .dsm is not rewritten frame by frame the way a .vbm is, so the window
//     owns an explicit Save: the edits live in memory (and in the running
//     playback) until the file is written back.
//
// Read the contract above FCEUMOV_SetRecord() in movie.cpp before changing
// anything here.

#include "dsmeditor.h"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <set>
#include <string>
#include <vector>

#include "types.h"
#include "movie.h"
#include "display.h"	// emu_paused
#include "main.h"
#include "resource.h"
#include "UiFont.h"
#include "dsmcsv.h"

extern char curMovieFilename[512];

namespace
{

// The window size survives closing and reopening the editor within one run of
// the program.  It is deliberately not written to the settings file: how much
// of the screen the grid should cover is not a preference.
SIZE g_lastSize = { 0, 0 };

// Light colours on purpose: the dialog family in this frontend does not follow
// the system dark mode (see DarkMode.h), so a dark grid would be the odd one
// out.
const COLORREF kPlayheadColor = RGB(198, 224, 255);
const COLORREF kMarkColor	  = RGB(255, 240, 190);

// The DS draws 59.8261 frames a second - 33513982 Hz / 6 / 355 lines / 263
// dots - and replay.cpp already prints movie length that way, so the timestamp
// column uses the same number instead of 60.
const double kFramesPerSecond = 33513982.0 / 6.0 / 355.0 / 263.0;

const int kRefreshMs	= 50;
const int kTimerId		= 1;
const int kListSubclass = 1;

// The grid is a strip of numbers and single letters; anything past this is a
// paste that would spend more time allocating than the user spends waiting.
const u32 kMaxMovieFrames = 4000000;

inline int imax(int a, int b) { return (a > b) ? a : b; }

// HH:mm:ss.SS for a frame number, computed the way replay.cpp computes the
// movie length: seconds = frames / frame rate, the fraction truncated to
// hundredths.
void formatStamp(char *buffer, size_t count, u32 row)
{
	const double total = (double)row / kFramesPerSecond;

	const u32 whole	  = (u32)total;
	const u32 frac	  = (u32)((total - (double)whole) * 100.0);
	const u32 seconds = whole % 60;
	const u32 minutes = (whole / 60) % 60;
	const u32 hours	  = whole / 3600;

	snprintf(buffer, count, "%02u:%02u:%02u.%02u", hours, minutes, seconds, frac);
	buffer[count - 1] = 0;
}

enum ColumnKind
{
	COL_PLAYHEAD = 0,	// ">" on the row the playhead is on
	COL_FRAME,			// the frame number
	COL_TIME,			// HH:mm:ss.SS of that frame
	COL_PAD,			// one of the thirteen pad bits
	COL_TOUCH,			// the stylus: x,y while it is down
	COL_MIC,			// MOVIECMD_MIC
	COL_LID,			// MOVIECMD_LID
	COL_RESET			// MOVIECMD_RESET - shown, never editable
};

const int COL_FIRST_KEY = 3;

struct Column
{
	int kind;
	u16 mask;			// COL_PAD only
	char label[8];		// header text, and the mark the cell shows
	int width;
};

// The pad bits in the order the .dsm stores them, which is also the order the
// emulator's own input display uses: R, L, D, U, T (select), S (start), B, A,
// Y, X, W and E (the shoulders) and G (the debug button).
struct PadKey
{
	u16		   mask;
	const char *label;
};

const PadKey kPadKeys[] = {
	{ 1 << 12, "R" },
	{ 1 << 11, "L" },
	{ 1 << 10, "D" },
	{ 1 <<  9, "U" },
	{ 1 <<  8, "T" },
	{ 1 <<  7, "S" },
	{ 1 <<  6, "B" },
	{ 1 <<  5, "A" },
	{ 1 <<  4, "Y" },
	{ 1 <<  3, "X" },
	{ 1 <<  2, "W" },
	{ 1 <<  1, "E" },
	{ 1 <<  0, "G" },
};

const int kPadKeyCount = int(sizeof(kPadKeys) / sizeof(kPadKeys[0]));

// The touch prompt.  The movie stores the stylus position in screen pixels,
// one byte per axis, so the dialog edits exactly what the file carries.
struct TouchRequest
{
	int	 x;
	int	 y;
	bool down;
};

} // namespace

LRESULT CALLBACK listSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                  UINT_PTR id, DWORD_PTR ref);
INT_PTR CALLBACK touchDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

///////////////////////////////////////////////////////////////////////////////
// the window

class DsmEditor
{
public:
	DsmEditor();
	~DsmEditor();

	INT_PTR handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	HWND hwnd() const { return m_hwnd; }

private:
	enum { NO_ROW = 0xFFFFFFFFu };

	// state
	void refresh();
	void resetView();
	void rebuildColumns(const MovieInputUsage &usage);
	void fitColumns();
	void updateControls();
	void updateTitle();

	// layout
	void captureLayout();
	void layoutChildren(int cx, int cy);
	void updateHeightFloor();
	int	 listHeaderHeight() const;
	RECT controlRect(int id) const;
	void moveControl(int id, const RECT &rect);

	bool editable() const;
	bool canRestructure() const;
	void denyRestructure();
	u32	 playheadRow() const;
	int	 focusedRow() const;
	std::vector<u32> selectedRows() const;
	void selectOnlyRow(u32 row);
	void setStatus(const char *text);
	void setStatusf(const char *format, ...);
	void repaintRow(u32 row);
	void followPlayhead(u32 row);
	void revealRow(u32 row, bool center);
	void syncFrameBox();
	void applyFrameBox();
	int	 rowHeight();

	// editing
	void toggleCell(u32 row, int column);
	void touchDefaults(u32 row, u8 &x, u8 &y) const;
	bool promptTouch(u32 row);
	void insertRows(u32 at, u32 count);
	bool deleteRows(const std::vector<u32> &rows);
	void deleteSelection();
	void insertSelection(bool after);
	void toggleMarkSelection();
	void gotoNeighbourMark(int direction);
	// The point is the list's client coordinates; showContextMenu() converts.
	void showContextMenu(POINT client);

	void marksInserted(u32 at, u32 count);
	void marksDeleted(const std::vector<u32> &rows);

	// clipboard
	enum ClipResult { CLIP_OK, CLIP_UNAVAILABLE, CLIP_TOO_LARGE };

	bool writeClipboard(const std::wstring &text);
	ClipResult readClipboard(std::wstring &text);
	void refreshClipboard();
	void copySelection(bool cut);
	void paste();
	void reportClipboardFailure();

	void saveMovie(bool saveAs);

	// notifications
	INT_PTR onCommand(int id, int code);
	INT_PTR onNotify(const NMHDR *header);
	void	onGetDispInfo(NMLVDISPINFOA *info);
	void	onGetDispInfoW(NMLVDISPINFOW *info);
	void	formatCell(u32 row, int column, char *buffer, size_t count) const;
	LRESULT onCustomDraw(NMLVCUSTOMDRAW *draw);
	void	onItemChanged(const NMLISTVIEW *info);
	void	onListClick(const NMITEMACTIVATE *info, bool right, bool doubleClick);

	HWND m_hwnd;
	HWND m_list;

	std::vector<Column> m_columns;
	std::set<u32>		m_marks;

	// Last seen movie state; every tick is diffed against this.
	bool			m_active;
	int				m_length;
	int				m_frame;
	u32				m_generation;
	std::string		m_identity;
	MovieInputUsage m_usage;

	bool m_editLock;	// the "Read-only" checkbox this window owns
	bool m_follow;
	bool m_dirty;		// the file on disk does not have these edits

	// What updateControls() applied last time, so the tick leaves the controls
	// alone when nothing changed (calling EnableWindow twenty times a second
	// makes the buttons flicker).  m_uiChecks packs the read-only mark in bit
	// 0, the follow mark in bit 1 and the read-only box's enabled state in bit
	// 2, because the mark has two sources while only one of them greys the box.
	int m_uiChecks;
	int m_uiEnable;
	int m_focusRow;

	struct Layout
	{
		SIZE client;
		RECT list;
		RECT hint;
		RECT status;
		RECT gotoLabel;
		RECT frameBox;
		RECT prevMark;
		RECT nextMark;
	};

	// m_layout is the last geometry read back from the controls (taken when
	// the dialog opens and after a DPI change); every WM_SIZE is applied as a
	// delta against it.  m_minTrack is the smallest window the frame allows,
	// m_fixedHeight everything in the window that is not the grid - so the
	// height floor is that plus one column header.  m_fittedWidth and
	// m_fittedFont are what the columns were last measured for.
	Layout m_layout;
	UINT   m_layoutDpi;
	SIZE   m_minTrack;
	SIZE   m_appliedClient;	// client size the children are currently laid out for
	int	   m_fixedHeight;
	int	   m_fittedWidth;
	HFONT  m_fittedFont;

	// What the clipboard held the last time it was looked at, keyed by the
	// clipboard sequence number so a large paste is parsed once and not once
	// per menu build.
	DWORD					 m_clipSequence;
	dsmcsv::Result			 m_clipResult;
	int						 m_clipBadLine;
	std::vector<dsmcsv::Row> m_clipRows;

	std::string m_status;
	std::string m_title;
	char		m_cellTextA[32];
	wchar_t		m_cellTextW[32];
};

static DsmEditor *g_editor = NULL;

DsmEditor::DsmEditor()
	: m_hwnd(NULL)
	, m_list(NULL)
	, m_active(false)
	, m_length(0)
	, m_frame(0)
	, m_generation(0xFFFFFFFFu)
	// A movie window opens locked, the same way File > Play Movie opens a
	// movie read-only: looking at a timeline should not be able to change it
	// by accident.  The box is this window's own switch, so clearing it is
	// always one click away.
	, m_editLock(true)
	, m_follow(true)
	, m_dirty(false)
	, m_uiChecks(-1)
	, m_uiEnable(-1)
	, m_focusRow(-1)
	, m_layoutDpi(0)
	, m_fixedHeight(0)
	, m_fittedWidth(0)
	, m_fittedFont(NULL)
	, m_clipSequence(0)
	, m_clipResult(dsmcsv::Result::Empty)
	, m_clipBadLine(0)
{
	m_usage.padMask = 0;
	m_usage.commandMask = 0;
	m_usage.touch = false;
	m_usage.micSample = false;

	m_layout.client.cx = 0;
	m_layout.client.cy = 0;
	ZeroMemory(&m_layout.list, sizeof(RECT));
	ZeroMemory(&m_layout.hint, sizeof(RECT));
	ZeroMemory(&m_layout.status, sizeof(RECT));
	ZeroMemory(&m_layout.gotoLabel, sizeof(RECT));
	ZeroMemory(&m_layout.frameBox, sizeof(RECT));
	ZeroMemory(&m_layout.prevMark, sizeof(RECT));
	ZeroMemory(&m_layout.nextMark, sizeof(RECT));

	m_minTrack.cx = 0;
	m_minTrack.cy = 0;

	m_appliedClient.cx = 0;
	m_appliedClient.cy = 0;

	m_cellTextA[0] = 0;
	m_cellTextW[0] = 0;
}

DsmEditor::~DsmEditor()
{
}

///////////////////////////////////////////////////////////////////////////////
// messages

INT_PTR DsmEditor::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
	{
		m_hwnd = hwnd;
		m_list = GetDlgItem(hwnd, IDC_DSM_LIST);

		// A virtual list keeps no per-item state, which is the whole point: a
		// two-hour movie is one SetItemCountEx() and nothing else.
		ListView_SetExtendedListViewStyle(m_list,
			ListView_GetExtendedListViewStyle(m_list)
			| LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

		SetWindowSubclass(m_list, listSubclassProc, kListSubclass, 0);

		SetDlgItemTextW(hwnd, IDC_DSM_HINT,
			L"Click a cell to toggle that input on that frame; double-click the stylus cell to type an exact "
			L"position. Right-click a row for frame commands, and press M to mark the selected rows. The Frame "
			L"box scrolls the grid - it does not seek the emulator.");

		// The size the template was drawn at gives the narrowest window the
		// frame allows; taking it from the live window rather than from dialog
		// units keeps it right at any DPI.
		RECT window;
		GetWindowRect(hwnd, &window);
		m_minTrack.cx = window.right - window.left;
		m_minTrack.cy = window.bottom - window.top;
		captureLayout();

		// Reopening the editor in the same run keeps the size the user dragged
		// it to, but never below the floor.
		int restoreWidth  = g_lastSize.cx;
		int restoreHeight = g_lastSize.cy;
		if (restoreWidth > 0 && restoreHeight > 0)
		{
			if (restoreWidth < m_minTrack.cx)
				restoreWidth = m_minTrack.cx;
			if (restoreHeight < m_minTrack.cy)
				restoreHeight = m_minTrack.cy;

			if (restoreWidth != window.right - window.left
			    || restoreHeight != window.bottom - window.top)
			{
				SetWindowPos(hwnd, NULL, 0, 0, restoreWidth, restoreHeight,
				             SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
			}
		}

		SetTimer(hwnd, kTimerId, kRefreshMs, NULL);

		refresh();

		// The window opens locked, so say why the first click on a cell will do
		// nothing.  refresh() only writes the status line when no movie is
		// loaded, so this survives the ticks that follow.
		if (m_active && m_editLock)
			setStatus("Read-only: clear the Read-only box to edit the timeline.");

		SetFocus(m_list);
		return TRUE;
	}

	case WM_TIMER:
		if (wParam == kTimerId)
			refresh();
		return TRUE;

	case WM_SIZE:
		if (m_list)
		{
			const int cx = LOWORD(lParam);
			const int cy = HIWORD(lParam);

			if (wParam != SIZE_MINIMIZED && cx > 0 && cy > 0 && m_layout.client.cx > 0)
			{
				// A per-monitor DPI change resizes the window before anything
				// is told about it: UiFont.cpp scales the children, then
				// applies the suggested rect (and swallows the WM_DPICHANGED
				// itself, so this is the only place that sees it).  The
				// controls are already in their new size here, so this size is
				// taken as the new baseline instead of as a delta on top of the
				// scaling, and the width floor follows the ratio.
				const UINT dpi = uiFont::dpiOf(m_hwnd);
				if (dpi != m_layoutDpi)
				{
					if (dpi != 0 && m_layoutDpi != 0)
						m_minTrack.cx = MulDiv(m_minTrack.cx, dpi, m_layoutDpi);
					captureLayout();
				}
				else
				{
					layoutChildren(cx, cy);
				}

				// Every key column shares whatever the three fixed ones leave
				// over, so a change in width has to be measured again.  A
				// vertical drag does not, and re-measuring per pixel would
				// repaint the grid for nothing.
				RECT area;
				GetClientRect(m_list, &area);
				if (area.right != m_fittedWidth)
				{
					SendMessage(m_list, WM_SETREDRAW, FALSE, 0);
					fitColumns();
					SendMessage(m_list, WM_SETREDRAW, TRUE, 0);
					InvalidateRect(m_list, NULL, FALSE);
				}
			}
		}
		return TRUE;

	case WM_EXITSIZEMOVE:
		// The sizing loop repaints as it goes, but a fast drag can drop a
		// paint; one full redraw when the mouse is released guarantees the
		// window settles without a leftover copy of anything.
		RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
		return TRUE;

	case WM_GETMINMAXINFO:
	{
		MINMAXINFO *info = (MINMAXINFO *)lParam;
		DefWindowProc(hwnd, msg, wParam, lParam);

		// Below the template width the frame box, the mark buttons and the
		// columns would be clipped sideways, and there is no horizontal
		// scrolling to fall back on.  The height is floored at the point where
		// the grid is down to its column header.
		if (m_minTrack.cx > 0)
			info->ptMinTrackSize.x = m_minTrack.cx;
		if (m_minTrack.cy > 0)
			info->ptMinTrackSize.y = m_minTrack.cy;
		return TRUE;
	}

	case WM_COMMAND:
		return onCommand(LOWORD(wParam), HIWORD(wParam));

	case WM_NOTIFY:
		return onNotify((const NMHDR *)lParam);

	case WM_CLOSE:
		DestroyWindow(hwnd);
		return TRUE;

	case WM_DESTROY:
		KillTimer(hwnd, kTimerId);

		{
			RECT window;
			GetWindowRect(hwnd, &window);
			g_lastSize.cx = window.right - window.left;
			g_lastSize.cy = window.bottom - window.top;
		}
		return TRUE;

	case WM_NCDESTROY:
	{
		// The window is gone; the object that ran it is not needed any more.
		// g_editor still points at it here, so WM_DESTROY above may not clear
		// the pointer: this is the only place that owns the lifetime.
		DsmEditor *editor = g_editor;
		g_editor = NULL;
		if (editor == this)
			delete this;
		return TRUE;
	}
	}

	return FALSE;
}

INT_PTR DsmEditor::onCommand(int id, int code)
{
	(void)code;

	switch (id)
	{
	case IDOK:
		// Enter belongs to the frame box; a modeless dialog must not close on it.
		applyFrameBox();
		return TRUE;

	case IDCANCEL:
		DestroyWindow(m_hwnd);
		return TRUE;

	case IDC_DSM_PLAY:
		if (m_active && emu_paused)
		{
			Unpause();
			refresh();
		}
		return TRUE;

	case IDC_DSM_PAUSE:
		if (m_active && !emu_paused)
		{
			Pause();
			refresh();
		}
		return TRUE;

	case IDC_DSM_STEP:
		// The main window owns the one playback path: this is the command its
		// own Frame Advance button sends, and the main loop halts again after
		// exactly one frame.
		if (m_active)
		{
			HWND main = MainWindow ? MainWindow->getHWnd() : NULL;
			if (main)
				SendMessage(main, WM_COMMAND, MAKEWPARAM(IDC_FRAMEADVANCE, 0), 0);
			refresh();
		}
		return TRUE;

	case IDC_DSM_GOTOPLAYHEAD:
	{
		// Scrolls the grid back to the row the emulator is on.  The window can
		// be scrolled anywhere - a movie that has been running for an hour is
		// thousands of rows away from where it started - so this is the way
		// back without hunting for the blue row.
		const u32 head = playheadRow();
		if (head == NO_ROW)
		{
			setStatus("No movie is loaded.");
		}
		else
		{
			revealRow(head, true);
			setStatusf("Showing frame %u, the frame the emulator is on.", head);
		}
		return TRUE;
	}

	case IDC_DSM_READONLY:
		m_editLock = IsDlgButtonChecked(m_hwnd, IDC_DSM_READONLY) != 0;
		updateControls();

		if (m_editLock)
			setStatus("Read-only: the timeline cannot be changed. Clear the box to edit.");
		else
			setStatus("Editing enabled. Pause the emulator to insert or delete frames.");
		return TRUE;

	case IDC_DSM_FOLLOW:
		m_follow = IsDlgButtonChecked(m_hwnd, IDC_DSM_FOLLOW) != 0;
		return TRUE;

	case IDC_DSM_SAVE:
		saveMovie(false);
		return TRUE;

	case IDC_DSM_PREVMARK:
		gotoNeighbourMark(-1);
		return TRUE;

	case IDC_DSM_NEXTMARK:
		gotoNeighbourMark(1);
		return TRUE;

	// The list subclass turns keys into these, so the menu, the keys and the
	// context menu all run the same code.
	case CMD_DSM_MARK:
		toggleMarkSelection();
		return TRUE;

	case CMD_DSM_INSERT:
		insertSelection(false);
		return TRUE;

	case CMD_DSM_INSERT_AFTER:
		insertSelection(true);
		return TRUE;

	case CMD_DSM_DELETE:
		deleteSelection();
		return TRUE;

	case CMD_DSM_CUT:
		copySelection(true);
		return TRUE;

	case CMD_DSM_COPY:
		copySelection(false);
		return TRUE;

	case CMD_DSM_PASTE:
		paste();
		return TRUE;

	case CMD_DSM_TOUCH:
	{
		const int row = focusedRow();
		if (row < 0)
			setStatus("Select a frame first.");
		else
			promptTouch((u32)row);
		return TRUE;
	}

	case CMD_DSM_SAVE:
		saveMovie(false);
		return TRUE;

	case CMD_DSM_SAVE_AS:
		saveMovie(true);
		return TRUE;
	}

	return FALSE;
}

INT_PTR DsmEditor::onNotify(const NMHDR *header)
{
	if (header->hwndFrom != m_list)
		return FALSE;

	switch (header->code)
	{
	// The list is a Unicode window (the dialog is created with CreateDialogW),
	// so the wide notification is the one that arrives; the narrow one is here
	// so an ANSI build of this file still works.
	case LVN_GETDISPINFOW:
		onGetDispInfoW((NMLVDISPINFOW *)header);
		return TRUE;

	case LVN_GETDISPINFOA:
		onGetDispInfo((NMLVDISPINFOA *)header);
		return TRUE;

	case NM_CUSTOMDRAW:
		SetWindowLongPtr(m_hwnd, DWLP_MSGRESULT, onCustomDraw((NMLVCUSTOMDRAW *)header));
		return TRUE;

	case LVN_ITEMCHANGED:
		onItemChanged((const NMLISTVIEW *)header);
		return TRUE;

	case NM_CLICK:
		onListClick((const NMITEMACTIVATE *)header, false, false);
		return TRUE;

	case NM_RCLICK:
		onListClick((const NMITEMACTIVATE *)header, true, false);
		return TRUE;

	case NM_DBLCLK:
		onListClick((const NMITEMACTIVATE *)header, false, true);
		return TRUE;
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// the list subclass
//
// 'M' marks the selection, and it has to be taken away from the list before the
// list sees it: a list view treats a typed character as an incremental search,
// and for an owner-data list that means asking the parent for the matching
// row.  Nobody answers, the control takes the default -1-as-0, and it *selects
// row 0* - so the multi-selection was gone by the time the key was done.
// Swallowing it here costs nothing: this window owns the key, the grid never
// searches.

LRESULT CALLBACK listSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                  UINT_PTR id, DWORD_PTR ref)
{
	(void)id;
	(void)ref;

	if (msg == WM_NCDESTROY)
		RemoveWindowSubclass(hwnd, listSubclassProc, kListSubclass);

	if (msg == WM_KEYDOWN && g_editor)
	{
		HWND	   parent = GetParent(hwnd);
		const bool ctrl	  = (GetKeyState(VK_CONTROL) < 0);

		switch (wParam)
		{
		case 'M':
			if (!ctrl)
			{
				SendMessage(parent, WM_COMMAND, MAKEWPARAM(CMD_DSM_MARK, 0), 0);
				return 0;
			}
			break;

		case 'C':
			if (ctrl)
			{
				SendMessage(parent, WM_COMMAND, MAKEWPARAM(CMD_DSM_COPY, 0), 0);
				return 0;
			}
			break;

		case 'X':
			if (ctrl)
			{
				SendMessage(parent, WM_COMMAND, MAKEWPARAM(CMD_DSM_CUT, 0), 0);
				return 0;
			}
			break;

		case 'V':
			if (ctrl)
			{
				SendMessage(parent, WM_COMMAND, MAKEWPARAM(CMD_DSM_PASTE, 0), 0);
				return 0;
			}
			break;

		case VK_DELETE:
			SendMessage(parent, WM_COMMAND, MAKEWPARAM(CMD_DSM_DELETE, 0), 0);
			return 0;

		case VK_INSERT:
			SendMessage(parent, WM_COMMAND, MAKEWPARAM(CMD_DSM_INSERT, 0), 0);
			return 0;
		}
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////////
// resizing
//
// The window has a thick frame: the grid takes every pixel that is added or
// removed, the hint and the status line stay glued to the bottom edge, and the
// frame box with its caption and the two mark buttons follow the right edge.
// Nothing is stretched sideways, so the layout at any size is the one the
// template describes plus a delta - which is also why captureLayout() only has
// to run again when the DPI changes.

RECT DsmEditor::controlRect(int id) const
{
	RECT rect;
	rect.left = rect.top = rect.right = rect.bottom = 0;

	if (HWND control = GetDlgItem(m_hwnd, id))
	{
		GetWindowRect(control, &rect);
		MapWindowPoints(HWND_DESKTOP, m_hwnd, (POINT *)&rect, 2);
	}

	return rect;
}

void DsmEditor::captureLayout()
{
	RECT client;
	GetClientRect(m_hwnd, &client);

	m_layout.client.cx = client.right;
	m_layout.client.cy = client.bottom;

	// The children are where this snapshot says they are, so this is also the
	// size they are laid out for.
	m_appliedClient = m_layout.client;
	m_layout.list	   = controlRect(IDC_DSM_LIST);
	m_layout.hint	   = controlRect(IDC_DSM_HINT);
	m_layout.status	   = controlRect(IDC_DSM_STATUS);
	m_layout.gotoLabel = controlRect(IDC_DSM_GOTOLABEL);
	m_layout.frameBox  = controlRect(IDC_DSM_FRAMENO);
	m_layout.prevMark  = controlRect(IDC_DSM_PREVMARK);
	m_layout.nextMark  = controlRect(IDC_DSM_NEXTMARK);

	m_layoutDpi = uiFont::dpiOf(m_hwnd);

	// Everything that is not the grid keeps its height while the window is
	// dragged, so it is a constant that can be measured once here.  It is
	// measured as a difference rather than added up from the controls so the
	// caption, the frame and any gap below the status line come along for free.
	RECT window;
	if (GetWindowRect(m_hwnd, &window))
		m_fixedHeight = (window.bottom - window.top) - (m_layout.list.bottom - m_layout.list.top);

	updateHeightFloor();
}

// The header of the list, in pixels: what the grid keeps when it is squeezed.
int DsmEditor::listHeaderHeight() const
{
	HWND header = (HWND)SendMessage(m_list, LVM_GETHEADER, 0, 0);
	RECT rect;
	if (!header || !GetWindowRect(header, &rect))
		return 0;

	return rect.bottom - rect.top;
}

void DsmEditor::updateHeightFloor()
{
	// The grid may give up every row it has, but not the header: below that the
	// columns are a strip of unlabelled numbers.  Because m_fixedHeight already
	// holds the caption, the transport row, the checkboxes and the two bottom
	// lines, this floor also keeps all of them on screen.
	const int header = listHeaderHeight();
	if (header > 0 && m_fixedHeight > 0)
		m_minTrack.cy = m_fixedHeight + header;
}

void DsmEditor::moveControl(int id, const RECT &rect)
{
	if (HWND control = GetDlgItem(m_hwnd, id))
	{
		// SWP_NOCOPYBITS: do not let the system blit the old contents to the
		// new size.  A stretched copy is what leaves half-drawn rows behind in
		// the list while the frame is being dragged.
		SetWindowPos(control, NULL, rect.left, rect.top,
		             rect.right - rect.left, rect.bottom - rect.top,
		             SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
	}
}

void DsmEditor::layoutChildren(int cx, int cy)
{
	// A drag delivers WM_SIZE faster than the window can paint, and a resize
	// that did not change the client size has nothing to lay out.
	if (cx == m_appliedClient.cx && cy == m_appliedClient.cy)
		return;

	m_appliedClient.cx = cx;
	m_appliedClient.cy = cy;

	const int dx = cx - m_layout.client.cx;
	const int dy = cy - m_layout.client.cy;

	// Every control is placed from the captured baseline plus the *total*
	// delta, never from where it happens to be now.  That keeps the result
	// identical no matter how many WM_SIZE messages the drag produced, and it
	// makes the round trip exact.
	RECT list = m_layout.list;
	list.right += dx;
	list.bottom += dy;

	if (list.right - list.left < 1)
		list.right = list.left + 1;
	if (list.bottom - list.top < 1)
		list.bottom = list.top + 1;

	// The two bottom lines follow the frame, but they must not climb over the
	// grid.
	int bottomDy = dy;
	if (m_layout.hint.top + bottomDy < list.top + 1)
		bottomDy = list.top + 1 - m_layout.hint.top;

	RECT hint	= m_layout.hint;
	RECT status = m_layout.status;
	hint.top += bottomDy;
	hint.bottom += bottomDy;
	hint.right += dx;
	status.top += bottomDy;
	status.bottom += bottomDy;
	status.right += dx;

	RECT label = m_layout.gotoLabel;
	RECT box   = m_layout.frameBox;
	RECT prev  = m_layout.prevMark;
	RECT next  = m_layout.nextMark;
	OffsetRect(&label, dx, 0);
	OffsetRect(&box, dx, 0);
	OffsetRect(&prev, dx, 0);
	OffsetRect(&next, dx, 0);

	moveControl(IDC_DSM_LIST, list);
	moveControl(IDC_DSM_HINT, hint);
	moveControl(IDC_DSM_STATUS, status);
	moveControl(IDC_DSM_GOTOLABEL, label);
	moveControl(IDC_DSM_FRAMENO, box);
	moveControl(IDC_DSM_PREVMARK, prev);
	moveControl(IDC_DSM_NEXTMARK, next);

	// Moving a child repaints the child and nothing else: the strip of dialog
	// the control used to cover is still holding its old pixels.  That is the
	// "ghost copy of the Frame label" - and it shows up only when the drag
	// outruns the paints, because otherwise the strip happens to be redrawn
	// for another reason.  Invalidating the dialog (WS_CLIPCHILDREN keeps the
	// children out of it) erases every strip in one go, once per resize.
	InvalidateRect(m_hwnd, NULL, TRUE);
}

///////////////////////////////////////////////////////////////////////////////
// state

void DsmEditor::refresh()
{
	if (!m_hwnd)
		return;

	const u32 oldHead = playheadRow();

	// A per-monitor DPI change and the "modern UI font" option both swap the
	// font under the list from outside this window (UiFont.cpp does it from a
	// subclass procedure), and the column widths are measured from that font.
	// The tick is the only place that reliably runs afterwards and it costs one
	// WM_GETFONT when nothing changed.
	HFONT font = (HFONT)SendMessage(m_list, WM_GETFONT, 0, 0);
	if (!m_columns.empty() && font != m_fittedFont)
	{
		updateHeightFloor();
		fitColumns();
	}

	// While a recording is running the movie belongs to the recorder: the file
	// is open for appending and the record vector grows once per frame, so there
	// is nothing here to edit and the grid would only be chasing the tail.  It
	// appears again as soon as the recording is stopped.
	const bool active = (movieMode == MOVIEMODE_PLAY || movieMode == MOVIEMODE_FINISHED);

	if (!active)
	{
		if (m_active || !m_columns.empty() || ListView_GetItemCount(m_list))
		{
			m_marks.clear();
			m_columns.clear();
			while (ListView_DeleteColumn(m_list, 0))
				;
			ListView_SetItemCountEx(m_list, 0, 0);
			InvalidateRect(m_list, NULL, FALSE);
		}

		m_active	 = false;
		m_length	 = 0;
		m_frame		 = 0;
		m_generation = 0xFFFFFFFFu;
		m_focusRow	 = -1;
		m_dirty		 = false;
		m_identity.clear();

		m_usage.padMask = 0;
		m_usage.commandMask = 0;
		m_usage.touch = false;
		m_usage.micSample = false;

		syncFrameBox();
		updateTitle();
		updateControls();

		if (movieMode == MOVIEMODE_RECORD)
			setStatus("Movie is recording - stop it to inspect the timeline.");
		else
			setStatus("No movie is loaded. Use File > Play Movie to open one.");
		return;
	}

	// A different movie took over the window: the marks belonged to the old
	// timeline and the focus row may not exist any more.
	const std::string identity = curMovieFilename;
	if (identity != m_identity)
	{
		m_identity = identity;
		m_marks.clear();
		m_dirty = false;
		resetView();
	}

	m_active = true;

	const int length	   = (int)FCEUMOV_GetLength();
	const u32 generation = FCEUMOV_GetGeneration();

	// The item count comes first: a virtual list only grows a vertical scroll
	// bar once it has rows, and fitColumns() has to measure the client area it
	// will actually have.
	//
	// An owner-data list stores no text of its own: every visible row is asked
	// for again when it paints, but nothing is repainted unless it is
	// invalidated.  A change in length moves rows that are already on screen -
	// an insert shifts everything below it down, a delete pulls it up - so the
	// whole client area has to be redrawn.
	//
	// There used to be a shortcut here for "exactly one frame was added", on
	// the theory that it must have been appended at the end.  It is not: an
	// insert of one frame in the middle produces the same count, and the
	// shortcut then repainted only the last row, leaving every row below the
	// insertion point showing what used to be there - which reads as the new
	// frame having overwritten its neighbour.
	if (length != m_length)
	{
		// m_length first: the paint that follows asks playheadRow() where the
		// arrow goes, and that clamps against the length.
		m_length = length;

		ListView_SetItemCountEx(m_list, length, 0);
		InvalidateRect(m_list, NULL, TRUE);

		// The caller is a menu command that does nothing else, and the grid is
		// the whole point of it: paint now rather than on the next tick.
		UpdateWindow(m_list);
	}

	// The column set only depends on the movie's contents, so it is rebuilt
	// only when the timeline was rewritten, and even then only if the set
	// really changed.
	if (generation != m_generation || m_columns.empty())
	{
		const MovieInputUsage usage = FCEUMOV_GetInputUsage();
		m_usage		 = usage;
		m_generation = generation;
		rebuildColumns(usage);
	}

	m_frame = currFrameCounter;

	// The focus is cached, so it has to be re-validated when the timeline
	// shrinks under it - a delete, a paste or another movie.
	if (m_focusRow >= 0 && m_focusRow >= m_length)
	{
		m_focusRow = -1;
		syncFrameBox();
	}

	// Nothing has been focused yet (a fresh window, or a different movie took
	// over): put the focus on the playhead row, so the frame box has a number
	// in it from the start and the row commands have a target.  Only ever done
	// when there is no focus at all - losing focus to another control does not
	// clear m_focusRow, so this cannot fight the user.
	if (m_focusRow < 0 && m_length > 0)
	{
		const u32 head = playheadRow();
		if (head != NO_ROW)
		{
			selectOnlyRow(head);
			ListView_EnsureVisible(m_list, (int)head, FALSE);
		}
	}

	const u32 newHead = playheadRow();
	if (newHead != oldHead)
	{
		repaintRow(oldHead);
		repaintRow(newHead);
		followPlayhead(newHead);
	}

	updateTitle();
	updateControls();
}

void DsmEditor::resetView()
{
	// force the next refresh to rebuild the column set for the new movie
	m_generation = 0xFFFFFFFFu;
	m_length	 = 0;
	m_frame		 = 0;
	m_focusRow	 = -1;

	m_columns.clear();
	while (ListView_DeleteColumn(m_list, 0))
		;
	ListView_SetItemCountEx(m_list, 0, 0);
	InvalidateRect(m_list, NULL, FALSE);

	syncFrameBox();
	updateTitle();
}

void DsmEditor::rebuildColumns(const MovieInputUsage &usage)
{
	std::vector<Column> columns;

	Column column;
	column.mask	 = 0;
	column.width = 0;

	column.kind = COL_PLAYHEAD;
	strcpy(column.label, "");
	columns.push_back(column);

	column.kind = COL_FRAME;
	strcpy(column.label, "Frame");
	columns.push_back(column);

	column.kind = COL_TIME;
	strcpy(column.label, "Time");
	columns.push_back(column);

	for (int i = 0; i < kPadKeyCount; ++i)
	{
		column.kind = COL_PAD;
		column.mask = kPadKeys[i].mask;
		strcpy(column.label, kPadKeys[i].label);
		columns.push_back(column);
	}

	// The stylus, the microphone and the lid are always offered: they are
	// inputs a TAS may want to add to a movie that has none yet, unlike the
	// reset command, which restarts the game and is not the editor's to invent.
	column.kind = COL_TOUCH;
	column.mask = 0;
	strcpy(column.label, "Tch");
	columns.push_back(column);

	column.kind = COL_MIC;
	strcpy(column.label, "Mic");
	columns.push_back(column);

	column.kind = COL_LID;
	strcpy(column.label, "Lid");
	columns.push_back(column);

	if (usage.commandMask & MOVIECMD_RESET)
	{
		column.kind = COL_RESET;
		strcpy(column.label, "Rst");
		columns.push_back(column);
	}

	if (columns.size() == m_columns.size())
	{
		size_t i = 0;
		while (i < columns.size() && columns[i].kind == m_columns[i].kind
		       && columns[i].mask == m_columns[i].mask)
			++i;
		if (i == columns.size())
			return;	// same set: leave the control (and the user's widths) alone
	}

	SendMessage(m_list, WM_SETREDRAW, FALSE, 0);

	while (ListView_DeleteColumn(m_list, 0))
		;

	for (size_t i = 0; i < columns.size(); ++i)
	{
		const int align = (columns[i].kind == COL_PLAYHEAD) ? LVCFMT_LEFT
		                : (columns[i].kind == COL_FRAME || columns[i].kind == COL_TIME) ? LVCFMT_RIGHT
		                                                                              : LVCFMT_CENTER;

		LVCOLUMN item;
		ZeroMemory(&item, sizeof(item));
		item.mask	  = LVCF_TEXT | LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM;
		item.fmt	  = align;
		item.cx		  = 0;
		item.pszText  = columns[i].label;
		item.iSubItem = (int)i;

		ListView_InsertColumn(m_list, (int)i, &item);
	}

	m_columns.swap(columns);
	fitColumns();
	updateHeightFloor();	// the header only exists now that there are columns

	SendMessage(m_list, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(m_list, NULL, FALSE);
}

void DsmEditor::fitColumns()
{
	// Whichever way out of this function is taken, the font it measured (or
	// skipped measuring) with is the current one.
	HFONT font	 = (HFONT)SendMessage(m_list, WM_GETFONT, 0, 0);
	m_fittedFont = font;

	const int count = (int)m_columns.size();
	if (count <= COL_FIRST_KEY)
		return;

	SIZE digit		 = { 6, 12 };
	SIZE frameHeader = { 30, 12 };
	SIZE stamp		 = { 60, 12 };
	SIZE timeHeader	 = { 20, 12 };
	int	 pad		 = 6;

	HDC dc = GetDC(m_list);
	if (dc)
	{
		HFONT	   old = font ? (HFONT)SelectObject(dc, font) : NULL;
		TEXTMETRIC metrics;

		if (GetTextMetrics(dc, &metrics))
		{
			GetTextExtentPoint32A(dc, "0", 1, &digit);
			GetTextExtentPoint32A(dc, "Frame", 5, &frameHeader);
			GetTextExtentPoint32A(dc, "Time", 4, &timeHeader);
			GetTextExtentPoint32A(dc, "00:00:00.00", 11, &stamp);
			pad = imax(4, (int)metrics.tmAveCharWidth);
		}

		if (old)
			SelectObject(dc, old);
		ReleaseDC(m_list, dc);
	}

	RECT client;
	GetClientRect(m_list, &client);

	int available = client.right - GetSystemMetrics(SM_CXVSCROLL) - pad;
	if (available < digit.cx * 14)
		available = digit.cx * 14;

	m_columns[COL_PLAYHEAD].width = imax(digit.cx * 2, 8);	// the playhead arrow

	// The frame number is the column that has to stay readable at a glance, so
	// it gets seven digit widths plus air on both sides, and never less than its
	// heading.  Everything left over is shared by the key columns.
	int frameWidth = digit.cx * 7 + pad * 2;
	if (frameHeader.cx + pad > frameWidth)
		frameWidth = frameHeader.cx + pad;
	m_columns[COL_FRAME].width = frameWidth;

	// A little slack on both sides: the stamp is right aligned, so the spare
	// room keeps it away from the key columns instead of touching their header.
	int stampWidth = stamp.cx + pad * 2;
	if (timeHeader.cx + pad * 2 > stampWidth)
		stampWidth = timeHeader.cx + pad * 2;
	m_columns[COL_TIME].width = stampWidth;

	const int fixed = m_columns[COL_PLAYHEAD].width + m_columns[COL_FRAME].width
	                + m_columns[COL_TIME].width;

	// The stylus cell carries "255,191", so it counts double when the leftover
	// width is shared out.
	int units = 0;
	for (int i = COL_FIRST_KEY; i < count; ++i)
		units += (m_columns[i].kind == COL_TOUCH) ? 2 : 1;

	int unit = (available - fixed) / imax(1, units);
	if (unit < digit.cx * 2)
		unit = digit.cx * 2;

	for (int i = COL_FIRST_KEY; i < count; ++i)
		m_columns[i].width = unit * ((m_columns[i].kind == COL_TOUCH) ? 2 : 1);

	// the last column absorbs the rounding remainder, so the grid ends at the
	// right edge of the client area instead of leaving a gap
	int used = fixed;
	for (int i = 0; i < count; ++i)
		used += m_columns[i].width;

	const int rest = m_columns[count - 1].width + (available - used);
	if (rest >= digit.cx * 2)
		m_columns[count - 1].width = rest;

	for (int i = 0; i < count; ++i)
		ListView_SetColumnWidth(m_list, i, m_columns[i].width);

	// Remembered so a WM_SIZE that only changes the height can skip this.
	m_fittedWidth = client.right;
}

void DsmEditor::updateControls()
{
	// The box is checked while the movie cannot be edited at all, and while the
	// user locked this window; only the second reason leaves it clickable.
	const bool movieLocked = !m_active || !FCEUMOV_IsEditable();

	const int checks = (movieLocked || m_editLock ? 1 : 0) | (m_follow ? 2 : 0)
	                 | (movieLocked ? 4 : 0);
	if (checks != m_uiChecks)
	{
		m_uiChecks = checks;
		CheckDlgButton(m_hwnd, IDC_DSM_READONLY, (checks & 1) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(m_hwnd, IDC_DSM_FOLLOW, (checks & 2) ? BST_CHECKED : BST_UNCHECKED);
		EnableWindow(GetDlgItem(m_hwnd, IDC_DSM_READONLY), (checks & 4) ? FALSE : TRUE);
	}

	// Transport: Play and Pause are the two states of File > Pause, so only the
	// one that would do something is offered.
	const bool paused  = (emu_paused != 0);
	const bool canPlay = m_active && paused;
	const bool canStop = m_active && !paused;
	const bool hasMark = !m_marks.empty();

	int mask = 0;
	if (canPlay) mask |= 0x01;
	if (canStop) mask |= 0x02;
	if (m_active) mask |= 0x04;	// frame advance
	if (m_active) mask |= 0x08;	// go to playhead
	if (m_active) mask |= 0x10;	// jump to a frame number
	if (hasMark) mask |= 0x20;
	if (hasMark) mask |= 0x40;
	if (m_active) mask |= 0x80;	// save

	if (mask == m_uiEnable)
		return;

	m_uiEnable = mask;
	EnableWindow(GetDlgItem(m_hwnd, IDC_DSM_PLAY), (mask & 0x01) ? TRUE : FALSE);
	EnableWindow(GetDlgItem(m_hwnd, IDC_DSM_PAUSE), (mask & 0x02) ? TRUE : FALSE);
	EnableWindow(GetDlgItem(m_hwnd, IDC_DSM_STEP), (mask & 0x04) ? TRUE : FALSE);
	EnableWindow(GetDlgItem(m_hwnd, IDC_DSM_GOTOPLAYHEAD), (mask & 0x08) ? TRUE : FALSE);
	EnableWindow(GetDlgItem(m_hwnd, IDC_DSM_FRAMENO), (mask & 0x10) ? TRUE : FALSE);
	EnableWindow(GetDlgItem(m_hwnd, IDC_DSM_PREVMARK), (mask & 0x20) ? TRUE : FALSE);
	EnableWindow(GetDlgItem(m_hwnd, IDC_DSM_NEXTMARK), (mask & 0x40) ? TRUE : FALSE);
	EnableWindow(GetDlgItem(m_hwnd, IDC_DSM_SAVE), (mask & 0x80) ? TRUE : FALSE);
}

void DsmEditor::updateTitle()
{
	std::string title = "DSM Editor";

	if (m_active && curMovieFilename[0])
	{
		const char *slash = strrchr(curMovieFilename, '\\');
		if (!slash)
			slash = strrchr(curMovieFilename, '/');

		title += " - ";
		title += (slash ? slash + 1 : curMovieFilename);
	}

	if (m_active && m_dirty)
		title += " *";

	if (title != m_title)
	{
		m_title = title;

		wchar_t wide[512];
		MultiByteToWideChar(CP_ACP, 0, title.c_str(), -1, wide, 512);
		SetWindowTextW(m_hwnd, wide);
	}
}

bool DsmEditor::editable() const
{
	// One record can be rewritten at any time: the emulation runs on this same
	// thread, between two message pumps, so nothing is reading the record
	// vector while the dialog procedure runs.  Changing the *shape* of the
	// timeline is a different matter - see canRestructure().
	return m_active && !m_editLock && FCEUMOV_IsEditable();
}

bool DsmEditor::canRestructure() const
{
	// Inserting or deleting moves every frame after the edit, including the one
	// currFrameCounter points at, so the run would replay a different timeline
	// from one frame to the next.  Pausing is what makes that a deliberate act.
	return editable() && (emu_paused != 0);
}

void DsmEditor::denyRestructure()
{
	if (!m_active)
		setStatus("No movie is loaded.");
	else if (!FCEUMOV_IsEditable())
		setStatus("The movie is being recorded - stop it first.");
	else if (m_editLock)
		setStatus("This window is read-only. Clear the Read-only box to edit.");
	else
		setStatus("Pause the emulator to insert or delete frames.");
}

u32 DsmEditor::playheadRow() const
{
	if (!m_active || m_length <= 0)
		return NO_ROW;

	// While playing, currFrameCounter is the frame that is about to run; when
	// it has run past the end, the last recorded row is the playhead.
	return (m_frame >= 0 && m_frame < m_length) ? (u32)m_frame : (u32)(m_length - 1);
}

int DsmEditor::focusedRow() const
{
	// Cached rather than asked for: GetNextItem(LVNI_FOCUSED) walks the list,
	// and this would otherwise run on every tick of a 100 000-frame movie.
	return m_focusRow;
}

std::vector<u32> DsmEditor::selectedRows() const
{
	std::vector<u32> rows;

	// The list walks in display order, so the result is already ascending -
	// which is what FCEUMOV_DeleteRecords() requires.
	int index = -1;
	while ((index = ListView_GetNextItem(m_list, index, LVNI_SELECTED)) >= 0)
		rows.push_back((u32)index);

	return rows;
}

void DsmEditor::selectOnlyRow(u32 row)
{
	const int target = (int)row;
	if (target < 0 || target >= ListView_GetItemCount(m_list))
		return;

	// -1 with LVIS_SELECTED clears the whole selection, which is how a jump
	// turns a multi-selection back into the single row it landed on.
	ListView_SetItemState(m_list, -1, 0, LVIS_SELECTED);
	ListView_SetItemState(m_list, target, LVIS_SELECTED | LVIS_FOCUSED,
	                      LVIS_SELECTED | LVIS_FOCUSED);

	m_focusRow = target;
	syncFrameBox();
}

void DsmEditor::setStatus(const char *text)
{
	if (m_status == text)
		return;

	m_status = text;

	wchar_t wide[512];
	MultiByteToWideChar(CP_ACP, 0, m_status.c_str(), -1, wide, 512);
	SetDlgItemTextW(m_hwnd, IDC_DSM_STATUS, wide);
}

void DsmEditor::setStatusf(const char *format, ...)
{
	char	buffer[512];
	va_list args;

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	buffer[sizeof(buffer) - 1] = 0;

	setStatus(buffer);
}

void DsmEditor::repaintRow(u32 row)
{
	if (row == NO_ROW || (int)row >= ListView_GetItemCount(m_list))
		return;

	ListView_RedrawItems(m_list, (int)row, (int)row);
}

///////////////////////////////////////////////////////////////////////////////
// list contents

void DsmEditor::formatCell(u32 row, int column, char *buffer, size_t count) const
{
	buffer[0] = 0;

	if (column < 0 || column >= (int)m_columns.size())
		return;

	const Column &col = m_columns[column];

	switch (col.kind)
	{
	case COL_PLAYHEAD:
		if (row == playheadRow())
			strcpy(buffer, ">");
		break;

	case COL_FRAME:
		snprintf(buffer, count, "%06u", row);
		buffer[count - 1] = 0;
		break;

	case COL_TIME:
		formatStamp(buffer, count, row);
		break;

	case COL_PAD:
	{
		MovieRecord record;
		if (FCEUMOV_GetRecord(row, record) && (record.pad & col.mask))
			strcpy(buffer, col.label);
		break;
	}

	case COL_TOUCH:
	{
		MovieRecord record;
		if (FCEUMOV_GetRecord(row, record) && record.touch.touch)
		{
			snprintf(buffer, count, "%u,%u", record.touch.x, record.touch.y);
			buffer[count - 1] = 0;
		}
		break;
	}

	case COL_MIC:
	case COL_LID:
	case COL_RESET:
	{
		MovieRecord record;
		if (!FCEUMOV_GetRecord(row, record))
			break;

		const bool set = (col.kind == COL_MIC) ? record.command_microphone()
		               : (col.kind == COL_LID) ? record.command_lid()
		                                       : record.command_reset();
		if (set)
			strcpy(buffer, "1");
		break;
	}
	}
}

void DsmEditor::onGetDispInfo(NMLVDISPINFOA *info)
{
	if (info->item.mask & LVIF_TEXT)
	{
		formatCell((u32)info->item.iItem, info->item.iSubItem, m_cellTextA, sizeof(m_cellTextA));
		info->item.pszText = m_cellTextA;
	}
}

void DsmEditor::onGetDispInfoW(NMLVDISPINFOW *info)
{
	if (info->item.mask & LVIF_TEXT)
	{
		formatCell((u32)info->item.iItem, info->item.iSubItem, m_cellTextA, sizeof(m_cellTextA));
		MultiByteToWideChar(CP_ACP, 0, m_cellTextA, -1, m_cellTextW, 32);
		info->item.pszText = m_cellTextW;
	}
}

LRESULT DsmEditor::onCustomDraw(NMLVCUSTOMDRAW *draw)
{
	switch (draw->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		return CDRF_NOTIFYITEMDRAW;

	case CDDS_ITEMPREPAINT:
	{
		const u32 row = (u32)draw->nmcd.dwItemSpec;
		if (row == playheadRow())
		{
			draw->clrText	= RGB(0, 0, 0);
			draw->clrTextBk = kPlayheadColor;
		}
		else if (m_marks.find(row) != m_marks.end())
		{
			draw->clrText	= RGB(0, 0, 0);
			draw->clrTextBk = kMarkColor;
		}

		return CDRF_NEWFONT;	// required for clrTextBk to be used
	}

	default:
		return CDRF_DODEFAULT;
	}
}

void DsmEditor::onItemChanged(const NMLISTVIEW *info)
{
	// Only the focus moves the frame box.  A Ctrl+click changes LVIS_SELECTED
	// too, and enumerating the selection here would run on every click of a
	// 100 000-frame movie for nothing.  Focus loss is ignored on purpose: when
	// focus moves from A to B the control reports A losing it first, and
	// reacting to that would blank the box for a moment.
	if ((info->uChanged & LVIF_STATE) && (info->uNewState & LVIS_FOCUSED)
	    && !(info->uOldState & LVIS_FOCUSED))
	{
		if (info->iItem != m_focusRow)
		{
			m_focusRow = info->iItem;
			syncFrameBox();
		}
	}
}

void DsmEditor::onListClick(const NMITEMACTIVATE *info, bool right, bool doubleClick)
{
	LVHITTESTINFO hit;
	ZeroMemory(&hit, sizeof(hit));
	hit.pt = info->ptAction;

	const int row = ListView_SubItemHitTest(m_list, &hit);
	if (row < 0)
		return;

	const int column = (hit.iSubItem >= 0 && hit.iSubItem < (int)m_columns.size())
	                 ? hit.iSubItem : -1;

	if (right)
	{
		// A right click on an unselected row moves the selection to it, so the
		// menu acts on what the user pointed at.
		if (!(ListView_GetItemState(m_list, row, LVIS_SELECTED) & LVIS_SELECTED))
			selectOnlyRow((u32)row);

		showContextMenu(info->ptAction);
		return;
	}

	if (doubleClick)
	{
		// The stylus needs two numbers, which a cell cannot take; the dialog is
		// the only place they can be typed in.  The first click of the pair
		// already toggled the cell (the list reports NM_CLICK, then NM_DBLCLK
		// instead of a second NM_CLICK), so it is toggled back here: opening the
		// position dialog must not change the frame by itself.
		if (column >= 0 && m_columns[column].kind == COL_TOUCH)
		{
			toggleCell((u32)row, column);
			promptTouch((u32)row);
			return;
		}
	}

	// Ctrl and Shift are selection gestures; a click that changes the selection
	// must not also flip a key.
	if (GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_SHIFT) < 0)
		return;

	// A cell edit means "this one frame", so it is only offered while exactly
	// one row is selected.
	if (selectedRows().size() != 1)
		return;

	if (column >= 0)
		toggleCell((u32)row, column);
}

///////////////////////////////////////////////////////////////////////////////
// the frame box and the playhead

void DsmEditor::syncFrameBox()
{
	char text[32] = { 0 };
	if (m_focusRow >= 0)
	{
		snprintf(text, sizeof(text), "%d", m_focusRow);
		text[sizeof(text) - 1] = 0;
	}

	wchar_t wide[32];
	MultiByteToWideChar(CP_ACP, 0, text, -1, wide, 32);
	SetDlgItemTextW(m_hwnd, IDC_DSM_FRAMENO, wide);
}

void DsmEditor::applyFrameBox()
{
	if (!m_active || m_length <= 0)
	{
		setStatus("No movie is loaded.");
		syncFrameBox();
		return;
	}

	wchar_t text[32] = { 0 };
	GetDlgItemTextW(m_hwnd, IDC_DSM_FRAMENO, text, 32);

	if (!text[0])
	{
		syncFrameBox();
		return;
	}

	unsigned long row = wcstoul(text, NULL, 10);
	if (row >= (unsigned long)m_length)
		row = (unsigned long)(m_length - 1);	// typing past the end means the last frame

	revealRow((u32)row, true);
}

int DsmEditor::rowHeight()
{
	int top = ListView_GetTopIndex(m_list);
	if (top < 0)
		top = 0;

	RECT rect;
	if (ListView_GetItemRect(m_list, top, &rect, LVIR_BOUNDS))
		return imax(1, (int)(rect.bottom - rect.top));

	return 16;
}

void DsmEditor::revealRow(u32 row, bool center)
{
	const int target = (int)row;
	if (target < 0 || target >= ListView_GetItemCount(m_list))
		return;

	// A jump always lands on exactly one row: leaving a multi-selection behind
	// would make the next Delete act on frames the user is no longer looking at.
	selectOnlyRow(row);
	ListView_EnsureVisible(m_list, target, FALSE);

	if (!center)
		return;

	const int perPage = imax(1, ListView_GetCountPerPage(m_list));
	const int want	  = target - perPage / 2;
	if (want <= 0)
		return;

	const int dy = (want - ListView_GetTopIndex(m_list)) * rowHeight();
	if (dy != 0)
		ListView_Scroll(m_list, 0, dy);
}

void DsmEditor::followPlayhead(u32 row)
{
	if (!m_follow || row == NO_ROW)
		return;

	const int target = (int)row;
	if (target >= ListView_GetItemCount(m_list))
		return;

	const int perPage = imax(1, ListView_GetCountPerPage(m_list));
	const int top	  = ListView_GetTopIndex(m_list);

	// On screen: leave the view where it is.  Once the playhead leaves, move a
	// whole page at a time so the grid does not creep one line per frame.
	if (target >= top && target < top + perPage)
		return;

	RECT client;
	GetClientRect(m_list, &client);
	const int pageH = imax(rowHeight(), (int)(client.bottom - client.top) - rowHeight());

	int pages = (target - top) / perPage;
	if (pages == 0)
		pages = (target > top) ? 1 : -1;

	ListView_Scroll(m_list, 0, pages * pageH);
	ListView_EnsureVisible(m_list, target, FALSE);	// insurance for a partial last page
}

///////////////////////////////////////////////////////////////////////////////
// editing

void DsmEditor::toggleCell(u32 row, int column)
{
	if (column < COL_FIRST_KEY || column >= (int)m_columns.size())
		return;

	const Column &col = m_columns[column];

	if (col.kind == COL_RESET)
	{
		// The emulator acts on a reset frame (it restarts the game), so it is
		// not "a key the user pressed" and the editor has no business
		// inventing one.
		setStatus("The reset command is shown, not edited.");
		return;
	}

	if (!editable())
	{
		denyRestructure();
		return;
	}

	MovieRecord record;
	if (!FCEUMOV_GetRecord(row, record))
		return;

	switch (col.kind)
	{
	case COL_PAD:
		record.pad ^= col.mask;
		break;

	case COL_TOUCH:
		if (record.touch.touch)
		{
			record.touch.touch = 0;
			record.touch.x	   = 0;
			record.touch.y	   = 0;
		}
		else
		{
			u8 x = 0;
			u8 y = 0;
			touchDefaults(row, x, y);

			record.touch.touch = 1;
			record.touch.x	   = x;
			record.touch.y	   = y;
		}
		break;

	case COL_MIC:
		record.commands ^= MOVIECMD_MIC;
		break;

	case COL_LID:
		record.commands ^= MOVIECMD_LID;
		break;

	default:
		return;
	}

	if (FCEUMOV_SetRecord(row, record) != MOVIE_EDIT_OK)
	{
		setStatus("The movie refused the change.");
		return;
	}

	m_dirty = true;
	repaintRow(row);
	updateTitle();
}

void DsmEditor::touchDefaults(u32 row, u8 &x, u8 &y) const
{
	// Turning the stylus on without saying where is the common case, and the
	// only sane answer is where it was last time: walk back for the most recent
	// frame that touches.  With none, the middle of the screen.
	x = 128;
	y = 96;

	for (u32 i = row; i > 0; --i)
	{
		MovieRecord record;
		if (FCEUMOV_GetRecord(i - 1, record) && record.touch.touch)
		{
			x = record.touch.x;
			y = record.touch.y;
			return;
		}
	}
}

bool DsmEditor::promptTouch(u32 row)
{
	MovieRecord record;
	if (!FCEUMOV_GetRecord(row, record))
		return false;

	TouchRequest request;
	request.x	 = record.touch.touch ? record.touch.x : 128;
	request.y	 = record.touch.touch ? record.touch.y : 96;
	request.down = (record.touch.touch != 0);

	if (DialogBoxParamW(hAppInst, MAKEINTRESOURCEW(IDD_DSM_TOUCH), m_hwnd,
	                    touchDialogProc, (LPARAM)&request) != IDOK)
		return false;

	if (!editable())
	{
		denyRestructure();
		return false;
	}

	record.touch.touch = request.down ? 1 : 0;
	record.touch.x	   = (u8)request.x;
	record.touch.y	   = (u8)request.y;

	if (FCEUMOV_SetRecord(row, record) != MOVIE_EDIT_OK)
	{
		setStatus("The movie refused the change.");
		return false;
	}

	m_dirty = true;
	repaintRow(row);
	updateTitle();

	setStatusf("Frame %u: stylus %s at %d,%d.", row, request.down ? "down" : "up",
	           request.x, request.y);
	return true;
}

void DsmEditor::insertRows(u32 at, u32 count)
{
	if (!canRestructure())
	{
		denyRestructure();
		return;
	}

	if ((u32)m_length + count > kMaxMovieFrames)
	{
		setStatus("That would make the movie too long.");
		return;
	}

	// At or above the playhead the shift changes what the emulator reads next;
	// below it only the recording changes.  Both are allowed, as asked.
	const bool shiftsPlayhead = (at <= (u32)m_frame);

	if (FCEUMOV_InsertRecords(at, NULL, count) != MOVIE_EDIT_OK)
	{
		setStatus("Could not insert frames.");
		return;
	}

	m_dirty = true;
	marksInserted(at, count);
	refresh();

	// the new blank row is what the user wants to fill in
	if ((int)at < m_length)
	{
		selectOnlyRow(at);
		ListView_EnsureVisible(m_list, (int)at, FALSE);
	}

	setStatusf("Inserted %u blank frame%s at %u.%s", count, (count == 1) ? "" : "s", at,
	           shiftsPlayhead ? " The playhead moved with the movie." : "");
}

bool DsmEditor::deleteRows(const std::vector<u32> &rows)
{
	if (!canRestructure())
	{
		denyRestructure();
		return false;
	}
	if (rows.empty())
	{
		setStatus("Select the frames to delete first.");
		return false;
	}

	const u32  first			 = rows.front();
	const bool shiftsPlayhead = (first <= (u32)m_frame);

	if (FCEUMOV_DeleteRecords(&rows[0], (u32)rows.size()) != MOVIE_EDIT_OK)
	{
		setStatus("Could not delete frames.");
		return false;
	}

	m_dirty = true;
	marksDeleted(rows);
	refresh();

	// Leave the focus where the first deleted row used to be, which is now the
	// row that followed it - or the last row, if the tail went away.
	if (m_length > 0)
	{
		const u32 target = (first < (u32)m_length) ? first : (u32)(m_length - 1);
		selectOnlyRow(target);
		ListView_EnsureVisible(m_list, (int)target, FALSE);
	}

	if (rows.size() == 1)
		setStatusf("Deleted frame %u.%s", first,
		           shiftsPlayhead ? " The playhead moved with the movie." : "");
	else
		setStatusf("Deleted %u frames.%s", (unsigned)rows.size(),
		           shiftsPlayhead ? " The playhead moved with the movie." : "");

	return true;
}

void DsmEditor::deleteSelection()
{
	deleteRows(selectedRows());
}

void DsmEditor::insertSelection(bool after)
{
	if (!canRestructure())
	{
		denyRestructure();
		return;
	}

	int row = focusedRow();
	if (row < 0)
	{
		setStatus("Select a frame first.");
		return;
	}

	insertRows((u32)row + (after ? 1 : 0), 1);
}

void DsmEditor::toggleMarkSelection()
{
	std::vector<u32> rows = selectedRows();
	if (rows.empty())
	{
		// A click on a cell focuses the row without necessarily selecting it,
		// and the keyboard can act on that row; fall back to it so M still
		// works on the row the user is looking at.
		const int focus = focusedRow();
		if (focus < 0)
		{
			setStatus("Select a frame first.");
			return;
		}
		rows.push_back((u32)focus);
	}

	// One entry, two meanings: a selection that is already marked as a whole is
	// what the user wants to clear.  Anything else marks all of it, so a mixed
	// selection converges on "all marked" instead of flipping row by row.
	bool allMarked = true;
	for (size_t i = 0; i < rows.size() && allMarked; ++i)
	{
		if (m_marks.find(rows[i]) == m_marks.end())
			allMarked = false;
	}

	for (size_t i = 0; i < rows.size(); ++i)
	{
		if (allMarked)
			m_marks.erase(rows[i]);
		else
			m_marks.insert(rows[i]);
	}

	// The rows are ascending and all of them changed, so one band repaint does
	// the whole action.
	ListView_RedrawItems(m_list, (int)rows.front(), (int)rows.back());
	updateControls();

	if (rows.size() == 1)
		setStatusf(allMarked ? "Unmarked frame %u." : "Marked frame %u.", rows.front());
	else
		setStatusf(allMarked ? "Unmarked %u frames." : "Marked %u frames.",
		           (unsigned)rows.size());
}

void DsmEditor::gotoNeighbourMark(int direction)
{
	if (m_marks.empty())
	{
		setStatus("No frames are marked.");
		return;
	}

	const int from	 = (focusedRow() >= 0) ? focusedRow() : (int)playheadRow();
	u32		  target = NO_ROW;

	if (direction > 0)
	{
		for (std::set<u32>::const_iterator it = m_marks.begin(); it != m_marks.end(); ++it)
		{
			if ((int)*it > from)
			{
				target = *it;
				break;
			}
		}

		if (target == NO_ROW)
			target = *m_marks.begin();	// wrap to the first mark
	}
	else
	{
		for (std::set<u32>::const_reverse_iterator it = m_marks.rbegin(); it != m_marks.rend(); ++it)
		{
			if ((int)*it < from)
			{
				target = *it;
				break;
			}
		}

		if (target == NO_ROW)
			target = *m_marks.rbegin();	// wrap to the last mark
	}

	if (target != NO_ROW)
		revealRow(target, true);
}

void DsmEditor::showContextMenu(POINT client)
{
	// NM_RCLICK reports ptAction in the list's client coordinates, and
	// TrackPopupMenu wants screen coordinates.  Passing one for the other puts
	// the menu at a fixed spot near the top left of the desktop - which is what
	// "the menu ignores the mouse" looks like.  The conversion lives here so a
	// caller cannot get it wrong.
	POINT screen = client;
	ClientToScreen(m_list, &screen);

	HMENU bar = LoadMenuW(hAppInst, MAKEINTRESOURCEW(DSMEDITOR_MENU));
	if (!bar)
		return;

	HMENU popup = GetSubMenu(bar, 0);
	if (!popup)
	{
		DestroyMenu(bar);
		return;
	}

	const bool restructure	= canRestructure();
	const bool hasSelection = !selectedRows().empty();

	EnableMenuItem(popup, CMD_DSM_MARK, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
	EnableMenuItem(popup, CMD_DSM_INSERT, MF_BYCOMMAND | (restructure ? MF_ENABLED : MF_GRAYED));
	EnableMenuItem(popup, CMD_DSM_INSERT_AFTER, MF_BYCOMMAND | (restructure ? MF_ENABLED : MF_GRAYED));
	EnableMenuItem(popup, CMD_DSM_DELETE, MF_BYCOMMAND | (restructure && hasSelection ? MF_ENABLED : MF_GRAYED));
	EnableMenuItem(popup, CMD_DSM_CUT, MF_BYCOMMAND | (restructure && hasSelection ? MF_ENABLED : MF_GRAYED));
	EnableMenuItem(popup, CMD_DSM_COPY, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
	EnableMenuItem(popup, CMD_DSM_TOUCH, MF_BYCOMMAND | (m_active ? MF_ENABLED : MF_GRAYED));
	EnableMenuItem(popup, CMD_DSM_SAVE, MF_BYCOMMAND | (m_active ? MF_ENABLED : MF_GRAYED));
	EnableMenuItem(popup, CMD_DSM_SAVE_AS, MF_BYCOMMAND | (m_active ? MF_ENABLED : MF_GRAYED));

	// Paste needs to know where to put the rows, and the clipboard has to be
	// parsed anyway to know whether it holds anything usable.
	refreshClipboard();
	const bool canPaste = restructure && selectedRows().size() == 1
	                   && m_clipResult == dsmcsv::Result::Ok;
	EnableMenuItem(popup, CMD_DSM_PASTE, MF_BYCOMMAND | (canPaste ? MF_ENABLED : MF_GRAYED));

	const int command = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON,
	                                   screen.x, screen.y, 0, m_hwnd, NULL);

	DestroyMenu(bar);

	// TrackPopupMenu takes the mouse capture and does not always hand it back
	// when the command was returned rather than sent; a null message flushes
	// that out (the documented workaround), otherwise the next click on the
	// grid is swallowed.
	PostMessage(m_hwnd, WM_NULL, 0, 0);

	if (command)
		SendMessage(m_hwnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);
}

///////////////////////////////////////////////////////////////////////////////
// marks

void DsmEditor::marksInserted(u32 at, u32 count)
{
	// Every mark at or below the insertion point keeps its frame; everything
	// from there on moved down by count.
	std::set<u32> moved;
	for (std::set<u32>::const_iterator it = m_marks.begin(); it != m_marks.end(); ++it)
		moved.insert(*it >= at ? *it + count : *it);

	m_marks.swap(moved);
}

void DsmEditor::marksDeleted(const std::vector<u32> &rows)
{
	// rows is ascending and unique, so one walk over both sets is enough: the
	// number of deleted rows below a mark is exactly how far that mark moved.
	std::set<u32> moved;
	size_t		  removed = 0;

	for (std::set<u32>::const_iterator it = m_marks.begin(); it != m_marks.end(); ++it)
	{
		while (removed < rows.size() && rows[removed] < *it)
			++removed;

		if (removed < rows.size() && rows[removed] == *it)
			continue;	// the marked frame itself is gone

		moved.insert(*it - (u32)removed);
	}

	m_marks.swap(moved);
}

///////////////////////////////////////////////////////////////////////////////
// clipboard
//
// Copy writes the selected rows as dsmcsv text, cut is copy-then-delete, and
// paste inserts the parsed rows after the focused frame.  Nothing here touches
// the movie except through the same calls the grid uses, so a paste is just a
// batch insert with the rows read from the clipboard.

bool DsmEditor::writeClipboard(const std::wstring &text)
{
	if (!OpenClipboard(m_hwnd))
		return false;

	bool	ok	  = false;
	HGLOBAL block = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));

	EmptyClipboard();

	if (block)
	{
		if (void *target = GlobalLock(block))
		{
			memcpy(target, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
			GlobalUnlock(block);

			if (SetClipboardData(CF_UNICODETEXT, block))
			{
				ok	  = true;
				block = NULL;	// the clipboard owns it now
			}
		}

		if (block)
			GlobalFree(block);
	}

	CloseClipboard();
	return ok;
}

DsmEditor::ClipResult DsmEditor::readClipboard(std::wstring &text)
{
	text.clear();

	if (!OpenClipboard(m_hwnd))
		return CLIP_UNAVAILABLE;

	ClipResult result = CLIP_UNAVAILABLE;

	// The character limit is checked before anything is copied, so a runaway
	// clipboard cannot be turned into a runaway allocation.
	const size_t maxChars = size_t(dsmcsv::MAX_CHARS);

	if (HANDLE handle = GetClipboardData(CF_UNICODETEXT))
	{
		const size_t bytes = GlobalSize(handle);
		const size_t limit = bytes / sizeof(wchar_t);

		if (limit > maxChars)
		{
			result = CLIP_TOO_LARGE;
		}
		else if (const wchar_t *source = (const wchar_t *)GlobalLock(handle))
		{
			size_t length = 0;
			while (length < limit && source[length] != 0)
				++length;

			text.assign(source, length);
			GlobalUnlock(handle);
			result = CLIP_OK;
		}
	}
	else if (HANDLE ansi = GetClipboardData(CF_TEXT))
	{
		// Older tools still put plain text on the clipboard; the movie's own
		// strings are read in the process code page, so this is the same door.
		const size_t bytes = GlobalSize(ansi);
		if (bytes > maxChars)
		{
			result = CLIP_TOO_LARGE;
		}
		else if (const char *source = (const char *)GlobalLock(ansi))
		{
			const int length = (int)strlen(source);
			text.resize((size_t)length);
			if (length > 0)
				MultiByteToWideChar(CP_ACP, 0, source, length, &text[0], length);
			GlobalUnlock(ansi);
			result = CLIP_OK;
		}
	}

	CloseClipboard();
	return result;
}

void DsmEditor::refreshClipboard()
{
	// GetClipboardSequenceNumber() changes whenever any program writes to the
	// clipboard, so it is exactly the cache key this needs.
	const DWORD sequence = GetClipboardSequenceNumber();
	if (sequence == m_clipSequence)
		return;

	m_clipSequence = sequence;
	m_clipResult   = dsmcsv::Result::Empty;
	m_clipBadLine  = 0;
	m_clipRows.clear();

	std::wstring text;
	switch (readClipboard(text))
	{
	case CLIP_OK:
		m_clipResult = dsmcsv::parse(text, m_clipRows, &m_clipBadLine);
		break;
	case CLIP_TOO_LARGE:
		m_clipResult = dsmcsv::Result::TooLarge;
		break;
	default:
		break;	// Unavailable: nothing to paste, and nothing to report yet
	}
}

void DsmEditor::reportClipboardFailure()
{
	switch (m_clipResult)
	{
	case dsmcsv::Result::TooLarge:
	case dsmcsv::Result::TooManyRows:
		setStatus("The clipboard holds more frames than this window can paste.");
		return;

	case dsmcsv::Result::Empty:
	case dsmcsv::Result::NoRows:
		setStatus("The clipboard does not hold a frame table.");
		return;

	default:
		break;
	}

	// A line number is the only thing that makes a rejected paste actionable:
	// without it the user is looking for one bad field in five hundred rows.
	if (m_clipBadLine > 0)
		setStatusf("Line %d of the clipboard is not a valid frame row.", m_clipBadLine);
	else
		setStatus("The clipboard does not hold a frame table.");
}

void DsmEditor::copySelection(bool cut)
{
	const std::vector<u32> rows = selectedRows();
	if (rows.empty())
	{
		setStatus("Select the frames to copy first.");
		return;
	}

	// Cut destroys rows, so it has to be refused before the clipboard is
	// touched; copy is always allowed, read-only movie or not.
	if (cut && !canRestructure())
	{
		denyRestructure();
		return;
	}

	std::vector<dsmcsv::Row> data;
	data.reserve(rows.size());

	for (size_t i = 0; i < rows.size(); ++i)
	{
		MovieRecord record;
		if (!FCEUMOV_GetRecord(rows[i], record))
			continue;

		dsmcsv::Row row;
		row.frame  = rows[i];
		row.pad	   = record.pad;
		row.touchX = record.touch.x;
		row.touchY = record.touch.y;
		row.touch  = (record.touch.touch != 0);
		row.mic	   = record.command_microphone();
		row.lid	   = record.command_lid();
		row.reset  = record.command_reset();
		data.push_back(row);
	}

	std::wstring text;
	if (dsmcsv::serialize(data, text) != dsmcsv::Result::Ok || !writeClipboard(text))
	{
		setStatus("Could not put the frames on the clipboard.");
		return;
	}

	if (!cut)
	{
		setStatusf("Copied %u frame%s.", (unsigned)rows.size(),
		           (rows.size() == 1) ? "" : "s");
		return;
	}

	// Cut only reports success once the rows are actually gone; if the delete
	// is refused the clipboard keeps the data and the status says why.
	const size_t count = rows.size();
	if (!deleteRows(rows))
		return;

	setStatusf("Cut %u frame%s.", (unsigned)count, (count == 1) ? "" : "s");
}

void DsmEditor::paste()
{
	const std::vector<u32> rows = selectedRows();
	if (rows.size() != 1)
	{
		setStatus("Select the frame to paste after first.");
		return;
	}

	if (!canRestructure())
	{
		denyRestructure();
		return;
	}

	refreshClipboard();
	if (m_clipResult != dsmcsv::Result::Ok)
	{
		reportClipboardFailure();
		return;
	}

	if ((u32)m_length + (u32)m_clipRows.size() > kMaxMovieFrames)
	{
		setStatus("That would make the movie too long.");
		return;
	}

	const u32 anchor = rows.front();
	const u32 count	 = (u32)m_clipRows.size();

	std::vector<MovieRecord> records(count);
	for (u32 i = 0; i < count; ++i)
	{
		const dsmcsv::Row &row = m_clipRows[i];

		records[i].pad			   = row.pad;
		records[i].touch.x		   = row.touchX;
		records[i].touch.y		   = row.touchY;
		records[i].touch.touch	   = row.touch ? 1 : 0;
		records[i].touch.micsample = 0;
		records[i].commands		   = 0;

		if (row.mic)
			records[i].commands |= MOVIECMD_MIC;
		if (row.lid)
			records[i].commands |= MOVIECMD_LID;
		if (row.reset)
			records[i].commands |= MOVIECMD_RESET;
	}

	if (FCEUMOV_InsertRecords(anchor + 1, &records[0], count) != MOVIE_EDIT_OK)
	{
		setStatus("Could not paste the frames.");
		return;
	}

	m_dirty = true;
	marksInserted(anchor + 1, count);
	refresh();

	const u32 first = anchor + 1;
	if (first < (u32)m_length)
	{
		selectOnlyRow(first);
		ListView_EnsureVisible(m_list, (int)first, FALSE);
	}

	setStatusf("Pasted %u frame%s after frame %u.%s", count, (count == 1) ? "" : "s", anchor,
	           (anchor <= (u32)m_frame) ? " The playhead moved with the movie." : "");
}

///////////////////////////////////////////////////////////////////////////////
// saving
//
// A .vbm is rewritten by its emulator on every edit; a .dsm is not, so this
// window owns the write.  The whole movie is dumped - header included - which
// is the same thing DeSmuME does when it rewrites a movie after a savestate
// load, and the format (text or binary) the file was loaded with is kept.

void DsmEditor::saveMovie(bool saveAs)
{
	if (!m_active)
	{
		setStatus("No movie is loaded.");
		return;
	}

	if (movieMode == MOVIEMODE_RECORD)
	{
		setStatus("The movie is being recorded - stop it first.");
		return;
	}

	if (!romloaded)
	{
		setStatus("Load the movie's ROM before saving.");
		return;
	}

	char path[MAX_PATH] = { 0 };

	if (!saveAs && curMovieFilename[0])
	{
		strncpy(path, curMovieFilename, MAX_PATH - 1);
	}
	else
	{
		wchar_t file[MAX_PATH * 2] = { 0 };
		if (curMovieFilename[0])
			MultiByteToWideChar(CP_ACP, 0, curMovieFilename, -1, file, MAX_PATH * 2);

		OPENFILENAMEW ofn;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize	 = sizeof(ofn);
		ofn.hwndOwner	 = m_hwnd;
		ofn.lpstrFilter	 = L"DeSmuME Movie (*.dsm)\0*.dsm\0All files (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.lpstrFile	 = file;
		ofn.nMaxFile	 = MAX_PATH * 2;
		ofn.lpstrTitle	 = L"Save Movie As";
		ofn.lpstrDefExt	 = L"dsm";
		ofn.Flags		 = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

		if (!GetSaveFileNameW(&ofn))
			return;

		WideCharToMultiByte(CP_ACP, 0, file, -1, path, MAX_PATH, NULL, NULL);
	}

	if (!path[0])
	{
		setStatus("Choose a file name first.");
		return;
	}

	const MOVIE_EDIT_RESULT result = FCEUMOV_SaveMovie(path);
	if (result != MOVIE_EDIT_OK)
	{
		setStatus((result == MOVIE_EDIT_IO) ? "Could not write the movie file."
		                                    : "The movie refused to be saved.");
		return;
	}

	m_dirty = false;
	updateTitle();

	setStatusf("Saved %d frames to %s", m_length, path);
}

///////////////////////////////////////////////////////////////////////////////
// the touch prompt

INT_PTR CALLBACK touchDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
	{
		TouchRequest *request = (TouchRequest *)lParam;

		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)request);

		SetDlgItemInt(hwnd, IDC_DSM_TOUCH_X, request->x, FALSE);
		SetDlgItemInt(hwnd, IDC_DSM_TOUCH_Y, request->y, FALSE);
		CheckDlgButton(hwnd, IDC_DSM_TOUCH_DOWN, request->down ? BST_CHECKED : BST_UNCHECKED);

		SendDlgItemMessage(hwnd, IDC_DSM_TOUCH_X, EM_SETLIMITTEXT, 3, 0);
		SendDlgItemMessage(hwnd, IDC_DSM_TOUCH_Y, EM_SETLIMITTEXT, 3, 0);

		return TRUE;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
		{
			TouchRequest *request = (TouchRequest *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			if (request)
			{
				int x = (int)GetDlgItemInt(hwnd, IDC_DSM_TOUCH_X, NULL, FALSE);
				int y = (int)GetDlgItemInt(hwnd, IDC_DSM_TOUCH_Y, NULL, FALSE);

				// The record stores one byte per axis; the touchscreen is
				// 256x192, so the clamp is what keeps a typo from wrapping.
				if (x < 0) x = 0;
				if (x > 255) x = 255;
				if (y < 0) y = 0;
				if (y > 191) y = 191;

				request->x	  = x;
				request->y	  = y;
				request->down = IsDlgButtonChecked(hwnd, IDC_DSM_TOUCH_DOWN) != 0;
			}

			EndDialog(hwnd, IDOK);
			return TRUE;
		}

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// entry points

static INT_PTR CALLBACK dsmEditorProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (!g_editor)
		return FALSE;

	return g_editor->handleMessage(hwnd, msg, wParam, lParam);
}

void DSMEditorOpen(HWND parent)
{
	if (g_editor)
	{
		HWND hwnd = g_editor->hwnd();
		if (hwnd)
		{
			if (IsIconic(hwnd))
				ShowWindow(hwnd, SW_RESTORE);
			SetForegroundWindow(hwnd);
			return;
		}

		g_editor = NULL;	// the window went away behind our back
	}

	DsmEditor *editor = new DsmEditor();
	g_editor = editor;

	if (!CreateDialogW(hAppInst, MAKEINTRESOURCEW(IDD_DSM_EDITOR), parent, dsmEditorProc))
	{
		g_editor = NULL;
		delete editor;
		return;
	}

	ShowWindow(g_editor->hwnd(), SW_SHOW);
}

HWND DSMEditorGetHwnd()
{
	return g_editor ? g_editor->hwnd() : NULL;
}
