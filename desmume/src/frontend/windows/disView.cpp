/*
	Copyright (C) 2006 yopyop
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

// Win32 host for the disassembler. Decoding, address navigation, the ARM/Thumb
// mode rules, register snapshots and the breakpoint list all live in
// ../shared/disassembly_core.cpp and are shared with the Cocoa and GTK3
// frontends; this file only paints, scrolls and forwards button presses.
//
// The dialog used to be duplicated, one template per CPU, and opening the tool
// created both windows. There is now a single window with a CPU combo box, and
// each CPU keeps its own scroll position and breakpoint-list offset.

#include "disView.h"

#include <algorithm>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <windowsx.h>

#include "NDSSystem.h"
#include "armcpu.h"
#include "../shared/disassembly_core.h"

#include "resource.h"
#include "main.h"
#include "UiFont.h"

// Number of breakpoint rows the template has room for.
static const int kBreakpointRows = 8;

// Height of one text line, plus the colour of the two row highlights. The old
// code painted breakpoint rows fully saturated red, which made the text
// unreadable; a light red keeps the instruction visible.
static const COLORREF kBreakpointRowColor = RGB(255, 170, 170);
static const COLORREF kCurrentRowColor    = RGB(255, 255, 0);

struct DisViewState
{
	HWND hwnd;
	HWND box;
	int cpu;
	int breakPos[DisassemblyCore::CPU_COUNT];
	bool autoUpdate;
	unsigned int autoUpdateFrames;
};

static DisViewState* DisView = NULL;
static HWND DisViewWnd = NULL;

// Owned by main.cpp; used to mark the tool closed when the window is destroyed.
extern TOOLSCLASS* ViewDisasm;

static DisassemblyCore& Core()
{
	return DisassemblyCore::Get();
}

static int ClampBreakPos(int position)
{
	const int count = (int)Core().BreakpointCount();
	if (position < 0)
	{
		return 0;
	}
	if (position >= count)
	{
		return (count > 0) ? (count - 1) : 0;
	}
	return position;
}

// ---------------------------------------------------------------------------
// Painting helpers
// ---------------------------------------------------------------------------

static int VisibleLineCount(HWND box)
{
	RECT rect;
	GetClientRect(box, &rect);

	HDC dc = GetDC(box);
	HGDIOBJ oldFont = SelectObject(dc, uiFont::mono(box));

	TEXTMETRIC tm;
	GetTextMetrics(dc, &tm);

	SelectObject(dc, oldFont);
	ReleaseDC(box, dc);

	const int lineHeight = (tm.tmHeight > 0) ? tm.tmHeight : 12;
	const int lines = (rect.bottom - rect.top) / lineHeight;
	return (lines > 0) ? lines : 1;
}

static void UpdateBoxScrollBar(HWND box)
{
	const uint32_t instructionSize = Core().InstructionSize();
	const int maxLine = (int)(DisassemblyCore::kMaxAddress / instructionSize);

	SCROLLINFO si;
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	si.nMin = 0;
	si.nMax = maxLine;
	si.nPage = (UINT)VisibleLineCount(box);
	si.nPos = (int)(Core().AnchorAddress() / instructionSize);
	SetScrollInfo(box, SB_VERT, &si, TRUE);
}

static void PaintDisassembly(HWND box)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(box, &ps);

	RECT client;
	GetClientRect(box, &client);
	const int width = client.right - client.left;
	const int height = client.bottom - client.top;

	HDC memDC = CreateCompatibleDC(hdc);
	HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
	HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);
	HGDIOBJ oldFont = SelectObject(memDC, uiFont::mono(box));

	FillRect(memDC, &client, (HBRUSH)GetStockObject(WHITE_BRUSH));

	TEXTMETRIC tm;
	GetTextMetrics(memDC, &tm);
	const int lineHeight = (tm.tmHeight > 0) ? tm.tmHeight : 12;
	const int lineCount = (height / lineHeight) + 1;

	std::vector<DisassemblyCore::Line> lines;
	Core().BuildLines((size_t)lineCount, lines);

	SetBkMode(memDC, OPAQUE);
	SetTextColor(memDC, RGB(0, 0, 0));

	int y = 0;
	for (size_t i = 0; i < lines.size(); ++i)
	{
		const DisassemblyCore::Line& line = lines[i];

		COLORREF background = RGB(255, 255, 255);
		if (line.Breakpoint)
		{
			background = kBreakpointRowColor;
		}
		if (line.Current)
		{
			background = kCurrentRowColor;
		}

		RECT row;
		row.left = 0;
		row.top = y;
		row.right = width;
		row.bottom = y + lineHeight;

		// ETO_OPAQUE fills the row with the background colour in one call, so
		// the highlight spans the full width like it did before.
		SetBkColor(memDC, background);
		ExtTextOutA(memDC, 2, y, ETO_OPAQUE, &row, line.Text.c_str(), (UINT)line.Text.length(), NULL);

		y += lineHeight;
	}

	BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

	SelectObject(memDC, oldFont);
	SelectObject(memDC, oldBitmap);
	DeleteObject(memBitmap);
	DeleteDC(memDC);
	EndPaint(box, &ps);
}

// Paints the CPSR flags into the placeholder control, red when set and grey
// when clear, the way the old dialog did -- but positioned from the control
// itself so it survives font and DPI changes.
static void PaintCpsrFlags(HWND hwnd, HDC hdc)
{
	HWND placeholder = GetDlgItem(hwnd, IDC_CPSR_FLAGS);
	if (placeholder == NULL)
	{
		return;
	}

	RECT rect;
	GetWindowRect(placeholder, &rect);
	MapWindowPoints(NULL, hwnd, (LPPOINT)&rect, 2);

	const DisassemblyCore::Registers regs = DisassemblyCore::ReadCpuRegisters(Core().GetCpu());
	const bool flags[8] = { regs.N, regs.Z, regs.C, regs.V, regs.Q, regs.I, regs.F, regs.T };
	const char* names[8] = { "N", "Z", "C", "V", "Q", "I", "F", "T" };

	HGDIOBJ oldFont = SelectObject(hdc, uiFont::message(hwnd));
	SetBkMode(hdc, TRANSPARENT);

	const int cellWidth = (rect.right - rect.left) / 8;
	for (int i = 0; i < 8; ++i)
	{
		SetTextColor(hdc, flags[i] ? RGB(200, 0, 0) : RGB(110, 110, 110));
		TextOutA(hdc, rect.left + (i * cellWidth), rect.top, names[i], 1);
	}

	SelectObject(hdc, oldFont);
}

// ---------------------------------------------------------------------------
// Control refreshes
// ---------------------------------------------------------------------------

static void UpdateTitle(HWND hwnd)
{
	const char* cpuName = (Core().GetCpu() == DisassemblyCore::CPU_ARM7) ? "ARM7" : "ARM9";
	char title[64];
	snprintf(title, sizeof(title), "Disassembler - %s", cpuName);
	SetWindowTextA(hwnd, title);
}

static void RefreshRegisters(HWND hwnd)
{
	const DisassemblyCore::Registers regs = DisassemblyCore::ReadCpuRegisters(Core().GetCpu());

	char text[16];
	for (int i = 0; i < 16; ++i)
	{
		snprintf(text, sizeof(text), "%08X", (unsigned int)regs.R[i]);
		SetDlgItemTextA(hwnd, IDC_R0 + i, text);
	}

	snprintf(text, sizeof(text), "%02X", regs.Mode);
	SetDlgItemTextA(hwnd, IDC_MODE, text);

	snprintf(text, sizeof(text), "%08X", (unsigned int)regs.SPSR);
	SetDlgItemTextA(hwnd, IDC_TMP, text);
}

static void RefreshBreakpointList(HWND hwnd)
{
	if (DisView == NULL)
	{
		return;
	}

	const int cpu = Core().GetCpu();
	const int count = (int)Core().BreakpointCount();
	DisView->breakPos[cpu] = ClampBreakPos(DisView->breakPos[cpu]);

	char text[16];
	for (int i = 0; i < kBreakpointRows; ++i)
	{
		const int index = DisView->breakPos[cpu] + i;
		if (index < count)
		{
			snprintf(text, sizeof(text), "%08X", (unsigned int)Core().BreakpointAt((size_t)index));
		}
		else
		{
			snprintf(text, sizeof(text), "%08X", 0);
		}
		SetDlgItemTextA(hwnd, IDC_BP0 + i, text);
	}

	snprintf(text, sizeof(text), "%02i", DisView->breakPos[cpu]);
	SetDlgItemTextA(hwnd, IDC_BPCOUNT, text);
}

static void SyncModeButtons(HWND hwnd)
{
	const DisassemblyCore::ViewMode mode = Core().GetViewMode();
	CheckRadioButton(hwnd, IDC_AUTO_DES, IDC_THUMB,
	                 (mode == DisassemblyCore::ViewMode_ARM) ? IDC_ARM :
	                 (mode == DisassemblyCore::ViewMode_Thumb) ? IDC_THUMB : IDC_AUTO_DES);
}

static void RefreshAll(HWND hwnd)
{
	if (DisView == NULL)
	{
		return;
	}

	UpdateTitle(hwnd);
	SyncModeButtons(hwnd);
	RefreshRegisters(hwnd);
	RefreshBreakpointList(hwnd);
	UpdateBoxScrollBar(DisView->box);
	InvalidateRect(DisView->box, NULL, FALSE);
	InvalidateRect(hwnd, NULL, FALSE);
}

// Selects the CPU in the combo box without letting CBN_SELCHANGE re-enter.
static void SyncCpuCombo(HWND hwnd)
{
	if (DisView == NULL)
	{
		return;
	}

	SendDlgItemMessageA(hwnd, IDC_DISASM_CPU, CB_SETCURSEL, (WPARAM)Core().GetCpu(), 0);
	(void)DisView;
}

// The host has to resume emulation itself: the shared core only arms the CPU.
static void ResumeEmulation()
{
	execute = true;
	paused = false;
}

// ---------------------------------------------------------------------------
// Custom disassembly child window
// ---------------------------------------------------------------------------

LRESULT CALLBACK ViewDisasmBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_PAINT:
			PaintDisassembly(hwnd);
			return 0;

		case WM_ERASEBKGND:
			return 1;

		case WM_SIZE:
			UpdateBoxScrollBar(hwnd);
			InvalidateRect(hwnd, NULL, FALSE);
			return 0;

		case WM_MOUSEWHEEL:
		{
			const short delta = GET_WHEEL_DELTA_WPARAM(wParam);
			Core().ScrollLines(-(delta / WHEEL_DELTA) * 3);
			UpdateBoxScrollBar(hwnd);
			InvalidateRect(hwnd, NULL, FALSE);
			return 0;
		}

		case WM_VSCROLL:
		{
			const uint32_t instructionSize = Core().InstructionSize();
			const int maxLine = (int)(DisassemblyCore::kMaxAddress / instructionSize);

			SCROLLINFO si;
			memset(&si, 0, sizeof(si));
			si.cbSize = sizeof(si);
			si.fMask = SIF_ALL;
			GetScrollInfo(hwnd, SB_VERT, &si);

			int position = si.nPos;
			switch (LOWORD(wParam))
			{
				case SB_LINEUP:       position -= 1; break;
				case SB_LINEDOWN:     position += 1; break;
				case SB_PAGEUP:       position -= (int)si.nPage; break;
				case SB_PAGEDOWN:     position += (int)si.nPage; break;
				case SB_THUMBTRACK:
				case SB_THUMBPOSITION: position = si.nTrackPos; break;
				case SB_TOP:          position = 0; break;
				case SB_BOTTOM:       position = maxLine; break;
				default:              return 0;
			}

			position = (position < 0) ? 0 : ((position > maxLine) ? maxLine : position);
			Core().SetAnchorAddress((uint32_t)position * instructionSize);
			UpdateBoxScrollBar(hwnd);
			InvalidateRect(hwnd, NULL, FALSE);
			return 0;
		}

	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Dialog
// ---------------------------------------------------------------------------

void DisView_NotifyDebugBreak(int cpu, u32 address)
{
	if (DisViewWnd == NULL)
	{
		return;
	}

	PostMessageA(DisViewWnd, WM_DISVIEW_DEBUG_BREAK, (WPARAM)cpu, (LPARAM)address);
}

HWND DisView_GetListHwnd(void)
{
	return (DisView != NULL) ? DisView->box : NULL;
}

BOOL CALLBACK ViewDisasmProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Bail out early if the dialog is not initialized yet.
	if ((DisView == NULL) && (message != WM_INITDIALOG))
	{
		return FALSE;
	}

	switch (message)
	{
		case WM_INITDIALOG:
		{
			DisView = new DisViewState;
			memset(DisView, 0, sizeof(DisViewState));
			DisView->hwnd = hwnd;
			DisView->cpu = DisassemblyCore::CPU_ARM9;
			DisView->autoUpdateFrames = 1;
			DisViewWnd = hwnd;

			Core().SetCpu(DisassemblyCore::CPU_ARM9);

			SendDlgItemMessageA(hwnd, IDC_DISASM_CPU, CB_ADDSTRING, 0, (LPARAM)"ARM9");
			SendDlgItemMessageA(hwnd, IDC_DISASM_CPU, CB_ADDSTRING, 0, (LPARAM)"ARM7");
			SendDlgItemMessageA(hwnd, IDC_DISASM_CPU, CB_SETCURSEL, 0, 0);

			DisView->box = GetDlgItem(hwnd, IDC_DES_BOX);

			// PC is shown but not editable: writing it from a text field while
			// the CPU is running has no sane semantics.
			SendDlgItemMessageA(hwnd, IDC_R15, EM_SETREADONLY, TRUE, 0);

			SendDlgItemMessageA(hwnd, IDC_AUTO_UPDATE_SPIN, UDM_SETRANGE, 0, MAKELONG(99, 1));
			SendDlgItemMessageA(hwnd, IDC_AUTO_UPDATE_SPIN, UDM_SETPOS32, 0, DisView->autoUpdateFrames);
			SetDlgItemInt(hwnd, IDC_AUTO_UPDATE_SECS, DisView->autoUpdateFrames, FALSE);

			CheckRadioButton(hwnd, IDC_AUTO_DES, IDC_THUMB, IDC_AUTO_DES);
			Core().SetViewMode(DisassemblyCore::ViewMode_Auto);

			// Start on the instruction the CPU is about to run, like the old
			// dialogs did not (they always opened at address 0).
			Core().FollowPC();

			RefreshAll(hwnd);
			return TRUE;
		}

		case WM_CLOSE:
		{
			if (DisView != NULL)
			{
				if (DisView->autoUpdate)
				{
					KillTimer(hwnd, IDT_VIEW_DISASM);
					DisView->autoUpdate = false;
				}

				delete DisView;
				DisView = NULL;
			}

			DisViewWnd = NULL;

			// The dialog is modeless (CreateDialog), so EndDialog would not
			// destroy it. Destroy it first and then tell the tool class, so
			// unregClass() runs after the last window of the class is gone.
			DestroyWindow(hwnd);
			if (ViewDisasm != NULL)
			{
				ViewDisasm->doClose();
			}
			return TRUE;
		}

		case WM_NCDESTROY:
			// Safety net for CloseAllToolWindows(), which destroys the window
			// without going through WM_CLOSE first.
			if (DisView != NULL)
			{
				delete DisView;
				DisView = NULL;
			}
			DisViewWnd = NULL;
			return TRUE;

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			PaintCpsrFlags(hwnd, hdc);
			EndPaint(hwnd, &ps);
			return TRUE;
		}

		case WM_MOUSEWHEEL:
		{
			// The list is a custom child window, so it only ever sees wheel
			// messages when it holds the focus -- and it rarely does, because
			// the dialog is modeless. Route by pointer position instead: main.cpp
			// covers the case where the main window has the focus, this covers
			// the one where an edit field inside this dialog has it.
			HWND box = DisView_GetListHwnd();
			POINT pt;
			pt.x = (short)LOWORD(lParam);
			pt.y = (short)HIWORD(lParam);
			if ((box != NULL) && (WindowFromPoint(pt) == box))
			{
				SendMessage(box, WM_MOUSEWHEEL, wParam, lParam);
				return TRUE;
			}
			break;
		}

		case WM_ACTIVATE:
			if (LOWORD(wParam) != WA_INACTIVE)
			{
				RefreshAll(hwnd);
			}
			return TRUE;

		case WM_DISVIEW_DEBUG_BREAK:
		{
			const int cpu = (int)wParam;
			const uint32_t address = (uint32_t)lParam;

			Core().SetCpu((cpu == NDS_DEBUG_CPU_ARM7) ? DisassemblyCore::CPU_ARM7 : DisassemblyCore::CPU_ARM9);
			SyncCpuCombo(hwnd);
			Core().SetAnchorAddress(address);

			RefreshAll(hwnd);
			ShowWindow(hwnd, SW_SHOW);
			SetForegroundWindow(hwnd);
			return TRUE;
		}

		case WM_TIMER:
			if (wParam == IDT_VIEW_DISASM)
			{
				// Only chase the PC while the CPU is actually executing. When
				// emulation is paused the PC cannot move, so re-anchoring would
				// just undo the user's scrolling, and the refresh itself is a
				// no-op.
				if (execute)
				{
					Core().FollowPC();
					RefreshAll(hwnd);
				}
			}
			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDC_DISASM_CPU:
					if (HIWORD(wParam) == CBN_SELCHANGE)
					{
						const int selection = (int)SendDlgItemMessageA(hwnd, IDC_DISASM_CPU, CB_GETCURSEL, 0, 0);
						Core().SetCpu((selection == 1) ? DisassemblyCore::CPU_ARM7 : DisassemblyCore::CPU_ARM9);
						Core().SetAnchorAddress(Core().AnchorAddress()); // re-align for the new CPU
						RefreshAll(hwnd);
					}
					return TRUE;

				case IDC_FERMER:
					SendMessage(hwnd, WM_CLOSE, 0, 0);
					return TRUE;

				case IDC_AUTO_DES:
					Core().SetViewMode(DisassemblyCore::ViewMode_Auto);
					RefreshAll(hwnd);
					return TRUE;

				case IDC_ARM:
					Core().SetViewMode(DisassemblyCore::ViewMode_ARM);
					RefreshAll(hwnd);
					return TRUE;

				case IDC_THUMB:
					Core().SetViewMode(DisassemblyCore::ViewMode_Thumb);
					RefreshAll(hwnd);
					return TRUE;

				case IDC_AUTO_UPDATE:
					if (DisView->autoUpdate)
					{
						KillTimer(hwnd, IDT_VIEW_DISASM);
						DisView->autoUpdate = false;
						EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), FALSE);
						EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), FALSE);
					}
					else
					{
						DisView->autoUpdate = true;
						EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), TRUE);
						EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), TRUE);
						SetTimer(hwnd, IDT_VIEW_DISASM, DisView->autoUpdateFrames * 20, NULL);
					}
					return TRUE;

				case IDC_AUTO_UPDATE_SECS:
				{
					BOOL translated = FALSE;
					const UINT frames = GetDlgItemInt(hwnd, IDC_AUTO_UPDATE_SECS, &translated, FALSE);
					if (translated && (frames >= 1) && (frames <= 99))
					{
						DisView->autoUpdateFrames = frames;
						if (DisView->autoUpdate)
						{
							SetTimer(hwnd, IDT_VIEW_DISASM, DisView->autoUpdateFrames * 20, NULL);
						}
					}
					return TRUE;
				}

				case IDC_REFRESH:
					Core().FollowPC();
					RefreshAll(hwnd);
					return TRUE;

				case IDC_STEP:
					Core().StepInstruction();
					ResumeEmulation();
					RefreshAll(hwnd);
					return TRUE;

				case IDC_CONTINUE:
					Core().Continue();
					ResumeEmulation();
					return TRUE;

				case IDC_RUNRET:
					Core().RunToReturn();
					ResumeEmulation();
					return TRUE;

				case IDC_STEPOVER:
					Core().StepOver();
					ResumeEmulation();
					return TRUE;

				case IDC_GO:
				{
					char text[32];
					GetDlgItemTextA(hwnd, IDC_GOTODES, text, (int)sizeof(text));

					uint32_t address = 0;
					if (DisassemblyCore::ParseHex32(text, address))
					{
						Core().SetAnchorAddress(address);
						RefreshAll(hwnd);
					}
					return TRUE;
				}

				case IDC_BREAKPOINT:
				{
					char text[32];
					GetDlgItemTextA(hwnd, IDC_BPDEST, text, (int)sizeof(text));

					uint32_t address = 0;
					if (DisassemblyCore::ParseHex32(text, address))
					{
						Core().AddBreakpoint(address);
						RefreshAll(hwnd);
					}
					return TRUE;
				}

				case IDC_DELBP:
					Core().RemoveBreakpointAt((size_t)DisView->breakPos[Core().GetCpu()]);
					RefreshAll(hwnd);
					return TRUE;

				case IDC_BPUP:
					DisView->breakPos[Core().GetCpu()] -= 1;
					RefreshBreakpointList(hwnd);
					return TRUE;

				case IDC_BPDOWN:
					DisView->breakPos[Core().GetCpu()] += 1;
					RefreshBreakpointList(hwnd);
					return TRUE;

				case IDC_REGUPDATE:
					for (int i = 0; i < 15; ++i)
					{
						char text[32];
						GetDlgItemTextA(hwnd, IDC_R0 + i, text, (int)sizeof(text));

						uint32_t value = 0;
						if (DisassemblyCore::ParseHex32(text, value))
						{
							Core().WriteRegister((unsigned int)i, value);
						}
					}
					RefreshAll(hwnd);
					return TRUE;
			}
			return FALSE;
	}

	return FALSE;
}
