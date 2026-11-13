/*
	Copyright (C) 2026 DeSmuME Team

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

#import <Cocoa/Cocoa.h>
#import "MacLuaScriptConsole.h"
#include "lua-engine.h"
#include <string>
#include <pthread.h>

#ifdef HAVE_LUA
#include "../../driver.h"
#include "macOS_driver.h"
#endif

#ifdef HAVE_LUA

@interface LuaConsoleWindowController : NSObject <NSWindowDelegate, NSTextFieldDelegate>
{
	NSWindow *window;
	NSTextField *pathField;
	NSButton *browseButton;
	NSButton *editButton;
	NSButton *stopButton;
	NSButton *runButton;
	NSButton *stdoutCheckbox;
	NSTextView *textView;
	NSScrollView *scrollView;
	
	int uid;
	std::string filename;
	BOOL started;
	BOOL closeOnStop;
	
	dispatch_source_t watchSource;
	int watchedFd;
}

@property (assign) int uid;
@property (assign) std::string filename;
@property (assign) BOOL started;
@property (assign) BOOL closeOnStop;
@property (assign) dispatch_source_t watchSource;
@property (assign) int watchedFd;

- (void)showWindow;
- (void)onPathChanged;
- (void)appendOutputMainThread:(NSString *)text;
- (void)onScriptStarted;
- (void)onScriptStopped;
- (NSWindow *)window;

@end

static NSMutableDictionary *g_consoles = nil;
static int g_next_uid = 1;

@implementation LuaConsoleWindowController

@synthesize uid;
@synthesize filename;
@synthesize started;
@synthesize closeOnStop;
@synthesize watchSource;
@synthesize watchedFd;

- (instancetype)init {
	self = [super init];
	if (self) {
		self.watchedFd = -1;
		self.started = NO;
		self.closeOnStop = NO;
		self.watchSource = nil;
		
		// Window
		NSRect frame = NSMakeRect(100, 100, 450, 300);
		NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
		window = [[NSWindow alloc] initWithContentRect:frame
											  styleMask:styleMask
												backing:NSBackingStoreBuffered
												  defer:NO];
		[window setTitle:@"Lua Script Console"];
		[window setMinSize:NSMakeSize(350, 200)];
		[window setDelegate:self];
		[window setReleasedWhenClosed:NO];
		
		NSView *contentView = [window contentView];
		
		// Path Field
		pathField = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 268, 430, 22)];
		[pathField setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
		[pathField setDelegate:self];
		[[pathField cell] setPlaceholderString:@"Enter path to script..."];
		[contentView addSubview:pathField];
		
		// Browse Button
		browseButton = [[NSButton alloc] initWithFrame:NSMakeRect(10, 236, 90, 25)];
		[browseButton setTitle:@"Browse..."];
		[browseButton setBezelStyle:NSBezelStyleRounded];
		[browseButton setTarget:self];
		[browseButton setAction:@selector(onBrowseClicked:)];
		[browseButton setAutoresizingMask:NSViewMinYMargin];
		[contentView addSubview:browseButton];
		
		// Edit Button
		editButton = [[NSButton alloc] initWithFrame:NSMakeRect(105, 236, 80, 25)];
		[editButton setTitle:@"Edit"];
		[editButton setBezelStyle:NSBezelStyleRounded];
		[editButton setTarget:self];
		[editButton setAction:@selector(onEditClicked:)];
		[editButton setEnabled:NO];
		[editButton setAutoresizingMask:NSViewMinYMargin];
		[contentView addSubview:editButton];
		
		// Stop Button
		stopButton = [[NSButton alloc] initWithFrame:NSMakeRect(270, 236, 80, 25)];
		[stopButton setTitle:@"Stop"];
		[stopButton setBezelStyle:NSBezelStyleRounded];
		[stopButton setTarget:self];
		[stopButton setAction:@selector(onStopClicked:)];
		[stopButton setEnabled:NO];
		[stopButton setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
		[contentView addSubview:stopButton];
		
		// Run Button
		runButton = [[NSButton alloc] initWithFrame:NSMakeRect(355, 236, 85, 25)];
		[runButton setTitle:@"Run"];
		[runButton setBezelStyle:NSBezelStyleRounded];
		[runButton setTarget:self];
		[runButton setAction:@selector(onRunClicked:)];
		[runButton setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
		[contentView addSubview:runButton];
		
		// stdout Checkbox
		stdoutCheckbox = [[NSButton alloc] initWithFrame:NSMakeRect(370, 212, 70, 18)];
		[stdoutCheckbox setTitle:@"stdout"];
		[stdoutCheckbox setButtonType:NSButtonTypeSwitch];
		[stdoutCheckbox setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
		[contentView addSubview:stdoutCheckbox];
		
		// Scroll View
		scrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(10, 10, 430, 195)];
		[scrollView setHasVerticalScroller:YES];
		[scrollView setHasHorizontalScroller:YES];
		[scrollView setBorderType:NSNoBorder];
		[scrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
		
		// Text View
		textView = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 430, 195)];
		[textView setEditable:NO];
		[textView setVerticallyResizable:YES];
		[textView setHorizontallyResizable:YES];
		[textView setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
		[textView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
		[[textView textContainer] setWidthTracksTextView:YES];
		[textView setFont:[NSFont userFixedPitchFontOfSize:10.0]];
		
		[scrollView setDocumentView:textView];
		[contentView addSubview:scrollView];
	}
	return self;
}

- (NSWindow *)window {
	return window;
}

- (void)showWindow {
	[window makeKeyAndOrderFront:nil];
}

- (void)onPathChanged {
	NSString *path = [pathField stringValue];
	if ([path length] > 0 && [[NSFileManager defaultManager] fileExistsAtPath:path]) {
		self.filename = [path UTF8String];
		NSString *lastComponent = [path lastPathComponent];
		[window setTitle:lastComponent];
		[self startFileWatcher:path];
	}
	[self updateEditButton];
}

- (void)updateEditButton {
	NSString *path = [pathField stringValue];
	if ([path length] == 0) {
		[editButton setTitle:@"Edit"];
		[editButton setEnabled:NO];
		return;
	}
	
	BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:path];
	BOOL isLua = [[path pathExtension] caseInsensitiveCompare:@"lua"] == NSOrderedSame;
	
	if (exists) {
		[editButton setTitle:isLua ? @"Edit" : @"Open"];
		[editButton setEnabled:YES];
	} else {
		[editButton setTitle:@"Create"];
		[editButton setEnabled:isLua];
	}
}

- (void)controlTextDidChange:(NSNotification *)obj {
	if ([obj object] == pathField) {
		[self onPathChanged];
	}
}

- (void)onBrowseClicked:(id)sender {
	NSOpenPanel *openPanel = [NSOpenPanel openPanel];
	[openPanel setAllowedFileTypes:[NSArray arrayWithObjects:@"lua", nil]];
	
	if (!self.filename.empty()) {
		NSString *dir = [[NSString stringWithUTF8String:self.filename.c_str()] stringByDeletingLastPathComponent];
		[openPanel setDirectoryURL:[NSURL fileURLWithPath:dir]];
	}
	
	[openPanel beginSheetModalForWindow:window completionHandler:^(NSInteger result) {
		if (result == NSModalResponseOK) {
			NSURL *url = [[openPanel URLs] firstObject];
			[pathField setStringValue:[url path]];
			[self onPathChanged];
		}
	}];
}

- (void)onEditClicked:(id)sender {
	NSString *path = [pathField stringValue];
	if ([path length] == 0) return;
	
	BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:path];
	BOOL created = NO;
	
	if (!exists) {
		BOOL isLua = [[path pathExtension] caseInsensitiveCompare:@"lua"] == NSOrderedSame;
		if (isLua) {
			created = [@"" writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:nil];
			exists = created;
		}
	}
	
	if (exists) {
		[[NSWorkspace sharedWorkspace] openFile:path];
	}
	
	if (created) {
		[self onPathChanged];
	}
}

- (void)onRunClicked:(id)sender {
	if (self.filename.empty()) {
		NSString *path = [pathField stringValue];
		if ([path length] == 0) return;
		self.filename = [path UTF8String];
	}
	[self startFileWatcher:[NSString stringWithUTF8String:self.filename.c_str()]];
	if (driver != NULL) {
		((macOS_driver *)driver)->QueueScript(self.uid, self.filename.c_str());
	}
}

- (void)onStopClicked:(id)sender {
	[self appendOutputMainThread:@"user clicked stop button\n"];
	StopLuaScript(self.uid);
}

- (void)startFileWatcher:(NSString *)path {
	[self stopFileWatcher];
	
	int fd = open([path fileSystemRepresentation], O_EVTONLY);
	if (fd < 0) return;
	
	self.watchedFd = fd;
	dispatch_queue_t queue = dispatch_get_main_queue();
	self.watchSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, fd,
											  DISPATCH_VNODE_WRITE | DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME,
											  queue);
	if (!self.watchSource) {
		close(fd);
		self.watchedFd = -1;
		return;
	}
	
	int currentUid = self.uid;
	__block LuaConsoleWindowController *weakSelf = self;
	dispatch_source_set_event_handler(self.watchSource, ^{
		LuaConsoleWindowController *strongSelf = weakSelf;
		if (!strongSelf) return;
		
		if (strongSelf.started && !strongSelf.filename.empty()) {
			RequestAbortLuaScript(currentUid, "terminated to reload the script");
			if (driver != NULL) {
				((macOS_driver *)driver)->QueueScript(currentUid, strongSelf.filename.c_str());
			}
		}
		
		[strongSelf startFileWatcher:[NSString stringWithUTF8String:strongSelf.filename.c_str()]];
	});
	
	dispatch_source_set_cancel_handler(self.watchSource, ^{
		close(fd);
	});
	
	dispatch_resume(self.watchSource);
}

- (void)stopFileWatcher {
	if (watchSource) {
		dispatch_source_cancel(watchSource);
		watchSource = nil;
	}
	self.watchedFd = -1;
}

- (void)appendOutputMainThread:(NSString *)text {
	if ([stdoutCheckbox state] == NSControlStateValueOn) {
		printf("%s", [text UTF8String]);
		fflush(stdout);
		return;
	}
	
	NSTextStorage *storage = [textView textStorage];
	
	if ([storage length] >= 250000) {
		[storage deleteCharactersInRange:NSMakeRange(0, [storage length] / 2)];
	}
	
	[storage beginEditing];
	NSDictionary *attrs = @{
		NSFontAttributeName: [NSFont userFixedPitchFontOfSize:10.0],
		NSForegroundColorAttributeName: [NSColor textColor]
	};
	NSAttributedString *attrStr = [[NSAttributedString alloc] initWithString:text attributes:attrs];
	[storage appendAttributedString:attrStr];
	[storage endEditing];
	
	[textView scrollRangeToVisible:NSMakeRange([storage length], 0)];
}

- (void)onScriptStarted {
	self.started = YES;
	NSTextStorage *storage = [textView textStorage];
	[storage setAttributedString:[[NSAttributedString alloc] initWithString:@""]];
	[runButton setTitle:@"Restart"];
	[runButton setEnabled:YES];
	[stopButton setEnabled:YES];
	[browseButton setEnabled:NO];
}

- (void)onScriptStopped {
	[self appendOutputMainThread:@"script stopped.\n"];
	self.started = NO;
	[runButton setTitle:@"Run"];
	[stopButton setEnabled:NO];
	[browseButton setEnabled:YES];
	
	if (self.closeOnStop) {
		[window close];
	}
}

- (BOOL)windowShouldClose:(id)sender {
	[self appendOutputMainThread:@"user closed script window\n"];
	[self stopFileWatcher];
	StopLuaScript(self.uid);
	
	if (self.started) {
		self.closeOnStop = YES;
		return NO;
	}
	return YES;
}

- (void)windowWillClose:(NSNotification *)notification {
	[self stopFileWatcher];
	CloseLuaContext(self.uid);
	[g_consoles removeObjectForKey:[NSNumber numberWithInt:self.uid]];
}

@end

static void lua_print_cb(int uid, const char *str) {
	LuaConsoleWindowController *con = [g_consoles objectForKey:[NSNumber numberWithInt:uid]];
	if (con) {
		[con performSelectorOnMainThread:@selector(appendOutputMainThread:) withObject:[NSString stringWithUTF8String:str] waitUntilDone:NO];
	}
}

static void lua_onstart_cb(int uid) {
	LuaConsoleWindowController *con = [g_consoles objectForKey:[NSNumber numberWithInt:uid]];
	if (con) {
		[con performSelectorOnMainThread:@selector(onScriptStarted) withObject:nil waitUntilDone:NO];
	}
}

static void lua_onstop_cb(int uid, bool statusOK) {
	LuaConsoleWindowController *con = [g_consoles objectForKey:[NSNumber numberWithInt:uid]];
	if (con) {
		[con performSelectorOnMainThread:@selector(onScriptStopped) withObject:nil waitUntilDone:NO];
	}
}

void lua_script_open_console(void) {
	if (!g_consoles) {
		g_consoles = [[NSMutableDictionary alloc] init];
	}
	
	LuaConsoleWindowController *controller = [[LuaConsoleWindowController alloc] init];
	int uid = g_next_uid++;
	controller.uid = uid;
	
	[g_consoles setObject:controller forKey:[NSNumber numberWithInt:uid]];
	
	OpenLuaContext(uid, lua_print_cb, lua_onstart_cb, lua_onstop_cb);
	
	[controller showWindow];
}

void lua_script_close_all(void) {
	NSArray *controllers = [g_consoles allValues];
	for (NSInteger i = (NSInteger)[controllers count] - 1; i >= 0; i--) {
		LuaConsoleWindowController *controller = controllers[i];
		[[controller window] performClose:nil];
	}
}

// The Lua graphics overlay is shared across threads: the Lua engine draws into
// it on the background emulation core thread, while the OpenGL/Metal renderer
// uploads it as a texture on the display thread. To avoid the renderer reading a
// half-drawn or half-cleared frame, we double-buffer it. The Lua engine always
// draws into the back buffer; once per frame the completed frame is copied to the
// front buffer under a mutex, and the renderer reads only the front buffer (under
// the same mutex).
static uint32_t macLuaGraphicsBufferBack[256 * 384];
static uint32_t macLuaGraphicsBufferFront[256 * 384];
static pthread_mutex_t macLuaGraphicsBufferMutex = PTHREAD_MUTEX_INITIALIZER;

uint32_t* lua_script_get_graphics_buffer(void) {
	return macLuaGraphicsBufferBack;
}

void lua_script_clear_graphics_buffer(void) {
	// Called at the start of a frame, before the deferred GUI draw calls are
	// flushed. Only the core thread touches the back buffer, so no lock is needed.
	memset(macLuaGraphicsBufferBack, 0, sizeof(macLuaGraphicsBufferBack));
}

void lua_script_present_graphics_buffer(void) {
	// Called after the frame's GUI draw calls have been flushed into the back
	// buffer. Publishes the completed frame to the front buffer for the renderer.
	pthread_mutex_lock(&macLuaGraphicsBufferMutex);
	memcpy(macLuaGraphicsBufferFront, macLuaGraphicsBufferBack, sizeof(macLuaGraphicsBufferFront));
	pthread_mutex_unlock(&macLuaGraphicsBufferMutex);
}

uint32_t* lua_script_lock_overlay_buffer(void) {
	pthread_mutex_lock(&macLuaGraphicsBufferMutex);
	return macLuaGraphicsBufferFront;
}

void lua_script_unlock_overlay_buffer(void) {
	pthread_mutex_unlock(&macLuaGraphicsBufferMutex);
}

#endif /* HAVE_LUA */
