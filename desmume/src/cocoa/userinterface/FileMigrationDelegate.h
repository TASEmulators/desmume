/*
	Copyright (C) 2013 DeSmuME team

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

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface FileMigrationDelegate : NSObject <NSWindowDelegate, NSOutlineViewDelegate, NSOutlineViewDataSource>
#else
@interface FileMigrationDelegate : NSObject
#endif
{
	NSObject *dummyObject;
	NSWindow *window;
	NSArrayController *fileListController;
	NSArray *_versionList;
	NSMutableDictionary *_portStringsDict;
	//NSMutableDictionary *_fileTree;
	
	BOOL filesPresent;
}

@property (readonly) IBOutlet NSObject *dummyObject;
@property (readonly) IBOutlet NSWindow *window;
@property (readonly) IBOutlet NSArrayController *fileListController;

@property (assign) BOOL filesPresent;

- (void) updateFileList;
- (IBAction) updateAndShowWindow:(id)sender;
- (IBAction) handleChoice:(id)sender;
- (IBAction) selectAll:(id)sender;
- (IBAction) selectNone:(id)sender;

@end
