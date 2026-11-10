/*
	Copyright (C) 2025 DeSmuME team

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

#ifndef MAC_LUA_FRONTEND_H
#define MAC_LUA_FRONTEND_H

#include <stdbool.h>

// Frontend hooks the Lua engine needs but cannot implement itself.
//
// On Windows these live inline in lua-engine.cpp behind WIN32_FRONTEND, talking
// to HWND/HMENU/joyGetPosEx directly.  Cocoa cannot include those headers, so
// the same operations are exposed here as a small C API and implemented in
// MacLuaFrontend.mm.  lua-engine.cpp calls into this from its
// DESMUME_COCOA branches.
//
// Everything here is called from the emulation thread unless noted, and every
// function that touches AppKit hops to the main thread itself.

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// gui.popup() / input.popup()
//
// type is one of "ok", "yesno", "yesnocancel", "okcancel", "abortretryignore";
// icon is one of "message", "question", "warning", "error".  Returns a static
// string naming the button that was pressed: "ok", "cancel", "abort", "retry",
// "ignore", "yes" or "no".
// ---------------------------------------------------------------------------
const char *MacLua_ShowPopup(const char *message, const char *type, const char *icon);

// ---------------------------------------------------------------------------
// emu.openscript()
//
// Starts filename in a new script context.  When filename is relative it is
// looked up next to the currently running script first, then as given.
// Returns NULL on success, or a static error string.
// ---------------------------------------------------------------------------
const char *MacLua_OpenScript(const char *filename, const char *extraDirectory);

// ---------------------------------------------------------------------------
// emu.addmenu() / emu.setmenuiteminfo()
//
// A "menu" here is an opaque NSMenu*; the engine never dereferences it.  Item
// ids are allocated by MacLua_MenuAllocID() and must be handed back with
// MacLua_MenuFreeID() when the owning script stops.  Selecting an item calls
// CallRegisteredLuaMenuHandlers() with the id it was created with.
// ---------------------------------------------------------------------------
void *MacLua_MenuRoot(void);
int MacLua_MenuAllocID(void);
void MacLua_MenuFreeID(int itemID);
bool MacLua_MenuIsOwnedID(int itemID);
bool MacLua_MenuIsEmpty(void *menu);
void *MacLua_MenuFindSubmenu(void *menu, const char *title, int *outItemID);
void *MacLua_MenuAddSubmenu(void *parentMenu, const char *title, int itemID);
bool MacLua_MenuAddItem(void *menu, int itemID, const char *title);
bool MacLua_MenuAddSeparator(void *menu, int itemID);
void MacLua_MenuRemoveItem(void *menu, int itemID);
void MacLua_MenuSetEnabled(void *menu, int itemID, bool enabled);
void MacLua_MenuSetChecked(void *menu, int itemID, bool checked);
void MacLua_MenuSetTitle(void *menu, int itemID, const char *title);

// ---------------------------------------------------------------------------
// input.getcurrentinputstatus() / input.get()
//
// Fills *out with the keys and mouse buttons the user is holding right now.
// keyNames receives pointers to static strings, using the same names the
// Windows build uses ("A".."Z", "0".."9", "left", "space", "F1", "leftclick"
// and so on), so existing scripts keep working.
//
// mouseX/mouseY are in touch-screen pixels, like the Windows version: the
// pointer's position over the 256x384 canvas has 192 subtracted from y, so the
// touch screen is 0..191 and the upper screen comes out negative.  mouseValid
// is true whenever the pointer is over the game display at all, again matching
// Windows, which always reports xmouse/ymouse once the pointer is in the client
// area.
// ---------------------------------------------------------------------------
typedef struct MacLuaInputStatus
{
	const char *keyNames[64];
	int keyCount;
	bool mouseLeft;
	bool mouseRight;
	bool mouseMiddle;
	bool mouseValid;
	int mouseX;
	int mouseY;
} MacLuaInputStatus;

void MacLua_GetInputStatus(MacLuaInputStatus *out);

// ---------------------------------------------------------------------------
// controller.get() / controller.read()
//
// Reads a physically attached gamepad.  dpad is {up, right, down, left};
// buttons[] holds up to 33 buttons; axes holds x, y, z, r, u, v scaled to
// -1..1.  Returns false (and leaves *out zeroed with connected == false) when
// that pad index is not present, which is what the Windows version reports as
// an empty table.
// ---------------------------------------------------------------------------
typedef struct MacLuaControllerStatus
{
	bool connected;
	bool dpad[4];
	int buttonCount;
	bool buttons[33];
	int axisCount;
	double axes[6];
} MacLuaControllerStatus;

bool MacLua_GetControllerStatus(unsigned int padIndex, bool returnDiagonals, MacLuaControllerStatus *out);

// ---------------------------------------------------------------------------
// emu.message() -> the on-screen HUD, matching Windows' osd->addLine().
// ---------------------------------------------------------------------------
void MacLua_ShowOsdMessage(const char *message);

// Called once per frame by the front end; copies the message that should still
// be on screen into buffer and returns true, or returns false when there is
// nothing to draw.
bool MacLua_GetActiveOsdMessage(char *buffer, int bufferSize);

// ---------------------------------------------------------------------------
// Tools > Lua Scripting > About Lua Scripting
//
// The Cocoa counterpart of the "About..." button in the Windows Lua Script
// window: an information alert naming the engine that is actually running, the
// architecture it was built for, the trace-JIT policy, the headers this binary
// was compiled against, and the dylib the entry points were resolved from.
// Loads the dylib first, so the dialog also works before any script has run.
// Safe to call from any thread; hops to the main thread itself.
// ---------------------------------------------------------------------------
void MacLua_ShowAboutDialog(void);

#ifdef __cplusplus
}
#endif

#endif // MAC_LUA_FRONTEND_H
