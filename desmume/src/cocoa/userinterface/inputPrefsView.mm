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
#import "preferencesWindowDelegate.h"

#import "cocoa_globals.h"
#import "cocoa_input.h"
#import "cocoa_util.h"

#define INPUT_HOLD_TIME		0.1		// Time in seconds to hold a button in its on state when mapping an input.

@implementation InputPrefProperties

@synthesize commandTag;
@synthesize icon;
@synthesize settingsSheet;

- (id)init
{
	return [self initWithCommandTag:@"" icon:nil sheet:nil];
}

- (id) initWithCommandTag:(NSString *)theCommandTag icon:(NSImage *)theIcon sheet:(NSWindow *)theSheet
{
	self = [super init];
    if (self == nil)
	{
		return self;
    }
	
	commandTag = [theCommandTag retain];
	icon = [theIcon retain];
	settingsSheet = [theSheet retain];
	
	return self;
}

- (void)dealloc
{
	[self setCommandTag:nil];
	[self setIcon:nil];
	[self setSettingsSheet:nil];
	
	[super dealloc];
}

@end


#pragma mark -

@implementation InputPrefsView

@synthesize prefWindow;
@synthesize inputPrefOutlineView;
@synthesize inputSettingsController;
@synthesize inputSettingsMicrophone;
@synthesize inputSettingsTouch;
@synthesize inputSettingsLoadStateSlot;
@synthesize inputSettingsSaveStateSlot;
@synthesize inputSettingsSetSpeedLimit;
@synthesize inputSettingsGPUState;
@synthesize inputManager;
@dynamic configInputTargetID;

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self == nil)
	{
		return self;
    }
	
	configInputTargetID = nil;
	configInputList = [[NSMutableDictionary alloc] initWithCapacity:128];
	
	inputPrefProperties = [[NSDictionary alloc] initWithObjectsAndKeys:
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonSelect_420x420" ofType:@"png"]] autorelease],		@"UNKNOWN COMMAND",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_ArrowUp_420x420" ofType:@"png"]] autorelease],			@"Up",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_ArrowDown_420x420" ofType:@"png"]] autorelease],			@"Down",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_ArrowLeft_420x420" ofType:@"png"]] autorelease],			@"Left",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_ArrowRight_420x420" ofType:@"png"]] autorelease],			@"Right",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonA_420x420" ofType:@"png"]] autorelease],			@"A",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonB_420x420" ofType:@"png"]] autorelease],			@"B",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonX_420x420" ofType:@"png"]] autorelease],			@"X",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonY_420x420" ofType:@"png"]] autorelease],			@"Y",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonL_420x420" ofType:@"png"]] autorelease],			@"L",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonR_420x420" ofType:@"png"]] autorelease],			@"R",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonStart_420x420" ofType:@"png"]] autorelease],		@"Start",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonSelect_420x420" ofType:@"png"]] autorelease],		@"Select",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Microphone_420x420" ofType:@"png"]] autorelease],			@"Microphone",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_ShowHUD_420x420" ofType:@"png"]] autorelease],			@"HUD",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Execute_420x420" ofType:@"png"]] autorelease],			@"Execute",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Pause_420x420" ofType:@"png"]] autorelease],				@"Pause",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Execute_420x420" ofType:@"png"]] autorelease],			@"Execute/Pause",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Reset_420x420" ofType:@"png"]] autorelease],				@"Reset",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonSelect_420x420" ofType:@"png"]] autorelease],		@"Touch",
						   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_VolumeMute_16x16" ofType:@"png"]] autorelease],			@"Mute/Unmute",
						   nil];
	
	commandTagList = [[[NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultKeyMappings" ofType:@"plist"]] valueForKey:@"CommandTagList"] retain];
	
    return self;
}

- (void)dealloc
{
	[configInputList release];
	[inputPrefProperties release];
	[inputSettingsMappings release];
	[commandTagList release];
	
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

- (NSString *) commandTagFromInputList:(NSArray *)inputList
{
	NSString *commandTag = nil;
	if (inputList == nil)
	{
		return commandTag;
	}
	
	NSDictionary *inputMappings = [inputManager inputMappings];
	for (NSString *tag in inputMappings)
	{
		if (inputList == [inputMappings valueForKey:tag])
		{
			commandTag = tag;
			break;
		}
	}
	
	return commandTag;
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
			NSString *commandTag = [self commandTagFromInputList:item];
			if (commandTag != nil)
			{
				NSImage *buttonImage = (NSImage *)[inputPrefProperties valueForKey:commandTag];
				if (buttonImage == nil)
				{
					buttonImage = (NSImage *)[inputPrefProperties valueForKey:@"UNKNOWN COMMAND"];
				}
				
				[outCell setTitle:commandTag];
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
			NSString *commandTag = [self commandTagFromInputList:[outlineView parentForItem:item]];
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
		NSString *commandTag = [commandTagList objectAtIndex:index];
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
		numberChildren = [commandTagList count];
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
			return [NSString stringWithFormat:(inputCount != 1) ? @"%ld Inputs Mapped" : @"%ld Input Mapped", inputCount];
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
	return [self commandTagFromInputList:item];
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
		[self setConfigInputTargetID:[self commandTagFromInputList:inputList]];
		[prefWindow makeFirstResponder:self];
	}
}

- (IBAction) removeInput:(id)sender
{
	NSOutlineView *outlineView = (NSOutlineView *)sender;
	const NSInteger rowNumber = [outlineView clickedRow];
	
	NSDictionary *deviceInfo = (NSDictionary *)[outlineView itemAtRow:rowNumber];
	[inputManager removeMappingUsingDeviceCode:[(NSString *)[deviceInfo valueForKey:@"deviceCode"] cStringUsingEncoding:NSUTF8StringEncoding] elementCode:[(NSString *)[deviceInfo valueForKey:@"elementCode"] cStringUsingEncoding:NSUTF8StringEncoding]];
	
	NSMutableArray *inputList = (NSMutableArray *)[outlineView parentForItem:deviceInfo];
	[inputList removeObject:deviceInfo];
	
	[outlineView reloadItem:inputList reloadChildren:YES];
	[inputManager writeDefaultsInputMappings];
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
	NSString *commandTag = [self commandTagFromInputList:[outlineView parentForItem:item]];
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

@end
