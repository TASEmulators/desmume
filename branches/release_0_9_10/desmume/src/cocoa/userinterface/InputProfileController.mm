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

#import "InputProfileController.h"
#import "cocoa_globals.h"


@implementation InputProfileController

@synthesize dummyObject;
@synthesize inputManager;
@synthesize profileOutlineView;

- (id)init
{
    self = [super init];
    if (self == nil)
	{
		return self;
    }
	
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (NSString *) commandTagFromInputList:(NSArray *)inputList
{
	NSDictionary *mappings = [(NSDictionary *)[self content] valueForKey:@"Mappings"];
	NSString *commandTag = nil;
	if (inputList == nil)
	{
		return commandTag;
	}
	
	for (NSString *tag in mappings)
	{
		if (inputList == [mappings valueForKey:tag])
		{
			commandTag = tag;
			break;
		}
	}
	
	return commandTag;
}

#pragma mark NSOutlineViewDelegate Protocol

- (BOOL)selectionShouldChangeInOutlineView:(NSOutlineView *)outlineView
{
	return NO;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldTrackCell:(NSCell *)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
	if ([(NSString *)[tableColumn identifier] isEqualToString:@"InputCommandTagColumn"])
	{
		return YES;
	}
	
	return NO;
}

- (NSCell *)outlineView:(NSOutlineView *)outlineView dataCellForTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
	NSString *columnID = (NSString *)[tableColumn identifier];
	NSCell *outCell = [tableColumn dataCellForRow:[outlineView rowForItem:item]];
	
	if ([columnID isEqualToString:@"InputCommandIconColumn"])
	{
		if ([item isKindOfClass:[NSArray class]])
		{
			NSString *commandTag = [self commandTagFromInputList:item];
			if (commandTag != nil)
			{
				NSImage *iconImage = (NSImage *)[[inputManager commandIcon] valueForKey:commandTag];
				if (iconImage == nil)
				{
					iconImage = (NSImage *)[[inputManager commandIcon] valueForKey:@"UNKNOWN COMMAND"];
				}
				
				[outCell setImage:iconImage];
			}
			else
			{
				outCell = [[[NSCell alloc] init] autorelease];
			}
		}
		else
		{
			outCell = [[[NSCell alloc] init] autorelease];
		}
	}
	else if ([columnID isEqualToString:@"InputDeviceColumn"])
	{
		NSFont *newFont = [[NSFontManager sharedFontManager] fontWithFamily:[[outCell font] familyName]
																	 traits:([item isKindOfClass:[NSArray class]]) ? NSBoldFontMask : 0
																	 weight:0
																	   size:[[outCell font] pointSize]];
		[outCell setFont:newFont];
	}
	else if ([columnID isEqualToString:@"InputSettingsSummaryColumn"])
	{
		NSFont *newFont = [[NSFontManager sharedFontManager] fontWithFamily:[[outCell font] familyName]
																	 traits:([item isKindOfClass:[NSArray class]]) ? NSBoldFontMask : 0
																	 weight:0
																	   size:[[outCell font] pointSize]];
		[outCell setFont:newFont];
	}
	
	return outCell;
}

#pragma mark NSOutlineViewDataSource Protocol
- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
	if (item == nil)
	{
		NSDictionary *mappings = [(NSDictionary *)[self content] valueForKey:@"Mappings"];
		NSString *commandTag = [[inputManager commandTagList] objectAtIndex:index];
		return [mappings valueForKey:commandTag];
	}
	else if ([item isKindOfClass:[NSArray class]])
	{
		return [(NSArray *)item objectAtIndex:index];
	}
	
	return nil;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
	if ([item isKindOfClass:[NSArray class]])
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
		numberChildren = [[inputManager commandTagList] count];
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
	
	if ([columnID isEqualToString:@"InputCommandIconColumn"])
	{
		NSString *commandTag = [self commandTagFromInputList:item];
		if (commandTag != nil)
		{
			NSImage *iconImage = (NSImage *)[[inputManager commandIcon] valueForKey:commandTag];
			if (iconImage == nil)
			{
				iconImage = (NSImage *)[[inputManager commandIcon] valueForKey:@"UNKNOWN COMMAND"];
			}
			
			return iconImage;
		}
		else
		{
			return nil;
		}
	}
	else if ([columnID isEqualToString:@"InputCommandTagColumn"])
	{
		if ([item isKindOfClass:[NSArray class]])
		{
			NSString *tag = [self commandTagFromInputList:item];
			return tag;
		}
		else
		{
			return @"";
		}
	}
	else if ([columnID isEqualToString:@"InputDeviceColumn"])
	{
		if ([item isKindOfClass:[NSArray class]])
		{
			const unsigned long inputCount = (unsigned long)[(NSArray *)item count];
			return [NSString stringWithFormat:(inputCount != 1) ? NSSTRING_INPUTPREF_NUM_INPUTS_MAPPED_PLURAL : NSSTRING_INPUTPREF_NUM_INPUTS_MAPPED, inputCount];
		}
		else if ([item isKindOfClass:[NSDictionary class]])
		{
			return [item valueForKey:@"deviceInfoSummary"];
		}
		
		return @"";
	}
	else if ([columnID isEqualToString:@"InputSettingsSummaryColumn"])
	{
		NSString *settingsSummary = @"";
		
		if ([item isKindOfClass:[NSDictionary class]])
		{
			settingsSummary = [item valueForKey:@"inputSettingsSummary"];
		}
		
		return settingsSummary;
	}
	
	return item;
}

@end
