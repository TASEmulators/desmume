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

// DarkMode.cpp : system dark mode for the main window chrome.
//
// Scope is deliberately narrow: the title bar, the menu bar, the toolbar and
// the client background of the main window.  Dialogs and tool windows keep
// their original light colours, and the emulation display itself is untouched.
//
// Nothing is linked statically to dwmapi or uxtheme.  Every entry point is
// resolved with LoadLibrary/GetProcAddress, so a build still starts on a
// system without the dark mode entry points.  The app mode entry points are
// exported by ordinal only and are not declared by the SDK, so their types
// are hand written here.
//
// The menu bar is painted by the system and has no owner-draw switch.  The
// only way in is to answer the WM_UAH* messages uxtheme uses for it; their
// meanings and structure layouts come from reverse engineering (see
// adzm/win32-custom-menubar-aero-theme) and have been stable since Vista.

// SetWindowSubclass and the per-DPI metrics need the Vista+ declarations.
// This translation unit includes windows.h directly rather than through a
// project wide prefix header, so it sets the target level itself.
#if defined(_WIN32_WINNT) && (_WIN32_WINNT < 0x0A00)
#	undef _WIN32_WINNT
#endif
#if !defined(_WIN32_WINNT)
#	define _WIN32_WINNT 0x0A00
#endif
#if defined(WINVER) && (WINVER < 0x0A00)
#	undef WINVER
#endif
#if !defined(WINVER)
#	define WINVER 0x0A00
#endif

#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <commctrl.h>

#include <vector>

#include "DarkMode.h"
#include "DPI.h"

#pragma comment(lib, "comctl32.lib")

// Not declared by every SDK version, but present since comctl32 5.80.
#ifndef TB_SETDISABLEDIMAGELIST
#	define TB_SETDISABLEDIMAGELIST (WM_USER + 54)
#endif

namespace
{
	const COLORREF kBarColor          = RGB(32, 32, 32);
	const COLORREF kBorderColor       = RGB(56, 56, 56);
	const COLORREF kTextColor         = RGB(228, 228, 228);
	// The toolbar is a control face, so it takes the dark counterpart of
	// COLOR_BTNFACE (0x333333) rather than the window colour above.  It is a
	// shade lighter than the menu bar, which is how Windows' own dark panels
	// read.
	const COLORREF kPanelColor        = RGB(0x33, 0x33, 0x33);
	const COLORREF kDisabledTextColor = RGB(128, 128, 128);
	const COLORREF kMenuHotColor      = RGB(58, 58, 58);
	const COLORREF kMenuOpenColor     = RGB(74, 74, 74);
	// The emulated screens go black when no ROM is loaded, like VBA's idle
	// display.  It also doubles as the window class background, so any area
	// a renderer leaves untouched stays dark instead of flashing white.
	const COLORREF kDisplayColor      = RGB(0, 0, 0);

	bool   g_enabled  = false;
	HBRUSH g_barBrush = NULL;
	HFONT  g_menuFont = NULL;

	typedef HRESULT (WINAPI *FnDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
	typedef int     (WINAPI *FnSetPreferredAppMode)(int);
	typedef void    (WINAPI *FnFlushMenuThemes)();
	typedef void    (WINAPI *FnRefreshImmersiveColorPolicyState)();

	FnDwmSetWindowAttribute g_setDwmAttribute = NULL;

	// PreferredAppMode, as used by the ordinal 135 entry point.
	enum
	{
		AppModeDefault   = 0,
		AppModeAllowDark = 1,
		AppModeForceDark = 2
	};

	bool systemUsesDarkMode()
	{
		DWORD value = 1;
		DWORD size  = sizeof(value);
		HKEY  key   = NULL;

		if (RegOpenKeyExW(HKEY_CURRENT_USER,
		                  L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
		                  0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
		{
			return false;
		}

		if (RegQueryValueExW(key, L"AppsUseLightTheme", NULL, NULL,
		                     reinterpret_cast<LPBYTE>(&value), &size) != ERROR_SUCCESS)
		{
			value = 1;
		}

		RegCloseKey(key);
		return value == 0;
	}

	// The menu bar has no owner-draw switch and no documented message; the
	// only way in is to answer the messages uxtheme itself uses for it.
#define WM_UAHDRAWMENU         0x0091
#define WM_UAHDRAWMENUITEM     0x0092
#define WM_UAHINITMENU         0x0093
#define WM_UAHMEASUREMENUITEM  0x0094

	struct UAHMENUITEMMETRICS
	{
		DWORD cx;
		DWORD cy;
	};

	struct UAHMENUPOPUPMETRICS
	{
		DWORD rgcx;
		DWORD fUpdateMaxWidths : 2;
	};

	struct UAHMENU
	{
		HMENU hmenu;
		HDC   hdc;
		DWORD dwFlags;
	};

	struct UAHMENUITEM
	{
		int                iPosition;
		UAHMENUITEMMETRICS umim;
		UAHMENUPOPUPMETRICS umpm;
	};

	struct UAHDRAWMENUITEM
	{
		DRAWITEMSTRUCT dis;
		UAHMENU        um;
		UAHMENUITEM    umi;
	};

	HFONT menuFont(HWND hwnd)
	{
		if (g_menuFont == NULL)
		{
			NONCLIENTMETRICSW metrics;
			ZeroMemory(&metrics, sizeof(metrics));
			metrics.cbSize = sizeof(metrics);

			// The frontend already resolves the per-monitor entry points, so
			// reuse them instead of loading user32 a second time.
			const UINT dpi = static_cast<UINT>(DPI_GetForWindow(hwnd));
			if (!DPI_SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS,
			                                    sizeof(metrics), &metrics, 0, dpi))
			{
				SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
			}

			g_menuFont = CreateFontIndirectW(&metrics.lfMenuFont);
		}
		return g_menuFont;
	}

	bool drawMenuBar(UAHMENU *menu, HWND hwnd)
	{
		MENUBARINFO info;
		ZeroMemory(&info, sizeof(info));
		info.cbSize = sizeof(info);

		if (!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &info))
			return false;

		RECT window;
		GetWindowRect(hwnd, &window);

		RECT rc = info.rcBar;
		OffsetRect(&rc, -window.left, -window.top);
		rc.bottom += 1;		// also covers the separator above the client area

		HBRUSH brush = CreateSolidBrush(kBarColor);
		FillRect(menu->hdc, &rc, brush);
		DeleteObject(brush);
		return true;
	}

	void drawMenuItem(const UAHDRAWMENUITEM *item, HWND hwnd)
	{
		HDC dc = item->um.hdc;

		wchar_t text[256] = L"";
		MENUITEMINFOW info;
		ZeroMemory(&info, sizeof(info));
		info.cbSize     = sizeof(info);
		info.fMask      = MIIM_STRING;
		info.dwTypeData = text;
		info.cch        = 255;
		GetMenuItemInfoW(item->um.hmenu, item->umi.iPosition, TRUE, &info);

		COLORREF background = kBarColor;
		if (item->dis.itemState & ODS_SELECTED)
			background = kMenuOpenColor;
		else if (item->dis.itemState & ODS_HOTLIGHT)
			background = kMenuHotColor;

		const COLORREF foreground = (item->dis.itemState & (ODS_GRAYED | ODS_DISABLED))
		                          ? kDisabledTextColor : kTextColor;

		HBRUSH brush = CreateSolidBrush(background);
		FillRect(dc, &item->dis.rcItem, brush);
		DeleteObject(brush);

		UINT flags = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
		if (item->dis.itemState & ODS_NOACCEL)
			flags |= DT_HIDEPREFIX;

		HFONT font = menuFont(hwnd);
		HGDIOBJ oldFont = font != NULL ? SelectObject(dc, font) : NULL;

		RECT rc = item->dis.rcItem;
		SetBkMode(dc, TRANSPARENT);
		SetTextColor(dc, foreground);
		DrawTextW(dc, text, -1, &rc, flags);

		if (oldFont != NULL)
			SelectObject(dc, oldFont);
	}

	// The theme paints a one pixel light border where the menu bar meets the
	// client area, and it does so after the bar has been drawn.  Painting the
	// same strip afterwards in the frame colour makes the bar read as part of
	// the window frame instead of a black band.
	void drawMenuBorder(HWND hwnd)
	{
		if (GetMenu(hwnd) == NULL)
			return;

		RECT client;
		GetClientRect(hwnd, &client);
		MapWindowPoints(hwnd, NULL, reinterpret_cast<POINT *>(&client), 2);

		RECT window;
		GetWindowRect(hwnd, &window);
		OffsetRect(&client, -window.left, -window.top);

		int thickness = GetSystemMetrics(SM_CYBORDER);
		if (thickness < 1)
			thickness = 1;

		RECT line;
		line.left   = client.left;
		line.right  = client.right;
		line.bottom = client.top;
		line.top    = client.top - thickness;

		HDC dc = GetWindowDC(hwnd);
		HBRUSH brush = CreateSolidBrush(kBorderColor);
		FillRect(dc, &line, brush);
		DeleteObject(brush);
		ReleaseDC(hwnd, dc);
	}

	LRESULT CALLBACK frameProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
	                           UINT_PTR, DWORD_PTR)
	{
		switch (msg)
		{
		case WM_UAHDRAWMENU:
			if (drawMenuBar(reinterpret_cast<UAHMENU *>(lParam), hwnd))
				return 0;
			break;

		case WM_UAHDRAWMENUITEM:
			drawMenuItem(reinterpret_cast<const UAHDRAWMENUITEM *>(lParam), hwnd);
			return 0;

		case WM_NCPAINT:
		case WM_NCACTIVATE:
			{
				// The theme draws its own light border along the bottom of
				// the menu bar after the bar itself was painted; cover it
				// once the window is done with the non client area.
				LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
				drawMenuBorder(hwnd);
				return result;
			}
		}

		return DefSubclassProc(hwnd, msg, wParam, lParam);
	}

	// SetWindowSubclass keeps the window procedure's character set, unlike
	// replacing it with SetWindowLongPtr.  The main window is registered with
	// RegisterClassExW and its WndProc ends in DefWindowProcW, so mixing the
	// two would corrupt WM_SETTEXT and the caption.
	void subclassFrame(HWND hwnd)
	{
		SetWindowSubclass(hwnd, frameProc, 1, 0);
	}

	// Toolbar bitmaps are opaque: their magenta key is replaced with the dark
	// panel colour instead of being turned into a real transparency mask.
	// When comctl32 renders a disabled button it treats that whole opaque
	// square as the image and floods it with the light system 3D face colour,
	// so a disabled button turns into a grey square with the artwork gone.
	// In light mode the square matches the button face and goes unnoticed.
	// Keep a dimmed copy of every bitmap and paint disabled buttons here,
	// skipping the control's own item drawing.
	HIMAGELIST g_toolBarImages = NULL;
	HWND       g_toolBarHwnd  = NULL;

	HBITMAP dimToolBarBitmap(HBITMAP src, COLORREF background, unsigned keepPercent)
	{
		BITMAP bm;
		if (GetObject(src, sizeof(bm), &bm) == 0 || bm.bmWidth <= 0 || bm.bmHeight <= 0)
			return NULL;

		const int width  = bm.bmWidth;
		const int height = bm.bmHeight;
		const int stride = ((width * 3 + 3) / 4) * 4;

		HDC screen = GetDC(NULL);
		if (screen == NULL)
			return NULL;

		HDC dc = CreateCompatibleDC(screen);
		if (dc == NULL)
		{
			ReleaseDC(NULL, screen);
			return NULL;
		}

		BITMAPINFO info;
		ZeroMemory(&info, sizeof(info));
		info.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth       = width;
		info.bmiHeader.biHeight      = height;
		info.bmiHeader.biPlanes      = 1;
		info.bmiHeader.biBitCount    = 24;
		info.bmiHeader.biCompression = BI_RGB;

		std::vector<unsigned char> pixels((size_t)stride * (size_t)height);
		HBITMAP result = NULL;

		if (GetDIBits(dc, src, 0, height, &pixels[0], &info, DIB_RGB_COLORS) != 0)
		{
			const unsigned other = 100 - keepPercent;
			const unsigned char bb = GetBValue(background);
			const unsigned char bg = GetGValue(background);
			const unsigned char br = GetRValue(background);

			// 24bpp DIB rows are BGR.  Blending towards the panel colour
			// leaves the key area exactly the panel colour and darkens only
			// the artwork, which reads as a disabled icon on a dark face.
			for (int y = 0; y < height; y++)
			{
				unsigned char *row = &pixels[(size_t)y * (size_t)stride];
				for (int x = 0; x < width; x++)
				{
					unsigned char *p = row + x * 3;
					p[0] = (unsigned char)((p[0] * keepPercent + bb * other) / 100);
					p[1] = (unsigned char)((p[1] * keepPercent + bg * other) / 100);
					p[2] = (unsigned char)((p[2] * keepPercent + br * other) / 100);
				}
			}

			result = CreateCompatibleBitmap(screen, width, height);
			if (result != NULL)
				SetDIBits(dc, result, 0, height, &pixels[0], &info, DIB_RGB_COLORS);
		}

		DeleteDC(dc);
		ReleaseDC(NULL, screen);
		return result;
	}

	void resetToolBarImages(HWND hwnd)
	{
		if (g_toolBarImages != NULL)
		{
			// Only detach from a control that is still alive: CToolBar
			// destroys the old HWND before creating the new one on a DPI
			// change, and Windows may reuse the handle value.
			if (g_toolBarHwnd != NULL && IsWindow(g_toolBarHwnd))
				SendMessage(g_toolBarHwnd, TB_SETDISABLEDIMAGELIST, 0, 0);
			ImageList_Destroy(g_toolBarImages);
			g_toolBarImages = NULL;
		}
		g_toolBarHwnd = hwnd;
	}

	// Resolve the button behind a custom draw item.  For a toolbar dwItemSpec
	// is normally the command id; some control versions pass the button
	// index instead, so accept that as a fallback.
	bool getToolBarButton(NMTBCUSTOMDRAW *draw, TBBUTTON *info)
	{
		HWND hwnd = draw->nmcd.hdr.hwndFrom;

		int button = (int)SendMessage(hwnd, TB_COMMANDTOINDEX, draw->nmcd.dwItemSpec, 0);
		if (button < 0)
		{
			const int count = (int)SendMessage(hwnd, TB_BUTTONCOUNT, 0, 0);
			if (draw->nmcd.dwItemSpec < (DWORD_PTR)count)
				button = (int)draw->nmcd.dwItemSpec;
		}
		if (button < 0)
			return false;

		ZeroMemory(info, sizeof(*info));
		return SendMessage(hwnd, TB_GETBUTTON, button,
		                   reinterpret_cast<LPARAM>(info)) != 0;
	}

	void drawDisabledToolBarButton(NMTBCUSTOMDRAW *draw, const TBBUTTON &info)
	{
		if (g_toolBarImages == NULL || draw->nmcd.hdr.hwndFrom != g_toolBarHwnd)
			return;
		if (info.iBitmap < 0 || info.iBitmap >= ImageList_GetImageCount(g_toolBarImages))
			return;

		int cx = 0, cy = 0;
		if (!ImageList_GetIconSize(g_toolBarImages, &cx, &cy))
			return;

		const RECT rc = draw->nmcd.rc;
		const int x = rc.left + (rc.right - rc.left - cx) / 2;
		const int y = rc.top + (rc.bottom - rc.top - cy) / 2;
		ImageList_Draw(g_toolBarImages, info.iBitmap, draw->nmcd.hdc, x, y, ILD_NORMAL);
	}
}

namespace darkMode
{
	bool enabled()
	{
		return g_enabled;
	}

	COLORREF barColor()
	{
		return kBarColor;
	}

	COLORREF panelColor()
	{
		return kPanelColor;
	}

	COLORREF panelTextColor()
	{
		return kTextColor;
	}

	COLORREF displayColor()
	{
		return kDisplayColor;
	}

	DWORD displayPixel32()
	{
		return 0xFF000000u
		     | (static_cast<DWORD>(GetBValue(kDisplayColor)) << 16)
		     | (static_cast<DWORD>(GetGValue(kDisplayColor)) << 8)
		     |  static_cast<DWORD>(GetRValue(kDisplayColor));
	}

	HBRUSH backgroundBrush()
	{
		// The window class holds on to this brush for as long as the process
		// lives, so it is deliberately created once and never destroyed.
		if (g_barBrush == NULL)
			g_barBrush = CreateSolidBrush(kDisplayColor);
		return g_barBrush;
	}

	void applyFrame(HWND hwnd)
	{
		if (!g_enabled || hwnd == NULL)
			return;

		if (g_setDwmAttribute != NULL)
		{
			BOOL dark = TRUE;
			// 20 (DWMWA_USE_IMMERSIVE_DARK_MODE) since Windows 10 2004,
			// 19 on 1809/1903; the failing one is simply ignored.
			g_setDwmAttribute(hwnd, 20, &dark, sizeof(dark));
			g_setDwmAttribute(hwnd, 19, &dark, sizeof(dark));
		}

		// Only the main window uses the "DeSmuME" class, so replacing the
		// class background leaves the tool windows on their system colours.
		HBRUSH brush = backgroundBrush();
		if (brush != NULL)
			SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(brush));

		subclassFrame(hwnd);
	}

	void prepareToolBar(HWND hwnd)
	{
		if (!g_enabled || hwnd == NULL)
			return;

		// SetDPI creates a brand new HWND; the cached dimmed bitmaps belong
		// to the previous incarnation and have to be rebuilt.
		resetToolBarImages(hwnd);

		// Keep the v6 theme: its flat buttons are transparent until they are
		// hot, so the dark background painted in NM_CUSTOMDRAW stays visible.
		// Dropping the theme would put the control on the classic renderer,
		// which paints every button face with the light COLOR_BTNFACE.
		// TB_SETCOLORSCHEME only covers the 3D edges (there is no clrBtnFace
		// in COLORSCHEME); the classic fallback and the separators still use
		// them, so give them the dark greys.
		COLORSCHEME scheme;
		ZeroMemory(&scheme, sizeof(scheme));
		scheme.dwSize          = sizeof(scheme);
		scheme.clrBtnHighlight = kMenuOpenColor;
		scheme.clrBtnShadow    = kBorderColor;
		SendMessage(hwnd, TB_SETCOLORSCHEME, 0, reinterpret_cast<LPARAM>(&scheme));

		InvalidateRect(hwnd, NULL, TRUE);
	}

	void addToolBarBitmap(HWND hwnd, int index, HBITMAP bitmap)
	{
		if (!g_enabled || hwnd == NULL || bitmap == NULL || index < 0)
			return;

		if (hwnd != g_toolBarHwnd)
			resetToolBarImages(hwnd);

		if (g_toolBarImages == NULL)
		{
			BITMAP bm;
			if (GetObject(bitmap, sizeof(bm), &bm) == 0)
				return;

			g_toolBarImages = ImageList_Create(bm.bmWidth, bm.bmHeight, ILC_COLORDDB, 4, 4);
			if (g_toolBarImages == NULL)
				return;

			// Belt and braces for repaint paths that bypass custom draw: the
			// control must not fall back to its light generated disabled art.
			SendMessage(hwnd, TB_SETDISABLEDIMAGELIST, 0,
			            reinterpret_cast<LPARAM>(g_toolBarImages));
		}

		HBITMAP dimmed = dimToolBarBitmap(bitmap, kPanelColor, 45);
		if (dimmed == NULL)
			return;

		// index is what TB_ADDBITMAP returned, so the dimmed copy lands at
		// exactly the same slot the control uses for this button.
		if (ImageList_GetImageCount(g_toolBarImages) <= index)
			ImageList_SetImageCount(g_toolBarImages, index + 1);
		ImageList_Replace(g_toolBarImages, index, dimmed, NULL);
		DeleteObject(dimmed);
	}

	LRESULT toolBarCustomDraw(LPARAM lParam)
	{
		if (!g_enabled || lParam == 0)
			return CDRF_DODEFAULT;

		NMTBCUSTOMDRAW *draw = reinterpret_cast<NMTBCUSTOMDRAW *>(lParam);

		switch (draw->nmcd.dwDrawStage)
		{
		case CDDS_PREPAINT:
			{
				// NMTBCUSTOMDRAW carries the colours the control uses for the
				// button faces and the hot highlight.  Setting them here makes
				// the themed renderer stop using its light Explorer colours,
				// including on builds where it does honour the fields.
				draw->clrBtnFace           = kPanelColor;
				draw->clrBtnHighlight      = kMenuHotColor;
				draw->clrHighlightHotTrack = kMenuHotColor;
				draw->clrText              = kTextColor;
				draw->clrTextHighlight     = kTextColor;
				draw->clrMark              = kTextColor;

				// The gaps between the buttons, and the area the control would
				// otherwise erase in a light colour.
				HBRUSH brush = CreateSolidBrush(kPanelColor);
				FillRect(draw->nmcd.hdc, &draw->nmcd.rc, brush);
				DeleteObject(brush);
				return CDRF_NOTIFYITEMDRAW;
			}

		case CDDS_ITEMPREPAINT:
			{
				// Ask the control for the button state as well: not every
				// comctl32 build sets CDIS_DISABLED on the item notification.
				// A disabled button is simply one without TBSTATE_ENABLED;
				// separators have no state bits at all and must be excluded.
				TBBUTTON info;
				const bool haveButton = getToolBarButton(draw, &info);
				const bool disabled =
					(draw->nmcd.uItemState & (CDIS_DISABLED | CDIS_GRAYED)) != 0 ||
					(haveButton && (info.fsStyle & TBSTYLE_SEP) == 0 &&
					 (info.fsState & TBSTATE_ENABLED) == 0);

				COLORREF face = kPanelColor;
				if (!disabled)
				{
					if (draw->nmcd.uItemState & (CDIS_SELECTED | CDIS_CHECKED))
						face = kMenuOpenColor;
					else if (draw->nmcd.uItemState & CDIS_HOT)
						face = kMenuHotColor;
				}

				draw->clrBtnFace = face;
				draw->clrHighlightHotTrack = kMenuHotColor;

				HBRUSH brush = CreateSolidBrush(face);
				FillRect(draw->nmcd.hdc, &draw->nmcd.rc, brush);
				DeleteObject(brush);

				if (disabled)
				{
					// The themed renderer would repaint this button face with
					// the light system 3D colour and swallow the icon.  Draw
					// the dimmed artwork ourselves and skip its item pass.
					if (haveButton)
						drawDisabledToolBarButton(draw, info);
					return CDRF_SKIPDEFAULT;
				}

				// Let the control draw the icon.  The bitmap is opaque and its
				// key colour is already kPanelColor, so no light halo shows
				// around the artwork even if the control repaints the button
				// face underneath it.
				return CDRF_DODEFAULT;
			}
		}

		return CDRF_DODEFAULT;
	}

	void resetFont()
	{
		if (g_menuFont != NULL)
		{
			DeleteObject(g_menuFont);
			g_menuFont = NULL;
		}
	}

	void init()
	{
		// Visual styles are what makes uxtheme ask the window to paint the
		// menu bar, and the toolbar needs the common controls initialised
		// before the first window exists.
		INITCOMMONCONTROLSEX controls;
		controls.dwSize = sizeof(controls);
		controls.dwICC  = ICC_WIN95_CLASSES;
		InitCommonControlsEx(&controls);

		HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll");
		if (dwmapi != NULL)
		{
			g_setDwmAttribute = reinterpret_cast<FnDwmSetWindowAttribute>(
				GetProcAddress(dwmapi, "DwmSetWindowAttribute"));
		}

		if (!systemUsesDarkMode())
			return;

		HMODULE uxtheme = LoadLibraryW(L"uxtheme.dll");
		if (uxtheme == NULL)
			return;

		FnSetPreferredAppMode setPreferredAppMode =
			reinterpret_cast<FnSetPreferredAppMode>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
		if (setPreferredAppMode == NULL)
			return;		// Windows 10 before 1809 has no dark mode for Win32 windows

		FnFlushMenuThemes flushMenuThemes =
			reinterpret_cast<FnFlushMenuThemes>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136)));
		FnRefreshImmersiveColorPolicyState refreshPolicy =
			reinterpret_cast<FnRefreshImmersiveColorPolicyState>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(104)));

		// ForceDark is what makes the drop down menus dark; they have no per
		// window switch of their own.  This is a process wide setting taken
		// once at startup, which matches the "read the system mode once"
		// policy of this frontend.
		setPreferredAppMode(AppModeForceDark);
		if (flushMenuThemes != NULL)
			flushMenuThemes();
		if (refreshPolicy != NULL)
			refreshPolicy();

		// Both modules stay loaded for the lifetime of the process: the
		// function pointers above are used on every repaint.
		g_enabled = true;
	}
}
