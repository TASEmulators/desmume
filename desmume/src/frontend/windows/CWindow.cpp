/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2017 DeSmuME team

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

#include "CWindow.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>

#include "debug.h"

#include "resource.h"
#include "main.h"
#include "DPI.h"
#include "DarkMode.h"
#include "UiFont.h"
#include "IORegView.h"
#include "windriver.h"


extern bool fsWindow;

//-----------------------------------------------------------------------------
//   The Toolkit - Helpers
//-----------------------------------------------------------------------------

DWORD GetFontQuality()
{
	BOOL aaEnabled = FALSE;
	UINT aaType = FE_FONTSMOOTHINGSTANDARD;

	SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &aaEnabled, 0);
	if (aaEnabled == FALSE)
		return NONANTIALIASED_QUALITY;

	if (SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &aaType, 0) == FALSE)
		return ANTIALIASED_QUALITY;

	if (aaType == FE_FONTSMOOTHINGCLEARTYPE)
		return CLEARTYPE_QUALITY;
	else
		return ANTIALIASED_QUALITY;
}

int DrawText(HDC hDC, char* text, int X, int Y, int Width, int Height, UINT format)
{
	RECT rc;
	SetRect(&rc, X, Y, X+Width, Y+Height);
	return DrawText(hDC, text, -1, &rc, format);
}

void GetFontSize(HWND hWnd, HFONT hFont, LPSIZE size)
{
	HDC dc = GetDC(hWnd);
	HFONT oldfont = (HFONT)SelectObject(dc, hFont);
	
	GetTextExtentPoint32(dc, " ", 1, size);

	SelectObject(dc, oldfont);
	ReleaseDC(hWnd, dc);
}

void MakeBitmapPseudoTransparent(HBITMAP hBmp, COLORREF cKeyColor, COLORREF cNewKeyColor)
{
	u8 keyr = (cKeyColor >> 16) & 0xFF;
	u8 keyg = (cKeyColor >> 8) & 0xFF;
	u8 keyb = cKeyColor & 0xFF;
	u8 nkeyr = (cNewKeyColor >> 16) & 0xFF;
	u8 nkeyg = (cNewKeyColor >> 8) & 0xFF;
	u8 nkeyb = cNewKeyColor & 0xFF;

	BITMAP bmp;
	BITMAPINFO bmpinfo;
	u8* bmpdata = NULL;
	
	HDC dc = CreateCompatibleDC(NULL);

	GetObject(hBmp, sizeof(BITMAP), &bmp);

	memset(&bmpinfo, 0, sizeof(BITMAPINFO));
	bmpinfo.bmiHeader.biSize = sizeof(BITMAPINFO);
	bmpinfo.bmiHeader.biBitCount = 24;
	bmpinfo.bmiHeader.biCompression = BI_RGB;
	bmpinfo.bmiHeader.biHeight = bmp.bmHeight;
	bmpinfo.bmiHeader.biWidth = bmp.bmWidth;
	bmpinfo.bmiHeader.biPlanes = bmp.bmPlanes;
	bmpdata = new u8[bmp.bmHeight * bmp.bmWidth * 3];

	GetDIBits(dc, hBmp, 0, bmp.bmHeight, (LPVOID)bmpdata, &bmpinfo, DIB_RGB_COLORS);

	int y2 = 0;
	for (int y = 0; y < bmp.bmHeight; y++)
	{
		for (int x = 0; x < bmp.bmWidth; x++)
		{
			int idx = y2 + x*3;
			u8 r = bmpdata[idx + 0];
			u8 g = bmpdata[idx + 1];
			u8 b = bmpdata[idx + 2];

			if ((r == keyr) && (g == keyg) && (b == keyb))
			{
				bmpdata[idx + 0] = nkeyr;
				bmpdata[idx + 1] = nkeyg;
				bmpdata[idx + 2] = nkeyb;
			}
		}
		y2 += bmp.bmWidth * 3;
	}

	SetDIBits(dc, hBmp, 0, bmp.bmHeight, (LPVOID)bmpdata, &bmpinfo, DIB_RGB_COLORS);
	DeleteDC(dc);
	delete[] bmpdata;
}

//-----------------------------------------------------------------------------
// Window class handling
//-----------------------------------------------------------------------------

vector<wstring> ReggedWndClasses;

bool RegWndClass(wstring name, WNDPROC wndProc, UINT style, int extraSize)
{
	return RegWndClass(name, wndProc, style, NULL, extraSize);
}

bool RegWndClass(wstring name, WNDPROC wndProc, UINT style, HICON icon, int extraSize)
{
	// If the class is already regged, don't re-reg it
	if (find(ReggedWndClasses.begin(), ReggedWndClasses.end(), name) != ReggedWndClasses.end())
		return true;

	WNDCLASSEXW wc;

	wc.cbSize         = sizeof(wc);
	wc.lpszClassName  = name.c_str();
	wc.hInstance      = hAppInst;
	wc.lpfnWndProc    = wndProc;
	wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon          = icon;
	wc.lpszMenuName   = 0;
	wc.hbrBackground  = GetSysColorBrush(COLOR_BTNFACE);
	wc.style          = style;
	wc.cbClsExtra     = 0;
	wc.cbWndExtra     = DWLP_USER + extraSize;
	wc.hIconSm        = 0;

	if (RegisterClassExW(&wc) != 0)
	{
		// If registration succeeded, add the class name into the list
		ReggedWndClasses.push_back(name);
		return true;
	}
	else
		return false;
}

void UnregWndClass(wstring name)
{
	vector<wstring>::iterator it = find(ReggedWndClasses.begin(), ReggedWndClasses.end(), name);

	// If the class wasn't regged, we can't unreg it :P
	if (it == ReggedWndClasses.end())
		return;

	// Otherwise unreg the class and remove its name from the list
	// ONLY if unregging was successful. Unregging will fail if one
	// or more windows using the class still exist.
	if (UnregisterClassW(name.c_str(), hAppInst) != 0)
		ReggedWndClasses.erase(it);
}

//-----------------------------------------------------------------------------
// Base toolwindow class
//-----------------------------------------------------------------------------

CToolWindow::CToolWindow(char* _className, WNDPROC _proc, char* _title, int _width, int _height)
	: hWnd(NULL)
	, className(_className)
	, proc((DLGPROC)_proc)
	, title(_title)
	, width(_width)
	, height(_height)
	, whichInit(0)
{
}

CToolWindow::CToolWindow(int _ID, DLGPROC _proc, char* _title)
	: hWnd(NULL)
	, ID(_ID)
	, proc(_proc)
	, title(_title)
	, whichInit(1)
{
}

void CToolWindow::PostInitialize()
{
	if(whichInit==0)
	{
		DWORD style = WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
		RECT rc;

		// The tool windows size themselves in pixels, which are logical units
		// here, so scale them for the monitor the main window sits on.
		HWND hRef = (MainWindow != NULL) ? MainWindow->getHWnd() : NULL;
		const int scaledWidth = DPI_ScaleForWindow(width, hRef);
		const int scaledHeight = DPI_ScaleForWindow(height, hRef);

		rc.left = 0;
		rc.right = scaledWidth;
		rc.top = 0;
		rc.bottom = scaledHeight;
		DPI_AdjustWindowRectEx(&rc, style, FALSE, 0, hRef);

		hWnd = CreateWindow(className, title.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT, 
			rc.right - rc.left, rc.bottom - rc.top, HWND_DESKTOP, NULL, hAppInst, (LPVOID)this);
	}
	else
	{
		hWnd = CreateDialogParam(hAppInst, MAKEINTRESOURCE(ID), HWND_DESKTOP, proc, (LPARAM)this);
		if (hWnd == NULL)
			return;

		SetWindowText(hWnd, title.c_str());
		SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(hAppInst, MAKEINTRESOURCE(ICONDESMUME)));
	}
}

CToolWindow::~CToolWindow()
{
}

//-----------------------------------------------------------------------------
// Toolwindow handling
//-----------------------------------------------------------------------------

CToolWindow* ToolWindowList = NULL;

bool OpenToolWindow(CToolWindow* wnd)
{
	// A hWnd value of NULL indicates failure to create the window.
	// In this case, just delete the toolwindow and return failure.
	if (wnd->hWnd == NULL)
	{
		delete wnd;
		return false;
	}

	// Add the toolwindow to the list
	if (ToolWindowList == NULL)
	{
		ToolWindowList = wnd;
		wnd->prev = NULL;
		wnd->next = NULL;
	}
	else
	{
		wnd->prev = NULL;
		wnd->next = ToolWindowList;
		wnd->next->prev = wnd;
		ToolWindowList = wnd;
	}

	// Show the toolwindow (otherwise it won't show :P )
	wnd->Show();

	return true;
}

void CloseToolWindow(CToolWindow* wnd)
{
	// Remove the toolwindow from the list
	if (wnd == ToolWindowList)
	{
		ToolWindowList = wnd->next;
		if (wnd->next) wnd->next->prev = NULL;
		wnd->next = NULL;
	}
	else
	{
		wnd->prev->next = wnd->next;
		if (wnd->next) wnd->next->prev = wnd->prev;
		wnd->prev = NULL;
		wnd->next = NULL;
	}

	// Delete the toolwindow object
	// its destructor will destroy the window
	delete wnd;
}

void CloseAllToolWindows()
{
	CToolWindow* wnd;
	CToolWindow* wnd_next;

	wnd = ToolWindowList;
	while (wnd)
	{
		wnd_next = wnd->next;

		wnd->prev = NULL;
		wnd->next = NULL;
		delete wnd;

		wnd = wnd_next;
	}

	ToolWindowList = NULL;
}

void RefreshAllToolWindows()
{
	CToolWindow* wnd;

	if (ToolWindowList == NULL)
		return;

	wnd = ToolWindowList;
	while (wnd)
	{
		wnd->Refresh();
		wnd = wnd->next;
	}
}

//-----------------------------------------------------------------------------
//   The Toolkit - Toolbar API wrapper
//-----------------------------------------------------------------------------

static const int kToolBarBitmapSize = 16;
static const int kToolBarEdgeGap = 6;  // logical pixels before the first button

// Nearest-neighbour upscale of a toolbar bitmap to size x size.
//
// The pixels are copied through GetDIBits/SetDIBits instead of StretchBlt: the
// artwork is pixel art, so replicating pixels is both sharper and completely
// predictable, and the result stays a plain device-dependent bitmap, which is
// what TB_ADDBITMAP expects. Returns the original handle when no scaling is
// needed; otherwise the original is destroyed and the caller owns the result.
static HBITMAP ScaleBitmapToSquare(HBITMAP hbmp, int size)
{
	BITMAP bm;
	if (GetObject(hbmp, sizeof(bm), &bm) == 0)
	{
		return hbmp;
	}

	if ( (bm.bmWidth == size) && (bm.bmHeight == size) )
	{
		return hbmp;
	}

	const int srcWidth = bm.bmWidth;
	const int srcHeight = bm.bmHeight;
	if ( (srcWidth <= 0) || (srcHeight <= 0) || (size <= 0) )
	{
		return hbmp;
	}

	const int srcStride = ((srcWidth * 3) + 3) & ~3;
	const int dstStride = ((size * 3) + 3) & ~3;

	HDC dcScreen = GetDC(NULL);
	if (dcScreen == NULL)
	{
		return hbmp;
	}

	HDC dc = CreateCompatibleDC(dcScreen);
	if (dc == NULL)
	{
		ReleaseDC(NULL, dcScreen);
		return hbmp;
	}

	BITMAPINFO srcInfo;
	ZeroMemory(&srcInfo, sizeof(srcInfo));
	srcInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	srcInfo.bmiHeader.biWidth = srcWidth;
	srcInfo.bmiHeader.biHeight = srcHeight;
	srcInfo.bmiHeader.biPlanes = 1;
	srcInfo.bmiHeader.biBitCount = 24;
	srcInfo.bmiHeader.biCompression = BI_RGB;

	std::vector<u8> srcPixels((size_t)srcStride * (size_t)srcHeight);
	if (GetDIBits(dc, hbmp, 0, srcHeight, &srcPixels[0], &srcInfo, DIB_RGB_COLORS) == 0)
	{
		DeleteDC(dc);
		ReleaseDC(NULL, dcScreen);
		return hbmp;
	}

	std::vector<u8> dstPixels((size_t)dstStride * (size_t)size);
	for (int y = 0; y < size; y++)
	{
		const int srcY = (y * srcHeight) / size;
		for (int x = 0; x < size; x++)
		{
			const int srcX = (x * srcWidth) / size;
			memcpy(&dstPixels[(size_t)y * dstStride + (size_t)x * 3],
			       &srcPixels[(size_t)srcY * srcStride + (size_t)srcX * 3], 3);
		}
	}

	// Use the screen DC here: a bitmap created from an empty memory DC would
	// come out monochrome.
	HBITMAP hbmpScaled = CreateCompatibleBitmap(dcScreen, size, size);
	if (hbmpScaled == NULL)
	{
		DeleteDC(dc);
		ReleaseDC(NULL, dcScreen);
		return hbmp;
	}

	BITMAPINFO dstInfo;
	ZeroMemory(&dstInfo, sizeof(dstInfo));
	dstInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	dstInfo.bmiHeader.biWidth = size;
	dstInfo.bmiHeader.biHeight = size;
	dstInfo.bmiHeader.biPlanes = 1;
	dstInfo.bmiHeader.biBitCount = 24;
	dstInfo.bmiHeader.biCompression = BI_RGB;

	SetDIBits(dc, hbmpScaled, 0, size, &dstPixels[0], &dstInfo, DIB_RGB_COLORS);

	DeleteDC(dc);
	ReleaseDC(NULL, dcScreen);
	DeleteObject(hbmp);

	return hbmpScaled;
}

//-----------------------------------------------------------------------------
//   Vector-style toolbar artwork from the Windows icon font
//-----------------------------------------------------------------------------

// The bundled BMPs are 16x16 pixel art.  Upscaling them for HiDPI only
// duplicates pixels, so render the equivalent Segoe icon glyph at the final
// DPI size instead.  Both the font and every individual glyph are probed at
// run time; anything missing falls back to the original bitmap resource.
static const wchar_t *const kIconFontFaces[] =
{
	L"Segoe Fluent Icons",  // Windows 11
	L"Segoe MDL2 Assets",   // Windows 10
};

// Segoe icon fonts are a single outline set, not the paired Regular/Filled
// families shipped as Fluent UI System Icons SVGs.  Every toolbar button uses
// the outline (hollow) glyph for a consistent line-art look; the renderer
// probes the installed font (Segoe Fluent Icons, then Segoe MDL2 Assets) and
// falls back to the bundled BMP resource when the glyph is missing.
struct IconGlyph
{
	wchar_t codes[3];  // zero-terminated, best first
	bool mirror;       // the font ships one rotation direction only
};

static const IconGlyph *findIconGlyph(int bitmapId)
{
	// Code points were verified against the installed fonts with
	// GetGlyphIndicesW.  Refresh and Rotate have no solid counterpart in
	// either font, so those two keep their outline artwork.
	switch (bitmapId)
	{
	case IDB_OPEN:
		{ static const IconGlyph g = { { 0xE838, 0, 0 }, false }; return &g; } // FolderOpen (outline)
	case IDB_PLAY:
		{ static const IconGlyph g = { { 0xE768, 0, 0 }, false }; return &g; } // Play (outline)
	case IDB_PAUSE:
		{ static const IconGlyph g = { { 0xE769, 0, 0 }, false }; return &g; } // Pause (outline)
	case IDB_STOP:
		{ static const IconGlyph g = { { 0xE71A, 0, 0 }, false }; return &g; } // Stop (outline)
	case IDB_RESET:
		{ static const IconGlyph g = { { 0xE72C, 0, 0 }, false }; return &g; } // Refresh (outline only)
	case IDB_FRAMEADVANCE:
		{ static const IconGlyph g = { { 0xE893, 0, 0 }, false }; return &g; } // Next (outline)
	case IDB_ROTATECW:
		{ static const IconGlyph g = { { 0xE7AD, 0, 0 }, false }; return &g; } // Rotate (outline only), CW
	case IDB_ROTATECCW:
		{ static const IconGlyph g = { { 0xE7AD, 0, 0 }, true  }; return &g; } // mirrored = CCW
	default:
		return NULL;
	}
}

static void iconFontTrace(const char *msg)
{
	OutputDebugStringA(msg);
	if (GetConsoleWindow() != NULL)
	{
		fputs(msg, stdout);
		fflush(stdout);
	}
}

struct IconFontLookup
{
	const wchar_t *face;
	bool found;
};

static int CALLBACK iconFontEnumProc(const LOGFONTW *lf, const TEXTMETRICW *, DWORD, LPARAM param)
{
	IconFontLookup *lookup = reinterpret_cast<IconFontLookup *>(param);
	if (wcsncmp(lf->lfFaceName, lookup->face, LF_FACESIZE) == 0)
		lookup->found = true;
	return TRUE;
}

// -2 = not probed yet, -1 = unavailable, 0..1 = index into kIconFontFaces.
static int g_iconFontFace = -2;

static const wchar_t *iconFontFace()
{
	if (g_iconFontFace != -2)
		return g_iconFontFace >= 0 ? kIconFontFaces[g_iconFontFace] : NULL;

	g_iconFontFace = -1;
	HDC dc = GetDC(NULL);
	if (dc != NULL)
	{
		for (int i = 0; i < 2 && g_iconFontFace < 0; i++)
		{
			IconFontLookup lookup = { kIconFontFaces[i], false };
			LOGFONTW lf;
			ZeroMemory(&lf, sizeof(lf));
			lf.lfCharSet = DEFAULT_CHARSET;
			wcsncpy(lf.lfFaceName, kIconFontFaces[i], LF_FACESIZE - 1);
			EnumFontFamiliesExW(dc, &lf, iconFontEnumProc,
				reinterpret_cast<LPARAM>(&lookup), 0);
			if (lookup.found)
				g_iconFontFace = i;
		}
		ReleaseDC(NULL, dc);
	}

	if (g_iconFontFace < 0)
	{
		iconFontTrace("[toolbar] icon font not found; using BMP resources\n");
		return NULL;
	}

	char msg[160];
	char face[64];
	WideCharToMultiByte(CP_ACP, 0, kIconFontFaces[g_iconFontFace], -1,
		face, sizeof(face), NULL, NULL);
	_snprintf(msg, sizeof(msg) - 1, "[toolbar] icon font active: %s\n", face);
	msg[sizeof(msg) - 1] = '\0';
	iconFontTrace(msg);
	return kIconFontFaces[g_iconFontFace];
}

// Mirror a 24bpp DIB in place.  Used to derive the counter-clockwise rotation
// glyph, since the font only contains the clockwise one.
static void flipBitmapHorizontal(HBITMAP hbmp)
{
	BITMAP bm;
	if (GetObject(hbmp, sizeof(bm), &bm) == 0 || bm.bmWidth < 2 || bm.bmHeight < 1)
		return;

	const int width = bm.bmWidth;
	const int height = bm.bmHeight;
	const int stride = ((width * 3 + 3) / 4) * 4;

	HDC dc = CreateCompatibleDC(NULL);
	if (dc == NULL)
		return;

	BITMAPINFO info;
	ZeroMemory(&info, sizeof(info));
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = height;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 24;
	info.bmiHeader.biCompression = BI_RGB;

	u8 *pixels = new u8[(size_t)stride * (size_t)height];
	if (GetDIBits(dc, hbmp, 0, height, pixels, &info, DIB_RGB_COLORS) != 0)
	{
		for (int y = 0; y < height; y++)
		{
			u8 *row = pixels + (size_t)y * stride;
			for (int x = 0; x < width / 2; x++)
			{
				u8 *a = row + x * 3;
				u8 *b = row + (width - 1 - x) * 3;
				for (int c = 0; c < 3; c++)
				{
					const u8 tmp = a[c];
					a[c] = b[c];
					b[c] = tmp;
				}
			}
		}
		SetDIBits(dc, hbmp, 0, height, pixels, &info, DIB_RGB_COLORS);
	}

	delete[] pixels;
	DeleteDC(dc);
}

// Returns NULL when the font is unavailable or the glyph is missing, so the
// caller can fall back to the bundled bitmap.  The background is already the
// button face colour, which is exactly what the opaque toolbar artwork needs.
static HBITMAP renderIconFontBitmap(const IconGlyph &icon, int size,
                                   COLORREF ink, COLORREF face)
{
	const wchar_t *fontFace = iconFontFace();
	if (fontFace == NULL || size <= 0)
		return NULL;

	HDC screen = GetDC(NULL);
	if (screen == NULL)
		return NULL;

	HDC dc = CreateCompatibleDC(screen);
	if (dc == NULL)
	{
		ReleaseDC(NULL, screen);
		return NULL;
	}

	HBITMAP result = CreateCompatibleBitmap(screen, size, size);
	if (result == NULL)
	{
		DeleteDC(dc);
		ReleaseDC(NULL, screen);
		return NULL;
	}

	HGDIOBJ oldBmp = SelectObject(dc, result);

	LOGFONTW lf;
	ZeroMemory(&lf, sizeof(lf));
	// At lfHeight = -size the glyph stays inside the square for every DPI
	// from 16 to 48 px; a larger font starts clipping the ascender.
	lf.lfHeight = -size;
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfQuality = ANTIALIASED_QUALITY;  // grey AA, no ClearType colour fringes
	wcsncpy(lf.lfFaceName, fontFace, LF_FACESIZE - 1);

	HFONT font = CreateFontIndirectW(&lf);
	if (font == NULL)
	{
		SelectObject(dc, oldBmp);
		DeleteObject(result);
		DeleteDC(dc);
		ReleaseDC(NULL, screen);
		return NULL;
	}
	HGDIOBJ oldFont = SelectObject(dc, font);

	// Older Windows builds ship a smaller glyph set.  Try the solid artwork
	// first, then the outline equivalent, and only then give up so the caller
	// can use the bundled bitmap instead of drawing a missing-glyph box.
	wchar_t code = 0;
	for (int i = 0; i < 3 && icon.codes[i] != 0; i++)
	{
		wchar_t candidate = icon.codes[i];
		WORD glyphIndex = 0;
		GetGlyphIndicesW(dc, &candidate, 1, &glyphIndex,
			GGI_MARK_NONEXISTING_GLYPHS);
		if (glyphIndex != 0 && glyphIndex != 0xFFFF)
		{
			code = candidate;
			break;
		}
	}
	if (code == 0)
	{
		SelectObject(dc, oldFont);
		DeleteObject(font);
		SelectObject(dc, oldBmp);
		DeleteObject(result);
		DeleteDC(dc);
		ReleaseDC(NULL, screen);
		return NULL;
	}
	if (code != icon.codes[0])
	{
		char msg[128];
		_snprintf(msg, sizeof(msg) - 1,
			"[toolbar] solid glyph U+%04X unavailable; using U+%04X\n",
			(unsigned)icon.codes[0], (unsigned)code);
		msg[sizeof(msg) - 1] = '\0';
		iconFontTrace(msg);
	}

	RECT rc = { 0, 0, size, size };
	HBRUSH brush = CreateSolidBrush(face);
	FillRect(dc, &rc, brush);
	DeleteObject(brush);

	SetBkMode(dc, TRANSPARENT);
	SetTextColor(dc, ink);
	DrawTextW(dc, &code, 1, &rc,
		DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	SelectObject(dc, oldFont);
	DeleteObject(font);
	SelectObject(dc, oldBmp);
	DeleteDC(dc);
	ReleaseDC(NULL, screen);

	if (icon.mirror)
		flipBitmapHorizontal(result);

	return result;
}

void CToolBar::CreateToolBarWindow()
{
	// Note: dropdown buttons look like crap without TBSTYLE_FLAT.
	// The themed WS_BORDER draws a light hairline even in dark mode, so the
	// dark toolbar reads better without it.
	DWORD style = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS;
	if (!darkMode::enabled())
		style |= WS_BORDER;

	hWnd = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, style,
		0, 0, 0, 0, hParent, NULL, hAppInst, NULL);

	if (hWnd == NULL)
	{
		return;
	}

	// Send it a few messages to finish setting it up
	SendMessage(hWnd, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
	SendMessage(hWnd, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);

	// Give the first button a gutter so the artwork is not flush with the
	// left edge of the window.  TB_SETINDENT shifts the whole strip without
	// inserting a button, so the button count and ordering are unaffected.
	// Scaling keeps the gap the same physical size at every DPI, and
	// CreateToolBarWindow() runs again on a DPI change, so the indent is
	// reapplied to the new HWND.
	SendMessage(hWnd, TB_SETINDENT, DPI_Scale(kToolBarEdgeGap, dpi), 0);

	// Must be set before the first bitmap is added; the button strip is sized
	// from the image size, which is what makes the toolbar grow with the DPI.
	// The unscaled case is left at the common-control default so the window
	// looks exactly as it always did at 100%.
	const int bitmapSize = DPI_Scale(kToolBarBitmapSize, dpi);
	if (bitmapSize != kToolBarBitmapSize)
	{
		SendMessage(hWnd, TB_SETBITMAPSIZE, 0, MAKELPARAM(bitmapSize, bitmapSize));
	}

	// SetDPI destroys and recreates this HWND, so the dark colours have to be
	// re-applied for every incarnation of the control.
	darkMode::prepareToolBar(hWnd);
}

void CToolBar::DestroyBitmaps()
{
	for (TBitmapList::iterator it = hBitmaps.begin(); it != hBitmaps.end(); it++)
	{
		DeleteObject(it->second.second);
	}

	hBitmaps.clear();
}

CToolBar::CToolBar(HWND hParent)
	: hWnd(NULL)
	, hParent(hParent)
	, dpi(DPI_GetForWindow(hParent))
	, hidden(false)
{
	CreateToolBarWindow();
}

CToolBar::~CToolBar()
{
	DestroyBitmaps();
}

void CToolBar::SetDPI(int newDPI)
{
	if ( (newDPI <= 0) || (newDPI == dpi) || (hWnd == NULL) )
	{
		return;
	}

	dpi = newDPI;

	const bool wasVisible = !hidden;

	DestroyBitmaps();
	DestroyWindow(hWnd);

	hWnd = NULL;
	CreateToolBarWindow();

	for (size_t i = 0; i < items.size(); i++)
	{
		AppendItem(items[i]);
	}

	Show(wasVisible);

	SetWindowPos(hWnd, NULL, 
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
		SWP_NOZORDER | SWP_NOMOVE);
}

void CToolBar::Show(bool bShow)
{
	DWORD style = GetWindowLong(hWnd, GWL_STYLE);

	if (bShow)
	{
		hidden = false;
		style |= WS_VISIBLE;
	}
	else
	{
		hidden = true;
		style &= ~WS_VISIBLE;
	}

	SetWindowLong(hWnd, GWL_STYLE, style);
}

void CToolBar::OnSize()
{
	// Not-fully-working wraparound handling code
	// the toolbar behaves weirdly when it comes to separators
	// TODO: figure out why.
	// Note: right now this code is useless, but it may be useful
	// if we use more toolbars
#if 0
	RECT rc;
	int parentwidth;

	GetClientRect(GetParent(hWnd), &rc);
	parentwidth = rc.right - rc.left;

	// When there's not enough space to fit all the buttons in one line,
	// we have to add the 'linebreaks' ourselves
	int numbtns = SendMessage(hWnd, TB_BUTTONCOUNT, 0, 0);
	int curwidth = 0;

	for (int i = 0; i < numbtns; i++)
	{
		SendMessage(hWnd, TB_GETITEMRECT, i, (LPARAM)&rc);
		curwidth += (rc.right - rc.left);

		if (i > 1)
		{
			TBBUTTON btn;
			TBBUTTONINFO btninfo;
			int cmdid;

			SendMessage(hWnd, TB_GETBUTTON, i-1, (LPARAM)&btn);
			if (btn.idCommand == -1)
				SendMessage(hWnd, TB_GETBUTTON, i-2, (LPARAM)&btn);

			cmdid = btn.idCommand;

			// Add/remove the TBSTATE_WRAP style if needed
			btninfo.cbSize = sizeof(TBBUTTONINFO);
			btninfo.dwMask = TBIF_STATE;
			SendMessage(hWnd, TB_GETBUTTONINFO, cmdid, (LPARAM)&btninfo);

			btninfo.dwMask = TBIF_STATE;
			if (curwidth > parentwidth) btninfo.fsState |= TBSTATE_WRAP;
			else btninfo.fsState &= ~TBSTATE_WRAP;
			SendMessage(hWnd, TB_SETBUTTONINFO, cmdid, (LPARAM)&btninfo);

			if (curwidth > parentwidth) curwidth = 0;
		}
	}
#endif
	SetWindowPos(hWnd, NULL, 
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
		SWP_NOZORDER | SWP_NOMOVE);
}

void CToolBar::AppendItem(const ToolbarItem &item)
{
	if (hWnd == NULL)
	{
		return;
	}

	TBBUTTON btn;
	memset(&btn, 0, sizeof(TBBUTTON));
	btn.idCommand = -1;
	btn.iString = -1;

	if (item.separator)
	{
		btn.fsStyle = TBSTYLE_SEP;
		SendMessage(hWnd, TB_ADDBUTTONS, 1, (LPARAM)&btn);
		return;
	}

	btn.fsStyle = item.dropdown ? TBSTYLE_DROPDOWN : TBSTYLE_BUTTON;
	btn.fsState = item.state;
	btn.idCommand = item.id;
	btn.iBitmap = GetBitmapIndex(item.bitmapId);

	SendMessage(hWnd, TB_ADDBUTTONS, 1, (LPARAM)&btn);
}

int CToolBar::GetBitmapIndex(int uBitmapID)
{
	// If we don't already have the bitmap, retrieve it, scale it for the
	// current DPI, adapt its key color and store it in the list
	TBitmapList::iterator it = hBitmaps.find(uBitmapID);
	if (it != hBitmaps.end())
	{
		return it->second.first;
	}

	const int size = DPI_Scale(kToolBarBitmapSize, dpi);

	// The artwork is opaque: its background must match the face the buttons
	// are drawn on.  In dark mode that face is our dark panel colour, not
	// COLOR_BTNFACE, which would leave a light square behind every icon.
	const COLORREF face = darkMode::enabled()
	                    ? darkMode::panelColor()
	                    : GetSysColor(COLOR_BTNFACE);
	const COLORREF ink = darkMode::enabled()
	                   ? darkMode::panelTextColor()
	                   : GetSysColor(COLOR_BTNTEXT);

	// Prefer the icon font so HiDPI gets real vector edges; findIconGlyph()
	// puts solid variants ahead of their outline equivalents.  Rendered
	// bitmaps already carry the face colour as their background; only the
	// bundled BMPs need their magenta key replaced with it.
	const IconGlyph *glyph = findIconGlyph(uBitmapID);
	HBITMAP hbmp = (glyph != NULL)
		? renderIconFontBitmap(*glyph, size, ink, face)
		: NULL;

	if (hbmp == NULL)
	{
		if (glyph != NULL)
			iconFontTrace("[toolbar] glyph render failed; using BMP resource\n");

		hbmp = LoadBitmap(hAppInst, MAKEINTRESOURCE(uBitmapID));
		if (hbmp == NULL)
		{
			return 0;
		}
		hbmp = ScaleBitmapToSquare(hbmp, size);
		MakeBitmapPseudoTransparent(hbmp, RGB(255, 0, 255), face);
	}

	// Add the bitmap to the toolbar's image list
	TBADDBITMAP bmp;
	bmp.hInst = NULL;
	bmp.nID = (UINT_PTR)hbmp;

	const int bmpid = (int)SendMessage(hWnd, TB_ADDBITMAP, 1, (LPARAM)&bmp);

	// Dark mode keeps a dimmed copy at the same image list index, so disabled
	// buttons can be painted without comctl32's light system 3D face colour.
	darkMode::addToolBarBitmap(hWnd, bmpid, hbmp);

	// Save the bitmap (if it gets deleted, the toolbar is too)
	hBitmaps[uBitmapID] = TBitmapPair(bmpid, hbmp);

	return bmpid;
}

void CToolBar::AppendButton(int uID, int uBitmapID, DWORD dwState, bool bDropdown)
{
	ToolbarItem item;
	item.id = uID;
	item.bitmapId = uBitmapID;
	item.state = dwState;
	item.dropdown = bDropdown;
	item.separator = false;

	items.push_back(item);
	AppendItem(item);
}

void CToolBar::AppendSeparator()
{
	ToolbarItem item;
	item.id = -1;
	item.bitmapId = 0;
	item.state = 0;
	item.dropdown = false;
	item.separator = true;

	items.push_back(item);
	AppendItem(item);
}

void CToolBar::ChangeButtonBitmap(int uID, int uBitmapID)
{
	// Finally change the bitmap
	SendMessage(hWnd, TB_CHANGEBITMAP, uID, MAKELPARAM(GetBitmapIndex(uBitmapID), 0));
}

void CToolBar::EnableButtonDropdown(int uID, bool bDropdown)
{
	TBBUTTONINFO btninfo;

	memset(&btninfo, 0, sizeof(TBBUTTONINFO));
	btninfo.cbSize = sizeof(TBBUTTONINFO);
	btninfo.dwMask = TBIF_STYLE;

	SendMessage(hWnd, TB_GETBUTTONINFO, uID, (LPARAM)&btninfo);

	btninfo.dwMask = TBIF_STYLE;
	if (bDropdown)
	{
		btninfo.fsStyle &= ~TBSTYLE_BUTTON;
		btninfo.fsStyle |= TBSTYLE_DROPDOWN;
	}
	else
	{
		btninfo.fsStyle |= TBSTYLE_BUTTON;
		btninfo.fsStyle &= ~TBSTYLE_DROPDOWN;
	}

	SendMessage(hWnd, TB_SETBUTTONINFO, uID, (LPARAM)&btninfo);
}

int CToolBar::GetHeight()
{
	if (hidden) return 0;

	RECT rc; GetWindowRect(hWnd, &rc);
	return rc.bottom - rc.top - 1;
}


WINCLASS::WINCLASS(LPSTR rclass, HINSTANCE hInst)
{
	memset(regclass, 0, sizeof(regclass));
	memcpy(regclass, rclass, strlen(rclass));

	hwnd = NULL;
	hmenu = NULL;
	hInstance = hInst;
	
	minWidth = 0;
	minHeight = 0;
}

WINCLASS::WINCLASS(LPWSTR rclass, HINSTANCE hInst)
{
	wcscpy(regclassW,rclass);

	hwnd = NULL;
	hmenu = NULL;
	hInstance = hInst;

	minWidth = 0;
	minHeight = 0;
}

WINCLASS::~WINCLASS()
{
}

bool WINCLASS::create(LPSTR caption, int x, int y, int width, int height, int style, HMENU menu)
{
	if (hwnd != NULL) return false;
	
	hwnd = CreateWindow(regclass, caption, style, x, y, width, height, NULL, menu, hInstance, NULL);
	
	if (hwnd != NULL) return true;
	return false;
}

bool WINCLASS::createW(LPWSTR caption, int x, int y, int width, int height, int style, HMENU menu)
{
	if (hwnd != NULL) return false;

	hwnd = CreateWindowW(regclassW, caption, style, x, y, width, height, NULL, menu, hInstance, NULL);

	if (hwnd != NULL) return true;
	return false;
}

bool WINCLASS::createEx(LPSTR caption, int x, int y, int width, int height, int style,  int styleEx, HMENU menu)
{
	if (hwnd != NULL) return false;

	hwnd = CreateWindowEx(styleEx, regclass, caption, style, x, y, width, height, NULL, menu, hInstance, NULL);
	
	if (hwnd != NULL) return true;
	return false;
}

bool WINCLASS::setMenu(HMENU menu)
{
	hmenu = menu;
	return SetMenu(hwnd, hmenu)!=FALSE;
}

bool WINCLASS::addMenuItem(u32 item, bool byPos, LPCMENUITEMINFO info)
{
	return InsertMenuItem(hmenu, item, byPos, info);
}

DWORD WINCLASS::checkMenu(UINT idd, bool check)
{
	return CheckMenuItem(hmenu, idd, MF_BYCOMMAND | (check?MF_CHECKED:MF_UNCHECKED));
}

void WINCLASS::enableMenu(UINT idd, bool enable)
{
	EnableMenuItem(hmenu, idd, MF_BYCOMMAND | (enable ? (MF_ENABLED) : MF_GRAYED));
}

HWND WINCLASS::getHWnd()
{
	return hwnd;
}

void WINCLASS::Show(int mode)
{
	ShowWindow(hwnd, mode);
}

void WINCLASS::Hide()
{
	ShowWindow(hwnd, SW_HIDE);
}

void WINCLASS::setMinSize(int width, int height)
{
	minWidth = width;
	minHeight = height;
}

static void MyAdjustWindowRectEx(RECT* rect, HWND hwnd)
{
	DPI_AdjustWindowRectEx(rect,GetWindowStyle(hwnd),TRUE,GetWindowExStyle(hwnd),hwnd);
	
	//get height of one menu to subtract off
	int cymenu = DPI_GetSystemMetricsForWindow(SM_CYMENU, hwnd);
	
	//get the height of the actual menu to add back on
	MENUBARINFO mbi;
	ZeroMemory(&mbi, sizeof(mbi));
	mbi.cbSize = sizeof(mbi);
	GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi);
	
	//if the menubar exists, its height is off by 1 (frame between bar and client area?)
	int menuHeight = (mbi.rcBar.bottom - mbi.rcBar.top);
	if(menuHeight != 0) menuHeight++;

	rect->bottom -= cymenu;
	rect->bottom += menuHeight;
}

void WINCLASS::sizingMsg(WPARAM wParam, LPARAM lParam, LONG keepRatio)
{
	RECT *rect = (RECT*)lParam;

	int prevRight = rect->right;
	int prevBottom = rect->bottom;

	int _minWidth, _minHeight;

	int tbheight = MainWindowToolbar->GetHeight();

	RECT adjr;
	SetRect(&adjr,0,0,minWidth,minHeight);
	if(!(keepRatio & FULLSCREEN))
		MyAdjustWindowRectEx(&adjr,hwnd);

	RECT frameInfo; 
	SetRect(&frameInfo,0,0,0,0);
	MyAdjustWindowRectEx(&frameInfo,hwnd);
	int frameWidth = frameInfo.right-frameInfo.left;
	int frameHeight = frameInfo.bottom-frameInfo.top + tbheight;

	if((keepRatio & FULLSCREEN))
		if(fsWindow)
			frameWidth = frameHeight = 0;

	// Calculate the minimum size in pixels
	_minWidth = adjr.right-adjr.left;
	_minHeight = adjr.bottom-adjr.top + tbheight;

	//zero 07-aug-2016 - this was really old code. Maybe it isn't needed anymore
	//it effectively let the content overshoot the window size.
	//really we simply should let the window not overshoot the content size (but that restriction is waived for fullscreen mode)
	//SIDE EFFECT: window can now be shrunk under the minimum size, which people have been wanting to do anyway
	////Clamp the size to the minimum size (256x384)
	//rect->right = (rect->left + std::max(_minWidth, (int)(rect->right - rect->left)));
	//rect->bottom = (rect->top + std::max(_minHeight, (int)(rect->bottom - rect->top)));

	bool horizontalDrag = (wParam == WMSZ_LEFT) || (wParam == WMSZ_RIGHT);
	bool verticalDrag = (wParam == WMSZ_TOP) || (wParam == WMSZ_BOTTOM);
	if(verticalDrag && !(keepRatio & KEEPY))
	{
		int clientHeight = rect->bottom - rect->top - frameHeight;
		if(clientHeight < minHeight)
			rect->bottom += minHeight - clientHeight;
	}
	else if(horizontalDrag && !(keepRatio & KEEPX))
	{
		int clientWidth = rect->right - rect->left - frameWidth;
		if(clientWidth < minWidth)
			rect->right += minWidth - clientWidth;
	}
	else
	{
		//Apply the ratio stuff

		float ratio1 = ((rect->right  - rect->left - frameWidth ) / (float)minWidth);
		float ratio2 = ((rect->bottom - rect->top  - frameHeight) / (float)minHeight);
		float ratio = std::min(ratio1,ratio2);
		if(keepRatio & FULLSCREEN)
		{
			keepRatio |= WINCLASS::KEEPX | WINCLASS::KEEPY;
			ratio1 = ratio;
			ratio2 = ratio;
		}

		LONG correctedHeight = (LONG)((rect->top  + frameHeight + (minHeight * ratio1)));
		LONG correctedWidth  = (LONG)((rect->left + frameWidth + (minWidth  * ratio2)));

		if(keepRatio & KEEPX)
		{
			if((keepRatio & KEEPY) || (rect->bottom < correctedHeight))
			{
				if(verticalDrag)
					rect->right = correctedWidth;
				else
					rect->bottom = correctedHeight;
			}
		}
		//else
		{
			if((keepRatio & KEEPY) && (rect->right < correctedWidth) || (keepRatio&FULLSCREEN))
			{
				if(horizontalDrag)
					rect->bottom = correctedHeight;
				else
					rect->right = correctedWidth;
			}
		}
	}

	// prevent "pushing" the window across the screen when resizing from the left or top
	if(wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT)
	{
		rect->left -= rect->right - prevRight;
		rect->right = prevRight;
	}
	if(wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT)
	{
		rect->top -= rect->bottom - prevBottom;
		rect->bottom = prevBottom;
	}

	// windows screws up the window size if the top of the window goes too high above the top of the screen
	if(keepRatio & KEEPY)
	{
		int titleBarHeight = DPI_GetSystemMetricsForWindow(SM_CYSIZE, hwnd);
		int topExceeded = -(titleBarHeight + rect->top);
		if(topExceeded > 0)
		{
			rect->top += topExceeded;
			rect->bottom += topExceeded;
		}
	}
}

void WINCLASS::setClientSize(int width, int height)
{
	height += MainWindowToolbar->GetHeight();

	//yep, do it twice, once in case the menu wraps, and once to accomodate that wrap
	for(int i=0;i<2;i++)
	{
		RECT rect;
		SetRect(&rect,0,0,width,height);
		MyAdjustWindowRectEx(&rect,hwnd);
		SetWindowPos(hwnd,0,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER);
	}
}

//========================================================= Thread class
extern DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
	THREADCLASS *tmp = (THREADCLASS *)lpParameter;
	return tmp->ThreadFunc();
}

THREADCLASS::THREADCLASS()
{
	hThread = NULL;
}

THREADCLASS::~THREADCLASS()
{
	closeThread();
}

void THREADCLASS::closeThread()
{
	if (hThread) 
	{
		CloseHandle(hThread);
		hThread = NULL;
	}
}

bool THREADCLASS::createThread()
{
	if (hThread) return false;

	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadProc, this, 0, &threadID);
	if (!hThread) return false;
	//WaitForSingleObject(hThread, INFINITE);
	return true;
}

//========================================================= Tools class
TOOLSCLASS::TOOLSCLASS(HINSTANCE hInst, int IDD, DLGPROC dlgproc)
{
	this->dlgproc = dlgproc;
	hwnd = NULL;
	hInstance = hInst;
	idd=IDD;
	isOpen = false;
	memset(class_name, 0, sizeof(class_name));
	memset(class_name2, 0, sizeof(class_name2));
}

TOOLSCLASS::~TOOLSCLASS()
{
	close();
}

void TOOLSCLASS::Activate()
{
	SetForegroundWindow(hwnd);
}

bool TOOLSCLASS::open(bool useThread)
{
	if(useThread)
	{
		if (!createThread()) return false;
		else return true;
	}

	if(doOpen()) return false;
	else return true;
}

bool TOOLSCLASS::close()
{
	return true;
}

DWORD TOOLSCLASS::doOpen()
{
	GetLastError();
	hwnd = CreateDialog(hInstance, MAKEINTRESOURCE(idd), NULL, (DLGPROC) dlgproc);

	if (!hwnd) 
	{
		LOG("error creating dialog\n");
		return (-2);
	}

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	isOpen = true;

	return 0;
}

void TOOLSCLASS::doClose()
{
	unregClass();
	hwnd = NULL;
	isOpen = false;
}

DWORD TOOLSCLASS::ThreadFunc()
{
	LOG("Start thread\n");

	// The font hook is per thread, and every tool window opens its dialog on a
	// thread of its own, so it has to be installed here before doOpen().
	uiFont::init();

	DWORD ret = doOpen();
	if(ret) return ret;
	
	MSG messages;
	while (GetMessage (&messages, NULL, 0, 0))
	{
		//we should not be using a thread here, because it takes too many programmer resources to do anything asynchronously.
		//but since someone decided to... better put a lock here so that we cant accidentally emulate with NDS_exec while stuff is going on
		Lock lock; //KLUDGE
		TranslateMessage(&messages);
		DispatchMessage(&messages);
	}

	doClose();

	closeThread();
	return 0;
}

void TOOLSCLASS::regClass(LPSTR class_name, WNDPROC wproc, bool SecondReg)
{
	WNDCLASSEX	wc;

	wc.cbSize         = sizeof(wc);
	if (SecondReg)
		strcpy(this->class_name2, class_name);
	else
		strcpy(this->class_name, class_name);

	wc.lpszClassName  = class_name;
	wc.hInstance      = hInstance;
	wc.lpfnWndProc    = wproc;
	wc.hCursor        = LoadCursor (NULL, IDC_ARROW);
	wc.hIcon          = 0;
	wc.lpszMenuName   = 0;
	wc.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);
	wc.style          = 0;
	wc.cbClsExtra     = 0;
	wc.cbWndExtra     = 0;
	wc.hIconSm        = 0;

	RegisterClassEx(&wc);
}

void TOOLSCLASS::unregClass()
{
	if (class_name[0])
	{
		UnregisterClass(class_name, hInstance);
	}
	if (class_name2[0])
	{
		UnregisterClass(class_name2, hInstance);
	}
	memset(class_name, 0, sizeof(class_name));
	memset(class_name2, 0, sizeof(class_name2));
}
