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

#import "FileMigrationDelegate.h"
#import "cocoa_file.h"
#import "cocoa_globals.h"

@implementation FileMigrationDelegate

@synthesize dummyObject;
@synthesize window;
@synthesize fileTreeOutlineView;
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
	
	_fileTree = [[NSMutableDictionary alloc] initWithCapacity:100];
	_fileTreeSelection = [[NSMutableDictionary alloc] initWithCapacity:1024];
	_fileTreeVersionList = [[NSMutableArray alloc] initWithCapacity:[_versionList count] * [_portStringsDict count]];
	
	filesPresent = NO;
	
	return self;
}

- (void)dealloc
{
	[_versionList release];
	[_portStringsDict release];
	[_fileTree release];
	[_fileTreeSelection release];
	[_fileTreeVersionList release];
	
	[super dealloc];
}

- (void) updateFileList
{
	NSDictionary *fileTypeInfoRootDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"FileTypeInfo" ofType:@"plist"]];
	NSDictionary *defaultPaths = (NSDictionary *)[fileTypeInfoRootDict valueForKey:@"DefaultPaths"];
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	
	[_fileTree removeAllObjects];
	[_fileTreeSelection removeAllObjects];
	[_fileTreeVersionList removeAllObjects];
	
	for (NSString *versionKey in _versionList)
	{
		NSArray *portStringsList = (NSArray *)[_portStringsDict valueForKey:versionKey];
		if (portStringsList == nil || [portStringsList count] < 1)
		{
			continue;
		}
		
		for (NSString *portKey in portStringsList)
		{
			NSDictionary *pathsDict = (NSDictionary *)[(NSDictionary *)[defaultPaths valueForKey:versionKey] valueForKey:portKey];
			if (pathsDict == nil || [pathsDict count] < 1)
			{
				continue;
			}
			
			NSMutableDictionary *newVersionTree = [NSMutableDictionary dictionaryWithCapacity:[pathsDict count]];
			
			for (NSString *kindKey in pathsDict)
			{
				NSMutableArray *fileList = [NSMutableArray arrayWithCapacity:128];
				NSURL *pathURL = [CocoaDSFile directoryURLByKind:kindKey version:versionKey port:portKey];
				[fileList addObjectsFromArray:[CocoaDSFile appFileList:pathURL fileKind:kindKey]];
				
				if ([fileList count] > 0)
				{
					[newVersionTree setObject:fileList forKey:kindKey];
				}
			}
			
			if ([newVersionTree count] > 0)
			{
				NSString *versionAndPortKey = [NSString stringWithFormat:@"%@ %@", versionKey, portKey];
				[_fileTree setObject:newVersionTree forKey:versionAndPortKey];
				[_fileTreeVersionList addObject:versionAndPortKey];
			}
		}
	}
	
	[fileManager release];
	
	[self setFilesPresent:([_fileTree count] > 0)];
	
	NSButton *closeButton = [[self window] standardWindowButton:NSWindowCloseButton];
	[closeButton setEnabled:![self filesPresent]];
	
	[fileTreeOutlineView reloadItem:nil reloadChildren:YES];
}

- (void) setFileSelectionInOutlineView:(NSOutlineView *)outlineView file:(NSMutableDictionary *)fileItem isSelected:(BOOL)isSelected
{
	[fileItem setValue:[NSNumber numberWithBool:isSelected] forKey:@"willMigrate"];
	NSString *fileName = (NSString *)[fileItem objectForKey:@"name"];
	
	if (isSelected)
	{
		NSMutableDictionary *oldSelection = (NSMutableDictionary *)[_fileTreeSelection objectForKey:fileName];
		
		if (oldSelection != nil && oldSelection != fileItem)
		{
			[oldSelection setValue:[NSNumber numberWithBool:NO] forKey:@"willMigrate"];
			[outlineView reloadItem:oldSelection];
			[outlineView reloadItem:[outlineView parentForItem:oldSelection]];
			[outlineView reloadItem:[outlineView parentForItem:[outlineView parentForItem:oldSelection]]];
		}
		
		[_fileTreeSelection setValue:fileItem forKey:fileName];
	}
	else
	{
		[_fileTreeSelection removeObjectForKey:fileName];
	}
}

+ (NSMutableDictionary *) fileItemFromURL:(NSURL *)fileURL fileManager:(NSFileManager *)fileManager
{
	if (fileURL == nil || fileManager == nil)
	{
		return nil;
	}
	
	NSNumber *willMigrate = [NSNumber numberWithBool:YES];
	NSString *filePath = [fileURL path];
	NSString *fileName = [filePath lastPathComponent];
	NSString *fileVersion = [CocoaDSFile fileVersionByURL:fileURL];
	NSString *fileKind = [CocoaDSFile fileKindByURL:fileURL];
	NSDictionary *fileAttributes = [fileManager attributesOfItemAtPath:filePath error:nil];
	
	return [NSMutableDictionary dictionaryWithObjectsAndKeys:
			willMigrate, @"willMigrate",
			fileName, @"name",
			fileVersion, @"version",
			fileKind, @"kind",
			[fileAttributes fileModificationDate], @"dateModified",
			[fileURL absoluteString], @"URL",
			nil];
}

- (void) moveSelectedFilesToCurrent
{
	NSArray *fileList = [_fileTreeSelection allKeys];
	for (NSString *fileName in fileList)
	{
		NSDictionary *fileItem = (NSDictionary *)[_fileTreeSelection objectForKey:fileName];
		const BOOL willMigrate = [[fileItem valueForKey:@"willMigrate"] boolValue];
		
		if (!willMigrate)
		{
			continue;
		}
		
		NSURL *fileURL = [NSURL URLWithString:[fileItem valueForKey:@"URL"]];
		[CocoaDSFile moveFileToCurrentDirectory:fileURL];
	}
}

- (void) copySelectedFilesToCurrent
{
	NSArray *fileList = [_fileTreeSelection allKeys];
	for (NSString *fileName in fileList)
	{
		NSDictionary *fileItem = (NSDictionary *)[_fileTreeSelection objectForKey:fileName];
		const BOOL willMigrate = [[fileItem valueForKey:@"willMigrate"] boolValue];
		
		if (!willMigrate)
		{
			continue;
		}
		
		NSURL *fileURL = [NSURL URLWithString:[fileItem valueForKey:@"URL"]];
		[CocoaDSFile copyFileToCurrentDirectory:fileURL];
	}
}

#pragma mark NSOutlineViewDelegate Protocol

- (BOOL)selectionShouldChangeInOutlineView:(NSOutlineView *)outlineView
{
	return NO;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldTrackCell:(NSCell *)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
	if ([(NSString *)[tableColumn identifier] isEqualToString:@"FileNameColumn"])
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
		NSFont *newFont = [[NSFontManager sharedFontManager] fontWithFamily:[[outCell font] familyName]
																	 traits:([item isKindOfClass:[NSString class]]) ? NSBoldFontMask : 0
																	 weight:5
																	   size:[[outCell font] pointSize]];
		[outCell setFont:newFont];
		
		if ([item isKindOfClass:[NSString class]])
		{
			[outCell setTitle:(NSString *)item];
			[outCell setAllowsMixedState:YES];
		}
		else if ([item isKindOfClass:[NSArray class]])
		{
			NSString *parentItem = [outlineView parentForItem:item];
			NSArray *allKeys = [(NSDictionary *)[_fileTree objectForKey:parentItem] allKeysForObject:item];
			[outCell setTitle:(NSString *)[allKeys objectAtIndex:0]];
			[outCell setAllowsMixedState:YES];
		}
		else if ([item isKindOfClass:[NSDictionary class]])
		{
			[outCell setTitle:(NSString *)[(NSDictionary *)item objectForKey:@"name"]];
			[outCell setAllowsMixedState:NO];
		}
	}
	
	return outCell;
}

#pragma mark NSOutlineViewDataSource Protocol
- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
	if ([item isKindOfClass:[NSString class]])
	{
		NSArray *versionKeys = [(NSDictionary *)[_fileTree objectForKey:(NSString *)item] allKeys];
		return [(NSDictionary *)[_fileTree objectForKey:(NSString *)item] objectForKey:[versionKeys objectAtIndex:index]];
	}
	else if ([item isKindOfClass:[NSArray class]])
	{
		return [(NSArray *)item objectAtIndex:index];
	}
	
	return [_fileTreeVersionList objectAtIndex:index];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
	if ([item isKindOfClass:[NSString class]] || [item isKindOfClass:[NSArray class]])
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
		numberChildren = [_fileTreeVersionList count];
	}
	else if ([item isKindOfClass:[NSString class]])
	{
		numberChildren = [(NSDictionary *)[_fileTree objectForKey:(NSString *)item] count];
	}
	else if ([item isKindOfClass:[NSArray class]])
	{
		numberChildren = [(NSArray *)item count];
	}
	
	return numberChildren;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
	NSString *columnID = (NSString *)[tableColumn identifier];
	
	if ([columnID isEqualToString:@"FileNameColumn"])
	{
		if ([item isKindOfClass:[NSString class]])
		{
			NSString *parentKey = [outlineView parentForItem:item];
			
			if (parentKey == nil)
			{
				NSDictionary *versionDict = (NSDictionary *)[_fileTree objectForKey:(NSString *)item];
				NSArray *kindKeyList = [versionDict allKeys];
				NSUInteger kindCount = [kindKeyList count];
				NSUInteger willMigrateKindCount = 0;
				
				for (NSString *kindKey in kindKeyList)
				{
					NSArray *fileList = (NSArray *)[versionDict objectForKey:kindKey];
					NSUInteger fileCount = [fileList count];
					NSUInteger willMigrateFileCount = 0;
					
					for (NSDictionary *fileAttr in fileList)
					{
						if ([(NSNumber *)[fileAttr objectForKey:@"willMigrate"] boolValue])
						{
							willMigrateFileCount++;
						}
						else
						{
							if (willMigrateFileCount > 0)
							{
								return [NSNumber numberWithInteger:NSMixedState];
							}
						}
					}
					
					if (willMigrateFileCount == 0)
					{
						if (willMigrateKindCount > 0)
						{
							return [NSNumber numberWithInteger:NSMixedState];
						}
					}
					else if (willMigrateFileCount == fileCount)
					{
						willMigrateKindCount++;
					}
					else
					{
						return [NSNumber numberWithInteger:NSMixedState];
					}
				}
				
				if (willMigrateKindCount == 0)
				{
					return [NSNumber numberWithInteger:NSOffState];
				}
				else if (willMigrateKindCount == kindCount)
				{
					return [NSNumber numberWithInteger:NSOnState];
				}
				else
				{
					return [NSNumber numberWithInteger:NSMixedState];
				}
			}
		}
		else if ([item isKindOfClass:[NSArray class]])
		{
			NSArray *fileList = (NSArray *)item;
			NSUInteger fileCount = [fileList count];
			NSUInteger willMigrateFileCount = 0;
			
			for (NSDictionary *fileAttr in fileList)
			{
				if ([(NSNumber *)[fileAttr objectForKey:@"willMigrate"] boolValue])
				{
					willMigrateFileCount++;
				}
				else
				{
					if (willMigrateFileCount > 0)
					{
						return [NSNumber numberWithInteger:NSMixedState];
					}
				}
			}
			
			if (willMigrateFileCount == 0)
			{
				return [NSNumber numberWithInteger:NSOffState];
			}
			else if (willMigrateFileCount == fileCount)
			{
				return [NSNumber numberWithInteger:NSOnState];
			}
			else
			{
				return [NSNumber numberWithInteger:NSMixedState];
			}
		}
		else if ([item isKindOfClass:[NSDictionary class]])
		{
			return [(NSDictionary *)item objectForKey:@"willMigrate"];
		}
		else
		{
			return @"";
		}
	}
	else if ([columnID isEqualToString:@"FileKindColumn"])
	{
		if ([item isKindOfClass:[NSDictionary class]])
		{
			return [(NSDictionary *)item objectForKey:@"kind"];
		}
		else
		{
			return @"";
		}
	}
	else if ([columnID isEqualToString:@"FileDateModifiedColumn"])
	{
		if ([item isKindOfClass:[NSDictionary class]])
		{
			return [(NSDictionary *)item objectForKey:@"dateModified"];
		}
		else
		{
			return @"";
		}
	}
	
	return item;
}

- (void)outlineView:(NSOutlineView *)outlineView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
	NSString *columnID = (NSString *)[tableColumn identifier];
	NSCell *itemCell = [tableColumn dataCellForRow:[outlineView rowForItem:item]];
	
	if ([columnID isEqualToString:@"FileNameColumn"])
	{
		if ([item isKindOfClass:[NSString class]])
		{
			NSMutableDictionary *versionDict = (NSMutableDictionary *)[_fileTree objectForKey:(NSString *)item];
			const BOOL newSelectState = !([itemCell state] == NSOnState);
			
			for (NSString *kindKey in versionDict)
			{
				NSArray *fileList = (NSArray *)[versionDict objectForKey:kindKey];
				
				if ([outlineView isItemExpanded:item])
				{
					if ([outlineView isItemExpanded:fileList])
					{
						for (NSMutableDictionary *fileAttr in fileList)
						{
							[self setFileSelectionInOutlineView:outlineView file:fileAttr isSelected:newSelectState];
							[outlineView reloadItem:fileAttr];
						}
					}
					else
					{
						for (NSMutableDictionary *fileAttr in fileList)
						{
							[self setFileSelectionInOutlineView:outlineView file:fileAttr isSelected:newSelectState];
						}
					}
					
					[outlineView reloadItem:fileList];
				}
				else
				{
					for (NSMutableDictionary *fileAttr in fileList)
					{
						[self setFileSelectionInOutlineView:outlineView file:fileAttr isSelected:newSelectState];
					}
				}
			}
		}
		else if ([item isKindOfClass:[NSArray class]])
		{
			const BOOL newSelectState = !([itemCell state] == NSOnState);
			NSArray *fileList = (NSArray *)item;
			
			if ([outlineView isItemExpanded:item])
			{
				for (NSMutableDictionary *fileAttr in fileList)
				{
					[self setFileSelectionInOutlineView:outlineView file:fileAttr isSelected:newSelectState];
					[outlineView reloadItem:fileAttr];
				}
			}
			else
			{
				for (NSMutableDictionary *fileAttr in fileList)
				{
					[self setFileSelectionInOutlineView:outlineView file:fileAttr isSelected:newSelectState];
				}
			}
			
			[outlineView reloadItem:[outlineView parentForItem:item]];
		}
		else if ([item isKindOfClass:[NSDictionary class]])
		{
			const BOOL newSelectState = !([itemCell state] == NSOnState);
			[self setFileSelectionInOutlineView:outlineView file:(NSMutableDictionary *)item isSelected:newSelectState];
			[outlineView reloadItem:[outlineView parentForItem:item]];
			[outlineView reloadItem:[outlineView parentForItem:[outlineView parentForItem:item]]];
		}
	}
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
	
	switch (option)
	{
		case COCOA_DIALOG_DEFAULT:
			[self copySelectedFilesToCurrent];
			break;
			
		case COCOA_DIALOG_OPTION:
			[self moveSelectedFilesToCurrent];
			break;
			
		default:
			break;
	}
	
	[[(NSControl *)sender window] close];
}

@end
