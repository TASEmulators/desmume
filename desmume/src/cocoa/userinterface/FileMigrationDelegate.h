/*
	Copyright (C) 2013-2015 DeSmuME team

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
	NSOutlineView *fileTreeOutlineView;
	NSArray *_versionList;
	NSMutableDictionary *_portStringsDict;
	NSMutableDictionary *_fileTree;
	NSMutableDictionary *_fileTreeSelection;
	NSMutableArray *_fileTreeVersionList;
	
	BOOL filesPresent;
}

@property (readonly) IBOutlet NSObject *dummyObject;
@property (readonly) IBOutlet NSWindow *window;
@property (readonly) IBOutlet NSOutlineView *fileTreeOutlineView;

@property (assign) BOOL filesPresent;

- (void) updateFileList;
- (void) setFileSelectionInOutlineView:(NSOutlineView *)outlineView file:(NSMutableDictionary *)fileItem isSelected:(BOOL)isSelected;
- (void) moveSelectedFilesToCurrent;
- (void) copySelectedFilesToCurrent;
- (IBAction) updateAndShowWindow:(id)sender;
- (IBAction) handleChoice:(id)sender;

@end
