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

// The system's own UI font for the frontend's dialogs.
//
// The dialog templates in resources.rc ask for 8 point "MS Shell Dlg" (or "MS
// Sans Serif", or DS_FIXEDSYS, which gets the stock bitmap "Fixedsys"), all of
// which are 2005 era faces.  Their control positions are in dialog units, and
// those are derived from the template font, so the face is replaced at run time
// instead of in the resource script: the layout is never touched.
//
// The replacement is NONCLIENTMETRICS.lfMessageFont, asked for with
// SystemParametersInfoForDpi() so a per-monitor aware process gets the right
// size on every monitor.  On a Chinese install that is 9 point "Microsoft YaHei
// UI", whose line is 17 pixels tall against the 13 the templates were drawn
// for; a static control that is too short clips its text and wraps labels that
// used to fit on one line.  The face is therefore used at the largest size that
// still fits the template's line height, which keeps every control where it is.
//
// The dialogs are discovered with a thread local WH_CALLWNDPROC hook, so no
// dialog procedure has to be touched.  Every dialog is subclassed afterwards to
// rebuild its font when it moves to a monitor with another DPI, and to follow
// the option when it is switched while the dialog is open.

// Declarations only: every per-monitor entry point below is still resolved with
// GetProcAddress, this just decides which SDK prototypes this file may use.
// SetWindowSubclass() and GetFontQuality()'s callers need at least XP.
#define _WIN32_WINNT 0x0600
#define WINVER       0x0600

#include <windows.h>
#include <commctrl.h>
#include <wchar.h>
#include <string.h>

#include "UiFont.h"
#include "DPI.h"

// From CWindow.cpp.  Declared here instead of pulling in CWindow.h, which drags
// the whole window toolkit into this file.
DWORD GetFontQuality();

#if !defined(WM_DPICHANGED)
#	define WM_DPICHANGED 0x02E0
#endif

namespace
{
	// Window properties.  The template font, the DPI and the line height are
	// stored per dialog so that a mode or DPI change can be applied without
	// guessing what the dialog started with.
	const wchar_t * const kDpiProp       = L"DeSmuMEUiFontDpi";
	const wchar_t * const kLimitProp     = L"DeSmuMEUiFontLimit";
	const wchar_t * const kTemplateProp  = L"DeSmuMEUiFontTemplate";
	const wchar_t * const kAppliedProp   = L"DeSmuMEUiFontApplied";
	const wchar_t * const kTrackedProp   = L"DeSmuMEUiFontTracked";
	const wchar_t * const kResizableProp = L"DeSmuMEUiFontResizable";

	// Posted to every converted dialog when the mode is switched, so the option
	// takes effect on the windows that are already open.
	const UINT kApplyMessage = WM_APP + 0x3E1;

	const int kSubclassId = 1;
	const int kMaxDpiSets = 8;
	const int kMaxMonoFonts = 16;
	const int kMaxDialogs = 32;

	struct CriticalSection
	{
		CRITICAL_SECTION cs;
		CriticalSection()  { InitializeCriticalSection(&cs); }
		~CriticalSection() { DeleteCriticalSection(&cs); }
	};

	// Function local static: initialized on first use, thread safe in C++11.
	CriticalSection &lock()
	{
		static CriticalSection instance;
		return instance;
	}

	class Guard
	{
	public:
		Guard()  { EnterCriticalSection(&lock().cs); }
		~Guard() { LeaveCriticalSection(&lock().cs); }
	};

	// One cache entry per monitor DPI the frontend has seen.  The fonts are
	// process wide GDI objects, so the tool windows' threads share the cache;
	// the (empty after startup) lock keeps that honest.
	struct FontSet
	{
		UINT  dpi;
		HFONT message;		// message font at this DPI
		HFONT fitted;		// message font cut down to fit a template line
		int   fitLimit;		// template line height the fitted font was made for
	};

	FontSet g_sets[kMaxDpiSets];
	int     g_setCount = 0;

	// The fixed pitch face is keyed by the text height it has to fit, not by
	// DPI: lfHeight is in pixels, so one font serves every monitor.
	struct MonoFont
	{
		int   limit;
		HFONT font;
	};

	MonoFont g_mono[kMaxMonoFonts];
	int      g_monoCount = 0;

	// Dialogs that carry our font, so switching the mode can reach the ones that
	// are already open.  They live on several threads (one per tool window), so
	// the list, like the font caches, is guarded.
	HWND g_dialogs[kMaxDialogs];
	int  g_dialogCount = 0;

	bool    g_modern = true;
	HFONT   g_systemFixed = NULL;

	// Per thread: the hook handle.  A WH_CALLWNDPROC hook only sees the windows
	// of the thread it was installed on, so the tool window threads each need
	// their own.
	thread_local HHOOK t_hook = NULL;

	HFONT systemFixed()
	{
		if (g_systemFixed == NULL)
		{
			g_systemFixed = (HFONT)GetStockObject(SYSTEM_FIXED_FONT);
		}

		return g_systemFixed;
	}

	void storeLayout(HWND dialog, UINT dpi, int limit)
	{
		SetPropW(dialog, kDpiProp, (HANDLE)(UINT_PTR)dpi);
		SetPropW(dialog, kLimitProp, (HANDLE)(UINT_PTR)limit);
	}

	int fontHeight(HFONT font)
	{
		if (font == NULL)
		{
			return 0;
		}

		HDC dc = GetDC(NULL);
		if (dc == NULL)
		{
			return 0;
		}

		HGDIOBJ old = SelectObject(dc, font);
		TEXTMETRICW metrics;
		const BOOL ok = GetTextMetricsW(dc, &metrics);
		SelectObject(dc, old);
		ReleaseDC(NULL, dc);

		return ok ? metrics.tmHeight : 0;
	}

	bool fontFace(HFONT font, wchar_t *face)
	{
		face[0] = L'\0';
		if (font == NULL)
		{
			return false;
		}

		HDC dc = GetDC(NULL);
		if (dc == NULL)
		{
			return false;
		}

		HGDIOBJ old = SelectObject(dc, font);
		const int length = GetTextFaceW(dc, LF_FACESIZE, face);
		SelectObject(dc, old);
		ReleaseDC(NULL, dc);

		return length > 0;
	}

	int CALLBACK countFace(const LOGFONTW *, const TEXTMETRICW *, DWORD, LPARAM param)
	{
		(*(int *)param)++;
		return 1;
	}

	bool faceExists(const wchar_t *face)
	{
		LOGFONTW filter;
		ZeroMemory(&filter, sizeof(filter));
		filter.lfCharSet = DEFAULT_CHARSET;
		wcsncpy(filter.lfFaceName, face, LF_FACESIZE - 1);

		int count = 0;
		HDC dc = GetDC(NULL);
		if (dc == NULL)
		{
			return false;
		}

		EnumFontFamiliesExW(dc, &filter, countFace, (LPARAM)&count, 0);
		ReleaseDC(NULL, dc);

		return count > 0;
	}

	// Best installed fixed pitch face, resolved once.  Consolas ships with Vista
	// and later, Cascadia Mono comes with the Windows Terminal, Courier New is
	// the last resort that is on every install.  Called with the lock held.
	const wchar_t *monoFaceWLocked()
	{
		static wchar_t chosen[LF_FACESIZE] = { 0 };
		if (chosen[0] != L'\0')
		{
			return chosen;
		}

		static const wchar_t * const candidates[] =
		{
			L"Consolas", L"Cascadia Mono", L"Courier New"
		};

		const wchar_t *pick = L"Courier New";
		for (int i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); i++)
		{
			if (faceExists(candidates[i]))
			{
				pick = candidates[i];
				break;
			}
		}

		wcsncpy(chosen, pick, LF_FACESIZE - 1);
		return chosen;
	}

	// The message font for one monitor.  Returns a LOGFONT so the caller can cut
	// the size down before creating anything.
	void messageLogFont(UINT dpi, LOGFONTW *lf)
	{
		NONCLIENTMETRICSW metrics;
		ZeroMemory(&metrics, sizeof(metrics));
		metrics.cbSize = sizeof(metrics);

		if (DPI_SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi))
		{
			*lf = metrics.lfMessageFont;
			return;
		}

		if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
		{
			*lf = metrics.lfMessageFont;
			// Before Windows 10 the metrics are the 96 DPI ones whatever the
			// monitor says, so scale them here.
			lf->lfHeight = MulDiv(lf->lfHeight, (int)dpi, 96);
			lf->lfWidth  = MulDiv(lf->lfWidth, (int)dpi, 96);
			return;
		}

		// No metrics at all: 9 point on the shell face, which is what Windows
		// itself falls back to.
		ZeroMemory(lf, sizeof(*lf));
		lf->lfHeight = -MulDiv(9, (int)dpi, 72);
		lf->lfWeight = FW_NORMAL;
		lf->lfCharSet = DEFAULT_CHARSET;
		wcsncpy(lf->lfFaceName, L"MS Shell Dlg", LF_FACESIZE - 1);
	}

	FontSet *setForLocked(UINT dpi)
	{
		for (int i = 0; i < g_setCount; i++)
		{
			if (g_sets[i].dpi == dpi)
			{
				return &g_sets[i];
			}
		}

		if (g_setCount >= kMaxDpiSets)
		{
			return &g_sets[0];	// cannot happen with a sane monitor count
		}

		FontSet *set = &g_sets[g_setCount++];
		ZeroMemory(set, sizeof(*set));
		set->dpi = dpi;
		set->fitLimit = -1;

		LOGFONTW lf;
		messageLogFont(dpi, &lf);
		set->message = CreateFontIndirectW(&lf);

		return set;
	}

	// Largest size of this DPI's message font that still fits limit pixels.
	HFONT fittedForLocked(FontSet *set, int limit)
	{
		if ( (set == NULL) || (set->message == NULL) || (limit <= 0) )
		{
			return (set != NULL) ? set->message : NULL;
		}

		if ( (set->fitted != NULL) && (set->fitLimit == limit) )
		{
			return set->fitted;
		}

		if (set->fitted != NULL)
		{
			DeleteObject(set->fitted);
			set->fitted = NULL;
			set->fitLimit = -1;
		}

		// One pixel is allowed: tmHeight counts line spacing, not ink.  The 8
		// point line the templates were drawn for is 13 pixels at 96 DPI and its
		// ink reaches row 12, and so does the ink of the 14 pixel face that is
		// accepted here.  A taller face would clip the descenders.
		if (fontHeight(set->message) <= limit + 1)
		{
			return set->message;
		}

		LOGFONTW lf;
		if (GetObjectW(set->message, sizeof(lf), &lf) == 0)
		{
			return set->message;
		}

		for (int height = lf.lfHeight + 1; height < -4; height++)
		{
			lf.lfHeight = height;
			HFONT candidate = CreateFontIndirectW(&lf);
			if (candidate == NULL)
			{
				break;
			}

			if (fontHeight(candidate) <= limit + 1)
			{
				set->fitted = candidate;
				set->fitLimit = limit;
				return set->fitted;
			}

			DeleteObject(candidate);
		}

		return set->message;
	}

	// Fixed pitch font whose line is at most limit pixels tall.
	HFONT monoForLocked(int limit)
	{
		if (limit <= 0)
		{
			return systemFixed();
		}

		for (int i = 0; i < g_monoCount; i++)
		{
			if (g_mono[i].limit == limit)
			{
				return g_mono[i].font;
			}
		}

		LOGFONTW lf;
		ZeroMemory(&lf, sizeof(lf));
		lf.lfWeight        = FW_NORMAL;
		lf.lfCharSet       = DEFAULT_CHARSET;
		lf.lfOutPrecision  = OUT_TT_ONLY_PRECIS;	// never a bitmap face
		lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
		lf.lfQuality       = GetFontQuality();
		lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
		wcsncpy(lf.lfFaceName, monoFaceWLocked(), LF_FACESIZE - 1);

		// Largest size that fits, so walk from the biggest down: a fixed pitch
		// face is about 1.2 times the requested height.
		HFONT font = NULL;
		for (int height = -(limit + 4); height <= -6; height++)
		{
			lf.lfHeight = height;
			font = CreateFontIndirectW(&lf);
			if (font == NULL)
			{
				break;
			}

			if (fontHeight(font) <= limit)
			{
				break;
			}

			DeleteObject(font);
			font = NULL;
		}

		if ( (font != NULL) && (g_monoCount < kMaxMonoFonts) )
		{
			g_mono[g_monoCount].limit = limit;
			g_mono[g_monoCount].font = font;
			g_monoCount++;
		}

		return (font != NULL) ? font : systemFixed();
	}

	// Only the emulator's own templates are converted.  Message boxes, the
	// common file dialog and the font picker are system windows that already ask
	// for the right font; walking into them would shrink their text.
	bool isLegacyTemplate(HFONT templ)
	{
		static const wchar_t * const legacy[] =
		{
			L"MS Sans Serif", L"MS Shell Dlg", L"MS Shell Dlg 2",
			L"Fixedsys", L"Terminal", L"System", L"Courier", L"Small Fonts"
		};

		wchar_t face[LF_FACESIZE];
		if ( !fontFace(templ, face) )
		{
			return false;
		}

		for (int i = 0; i < (int)(sizeof(legacy) / sizeof(legacy[0])); i++)
		{
			if (_wcsicmp(face, legacy[i]) == 0)
			{
				return true;
			}
		}

		return false;
	}

	struct FontSwap
	{
		HFONT from;		// font the dialog started with
		HFONT applied;	// font this module installed, NULL if none
		HFONT to;		// font to install now
	};

	BOOL CALLBACK replaceFont(HWND child, LPARAM param)
	{
		const FontSwap *swap = (const FontSwap *)param;

		const HFONT current = (HFONT)SendMessageW(child, WM_GETFONT, 0, 0);
		if ( (current == swap->from) || ((swap->applied != NULL) && (current == swap->applied)) )
		{
			SendMessageW(child, WM_SETFONT, (WPARAM)swap->to, FALSE);
		}

		return TRUE;
	}

	// Gives the dialog and its children the font the current mode asks for.
	// Controls that were given a font of their own (the memory viewer's hex
	// box, the IO view's combo boxes) are left alone: only the template font and
	// the font this module installed are replaced.
	void applyModeLocked(HWND dialog)
	{
		const HFONT templ = (HFONT)GetPropW(dialog, kTemplateProp);
		if (templ == NULL)
		{
			return;
		}

		const HFONT applied = (HFONT)GetPropW(dialog, kAppliedProp);
		HFONT target = templ;

		if (g_modern)
		{
			const int limit = (int)(INT_PTR)GetPropW(dialog, kLimitProp);
			HFONT fitted = fittedForLocked(setForLocked((UINT)DPI_GetForWindow(dialog)), limit);
			if (fitted != NULL)
			{
				target = fitted;
			}
		}

		if (target == applied)
		{
			return;
		}

		SendMessageW(dialog, WM_SETFONT, (WPARAM)target, FALSE);

		FontSwap swap = { templ, applied, target };
		EnumChildWindows(dialog, replaceFont, (LPARAM)&swap);

		SetPropW(dialog, kAppliedProp, (HANDLE)target);
	}

	void registerDialog(HWND dialog)
	{
		for (int i = 0; i < g_dialogCount; i++)
		{
			if (g_dialogs[i] == dialog)
			{
				return;
			}
		}

		if (g_dialogCount < kMaxDialogs)
		{
			g_dialogs[g_dialogCount++] = dialog;
		}
	}

	void unregisterDialog(HWND dialog)
	{
		for (int i = 0; i < g_dialogCount; i++)
		{
			if (g_dialogs[i] == dialog)
			{
				g_dialogs[i] = g_dialogs[--g_dialogCount];
				g_dialogs[g_dialogCount] = NULL;
				return;
			}
		}
	}

	LRESULT CALLBACK dialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR);

	// Returns true when this is one of the emulator's own dialogs, which is what
	// tells the hook to keep track of it.  Message boxes and the common dialogs
	// handle their own fonts and their own DPI scaling and must be left alone.
	bool prepareDialog(HWND dialog)
	{
		if (dialog == NULL)
		{
			return false;
		}

		const HFONT templ = (HFONT)SendMessageW(dialog, WM_GETFONT, 0, 0);
		if ( !isLegacyTemplate(templ) )
		{
			return false;
		}

		Guard guard;

		SetPropW(dialog, kTemplateProp, (HANDLE)templ);
		storeLayout(dialog, (UINT)DPI_GetForWindow(dialog), fontHeight(templ));

		applyModeLocked(dialog);
		registerDialog(dialog);

		// Subclassed once, so the font can follow a mode change and the controls
		// can follow a monitor change.
		if (GetPropW(dialog, kTrackedProp) == NULL)
		{
			SetWindowSubclass(dialog, dialogProc, kSubclassId, 0);
			SetPropW(dialog, kTrackedProp, (HANDLE)1);
		}

		return true;
	}

	struct ScaleContext
	{
		UINT from;
		UINT to;
	};

	BOOL CALLBACK scaleChild(HWND child, LPARAM param)
	{
		const ScaleContext *scale = (const ScaleContext *)param;
		if (scale->from == scale->to)
		{
			return TRUE;
		}

		RECT rect;
		GetWindowRect(child, &rect);
		MapWindowPoints(NULL, GetParent(child), (POINT *)&rect, 2);

		const int x = MulDiv(rect.left, scale->to, scale->from);
		const int y = MulDiv(rect.top, scale->to, scale->from);
		const int w = MulDiv(rect.right  - rect.left, scale->to, scale->from);
		const int h = MulDiv(rect.bottom - rect.top,  scale->to, scale->from);

		SetWindowPos(child, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
		return TRUE;
	}

	void scaleChildren(HWND dialog, UINT fromDpi, UINT toDpi)
	{
		if ( (dialog == NULL) || (fromDpi == toDpi) || (fromDpi == 0) || (toDpi == 0) )
		{
			return;
		}

		ScaleContext scale = { fromDpi, toDpi };
		EnumChildWindows(dialog, scaleChild, (LPARAM)&scale);
	}

	void onDpiChangedInternal(HWND dialog, UINT oldDpi, UINT newDpi)
	{
		if ( (dialog == NULL) || (newDpi == 0) )
		{
			return;
		}

		if (oldDpi == 0)
		{
			oldDpi = newDpi;
		}

		int limit = (int)(INT_PTR)GetPropW(dialog, kLimitProp);
		if ( (limit > 0) && (oldDpi != newDpi) )
		{
			limit = MulDiv(limit, newDpi, oldDpi);
		}

		Guard guard;

		storeLayout(dialog, newDpi, limit);

		// Rebuilds the font for the new DPI; the children follow because they
		// still carry the font this module installed.
		applyModeLocked(dialog);
	}

	LRESULT CALLBACK dialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
	{
		switch (message)
		{
			case WM_INITDIALOG:
				(void)prepareDialog(dialog);
				break;

			case kApplyMessage:
				{
					Guard guard;
					applyModeLocked(dialog);
					RedrawWindow(dialog, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
				}
				return 0;

			case WM_DPICHANGED:
				{
					// Dialogs that lay themselves out (the resizable viewers)
					// do their own scaling after telling their sizer about the
					// new client size, so the hook must not move them twice.
					if (GetPropW(dialog, kResizableProp) == NULL)
					{
						UINT oldDpi = (UINT)(UINT_PTR)GetPropW(dialog, kDpiProp);
						const UINT newDpi = LOWORD(wParam);
						if (oldDpi == 0)
						{
							oldDpi = newDpi;
						}

						scaleChildren(dialog, oldDpi, newDpi);

						const RECT *target = (const RECT *)lParam;
						SetWindowPos(dialog, NULL, target->left, target->top,
						             target->right - target->left,
						             target->bottom - target->top,
						             SWP_NOZORDER | SWP_NOACTIVATE);

						onDpiChangedInternal(dialog, oldDpi, newDpi);
						return 0;
					}
				}
				break;

			case WM_NCDESTROY:
				{
					Guard guard;
					unregisterDialog(dialog);
				}
				RemoveWindowSubclass(dialog, dialogProc, kSubclassId);
				break;
		}

		return DefSubclassProc(dialog, message, wParam, lParam);
	}

	LRESULT CALLBACK hookProc(int code, WPARAM wParam, LPARAM lParam)
	{
		if (code == HC_ACTION)
		{
			const CWPSTRUCT *info = (const CWPSTRUCT *)lParam;
			if (info->message == WM_INITDIALOG)
			{
				(void)prepareDialog(info->hwnd);
			}
		}

		return CallNextHookEx(t_hook, code, wParam, lParam);
	}
}

namespace uiFont
{
	void init()
	{
		{
			Guard guard;
			systemFixed();
			monoFaceWLocked();

			// Build the fonts for the monitor the process starts on so that the
			// first dialog does not pay for it.
			setForLocked((UINT)DPI_GetSystem());
		}

		if (t_hook != NULL)
		{
			return;
		}

		t_hook = SetWindowsHookExW(WH_CALLWNDPROC, hookProc, NULL, GetCurrentThreadId());
	}

	void setModern(bool enabled)
	{
		HWND dialogs[kMaxDialogs];
		int count = 0;

		{
			Guard guard;
			if (g_modern == enabled)
			{
				return;
			}

			g_modern = enabled;

			// The dialogs are on other threads as well, so take a copy and post
			// to them once the lock is released.
			count = g_dialogCount;
			for (int i = 0; i < count; i++)
			{
				dialogs[i] = g_dialogs[i];
			}
		}

		for (int i = 0; i < count; i++)
		{
			PostMessageW(dialogs[i], kApplyMessage, 0, 0);
		}
	}

	bool modern()
	{
		return g_modern;
	}

	HFONT message(HWND window)
	{
		const UINT dpi = (UINT)DPI_GetForWindow(window);

		Guard guard;
		FontSet *set = setForLocked(dpi);
		return (set != NULL) ? set->message : NULL;
	}

	HFONT mono(HWND window)
	{
		if (!g_modern)
		{
			return systemFixed();
		}

		// Take the size from whatever the caller already uses, so the machine
		// generated text lines up with the labels next to it.
		HFONT reference = (window != NULL) ? (HFONT)SendMessageW(window, WM_GETFONT, 0, 0) : NULL;
		if ( (reference == NULL) && (window != NULL) )
		{
			reference = (HFONT)SendMessageW(GetParent(window), WM_GETFONT, 0, 0);
		}

		int limit = fontHeight(reference);
		if (limit <= 0)
		{
			limit = fontHeight(message(window));
		}

		Guard guard;
		return monoForLocked(limit);
	}

	const char *monoFace()
	{
		static char faceA[LF_FACESIZE] = { 0 };
		if (faceA[0] != '\0')
		{
			return faceA;
		}

		Guard guard;
		const wchar_t *faceW = monoFaceWLocked();
		WideCharToMultiByte(CP_ACP, 0, faceW, -1, faceA, LF_FACESIZE, NULL, NULL);
		return faceA;
	}

	void applyToDialog(HWND dialog)
	{
		(void)prepareDialog(dialog);
	}

	void scaleDialog(HWND dialog, UINT fromDpi, UINT toDpi)
	{
		scaleChildren(dialog, fromDpi, toDpi);
	}

	UINT dpiOf(HWND window)
	{
		return (UINT)DPI_GetForWindow(window);
	}

	UINT layoutDpi(HWND dialog)
	{
		const UINT dpi = (UINT)(UINT_PTR)GetPropW(dialog, kDpiProp);
		return (dpi != 0) ? dpi : (UINT)DPI_GetForWindow(dialog);
	}

	void onDpiChanged(HWND dialog, UINT oldDpi, UINT newDpi)
	{
		onDpiChangedInternal(dialog, oldDpi, newDpi);
	}

	void markResizable(HWND dialog)
	{
		if (dialog != NULL)
		{
			SetPropW(dialog, kResizableProp, (HANDLE)1);
		}
	}
}
