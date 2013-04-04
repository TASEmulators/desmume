/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2013 DeSmuME team

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

#import "inputPrefsView.h"
#import "InputProfileController.h"
#import "preferencesWindowDelegate.h"

#import "cocoa_globals.h"
#import "cocoa_util.h"

#define INPUT_HOLD_TIME		0.1		// Time in seconds to hold a button in its on state when mapping an input.


@implementation InputPrefsView

@synthesize dummyObject;
@synthesize prefWindow;
@synthesize inputProfileMenu;
@synthesize inputProfilePreviousButton;
@synthesize inputProfileNextButton;
@synthesize inputPrefOutlineView;
@synthesize inputSettingsController;
@synthesize inputProfileController;
@synthesize inputSettingsMicrophone;
@synthesize inputSettingsTouch;
@synthesize inputSettingsLoadStateSlot;
@synthesize inputSettingsSaveStateSlot;
@synthesize inputSettingsSetSpeedLimit;
@synthesize inputSettingsGPUState;
@synthesize inputProfileSheet;
@synthesize inputProfileRenameSheet;
@synthesize inputManager;
@dynamic configInputTargetID;

- (id)initWithFrame:(NSRect)frame
{
	self = [super initWithFrame:frame];
    if (self == nil)
	{
		return self;
    }
	
	NSDictionary *defaultKeyMappingsDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultKeyMappings" ofType:@"plist"]];
	NSArray *defaultProfileList = [defaultKeyMappingsDict valueForKey:@"DefaultInputProfiles"];
	_defaultProfileListCount = [defaultProfileList count];
	
	NSArray *userDefaultsSavedProfilesList = (NSArray *)[[NSUserDefaults standardUserDefaults] arrayForKey:@"Input_SavedProfiles"];
	savedProfilesList = [[NSMutableArray alloc] initWithCapacity:32];
	
	for (NSDictionary *savedProfile in userDefaultsSavedProfilesList)
	{
		[savedProfilesList addObject:[NSMutableDictionary dictionaryWithDictionary:savedProfile]];
	}
	
	configInputTargetID = nil;
	configInputList = [[NSMutableDictionary alloc] initWithCapacity:128];
		
    return self;
}

- (void)dealloc
{
	[configInputList release];
	[inputSettingsMappings release];
	[savedProfilesList release];
	
	[super dealloc];
}

#pragma mark Dynamic Properties
- (void) setConfigInputTargetID:(NSString *)targetID
{
	if (targetID == nil)
	{
		[configInputTargetID release];
	}
	
	configInputTargetID = [targetID retain];
	[[self inputManager] setHidInputTarget:(targetID == nil) ? nil : self];
}

- (NSString *) configInputTargetID
{
	return configInputTargetID;
}

#pragma mark Class Methods

- (void) initSettingsSheets
{
	inputSettingsMappings = [[NSDictionary alloc] initWithObjectsAndKeys:
							 inputSettingsMicrophone,		@"Microphone",
							 inputSettingsTouch,			@"Touch",
							 inputSettingsLoadStateSlot,	@"Load State Slot",
							 inputSettingsSaveStateSlot,	@"Save State Slot",
							 inputSettingsSetSpeedLimit,	@"Set Speed",
							 inputSettingsGPUState,			@"Enable/Disable GPU State",
							 nil];
}

- (void) populateInputProfileMenu
{
	NSDictionary *defaultKeyMappingsDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultKeyMappings" ofType:@"plist"]];
	NSArray *defaultProfileList = [defaultKeyMappingsDict valueForKey:@"DefaultInputProfiles"];
	if (defaultKeyMappingsDict == nil || defaultProfileList == nil)
	{
		return;
	}
	
	// We're going to populate this menu from scratch, so remove all existing profile items.
	NSMenu *profileMenu = [inputProfileMenu menu];
	NSInteger menuItemCount = [profileMenu numberOfItems] - 2;
	for (NSInteger i = 0; i < menuItemCount; i++)
	{
		[profileMenu removeItemAtIndex:i];
	}
	
	// Need this to keep track of profile indexes.
	NSInteger profileIndex = 0;
	
	// Add the menu items for the default profiles.
	for (NSUInteger i = 0; i < _defaultProfileListCount; i++)
	{
		NSDictionary *profileDict = [defaultProfileList objectAtIndex:i];
		NSString *profileName = (NSString *)[profileDict valueForKey:@"Name"];
		if (profileName == nil)
		{
			profileName = @"";
		}
		
		NSMenuItem *newProfileMenuItem = [[[NSMenuItem alloc] initWithTitle:profileName
																	 action:@selector(profileSelect:)
															  keyEquivalent:@""] autorelease];
		[newProfileMenuItem setTag:profileIndex];
		[newProfileMenuItem setTarget:self];
		
		[profileMenu insertItem:newProfileMenuItem atIndex:profileIndex];
		profileIndex++;
	}
	
	// Add a separator item in between default profiles and user saved profiles.
	[[inputProfileMenu menu] insertItem:[NSMenuItem separatorItem] atIndex:profileIndex];
	
	// Add the menu items for the user saved profiles.
	if ([savedProfilesList count] < 1)
	{
		[inputProfileMenu insertItemWithTitle:NSSTRING_INPUTPREF_NO_SAVED_PROFILES atIndex:profileIndex+1];
		NSMenuItem *noSavedConfigItem = [inputProfileMenu itemAtIndex:profileIndex+1];
		[noSavedConfigItem setEnabled:NO];
	}
	else
	{
		for (NSUInteger i = 0; i < [savedProfilesList count]; i++)
		{
			NSDictionary *profileDict = [savedProfilesList objectAtIndex:i];
			NSString *profileName = (NSString *)[profileDict valueForKey:@"Name"];
			if (profileName == nil)
			{
				profileName = @"";
			}
			
			NSMenuItem *newProfileMenuItem = [[[NSMenuItem alloc] initWithTitle:profileName
																		 action:@selector(profileSelect:)
																  keyEquivalent:@""] autorelease];
			[newProfileMenuItem setTag:profileIndex];
			[newProfileMenuItem setTarget:self];
			
			[profileMenu insertItem:newProfileMenuItem atIndex:profileIndex+1];
			profileIndex++;
		}
	}
	
	[inputProfileMenu selectItemAtIndex:0];
	[self profileSelect:[inputProfileMenu itemAtIndex:0]];
}

- (BOOL) handleKeyboardEvent:(NSEvent *)theEvent keyPressed:(BOOL)keyPressed
{
	const InputAttributes inputAttr = InputManagerEncodeKeyboardInput([theEvent keyCode], keyPressed);
	return [self addMappingUsingInputAttributes:&inputAttr commandTag:[self configInputTargetID]];
}

- (BOOL) handleMouseButtonEvent:(NSEvent *)mouseEvent buttonPressed:(BOOL)buttonPressed
{
	const InputAttributes inputAttr = InputManagerEncodeMouseButtonInput([mouseEvent buttonNumber], NSMakePoint(0.0f, 0.0f), buttonPressed);
	return [self addMappingUsingInputAttributes:&inputAttr commandTag:[self configInputTargetID]];
}

- (BOOL) addMappingUsingInputAttributes:(const InputAttributes *)inputAttr commandTag:(NSString *)commandTag
{
	BOOL didMap = NO;
	
	if (commandTag == nil)
	{
		return didMap;
	}
	
	const char *cmdTag = [commandTag cStringUsingEncoding:NSUTF8StringEncoding];
	if (cmdTag == NULL)
	{
		return didMap;
	}
	
	// Add the input mapping.
	const CommandAttributes cmdAttr = [inputManager defaultCommandAttributesForCommandTag:cmdTag];
	[inputManager addMappingUsingInputAttributes:inputAttr commandAttributes:&cmdAttr];
	[inputManager writeDefaultsInputMappings];
	
	// Deselect the row of the command tag.
	NSDictionary *inputMappings = [inputManager inputMappings];
	NSArray *mappingList = (NSArray *)[inputMappings valueForKey:[self configInputTargetID]];
	const NSInteger rowNumber = [inputPrefOutlineView rowForItem:mappingList];
	if (rowNumber != -1)
	{
		[inputPrefOutlineView deselectRow:rowNumber];
	}
	
	// Update all expanded command tags.
	for (NSString *tag in inputMappings)
	{
		NSArray *inputList = (NSArray *)[inputMappings valueForKey:tag];
		if ([inputPrefOutlineView isItemExpanded:inputList])
		{
			[inputPrefOutlineView reloadItem:inputList reloadChildren:YES];
		}
	}
	
	[self setConfigInputTargetID:nil];
	
	didMap = YES;
	return didMap;
}

- (void) setMappingUsingDeviceInfoDictionary:(NSMutableDictionary *)deviceInfo
{
	if (deviceInfo == nil)
	{
		return;
	}
	
	NSString *deviceCode = (NSString *)[deviceInfo valueForKey:@"deviceCode"];
	NSString *elementCode = (NSString *)[deviceInfo valueForKey:@"elementCode"];
	const char *devCode = [deviceCode cStringUsingEncoding:NSUTF8StringEncoding];
	const char *elCode = [elementCode cStringUsingEncoding:NSUTF8StringEncoding];
	
	CommandAttributes cmdAttr = [inputManager mappedCommandAttributesOfDeviceCode:devCode elementCode:elCode];
	UpdateCommandAttributesWithDeviceInfoDictionary(&cmdAttr, deviceInfo);
	[inputManager updateInputSettingsSummaryInDeviceInfoDictionary:deviceInfo commandTag:cmdAttr.tag];
	[inputManager setMappedCommandAttributes:&cmdAttr deviceCode:devCode elementCode:elCode];
	[inputManager writeDefaultsInputMappings];
}

- (BOOL) doesProfileNameExist:(NSString *)profileName
{
	BOOL doesExist = NO;
	
	for (NSMutableDictionary *savedProfile in savedProfilesList)
	{
		NSString *savedProfileName = (NSString *)[savedProfile valueForKey:@"Name"];
		if ([savedProfileName isEqualToString:profileName])
		{
			doesExist = YES;
			break;
		}
	}
	
	return doesExist;
}

- (void) updateSelectedProfileName
{
	NSInteger profileID = [inputProfileMenu indexOfSelectedItem];
	NSMutableDictionary *selectedProfile = (NSMutableDictionary *)[inputProfileController content];
	if (profileID < (NSInteger)_defaultProfileListCount || selectedProfile == nil)
	{
		return;
	}
	
	NSString *profileName = (NSString *)[selectedProfile valueForKey:@"Name"];
	if (profileName == nil)
	{
		profileName = @"";
	}
	
	[[inputProfileMenu selectedItem] setTitle:profileName];
	[[NSUserDefaults standardUserDefaults] setObject:savedProfilesList forKey:@"Input_SavedProfiles"];
}

- (void) didEndSettingsSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
    [sheet orderOut:self];
	
	NSOutlineView *outlineView = (NSOutlineView *)contextInfo;
	NSMutableDictionary *deviceInfo = (NSMutableDictionary *)[inputSettingsController content];
	
	switch (returnCode)
	{
		case NSCancelButton:
			break;
			
		case NSOKButton:
			[self setMappingUsingDeviceInfoDictionary:deviceInfo];
			[outlineView reloadItem:deviceInfo reloadChildren:NO];
			break;
			
		default:
			break;
	}
	
	[inputSettingsController setContent:nil];
}

- (void) didEndProfileSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
}

- (void) didEndProfileRenameSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
}

#pragma mark InputHIDManagerTarget Protocol
- (BOOL) handleHIDQueue:(IOHIDQueueRef)hidQueue
{
	BOOL isHandled = NO;
	
	if ([self configInputTargetID] == nil)
	{
		ClearHIDQueue(hidQueue);
		return isHandled;
	}
	
	InputAttributesList inputList = InputManagerEncodeHIDQueue(hidQueue);
	const size_t inputCount = inputList.size();
	
	for (unsigned int i = 0; i < inputCount; i++)
	{
		const InputAttributes &inputAttr = inputList[i];
		char inputKey[INPUT_HANDLER_STRING_LENGTH*2];
		strlcpy(inputKey, inputAttr.deviceCode, INPUT_HANDLER_STRING_LENGTH*2);
		strlcat(inputKey, ":", INPUT_HANDLER_STRING_LENGTH*2);
		strlcat(inputKey, inputAttr.elementCode, INPUT_HANDLER_STRING_LENGTH*2);
		
		NSString *inputKeyStr = [NSString stringWithCString:inputKey encoding:NSUTF8StringEncoding];
		NSDate *inputOnDate = [configInputList valueForKey:inputKeyStr];
		
		if (inputAttr.state == INPUT_ATTRIBUTE_STATE_ON)
		{
			if (inputOnDate == nil)
			{
				[configInputList setValue:[NSDate date] forKey:inputKeyStr];
			}
		}
		else
		{
			if (inputOnDate != nil)
			{
				if (([inputOnDate timeIntervalSinceNow] * -1.0) < INPUT_HOLD_TIME)
				{
					// If the button isn't held for at least INPUT_HOLD_TIME seconds, then reject the input.
					[configInputList setValue:nil forKey:inputKeyStr];
				}
				else
				{
					isHandled = [self addMappingUsingInputAttributes:&inputAttr commandTag:[self configInputTargetID]];
					break;
				}
			}
		}
	}
	
	isHandled = YES;
	return isHandled;
}

#pragma mark NSOutlineViewDelegate Protocol

- (BOOL)selectionShouldChangeInOutlineView:(NSOutlineView *)outlineView
{
	return NO;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldTrackCell:(NSCell *)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
	if ([(NSString *)[tableColumn identifier] isEqualToString:@"InputCommandTagColumn"] ||
		[(NSString *)[tableColumn identifier] isEqualToString:@"InputSettingsColumn"] ||
		[(NSString *)[tableColumn identifier] isEqualToString:@"RemoveInputColumn"])
	{
		return YES;
	}
	
	return NO;
}

- (NSCell *)outlineView:(NSOutlineView *)outlineView dataCellForTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
	NSString *columnID = (NSString *)[tableColumn identifier];
	NSCell *outCell = [tableColumn dataCellForRow:[outlineView rowForItem:item]];
	
	if ([columnID isEqualToString:@"InputCommandTagColumn"])
	{
		if ([item isKindOfClass:[NSArray class]])
		{
			NSString *commandTag = [inputManager commandTagFromInputList:item];
			if (commandTag != nil)
			{
				NSImage *buttonImage = (NSImage *)[[inputManager commandIcon] valueForKey:commandTag];
				if (buttonImage == nil)
				{
					buttonImage = (NSImage *)[[inputManager commandIcon] valueForKey:@"UNKNOWN COMMAND"];
				}
				
				[outCell setTitle:NSLocalizedString(commandTag, nil)];
				[outCell setImage:buttonImage];
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
	else if ([columnID isEqualToString:@"InputSettingsColumn"])
	{
		if ([item isKindOfClass:[NSDictionary class]])
		{
			NSString *commandTag = [inputManager commandTagFromInputList:[outlineView parentForItem:item]];
			NSWindow *theSheet = (NSWindow *)[inputSettingsMappings valueForKey:commandTag];
			[outCell setEnabled:(theSheet == nil) ? NO : YES];
		}
		else
		{
			outCell = [[[NSCell alloc] init] autorelease];
		}
	}
	else if ([columnID isEqualToString:@"RemoveInputColumn"])
	{
		if (![item isKindOfClass:[NSDictionary class]])
		{
			outCell = [[[NSCell alloc] init] autorelease];
		}
	}
	
	return outCell;
}

#pragma mark NSOutlineViewDataSource Protocol
- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
	if (item == nil)
	{
		NSString *commandTag = [[inputManager commandTagList] objectAtIndex:index];
		return [[inputManager inputMappings] valueForKey:commandTag];
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
	
	if ([columnID isEqualToString:@"InputCommandTagColumn"])
	{
		if ([item isKindOfClass:[NSDictionary class]])
		{
			return nil;
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
	else if ([columnID isEqualToString:@"InputSettingsColumn"])
	{
		return nil;
	}
	else if ([columnID isEqualToString:@"RemoveInputColumn"])
	{
		return nil;
	}
	
	return item;
}

- (id)outlineView:(NSOutlineView *)outlineView persistentObjectForItem:(id)item
{
	return [inputManager commandTagFromInputList:item];
}

- (id)outlineView:(NSOutlineView *)outlineView itemForPersistentObject:(id)object
{
	NSString *commandTag = (NSString *)object;
	NSArray *inputList = (NSArray *)[[inputManager inputMappings] valueForKey:commandTag];
	
	return inputList;
}

#pragma mark NSResponder Methods

- (void)keyDown:(NSEvent *)theEvent
{
	BOOL isHandled = [self handleKeyboardEvent:theEvent keyPressed:YES];
	if (!isHandled)
	{
		[super keyDown:theEvent];
	}
}

- (void)mouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [self handleMouseButtonEvent:theEvent buttonPressed:YES];
	if (!isHandled)
	{
		[super mouseDown:theEvent];
	}
}

- (void)mouseDragged:(NSEvent *)theEvent
{
	[self mouseDown:theEvent];
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [self handleMouseButtonEvent:theEvent buttonPressed:YES];
	if (!isHandled)
	{
		[super rightMouseDown:theEvent];
	}
}

- (void)rightMouseDragged:(NSEvent *)theEvent
{
	[self rightMouseDown:theEvent];
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [self handleMouseButtonEvent:theEvent buttonPressed:YES];
	if (!isHandled)
	{
		[super otherMouseDown:theEvent];
	}
}

- (void)otherMouseDragged:(NSEvent *)theEvent
{
	[self otherMouseDown:theEvent];
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (BOOL)becomeFirstResponder
{
	return YES;
}

- (BOOL)resignFirstResponder
{
	return YES;
}

#pragma mark IBAction Methods

- (IBAction) setInputAdd:(id)sender
{
	NSOutlineView *outlineView = (NSOutlineView *)sender;
	const NSInteger rowNumber = [outlineView clickedRow];
	
	[configInputList removeAllObjects];
	
	if ([outlineView isRowSelected:rowNumber])
	{
		[outlineView deselectRow:rowNumber];
		[self setConfigInputTargetID:nil];
	}
	else
	{
		[outlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:rowNumber] byExtendingSelection:NO];
		
		NSArray *inputList = (NSArray *)[outlineView itemAtRow:rowNumber];
		[self setConfigInputTargetID:[inputManager commandTagFromInputList:inputList]];
		[prefWindow makeFirstResponder:self];
	}
}

- (IBAction) removeInput:(id)sender
{
	NSOutlineView *outlineView = (NSOutlineView *)sender;
	const NSInteger rowNumber = [outlineView clickedRow];
	
	NSDictionary *deviceInfo = (NSDictionary *)[outlineView itemAtRow:rowNumber];
	NSMutableArray *inputList = (NSMutableArray *)[outlineView parentForItem:deviceInfo];
	
	[inputManager removeMappingUsingDeviceCode:[(NSString *)[deviceInfo valueForKey:@"deviceCode"] cStringUsingEncoding:NSUTF8StringEncoding] elementCode:[(NSString *)[deviceInfo valueForKey:@"elementCode"] cStringUsingEncoding:NSUTF8StringEncoding]];
	[inputManager writeDefaultsInputMappings];
	
	[outlineView reloadItem:inputList reloadChildren:YES];
}

- (IBAction) changeSpeed:(id)sender
{
	NSMutableDictionary *deviceInfo = (NSMutableDictionary *)[inputSettingsController content];
	if (deviceInfo != nil)
	{
		const float speedScalar = (float)[CocoaDSUtil getIBActionSenderTag:sender] / 100.0f;
		[deviceInfo setObject:[NSNumber numberWithFloat:speedScalar] forKey:@"floatValue0"];
	}
}

- (IBAction) showSettingsSheet:(id)sender
{
	NSOutlineView *outlineView = (NSOutlineView *)sender;
	const NSInteger rowNumber = [outlineView clickedRow];
	
	NSMutableDictionary *item = (NSMutableDictionary *)[outlineView itemAtRow:rowNumber];
	NSString *commandTag = [inputManager commandTagFromInputList:[outlineView parentForItem:item]];
	NSWindow *theSheet = (NSWindow *)[inputSettingsMappings valueForKey:commandTag];
	
	if (theSheet == nil)
	{
		return;
	}
	
	[inputSettingsController setContent:item];
	
	[NSApp beginSheet:theSheet
	   modalForWindow:prefWindow
		modalDelegate:self
	   didEndSelector:@selector(didEndSettingsSheet:returnCode:contextInfo:)
		  contextInfo:outlineView];
}

- (IBAction) closeSettingsSheet:(id)sender
{
	NSWindow *sheet = [(NSControl *)sender window];
	[sheet makeFirstResponder:nil]; // Force end of editing of any text fields.
    [NSApp endSheet:sheet returnCode:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) profileNew:(id)sender
{
	static NSUInteger untitledCount = 1;
	NSString *newProfileName = (untitledCount == 1) ? @"Untitled" : [NSString stringWithFormat:@"Untitled %ld", (unsigned long)untitledCount];
	
	while([self doesProfileNameExist:newProfileName])
	{
		newProfileName = [NSString stringWithFormat:@"Untitled %ld", (unsigned long)++untitledCount];
	}
	
	NSMutableDictionary *newProfile = [NSMutableDictionary dictionaryWithObjectsAndKeys:
									   newProfileName, @"Name",
									   [NSNumber numberWithBool:NO], @"IsDefaultType",
									   [[[NSMutableDictionary alloc] initWithDictionary:[inputManager inputMappings] copyItems:YES] autorelease], @"Mappings",
									   nil];
	
	[savedProfilesList addObject:newProfile];
	
	NSInteger profileIndex = _defaultProfileListCount + [savedProfilesList count] - 1;
	NSMenu *profileMenu = [inputProfileMenu menu];
	NSMenuItem *newProfileMenuItem = [[[NSMenuItem alloc] initWithTitle:newProfileName
																 action:@selector(profileSelect:)
														  keyEquivalent:@""] autorelease];
	[newProfileMenuItem setTag:profileIndex];
	[newProfileMenuItem setTarget:self];
	
	[profileMenu insertItem:newProfileMenuItem atIndex:profileIndex+1];
	
	// Remove the "no items" menu item when the first profile is saved.
	if ([savedProfilesList count] == 1)
	{
		[inputProfileMenu removeItemAtIndex:profileIndex+2];
	}
	
	[inputProfileMenu selectItemAtIndex:profileIndex+1];
	[self profileSelect:newProfileMenuItem];
	[[NSUserDefaults standardUserDefaults] setObject:savedProfilesList forKey:@"Input_SavedProfiles"];
	
	// Give the user a chance to name the new profile.
	[self profileRename:nil];
}

- (IBAction) profileView:(id)sender
{
	[NSApp beginSheet:inputProfileSheet
	   modalForWindow:prefWindow
		modalDelegate:self
	   didEndSelector:@selector(didEndProfileSheet:returnCode:contextInfo:)
		  contextInfo:nil];
}

- (IBAction) profileApply:(id)sender
{
	NSMutableDictionary *selectedProfile = (NSMutableDictionary *)[inputProfileController content];
	NSMutableDictionary *profileMappings = (NSMutableDictionary *)[selectedProfile valueForKey:@"Mappings"];
	
	if (profileMappings != nil)
	{
		// Use setMappingsWithMappings: instead of replacing the input mappings
		// completely since we need to keep this particular pointer address for
		// the inputMappings dictionary. If we don't do this, the outline view
		// will desync.
		[inputManager setMappingsWithMappings:profileMappings];
		[inputPrefOutlineView reloadData];
		[inputManager writeDefaultsInputMappings];
	}
}

- (IBAction) profileRename:(id)sender
{
	[NSApp beginSheet:inputProfileRenameSheet
	   modalForWindow:prefWindow
		modalDelegate:self
	   didEndSelector:@selector(didEndProfileRenameSheet:returnCode:contextInfo:)
		  contextInfo:nil];
}

- (IBAction) profileSave:(id)sender
{
	NSMutableDictionary *selectedProfile = (NSMutableDictionary *)[inputProfileController content];
	if (selectedProfile == nil)
	{
		return;
	}
	
	[selectedProfile setValue:[[[NSMutableDictionary alloc] initWithDictionary:[inputManager inputMappings] copyItems:YES] autorelease] forKey:@"Mappings"];
	[[NSUserDefaults standardUserDefaults] setObject:savedProfilesList forKey:@"Input_SavedProfiles"];
}

- (IBAction) profileDelete:(id)sender
{
	NSMutableDictionary *selectedProfile = (NSMutableDictionary *)[inputProfileController content];
	if (selectedProfile == nil)
	{
		return;
	}
	
	NSInteger profileIndex = [inputProfileMenu indexOfSelectedItem] - 1;
	NSMenu *profileMenu = [inputProfileMenu menu];
	
	[profileMenu removeItemAtIndex:[inputProfileMenu indexOfSelectedItem]];
	[savedProfilesList removeObjectAtIndex:(profileIndex - _defaultProfileListCount)];
	[inputProfileMenu selectItemAtIndex:0];
	
	// Add the "no items" menu item if there are no profiles saved.
	if ([savedProfilesList count] < 1)
	{
		NSMenuItem *noSavedConfigItem = [[[NSMenuItem alloc] initWithTitle:NSSTRING_INPUTPREF_NO_SAVED_PROFILES
																	 action:NULL
															  keyEquivalent:@""] autorelease];
		[noSavedConfigItem setEnabled:NO];
		[profileMenu insertItem:noSavedConfigItem atIndex:profileIndex+1];
	}
	else // Update the profile indices in the menu tags.
	{
		for (NSUInteger i = 0; i < [savedProfilesList count]; i++)
		{
			[[profileMenu itemAtIndex:_defaultProfileListCount+i+1] setTag:_defaultProfileListCount+i];
		}
	}
	
	[self profileSelect:[inputProfileMenu itemAtIndex:0]];
	[[NSUserDefaults standardUserDefaults] setObject:savedProfilesList forKey:@"Input_SavedProfiles"];
}

- (IBAction) profileSelect:(id)sender
{
	NSInteger profileID = [CocoaDSUtil getIBActionSenderTag:sender];
	NSArray *profileList = nil;
	
	if (profileID < 0 || profileID >= (NSInteger)(_defaultProfileListCount + [savedProfilesList count]))
	{
		return;
	}
	
	if (profileID < (NSInteger)_defaultProfileListCount) // Select one of the default profiles
	{
		NSDictionary *defaultKeyMappingsDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultKeyMappings" ofType:@"plist"]];
		profileList = [defaultKeyMappingsDict valueForKey:@"DefaultInputProfiles"];
	}
	else // Select one of the saved configs
	{
		profileList = savedProfilesList;
	}
	
	[inputProfilePreviousButton setTag:profileID-1];
	[inputProfilePreviousButton setEnabled:(profileID > 0) ? YES : NO];
	[inputProfileNextButton setTag:profileID+1];
	[inputProfileNextButton setEnabled:(profileID < ((NSInteger)_defaultProfileListCount + (NSInteger)[savedProfilesList count] - 1)) ? YES : NO];
	
	if (![sender isMemberOfClass:[NSMenuItem class]])
	{
		[inputProfileMenu selectItemAtIndex:(profileID < (NSInteger)_defaultProfileListCount) ? profileID : profileID+1];
	}
	
	[inputProfileController setContent:[profileList objectAtIndex:(profileID < (NSInteger)_defaultProfileListCount) ? profileID : (profileID - _defaultProfileListCount)]];
	[[inputProfileController profileOutlineView] reloadData];
	[[inputProfileController profileOutlineView] expandItem:nil expandChildren:YES];
}

- (IBAction) closeProfileSheet:(id)sender
{
	NSWindow *sheet = [(NSControl *)sender window];
	[sheet makeFirstResponder:nil]; // Force end of editing of any text fields.
	[NSApp endSheet:sheet returnCode:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) closeProfileRenameSheet:(id)sender
{
	NSWindow *sheet = [(NSControl *)sender window];
	[sheet makeFirstResponder:nil]; // Force end of editing of any text fields.
	[NSApp endSheet:sheet returnCode:[CocoaDSUtil getIBActionSenderTag:sender]];
}

#pragma mark NSControl Delegate Methods

- (void)controlTextDidEndEditing:(NSNotification *)aNotification
{
	// Called when the profile view sheet changes the profile name
	[self updateSelectedProfileName];
}
		  
@end
