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

// Cocoa implementations of the frontend hooks lua-engine.cpp needs.  On
// Windows the same operations are inline in lua-engine.cpp and talk to
// HWND/HMENU/joyGetPosEx; here they reach for AppKit instead.

#include "MacLuaFrontend.h"

#ifdef HAVE_LUA

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#include <Carbon/Carbon.h>

#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <IOKit/hid/IOHIDLib.h>

#include "lua-engine.h"
#include "LuaJITApi.h"

// Runs block on the main thread and waits for it.  The Lua engine executes on
// the emulation thread, so anything that has to touch AppKit goes through here.
static void RunOnMainThreadSync(dispatch_block_t block)
{
	if ([NSThread isMainThread])
	{
		block();
		return;
	}
	dispatch_sync(dispatch_get_main_queue(), block);
}

// Fire-and-forget variant.  Menu state changes are usually made from
// emu.registermenustart(), which runs while AppKit is tracking the open menu in
// NSEventTrackingRunLoopMode.  The main dispatch queue is not serviced in that
// mode, so a dispatch_sync() here would freeze the emulation thread until the
// user closes the menu.  These calls have no result the caller needs.
static void RunOnMainThreadAsync(dispatch_block_t block)
{
	dispatch_async(dispatch_get_main_queue(), block);
}

#pragma mark - Popups

const char *MacLua_ShowPopup(const char *message, const char *type, const char *icon)
{
	NSString *title = @"Notice";
	const char *theIcon = icon ? icon : "message";
	if (strcmp(theIcon, "question") == 0)      title = @"Question";
	else if (strcmp(theIcon, "warning") == 0)  title = @"Warning";
	else if (strcmp(theIcon, "error") == 0)    title = @"Error";

	NSAlertStyle style = NSAlertStyleInformational;
	if (strcmp(theIcon, "warning") == 0) style = NSAlertStyleWarning;
	if (strcmp(theIcon, "error") == 0)   style = NSAlertStyleCritical;

	NSString *body = message ? [NSString stringWithUTF8String:message] : @"";
	const char *theType = type ? type : "ok";

	__block const char *answer = "ok";
	RunOnMainThreadSync(^{
		NSAlert *alert = [[NSAlert alloc] init];
		alert.messageText = title;
		alert.informativeText = body;
		alert.alertStyle = style;

		// Keep the button order the Win32 MessageBox uses so scripts that
		// switch on the result behave the same way on both platforms.
		if (strcmp(theType, "yesno") == 0)
		{
			[alert addButtonWithTitle:@"Yes"];
			[alert addButtonWithTitle:@"No"];
		}
		else if (strcmp(theType, "yesnocancel") == 0)
		{
			[alert addButtonWithTitle:@"Yes"];
			[alert addButtonWithTitle:@"No"];
			[alert addButtonWithTitle:@"Cancel"];
		}
		else if (strcmp(theType, "okcancel") == 0)
		{
			[alert addButtonWithTitle:@"OK"];
			[alert addButtonWithTitle:@"Cancel"];
		}
		else if (strcmp(theType, "abortretryignore") == 0)
		{
			[alert addButtonWithTitle:@"Abort"];
			[alert addButtonWithTitle:@"Retry"];
			[alert addButtonWithTitle:@"Ignore"];
		}
		else
		{
			[alert addButtonWithTitle:@"OK"];
		}

		const NSInteger pressed = [alert runModal]; // 1-based
		if (strcmp(theType, "yesno") == 0)
			answer = (pressed == NSAlertFirstButtonReturn) ? "yes" : "no";
		else if (strcmp(theType, "yesnocancel") == 0)
			answer = (pressed == NSAlertFirstButtonReturn) ? "yes"
			       : (pressed == NSAlertSecondButtonReturn) ? "no" : "cancel";
		else if (strcmp(theType, "okcancel") == 0)
			answer = (pressed == NSAlertFirstButtonReturn) ? "ok" : "cancel";
		else if (strcmp(theType, "abortretryignore") == 0)
			answer = (pressed == NSAlertFirstButtonReturn) ? "abort"
			       : (pressed == NSAlertSecondButtonReturn) ? "retry" : "ignore";
		else
			answer = "ok";
	});

	return answer;
}

#pragma mark - About Lua Scripting

// A fixed-pitch body keeps the "Engine : ..." columns aligned the way the Win32
// MessageBox does; NSAlert's own informativeText is proportional, which turns
// the table into ragged prose.
static NSTextField *MacLuaMakeAboutBody(NSString *string)
{
	NSFont *font = [NSFont userFixedPitchFontOfSize:11.0];
	NSDictionary *attributes = [[NSDictionary alloc] initWithObjectsAndKeys:font, NSFontAttributeName, nil];

	// sizeWithAttributes: honours the newlines, so this is the bounding box of
	// the whole block: width of the longest line, height of all of them.
	NSSize size = [string sizeWithAttributes:attributes];
	size.width = ceil(size.width) + 2.0;
	size.height = ceil(size.height) + 2.0;

	NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, size.width, size.height)];
	[field setStringValue:string];
	[field setFont:font];
	[field setEditable:NO];
	[field setSelectable:YES];	// so the version string can be copied out
	[field setBordered:NO];
	[field setDrawsBackground:NO];
	[field setAutoresizingMask:NSViewNotSizable];
	return field;
}

void MacLua_ShowAboutDialog(void)
{
	LuaEngineInfo info;
	const bool engineReady = GetLuaEngineInfo(&info);

	char libraryPath[1024] = "";
	const char *libraryProblem = "";
#ifdef DESMUME_LUA_DYNAMIC
	// GetLuaEngineInfo() already went through LuaJIT_LoadLibrary(); asking again
	// is idempotent and makes the path available on the failure branch too.
	if (LuaJIT_LoadLibrary())
		snprintf(libraryPath, sizeof(libraryPath), "%s", LuaJIT_LibraryPath());
	else
		libraryProblem = LuaJIT_LoadError();
#else
	strncpy(libraryPath, "(linked at build time)", sizeof(libraryPath) - 1);
#endif

	char text[2048];
	if (!engineReady)
	{
		snprintf(text, sizeof(text),
			"The Lua engine could not be started.\n\n"
			"Reason     : %s\n"
			"Library    : %s\n",
			info.jit[0] ? info.jit : "unknown",
			libraryPath[0] ? libraryPath : (libraryProblem[0] ? libraryProblem : "not loaded"));
	}
	else
	{
		snprintf(text, sizeof(text),
			"DeSmuME Lua Scripting\n\n"
			"Engine     : %s\n"
			"Platform   : %s\n"
			"Trace JIT  : %s\n"
			"Headers    : %s\n"
			"Library    : %s\n",
			info.version,
			info.platform[0] ? info.platform : "n/a (not LuaJIT)",
			info.jit,
			info.headers,
			libraryPath[0] ? libraryPath : "(not loaded as a dylib)");
	}

	NSString *body = [NSString stringWithUTF8String:text];

	RunOnMainThreadSync(^{
		NSAlert *alert = [[NSAlert alloc] init];
		alert.alertStyle = engineReady ? NSAlertStyleInformational : NSAlertStyleCritical;
		alert.messageText = engineReady ? @"About Lua Scripting" : @"Lua Scripting";
		if (engineReady)
		{
			alert.informativeText = @"";
			alert.accessoryView = MacLuaMakeAboutBody(body);
		}
		else
		{
			alert.informativeText = body;
		}
		[alert addButtonWithTitle:@"OK"];
		[alert runModal];
	});
}

#pragma mark - Script directory / emu.openscript

// Declared in MacLuaScriptConsole.mm: opens a console window and queues the
// given file into it.  Returns an error string, or NULL on success.
extern "C" const char *MacLua_ConsoleRunFile(const char *path);

const char *MacLua_OpenScript(const char *filename, const char *extraDirectory)
{
	if (filename == NULL || filename[0] == '\0')
		return "emu.openscript: no filename given";

	NSFileManager *fm = [NSFileManager defaultManager];
	NSString *candidate = [fm stringWithFileSystemRepresentation:filename length:strlen(filename)];

	if (![candidate isAbsolutePath] && extraDirectory != NULL && extraDirectory[0] != '\0')
	{
		NSString *sibling = [[fm stringWithFileSystemRepresentation:extraDirectory
															length:strlen(extraDirectory)]
								stringByAppendingPathComponent:candidate];
		if ([fm fileExistsAtPath:sibling])
			candidate = sibling;
	}

	if (![fm fileExistsAtPath:candidate])
	{
		static char error[1200];
		snprintf(error, sizeof(error), "emu.openscript: cannot find \"%s\"", filename);
		return error;
	}

	const char *error = MacLua_ConsoleRunFile([candidate fileSystemRepresentation]);
	return error;
}

#pragma mark - Menus

// Menu items are identified by an integer tag.  The range is deliberately
// small and separate from anything AppKit or the main nib uses.
enum
{
	kMacLuaMenuItemFirst = 40000,
	kMacLuaMenuItemCount = 512,
};

static bool sMenuItemUsed[kMacLuaMenuItemCount];

// One target shared by every Lua menu item, and the delegate of the Lua menus.
@interface MacLuaMenuTarget : NSObject <NSMenuDelegate>
- (void)menuItemSelected:(id)sender;
@end

@implementation MacLuaMenuTarget
- (void)menuItemSelected:(id)sender
{
	NSMenuItem *item = (NSMenuItem *)sender;
	const size_t itemID = (size_t)[item tag];
	if (itemID == 0)
		return;
	// The handler runs Lua, and the Lua state belongs to the emulation thread.
	// Park the click instead of calling into it from here.
	MacLua_QueueMenuHandler((int)itemID);
}

// The macOS counterpart of WM_INITMENUPOPUP, which is where Windows gives
// scripts a chance to refresh their items (emu.registermenustart).  Delegates
// run on the main thread too, so this only raises a flag for whichever frame
// pump is active (cocoa_core.mm's loop, or macOS_driver.cpp's EMU_StepMainLoop
// when a script drives frames with emu.frameadvance()).  The menu is therefore
// painted from whatever the last frame left behind, and picks the new state up
// on the next open if a script changed it mid-track.
- (void)menuNeedsUpdate:(NSMenu *)menu
{
	MacLua_QueueMenuInit();
}
@end

// Menu clicks waiting for the emulation thread.  Small and fixed: a user cannot
// realistically out-click the frame loop, and dropping the excess is better than
// blocking AppKit on a mutex the emulator holds.
enum { kMacLuaPendingMenuMax = 16 };
static pthread_mutex_t sPendingMenuLock = PTHREAD_MUTEX_INITIALIZER;
static int sPendingMenuItems[kMacLuaPendingMenuMax];
static int sPendingMenuCount = 0;
static bool sMenuInitPending = false;

void MacLua_QueueMenuHandler(int itemID)
{
	pthread_mutex_lock(&sPendingMenuLock);
	if (sPendingMenuCount < kMacLuaPendingMenuMax)
		sPendingMenuItems[sPendingMenuCount++] = itemID;
	pthread_mutex_unlock(&sPendingMenuLock);
}

void MacLua_QueueMenuInit(void)
{
	pthread_mutex_lock(&sPendingMenuLock);
	sMenuInitPending = true;
	pthread_mutex_unlock(&sPendingMenuLock);
}

void MacLua_DrainMenuCalls(void)
{
	int pending[kMacLuaPendingMenuMax];
	int count = 0;
	bool runMenuInit = false;

	pthread_mutex_lock(&sPendingMenuLock);
	count = sPendingMenuCount;
	memcpy(pending, sPendingMenuItems, (size_t)count * sizeof(pending[0]));
	sPendingMenuCount = 0;
	runMenuInit = sMenuInitPending;
	sMenuInitPending = false;
	pthread_mutex_unlock(&sPendingMenuLock);

	// emu.registermenustart() first: its whole job is to set the state that the
	// click handlers below may then act on.
	if (runMenuInit)
		CallRegisteredLuaFunctions(LUACALL_ONINITMENU);

	// Handlers may add or remove menu items (and scripts may stop), so run them
	// outside the lock and look nothing up in between.
	for (int i = 0; i < count; i++)
		CallRegisteredLuaMenuHandlers((PlatformMenuItem)pending[i]);
}

static MacLuaMenuTarget *sMenuTarget = nil;

static MacLuaMenuTarget *MenuTarget(void)
{
	if (sMenuTarget == nil)
		sMenuTarget = [[MacLuaMenuTarget alloc] init];
	return sMenuTarget;
}

static NSMenuItem *FindItem(NSMenu *menu, int itemID)
{
	for (NSMenuItem *item in [menu itemArray])
	{
		if ([item tag] == itemID)
			return item;
		if ([item submenu] != nil)
		{
			NSMenuItem *nested = FindItem([item submenu], itemID);
			if (nested != nil)
				return nested;
		}
	}
	return nil;
}

// The item that owns this submenu, so items can be inserted in order.
static NSMenuItem *FindItemOwningSubmenu(NSMenu *root, NSMenu *submenu)
{
	if (submenu == nil || root == nil)
		return nil;
	for (NSMenuItem *item in [root itemArray])
	{
		if ([item submenu] == submenu)
			return item;
		if ([item submenu] != nil)
		{
			NSMenuItem *nested = FindItemOwningSubmenu([item submenu], submenu);
			if (nested != nil)
				return nested;
		}
	}
	return nil;
}

void *MacLua_MenuRoot(void)
{
	__block NSMenu *root = nil;
	RunOnMainThreadSync(^{
		root = [NSApp mainMenu];
		if (root == nil)
		{
			// No nib menu (embedded builds); make a stand-in so scripts still
			// get a menu instead of silently doing nothing.
			root = [[NSMenu alloc] initWithTitle:@"MainMenu"];
			[NSApp setMainMenu:root];
		}
	});
	return root;
}

int MacLua_MenuAllocID(void)
{
	// Called on the emulation thread; the table is only touched here.
	for (int i = 0; i < kMacLuaMenuItemCount; i++)
	{
		if (!sMenuItemUsed[i])
		{
			sMenuItemUsed[i] = true;
			return kMacLuaMenuItemFirst + i;
		}
	}
	return -1;
}

void MacLua_MenuFreeID(int itemID)
{
	const int index = itemID - kMacLuaMenuItemFirst;
	if (index >= 0 && index < kMacLuaMenuItemCount)
		sMenuItemUsed[index] = false;
}

bool MacLua_MenuIsOwnedID(int itemID)
{
	const int index = itemID - kMacLuaMenuItemFirst;
	return index >= 0 && index < kMacLuaMenuItemCount && sMenuItemUsed[index];
}

bool MacLua_MenuIsEmpty(void *menu)
{
	if (menu == NULL)
		return true;
	NSMenu *theMenu = (NSMenu *)menu;
	__block bool empty = true;
	RunOnMainThreadSync(^{
		empty = ([theMenu numberOfItems] == 0);
	});
	return empty;
}

void *MacLua_MenuFindSubmenu(void *menu, const char *title, int *outItemID)
{
	if (menu == NULL || title == NULL)
		return NULL;

	NSMenu *theMenu = (NSMenu *)menu;
	NSString *wanted = [NSString stringWithUTF8String:title];
	__block NSMenu *found = nil;
	__block int foundID = 0;

	RunOnMainThreadSync(^{
		for (NSMenuItem *item in [theMenu itemArray])
		{
			if ([item submenu] != nil && [[item title] isEqualToString:wanted])
			{
				found = [item submenu];
				foundID = (int)[item tag];
				break;
			}
		}
	});

	if (found != nil && outItemID != NULL)
		*outItemID = foundID;
	return found;
}

void *MacLua_MenuAddSubmenu(void *parentMenu, const char *title, int itemID)
{
	if (parentMenu == NULL || title == NULL)
		return NULL;

	NSMenu *parent = (NSMenu *)parentMenu;
	NSString *theTitle = [NSString stringWithUTF8String:title];
	__block NSMenu *created = nil;

	RunOnMainThreadSync(^{
		NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:theTitle action:nil keyEquivalent:@""];
		item.tag = itemID;
		NSMenu *submenu = [[NSMenu alloc] initWithTitle:theTitle];
		[submenu setAutoenablesItems:NO];
		// Gives scripts the same "menu is about to open" hook Windows has;
		// see MacLuaMenuTarget -menuNeedsUpdate:.
		[submenu setDelegate:MenuTarget()];
		[item setSubmenu:submenu];
		[parent addItem:item];
		created = submenu;
	});

	return created;
}

bool MacLua_MenuAddItem(void *menu, int itemID, const char *title)
{
	if (menu == NULL || title == NULL)
		return false;

	NSMenu *theMenu = (NSMenu *)menu;
	NSString *theTitle = [NSString stringWithUTF8String:title];

	RunOnMainThreadSync(^{
		NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:theTitle
													 action:@selector(menuItemSelected:)
											  keyEquivalent:@""];
		item.tag = itemID;
		item.target = MenuTarget();
		[theMenu addItem:item];
		// Script-provided items come and go with the script, so do not let
		// AppKit decide their enabled state based on the responder chain.
		[theMenu setAutoenablesItems:NO];
	});

	return true;
}

bool MacLua_MenuAddSeparator(void *menu, int itemID)
{
	if (menu == NULL)
		return false;

	NSMenu *theMenu = (NSMenu *)menu;
	RunOnMainThreadSync(^{
		NSMenuItem *separator = [NSMenuItem separatorItem];
		separator.tag = itemID;
		[theMenu addItem:separator];
	});
	return true;
}

void MacLua_MenuRemoveItem(void *menu, int itemID)
{
	if (menu == NULL)
		return;
	NSMenu *theMenu = (NSMenu *)menu;
	RunOnMainThreadAsync(^{
		NSMenuItem *item = FindItem(theMenu, itemID);
		if (item != nil)
			[theMenu removeItem:item];
	});
}

void MacLua_MenuSetEnabled(void *menu, int itemID, bool enabled)
{
	if (menu == NULL)
		return;
	NSMenu *theMenu = (NSMenu *)menu;
	RunOnMainThreadAsync(^{
		NSMenuItem *item = FindItem(theMenu, itemID);
		if (item != nil)
			[item setEnabled:enabled ? YES : NO];
	});
}

void MacLua_MenuSetChecked(void *menu, int itemID, bool checked)
{
	if (menu == NULL)
		return;
	NSMenu *theMenu = (NSMenu *)menu;
	RunOnMainThreadAsync(^{
		NSMenuItem *item = FindItem(theMenu, itemID);
		if (item != nil)
			[item setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
	});
}

void MacLua_MenuSetTitle(void *menu, int itemID, const char *title)
{
	if (menu == NULL || title == NULL)
		return;
	NSMenu *theMenu = (NSMenu *)menu;
	NSString *theTitle = [NSString stringWithUTF8String:title];
	RunOnMainThreadAsync(^{
		NSMenuItem *item = FindItem(theMenu, itemID);
		if (item != nil)
			[item setTitle:theTitle];
	});
}

#pragma mark - Key names

// macOS virtual key codes mapped to the names the Windows build exposes
// through input.getcurrentinputstatus(), so scripts do not care which
// platform they run on.
struct KeyNameEntry
{
	unsigned short keyCode;
	const char *name;
};

static const KeyNameEntry kKeyNames[] =
{
	{ kVK_ANSI_A, "A" }, { kVK_ANSI_B, "B" }, { kVK_ANSI_C, "C" },
	{ kVK_ANSI_D, "D" }, { kVK_ANSI_E, "E" }, { kVK_ANSI_F, "F" },
	{ kVK_ANSI_G, "G" }, { kVK_ANSI_H, "H" }, { kVK_ANSI_I, "I" },
	{ kVK_ANSI_J, "J" }, { kVK_ANSI_K, "K" }, { kVK_ANSI_L, "L" },
	{ kVK_ANSI_M, "M" }, { kVK_ANSI_N, "N" }, { kVK_ANSI_O, "O" },
	{ kVK_ANSI_P, "P" }, { kVK_ANSI_Q, "Q" }, { kVK_ANSI_R, "R" },
	{ kVK_ANSI_S, "S" }, { kVK_ANSI_T, "T" }, { kVK_ANSI_U, "U" },
	{ kVK_ANSI_V, "V" }, { kVK_ANSI_W, "W" }, { kVK_ANSI_X, "X" },
	{ kVK_ANSI_Y, "Y" }, { kVK_ANSI_Z, "Z" },

	{ kVK_ANSI_0, "0" }, { kVK_ANSI_1, "1" }, { kVK_ANSI_2, "2" },
	{ kVK_ANSI_3, "3" }, { kVK_ANSI_4, "4" }, { kVK_ANSI_5, "5" },
	{ kVK_ANSI_6, "6" }, { kVK_ANSI_7, "7" }, { kVK_ANSI_8, "8" },
	{ kVK_ANSI_9, "9" },

	{ kVK_ANSI_Keypad0, "numpad0" }, { kVK_ANSI_Keypad1, "numpad1" },
	{ kVK_ANSI_Keypad2, "numpad2" }, { kVK_ANSI_Keypad3, "numpad3" },
	{ kVK_ANSI_Keypad4, "numpad4" }, { kVK_ANSI_Keypad5, "numpad5" },
	{ kVK_ANSI_Keypad6, "numpad6" }, { kVK_ANSI_Keypad7, "numpad7" },
	{ kVK_ANSI_Keypad8, "numpad8" }, { kVK_ANSI_Keypad9, "numpad9" },
	{ kVK_ANSI_KeypadDecimal, "numpad." }, { kVK_ANSI_KeypadMultiply, "numpad*" },
	{ kVK_ANSI_KeypadPlus, "numpad+" }, { kVK_ANSI_KeypadMinus, "numpad-" },
	{ kVK_ANSI_KeypadDivide, "numpad/" }, { kVK_ANSI_KeypadEnter, "enter" },

	{ kVK_Return, "enter" }, { kVK_Tab, "tab" }, { kVK_Space, "space" },
	{ kVK_Delete, "backspace" },        // the key labelled "delete"
	{ kVK_ForwardDelete, "delete" },
	{ kVK_Escape, "escape" },
	{ kVK_Home, "home" }, { kVK_End, "end" },
	{ kVK_PageUp, "pageup" }, { kVK_PageDown, "pagedown" },
	{ kVK_LeftArrow, "left" }, { kVK_RightArrow, "right" },
	{ kVK_UpArrow, "up" }, { kVK_DownArrow, "down" },
	{ kVK_Help, "insert" },

	{ kVK_Shift, "shift" }, { kVK_RightShift, "shift" },
	{ kVK_Control, "control" }, { kVK_RightControl, "control" },
	{ kVK_Option, "alt" }, { kVK_RightOption, "alt" },
	{ kVK_Command, "command" }, { kVK_RightCommand, "command" },
	{ kVK_CapsLock, "capslock" },

	{ kVK_F1, "F1" }, { kVK_F2, "F2" }, { kVK_F3, "F3" }, { kVK_F4, "F4" },
	{ kVK_F5, "F5" }, { kVK_F6, "F6" }, { kVK_F7, "F7" }, { kVK_F8, "F8" },
	{ kVK_F9, "F9" }, { kVK_F10, "F10" }, { kVK_F11, "F11" }, { kVK_F12, "F12" },
	{ kVK_F13, "F13" }, { kVK_F14, "F14" }, { kVK_F15, "F15" }, { kVK_F16, "F16" },
	{ kVK_F17, "F17" }, { kVK_F18, "F18" }, { kVK_F19, "F19" }, { kVK_F20, "F20" },
};

void MacLua_GetInputStatus(MacLuaInputStatus *out)
{
	if (out == NULL)
		return;
	memset(out, 0, sizeof(*out));

	CGEventSourceStateID state = kCGEventSourceStateCombinedSessionState;

	// Mouse buttons share the same namespace as the keyboard on Windows
	// ("leftclick" and friends), so report them the same way.
	if (CGEventSourceButtonState(state, kCGMouseButtonLeft))
	{
		out->mouseLeft = true;
		out->keyNames[out->keyCount++] = "leftclick";
	}
	if (CGEventSourceButtonState(state, kCGMouseButtonRight))
	{
		out->mouseRight = true;
		out->keyNames[out->keyCount++] = "rightclick";
	}
	if (CGEventSourceButtonState(state, kCGMouseButtonCenter))
	{
		out->mouseMiddle = true;
		out->keyNames[out->keyCount++] = "middleclick";
	}

	for (size_t i = 0; i < sizeof(kKeyNames) / sizeof(kKeyNames[0]); i++)
	{
		if (out->keyCount >= (int)(sizeof(out->keyNames) / sizeof(out->keyNames[0])))
			break;
		if (CGEventSourceKeyState(state, kKeyNames[i].keyCode))
		{
			// A key can appear twice (left/right shift, both arrows on a
			// numpad-less layout); only report the name once.
			bool alreadyReported = false;
			for (int k = 0; k < out->keyCount; k++)
			{
				if (strcmp(out->keyNames[k], kKeyNames[i].name) == 0)
				{
					alreadyReported = true;
					break;
				}
			}
			if (!alreadyReported)
				out->keyNames[out->keyCount++] = kKeyNames[i].name;
		}
	}

	// Pointer position in DS screen pixels.  Windows works this out from the
	// client rect and the current scaling; here the same idea is applied to the
	// display view, which covers the unrotated, unfiltered case exactly.
	__block int dsX = -1;
	__block int dsY = -1;
	__block bool valid = false;

	RunOnMainThreadSync(^{
		NSWindow *window = [NSApp mainWindow];
		if (window == nil)
			window = [NSApp keyWindow];
		if (window == nil)
			return;

		// Find the view that actually renders the DS screens.
		NSView *displayView = nil;
		NSMutableArray *pending = [NSMutableArray arrayWithObject:[window contentView]];
		while ([pending count] > 0 && displayView == nil)
		{
			NSView *view = [pending objectAtIndex:0];
			[pending removeObjectAtIndex:0];
			NSString *className = NSStringFromClass([view class]);
			if ([className rangeOfString:@"DisplayView"].location != NSNotFound)
			{
				displayView = view;
				break;
			}
			[pending addObjectsFromArray:[view subviews]];
		}
		if (displayView == nil)
			return;

		const NSPoint inWindow = [window mouseLocationOutsideOfEventStream];
		const NSPoint local = [displayView convertPoint:inWindow fromView:nil];
		const NSRect bounds = [displayView bounds];
		if (bounds.size.width <= 0.0 || bounds.size.height <= 0.0)
			return;
		if (!NSMouseInRect(local, bounds, [displayView isFlipped]))
			return;

		// The HUD is 256x384 with the main screen on top; this matches the
		// coordinate space agg.* and gui.text() use.
		double px = (local.x - bounds.origin.x) * 256.0 / bounds.size.width;
		double py = (local.y - bounds.origin.y) * 384.0 / bounds.size.height;
		if (![displayView isFlipped])
			py = 384.0 - py;

		// Windows reports the pointer relative to the touch screen (its
		// ToDSScreenRelativeCoords(.., 1) subtracts the upper screen's height),
		// so do the same instead of handing scripts canvas coordinates.
		// Over the upper screen the result is negative, exactly as on Windows.
		dsX = (int)px;
		dsY = (int)py - 192;
		valid = (dsX >= 0 && dsX < 256 && dsY >= -192 && dsY < 192);
	});

	out->mouseValid = valid;
	out->mouseX = valid ? dsX : -1;
	out->mouseY = valid ? dsY : -1;
}

#pragma mark - Game controllers

// A background thread keeps a snapshot of every attached gamepad, because
// IOHID values are only current while the device is queued on a live run loop.
enum
{
	kMacLuaMaxPads = 4,
	kMacLuaMaxButtons = 33,
	kMacLuaMaxAxes = 6,
};

typedef struct
{
	bool connected;
	bool hasHat;
	int hat;           // -1 when centred
	bool dpad[4];      // only used when the pad has no hat switch
	int buttonCount;
	bool buttons[kMacLuaMaxButtons];
	int axisCount;
	double axes[kMacLuaMaxAxes];
} PadSnapshot;

static PadSnapshot sPads[kMacLuaMaxPads];
static IOHIDManagerRef sPadManager = NULL;
static pthread_mutex_t sPadLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t sPadOnce = PTHREAD_ONCE_INIT;

// Cookies of the elements we care about, per device.
typedef struct
{
	IOHIDElementCookie hat;
	IOHIDElementCookie up, right, down, left;
	IOHIDElementCookie buttons[kMacLuaMaxButtons];
	IOHIDElementCookie axes[kMacLuaMaxAxes];
	int buttonCount;
	int axisCount;
} PadElementMap;

static PadElementMap sPadElements[kMacLuaMaxPads];

static void PadInputValueCallback(void *context, IOReturn result, void *sender, IOHIDValueRef value)
{
	if (value == NULL)
		return;

	const IOHIDElementRef element = IOHIDValueGetElement(value);
	const IOHIDElementCookie cookie = IOHIDElementGetCookie(element);
	const CFIndex intValue = IOHIDValueGetIntegerValue(value);

	pthread_mutex_lock(&sPadLock);
	for (int pad = 0; pad < kMacLuaMaxPads; pad++)
	{
		PadElementMap &map = sPadElements[pad];
		if (!sPads[pad].connected)
			continue;

		if (cookie == map.hat)
		{
			// HID hats are 0..7 clockwise from north; 8 (or -1) is centred.
			sPads[pad].hat = (int)intValue;
		}
		if (cookie == map.up)    sPads[pad].dpad[0] = intValue != 0;
		if (cookie == map.right) sPads[pad].dpad[1] = intValue != 0;
		if (cookie == map.down)  sPads[pad].dpad[2] = intValue != 0;
		if (cookie == map.left)  sPads[pad].dpad[3] = intValue != 0;

		for (int b = 0; b < map.buttonCount; b++)
		{
			if (cookie == map.buttons[b])
				sPads[pad].buttons[b] = intValue != 0;
		}

		for (int a = 0; a < map.axisCount; a++)
		{
			if (cookie != map.axes[a])
				continue;
			const CFIndex min = IOHIDElementGetLogicalMin(element);
			const CFIndex max = IOHIDElementGetLogicalMax(element);
			if (max > min)
				sPads[pad].axes[a] = ((double)(intValue - min) * 2.0 / (double)(max - min)) - 1.0;
		}
	}
	pthread_mutex_unlock(&sPadLock);
}

static void PadDeviceMatched(void *context, IOReturn result, void *sender, IOHIDDeviceRef device)
{
	for (int pad = 0; pad < kMacLuaMaxPads; pad++)
	{
		pthread_mutex_lock(&sPadLock);
		if (!sPads[pad].connected)
		{
			memset(&sPads[pad], 0, sizeof(sPads[pad]));
			memset(&sPadElements[pad], 0, sizeof(sPadElements[pad]));
			sPads[pad].connected = true;
			sPads[pad].axisCount = 0;
			sPads[pad].buttonCount = 0;
			pthread_mutex_unlock(&sPadLock);

			// Map the device's elements onto our fixed slots.
			CFArrayRef elements = IOHIDDeviceCopyMatchingElements(device, NULL, kIOHIDOptionsTypeNone);
			if (elements != NULL)
			{
				PadElementMap &map = sPadElements[pad];
				for (CFIndex i = 0; i < CFArrayGetCount(elements); i++)
				{
					IOHIDElementRef element = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
					if (element == NULL)
						continue;

					// Only the three input element kinds carry a live value; the
					// mixed && / || test used to read as a precedence bug.
					const IOHIDElementType elementType = IOHIDElementGetType(element);
					if (elementType != kIOHIDElementTypeInput_Misc &&
						elementType != kIOHIDElementTypeInput_Button &&
						elementType != kIOHIDElementTypeInput_Axis)
						continue;

					const uint32_t usagePage = IOHIDElementGetUsagePage(element);
					const uint32_t usage = IOHIDElementGetUsage(element);
					if (usagePage == kHIDPage_Button)
					{
						if (map.buttonCount < kMacLuaMaxButtons)
							map.buttons[map.buttonCount++] = IOHIDElementGetCookie(element);
					}
					else if (usagePage == kHIDPage_GenericDesktop)
					{
						switch (usage)
						{
							case kHIDUsage_GD_Hatswitch:
								map.hat = IOHIDElementGetCookie(element);
								pthread_mutex_lock(&sPadLock);
								sPads[pad].hasHat = true;
								sPads[pad].hat = -1;
								pthread_mutex_unlock(&sPadLock);
								break;
							case kHIDUsage_GD_X: case kHIDUsage_GD_Y: case kHIDUsage_GD_Z:
							case kHIDUsage_GD_Rx: case kHIDUsage_GD_Ry: case kHIDUsage_GD_Rz:
								if (map.axisCount < kMacLuaMaxAxes)
									map.axes[map.axisCount++] = IOHIDElementGetCookie(element);
								break;
							default:
								break;
						}
					}
				}
				CFRelease(elements);
			}

			pthread_mutex_lock(&sPadLock);
			sPads[pad].buttonCount = sPadElements[pad].buttonCount;
			sPads[pad].axisCount = sPadElements[pad].axisCount;
			pthread_mutex_unlock(&sPadLock);

			IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
			IOHIDDeviceRegisterInputValueCallback(device, PadInputValueCallback, NULL);
			IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
			return;
		}
		pthread_mutex_unlock(&sPadLock);
	}
}

static void PadDeviceRemoved(void *context, IOReturn result, void *sender, IOHIDDeviceRef device)
{
	pthread_mutex_lock(&sPadLock);
	for (int pad = 0; pad < kMacLuaMaxPads; pad++)
	{
		// We do not track which IOHIDDeviceRef belongs to which slot, so drop
		// them all and let the re-match repopulate.  Removing a pad is rare.
		memset(&sPads[pad], 0, sizeof(sPads[pad]));
		memset(&sPadElements[pad], 0, sizeof(sPadElements[pad]));
	}
	pthread_mutex_unlock(&sPadLock);

	IOHIDDeviceUnscheduleFromRunLoop(device, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
}

static void *PadThreadMain(void *arg)
{
	sPadManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
	if (sPadManager == NULL)
		return NULL;

	// kIOHIDDeviceUsagePageKey / kIOHIDDeviceUsageKey are C string macros in
	// IOHIDKeys.h, not CFStringRefs, so they cannot be bridged.  Spell out the
	// property names they expand to instead.
	IOHIDManagerSetDeviceMatchingMultiple(sPadManager, (__bridge CFArrayRef)@[
		@{ @"DeviceUsagePage" : @(kHIDPage_GenericDesktop),
		   @"DeviceUsage" : @(kHIDUsage_GD_Joystick) },
		@{ @"DeviceUsagePage" : @(kHIDPage_GenericDesktop),
		   @"DeviceUsage" : @(kHIDUsage_GD_GamePad) },
		@{ @"DeviceUsagePage" : @(kHIDPage_GenericDesktop),
		   @"DeviceUsage" : @(kHIDUsage_GD_MultiAxisController) },
	]);

	IOHIDManagerRegisterDeviceMatchingCallback(sPadManager, PadDeviceMatched, NULL);
	IOHIDManagerRegisterDeviceRemovalCallback(sPadManager, PadDeviceRemoved, NULL);
	IOHIDManagerScheduleWithRunLoop(sPadManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	IOHIDManagerOpen(sPadManager, kIOHIDOptionsTypeNone);

	// Run until the process exits; there is nothing to tear down gracefully.
	CFRunLoopRun();

	return NULL;
}

static void PadStartOnce(void)
{
	pthread_t thread;
	if (pthread_create(&thread, NULL, PadThreadMain, NULL) == 0)
		pthread_detach(thread);
}

bool MacLua_GetControllerStatus(unsigned int padIndex, bool returnDiagonals, MacLuaControllerStatus *out)
{
	if (out == NULL)
		return false;
	memset(out, 0, sizeof(*out));

	if (padIndex >= (unsigned int)kMacLuaMaxPads)
		return false;

	pthread_once(&sPadOnce, PadStartOnce);
	// Give the input thread a moment to report the first device the very first
	// time a script asks, so a pad that is already attached is not missed.
	for (int attempt = 0; attempt < 20; attempt++)
	{
		pthread_mutex_lock(&sPadLock);
		const bool connected = sPads[padIndex].connected;
		pthread_mutex_unlock(&sPadLock);
		if (connected)
			break;
		usleep(5000);
	}

	pthread_mutex_lock(&sPadLock);
	const PadSnapshot &snapshot = sPads[padIndex];
	out->connected = snapshot.connected;
	if (snapshot.connected)
	{
		if (snapshot.hasHat)
		{
			// 0..7 clockwise from north, 8 (or anything else) means centred.
			const int hat = snapshot.hat;
			if (hat >= 0 && hat <= 7)
			{
				const bool north = (hat == 7 || hat == 0 || hat == 1);
				const bool east  = (hat == 1 || hat == 2 || hat == 3);
				const bool south = (hat == 3 || hat == 4 || hat == 5);
				const bool west  = (hat == 5 || hat == 6 || hat == 7);
				const bool diagonal = (hat % 2) != 0;

				if (diagonal && returnDiagonals)
				{
					out->dpad[0] = north;
					out->dpad[1] = east;
					out->dpad[2] = south;
					out->dpad[3] = west;
				}
				else if (!diagonal)
				{
					out->dpad[0] = north;
					out->dpad[1] = east;
					out->dpad[2] = south;
					out->dpad[3] = west;
				}
			}
		}
		else
		{
			for (int i = 0; i < 4; i++)
				out->dpad[i] = snapshot.dpad[i];
		}

		out->buttonCount = snapshot.buttonCount;
		for (int i = 0; i < snapshot.buttonCount && i < kMacLuaMaxButtons; i++)
			out->buttons[i] = snapshot.buttons[i];
		out->axisCount = snapshot.axisCount;
		for (int i = 0; i < snapshot.axisCount && i < kMacLuaMaxAxes; i++)
			out->axes[i] = snapshot.axes[i];
	}
	pthread_mutex_unlock(&sPadLock);

	return out->connected;
}

#pragma mark - HUD messages

// emu.message() on Windows ends up in osd->addLine().  Cocoa has no OSDCLASS,
// so keep the last message here and let the frame loop paint it into the Metal
// OSD for a few seconds.
static pthread_mutex_t sMessageLock = PTHREAD_MUTEX_INITIALIZER;
static char sPendingMessage[512] = "";
static bool sMessageActive = false;
static double sMessageExpiry = 0.0;

void MacLua_ShowOsdMessage(const char *message)
{
	if (message == NULL)
		return;
	pthread_mutex_lock(&sMessageLock);
	strncpy(sPendingMessage, message, sizeof(sPendingMessage) - 1);
	sPendingMessage[sizeof(sPendingMessage) - 1] = '\0';
	sMessageActive = true;
	sMessageExpiry = [NSDate timeIntervalSinceReferenceDate] + 3.0;
	pthread_mutex_unlock(&sMessageLock);

	NSLog(@"%s", message);
}

// Called once per frame by the front end.  Copies the message into buffer while
// it is still fresh and returns true; returns false when there is nothing to
// draw.
bool MacLua_GetActiveOsdMessage(char *buffer, int bufferSize)
{
	if (bufferSize <= 0)
		return false;

	bool active = false;
	pthread_mutex_lock(&sMessageLock);
	if (sMessageActive)
	{
		if ([NSDate timeIntervalSinceReferenceDate] < sMessageExpiry)
		{
			strncpy(buffer, sPendingMessage, (size_t)bufferSize - 1);
			buffer[bufferSize - 1] = '\0';
			active = true;
		}
		else
		{
			sMessageActive = false;
		}
	}
	pthread_mutex_unlock(&sMessageLock);

	return active;
}

#endif /* HAVE_LUA */
