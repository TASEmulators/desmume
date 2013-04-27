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

#import "FileMigrationDelegate.h"
#import "cocoa_file.h"
#import "cocoa_globals.h"

@implementation FileMigrationDelegate

@synthesize dummyObject;
@synthesize window;
@synthesize fileListController;
@synthesize filesPresent;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	NSDictionary *fileTypeInfoRootDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"FileTypeInfo" ofType:@"plist"]];
	_versionList = [[NSArray alloc] initWithArray:[fileTypeInfoRootDict valueForKey:@"VersionStrings"]];
	
	NSDictionary *portStrings = (NSDictionary *)[fileTypeInfoRootDict valueForKey:@"PortStrings"];
	_portStringsDict = [[NSMutableDictionary alloc] initWithCapacity:[portStrings count]];
	
	for (NSString *versionKey in portStrings)
	{
		NSArray *portStringsList = (NSArray *)[portStrings valueForKey:versionKey];
		NSMutableArray *newPortStringsList = [NSMutableArray arrayWithCapacity:[portStringsList count]];
		
		for (NSString *portString in portStringsList)
		{
			[newPortStringsList addObject:[NSMutableString stringWithString:portString]];
		}
		
		[_portStringsDict setObject:newPortStringsList forKey:versionKey];
	}
	
	//_fileTree = [[NSMutableDictionary alloc] initWithCapacity:100];
	
	filesPresent = NO;
	
	return self;
}

- (void)dealloc
{
	[_versionList release];
	[_portStringsDict release];
	//[_fileTree release];
	
	[super dealloc];
}

- (void) updateFileList
{
	NSMutableArray *fileList = [CocoaDSFile completeFileList];
	if (fileList != nil)
	{
		[self setFilesPresent:([fileList count] == 0) ? NO : YES];
	}
	
	[fileListController setContent:fileList];
	
	// Refresh the file tree.
	/*
	NSDictionary *fileTypeInfoRootDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"FileTypeInfo" ofType:@"plist"]];
	NSDictionary *defaultPaths = (NSDictionary *)[fileTypeInfoRootDict valueForKey:@"DefaultPaths"];
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	
	[_fileTree removeAllObjects];
	
	for (NSString *versionKey in _versionList)
	{
		NSArray *portStringsList = (NSArray *)[_portStringsDict valueForKey:versionKey];
		if (portStringsList == nil || [portStringsList count] < 1)
		{
			continue;
		}
		
		NSMutableDictionary *newVersionTree = [NSMutableDictionary dictionaryWithCapacity:[portStringsList count]];
		
		for (NSString *portKey in portStringsList)
		{
			NSDictionary *pathsDict = (NSDictionary *)[(NSDictionary *)[defaultPaths valueForKey:versionKey] valueForKey:portKey];
			if (pathsDict == nil || [pathsDict count] < 1)
			{
				continue;
			}
			
			NSMutableArray *newPortTree = [NSMutableArray arrayWithCapacity:128];
			
			for (NSString *pathKey in pathsDict)
			{
				NSString *pathString = (NSString *)[pathsDict valueForKey:pathKey];
				NSArray *fileList = [fileManager contentsOfDirectoryAtPath:pathString error:nil];
				
				for (NSString *filePath in fileList)
				{
					[newPortTree addObject:[NSMutableDictionary dictionaryWithObject:filePath forKey:@"FileName"]];
				}
			}
			
			[newVersionTree setObject:newPortTree forKey:portKey];
		}
		
		[_fileTree setObject:newVersionTree forKey:versionKey];
	}
	
	[fileManager release];
	 */
}

#pragma mark NSOutlineViewDelegate Protocol

- (BOOL)selectionShouldChangeInOutlineView:(NSOutlineView *)outlineView
{
	return NO;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldTrackCell:(NSCell *)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
	if ([(NSString *)[tableColumn identifier] isEqualToString:@"FileWillMigrateColumn"])
	{
		return YES;
	}
	
	return NO;
}

- (NSCell *)outlineView:(NSOutlineView *)outlineView dataCellForTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
	NSString *columnID = (NSString *)[tableColumn identifier];
	NSCell *outCell = [tableColumn dataCellForRow:[outlineView rowForItem:item]];
	
	if ([columnID isEqualToString:@"FileNameColumn"])
	{
		if ([item isKindOfClass:[NSString class]])
		{
			NSFont *newFont = [[NSFontManager sharedFontManager] fontWithFamily:[[outCell font] familyName]
																		 traits:NSBoldFontMask
																		 weight:0
																		   size:[[outCell font] pointSize]];
			[outCell setFont:newFont];
		}
	}
	
	return outCell;
}

#pragma mark NSOutlineViewDataSource Protocol
- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
	if (item == nil)
	{
		return [_versionList objectAtIndex:index];
	}
	else if ([item isKindOfClass:[NSString class]])
	{
		if ([_versionList containsObject:item])
		{
			return [(NSArray *)[_portStringsDict valueForKey:item] objectAtIndex:index];
		}
		else
		{
			NSString *versionKey = (NSString *)[outlineView parentForItem:item];
			//return [(NSArray *)[(NSDictionary *)[_fileTree valueForKey:versionKey] valueForKey:item] objectAtIndex:index];
			return nil;
		}
	}
	
	return nil;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
	if ([item isKindOfClass:[NSString class]])
	{
		return YES;
	}
	
	return NO;
}

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
	NSInteger numberChildren = 0;
	
	if (item == nil)
	{
		numberChildren = [_versionList count];
	}
	else if ([item isKindOfClass:[NSString class]])
	{
		if ([_versionList containsObject:item])
		{
			return [[_portStringsDict valueForKey:item] count];
		}
		else
		{
			NSString *versionKey = (NSString *)[outlineView parentForItem:item];
			//return [(NSArray *)[(NSDictionary *)[_fileTree valueForKey:versionKey] valueForKey:item] count];
			return 0;
		}
	}
	
	return numberChildren;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
	NSString *columnID = (NSString *)[tableColumn identifier];
	
	if ([columnID isEqualToString:@"FileWillMigrateColumn"])
	{
		return nil;
	}
	else if ([columnID isEqualToString:@"FileNameColumn"])
	{
		if ([item isKindOfClass:[NSString class]])
		{
			return item;
		}
		else if ([item isKindOfClass:[NSDictionary class]])
		{
			NSString *fileName = [item valueForKey:@"FileName"];
			return (fileName != nil) ? fileName : @"";
		}
		else
		{
			return @"";
		}
	}
	else if ([columnID isEqualToString:@"FileKindColumn"])
	{
		if ([item isKindOfClass:[NSString class]])
		{
			return @"";
		}
		else if ([item isKindOfClass:[NSDictionary class]])
		{
			return @"";
		}
		else
		{
			return @"";
		}
	}
	else if ([columnID isEqualToString:@"FileDateModifiedColumn"])
	{
		if ([item isKindOfClass:[NSString class]])
		{
			return @"";
		}
		else if ([item isKindOfClass:[NSDictionary class]])
		{
			return @"";
		}
		else
		{
			return @"";
		}
	}
	
	return item;
}

#pragma mark IBActions

- (IBAction) updateAndShowWindow:(id)sender
{
	[self updateFileList];
	[window center];
	[window makeKeyAndOrderFront:nil];
}

- (IBAction) handleChoice:(id)sender
{
	const NSInteger option = [(NSControl *)sender tag];
	NSMutableArray *fileList = [fileListController content];
	
	switch (option)
	{
		case COCOA_DIALOG_DEFAULT:
			[CocoaDSFile copyFileListToCurrent:fileList];
			break;
			
		case COCOA_DIALOG_OPTION:
			[CocoaDSFile moveFileListToCurrent:fileList];
			break;
			
		default:
			break;
	}
	
	[[(NSControl *)sender window] close];
}

- (IBAction) selectAll:(id)sender
{
	NSMutableArray *fileList = [fileListController content];
	
	for (NSMutableDictionary *fileAttr in fileList)
	{
		[fileAttr setValue:[NSNumber numberWithBool:YES] forKey:@"willMigrate"];
	}
}

- (IBAction) selectNone:(id)sender
{
	NSMutableArray *fileList = [fileListController content];
	
	for (NSMutableDictionary *fileAttr in fileList)
	{
		[fileAttr setValue:[NSNumber numberWithBool:NO] forKey:@"willMigrate"];
	}
}

@end
