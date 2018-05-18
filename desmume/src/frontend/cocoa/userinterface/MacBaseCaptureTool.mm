/*
	Copyright (C) 2018 DeSmuME team
 
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

#import "../cocoa_globals.h"
#import "MacBaseCaptureTool.h"


@implementation MacBaseCaptureToolDelegate

@synthesize dummyObject;
@synthesize window;
@synthesize saveDirectoryPathTextField;
@synthesize sharedData;
@synthesize saveDirectoryPath;
@synthesize romName;
@synthesize formatID;
@synthesize displayMode;
@synthesize displayLayout;
@synthesize displayOrder;
@synthesize displaySeparation;
@synthesize displayScale;
@synthesize displayRotation;
@synthesize useDeposterize;
@synthesize outputFilterID;
@synthesize pixelScalerID;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	sharedData			= nil;
	saveDirectoryPath	= nil;
	romName				= @"No_ROM_loaded";
	
	formatID			= 0;
	displayMode			= ClientDisplayMode_Dual;
	displayLayout		= ClientDisplayLayout_Vertical;
	displayOrder		= ClientDisplayOrder_MainFirst;
	displaySeparation	= 0;
	displayScale		= 0;
	displayRotation		= 0;
	useDeposterize		= NO;
	outputFilterID		= OutputFilterTypeID_Bilinear;
	pixelScalerID		= VideoFilterTypeID_None;
	
	return self;
}

- (void)dealloc
{
	[self setSharedData:nil];
	[self setSaveDirectoryPath:nil];
	[self setRomName:nil];
	
	[super dealloc];
}

- (IBAction) chooseDirectoryPath:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanCreateDirectories:YES];
	[panel setCanChooseDirectories:YES];
	[panel setCanChooseFiles:NO];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_SAVE_SCREENSHOT_PANEL];
	
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel beginSheetModalForWindow:[self window]
				  completionHandler:^(NSInteger result) {
					  [self chooseDirectoryPathDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:nil
				   modalForWindow:[self window]
					modalDelegate:self
				   didEndSelector:@selector(chooseDirectoryPathDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (void) chooseDirectoryPathDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	if (returnCode == NSCancelButton)
	{
		return;
	}
	
	NSURL *selectedFileURL = [[sheet URLs] lastObject]; //hopefully also the first object
	if(selectedFileURL == nil)
	{
		return;
	}
	
	[self setSaveDirectoryPath:[selectedFileURL path]];
}

#pragma mark DirectoryURLDragDestTextFieldProtocol Protocol
- (void)assignDirectoryPath:(NSString *)dirPath textField:(NSTextField *)textField
{
	if (textField == saveDirectoryPathTextField)
	{
		[self setSaveDirectoryPath:dirPath];
	}
}

@end
