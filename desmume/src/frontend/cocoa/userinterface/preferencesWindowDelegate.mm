/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2018 DeSmuME Team

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

#import "preferencesWindowDelegate.h"
#import "EmuControllerDelegate.h"

#import "cocoa_core.h"
#import "cocoa_GPU.h"
#import "cocoa_cheat.h"
#import "cocoa_globals.h"
#import "cocoa_input.h"
#import "cocoa_file.h"
#import "cocoa_videofilter.h"
#import "cocoa_util.h"

#ifdef MAC_OS_X_VERSION_10_7
#include "../OGLDisplayOutput_3_2.h"
#else
#include "../OGLDisplayOutput.h"
#endif


#pragma mark -
@implementation DisplayPreviewView

@dynamic filtersPreferGPU;
@dynamic sourceDeposterize;
@dynamic pixelScaler;
@dynamic outputFilter;

- (id)initWithFrame:(NSRect)frameRect
{
	self = [super initWithFrame:frameRect];
	if (self == nil)
	{
		return self;
	}
	
#if defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7)
	if ([self respondsToSelector:@selector(setWantsBestResolutionOpenGLSurface:)])
	{
		[self setWantsBestResolutionOpenGLSurface:YES];
	}
#endif
	
	isPreviewImageLoaded = false;
		
	// Initialize the OpenGL context
	bool useContext_3_2 = false;
	NSOpenGLPixelFormatAttribute attributes[] = {
		NSOpenGLPFAColorSize, (NSOpenGLPixelFormatAttribute)24,
		NSOpenGLPFAAlphaSize, (NSOpenGLPixelFormatAttribute)8,
		NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)0,
		NSOpenGLPFAStencilSize, (NSOpenGLPixelFormatAttribute)0,
		(NSOpenGLPixelFormatAttribute)0, (NSOpenGLPixelFormatAttribute)0,
		(NSOpenGLPixelFormatAttribute)0 };
	
#ifdef _OGLDISPLAYOUTPUT_3_2_H_
	// If we can support a 3.2 Core Profile context, then request that in our
	// pixel format attributes.
	useContext_3_2 = IsOSXVersionSupported(10, 7, 0);
	if (useContext_3_2)
	{
		attributes[8] = NSOpenGLPFAOpenGLProfile;
		attributes[9] = NSOpenGLProfileVersion3_2Core;
	}
#endif
	
	NSOpenGLPixelFormat *format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	if (format == nil)
	{
		// If we can't get a 3.2 Core Profile context, then switch to using a
		// legacy context instead.
		useContext_3_2 = false;
		attributes[8] = (NSOpenGLPixelFormatAttribute)0;
		attributes[9] = (NSOpenGLPixelFormatAttribute)0;
		format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	}
	
	context = [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
	[format release];
	cglDisplayContext = (CGLContextObj)[context CGLContextObj];
	
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglDisplayContext);
	
	NSRect newViewportRect = frameRect;
#if defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7)
	if ([self respondsToSelector:@selector(convertRectToBacking:)])
	{
		newViewportRect = [self convertRectToBacking:frameRect];
	}
#endif
	
	OGLContextInfo *contextInfo = NULL;
	
#ifdef _OGLDISPLAYOUTPUT_3_2_H_
	if (useContext_3_2)
	{
		contextInfo = new OGLContextInfo_3_2;
	}
	else
#endif
	{
		contextInfo = new OGLContextInfo_Legacy;
	}
	
	oglImage = new OGLImage(contextInfo, 64, 64, newViewportRect.size.width, newViewportRect.size.height);
	oglImage->SetFiltersPreferGPUOGL(true);
	oglImage->SetSourceDeposterize(false);
	oglImage->SetOutputFilterOGL(OutputFilterTypeID_Bilinear);
	oglImage->SetPixelScalerOGL(VideoFilterTypeID_None);
	
	CGLSetCurrentContext(prevContext);
	
	return self;
}

- (void)dealloc
{
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglDisplayContext);
	delete oglImage;
	CGLSetCurrentContext(prevContext);
	
	[context clearDrawable];
	[context release];
	
	[super dealloc];
}

- (void) setFiltersPreferGPU:(BOOL)theState
{
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglDisplayContext);
	oglImage->SetFiltersPreferGPUOGL(theState ? true : false);
	oglImage->ProcessOGL();
	CGLSetCurrentContext(prevContext);
}

- (BOOL) filtersPreferGPU
{
	return (oglImage->GetFiltersPreferGPU() ? YES : NO);
}

- (void) setSourceDeposterize:(BOOL)theState
{
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglDisplayContext);
	oglImage->SetSourceDeposterize(theState ? true : false);
	oglImage->ProcessOGL();
	CGLSetCurrentContext(prevContext);
}

- (BOOL) sourceDeposterize
{
	return (oglImage->GetSourceDeposterize() ? YES : NO);
}

- (void) setPixelScaler:(NSInteger)scalerID
{
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglDisplayContext);
	oglImage->SetPixelScalerOGL(scalerID);
	oglImage->ProcessOGL();
	CGLSetCurrentContext(prevContext);
}

- (NSInteger) pixelScaler
{
	return (NSInteger)oglImage->GetPixelScaler();
}

- (void) setOutputFilter:(NSInteger)outputFilterID
{
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglDisplayContext);
	oglImage->SetOutputFilterOGL(outputFilterID);
	CGLSetCurrentContext(prevContext);
}

- (NSInteger) outputFilter
{
	return (NSInteger)oglImage->GetOutputFilter();
}

- (void) loadPreviewImage:(NSImage *)previewImage
{
	NSArray *imageRepArray = [previewImage representations];
	const NSBitmapImageRep *imageRep = [imageRepArray objectAtIndex:0];
	const size_t previewWidth = (GLsizei)[previewImage size].width;
	const size_t previewHeight = (GLsizei)[previewImage size].height;
	
	// When an NSImage is loaded from file, it is loaded as ABGR (or RGBA for big-endian).
	// However, the OpenGL blitter takes BGRA format. Therefore, we need to convert the
	// pixel format before sending to OpenGL.
	
	uint32_t *bitmapData = (uint32_t *)[imageRep bitmapData];
	for (size_t i = 0; i < previewWidth * previewHeight; i++)
	{
		const uint32_t color = bitmapData[i];
		
#if defined(__i386__) || defined(__x86_64__)
		bitmapData[i]	=           0xFF000000         | // lA
						  ((color & 0x00FF0000) >> 16) | // lB -> lR
						   (color & 0x0000FF00)        | // lG
						  ((color & 0x000000FF) << 16);  // lR -> lB
#else
		bitmapData[i]	=           0xFF000000         | // lA
						  ((color & 0xFF000000) >>  8) | // bR -> lR
						   (color & 0x00FF0000) >>  8  | // bG -> lG
						  ((color & 0x0000FF00) >>  8);  // bB -> lB
#endif
		
	}
	
	// Send the NSImage to OpenGL.
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglDisplayContext);
	oglImage->LoadFrameOGL(bitmapData, 0, 0, previewWidth, previewHeight);
	oglImage->ProcessOGL();
	CGLSetCurrentContext(prevContext);
}

- (BOOL)isOpaque
{
	return YES;
}

- (void)lockFocus
{
	[super lockFocus];
	
	if ([context view] != self)
	{
		[context setView:self];
	}
}

- (void)drawRect:(NSRect)dirtyRect
{
	if (!isPreviewImageLoaded)
	{
		// Load the preview image.
		NSImage *previewImage = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"VideoFilterPreview_64x64" ofType:@"png"]];
		[self loadPreviewImage:previewImage];
		[previewImage release];
		
		isPreviewImageLoaded = true;
	}
	
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglDisplayContext);
	oglImage->RenderOGL();
	glFlush();
	CGLSetCurrentContext(prevContext);
}

@end

#pragma mark -

@implementation PreferencesWindowDelegate

@synthesize dummyObject;
@synthesize window;
@synthesize toolbar;
@synthesize firmwareConfigSheet;
@synthesize cdsCoreController;
@synthesize emuController;
@synthesize prefWindowController;
@synthesize cheatWindowController;
@synthesize cheatDatabaseController;

@synthesize toolbarItemGeneral;
@synthesize toolbarItemInput;
@synthesize toolbarItemDisplay;
@synthesize toolbarItemSound;
@synthesize toolbarItemEmulation;

@synthesize viewGeneral;
@synthesize viewInput;
@synthesize viewDisplay;
@synthesize viewSound;
@synthesize viewEmulation;

@synthesize displayRotationMenu;
@synthesize displayRotationMenuCustomItem;
@synthesize displayRotationField;
@synthesize spuSyncMethodMenu;

@synthesize openglMSAAPopUpButton;

@synthesize previewView;

@synthesize firmwareMACAddressString;
@synthesize subnetMaskString_AP1;
@synthesize subnetMaskString_AP2;
@synthesize subnetMaskString_AP3;

@synthesize bindings;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	bindings = [[NSMutableDictionary alloc] init];
	if (bindings == nil)
	{
		[self release];
		self = nil;
		return self;
	}
	
	firmwareMACAddressString = @"00:09:BF:FF:FF:FF";
	subnetMaskString_AP1 = @"0.0.0.0";
	subnetMaskString_AP2 = @"0.0.0.0";
	subnetMaskString_AP3 = @"0.0.0.0";
	
	// Load the volume icons.
	iconVolumeFull		= [[NSImage imageNamed:@"Icon_VolumeFull_16x16"] retain];
	iconVolumeTwoThird	= [[NSImage imageNamed:@"Icon_VolumeTwoThird_16x16"] retain];
	iconVolumeOneThird	= [[NSImage imageNamed:@"Icon_VolumeOneThird_16x16"] retain];
	iconVolumeMute		= [[NSImage imageNamed:@"Icon_VolumeMute_16x16"] retain];
	[bindings setObject:iconVolumeFull forKey:@"volumeIconImage"];
	
	prefViewDict = nil;
	
	return self;
}

- (void)dealloc
{
	[iconVolumeFull release];
	[iconVolumeTwoThird release];
	[iconVolumeOneThird release];
	[iconVolumeMute release];
	[bindings release];
	[prefViewDict release];
	
	[self setFirmwareMACAddressString:nil];
	[self setSubnetMaskString_AP1:nil];
	[self setSubnetMaskString_AP2:nil];
	[self setSubnetMaskString_AP3:nil];
	
	[super dealloc];
}

- (IBAction) changePrefView:(id)sender
{
	if (prefViewDict == nil)
	{
		// Associates NSView objects to their respective toolbar identifiers.
		prefViewDict = [[NSDictionary alloc] initWithObjectsAndKeys:
						viewGeneral,	[toolbarItemGeneral itemIdentifier],
						viewInput,		[toolbarItemInput itemIdentifier],
						viewDisplay,	[toolbarItemDisplay itemIdentifier],
						viewSound,		[toolbarItemSound itemIdentifier],
						viewEmulation,	[toolbarItemEmulation itemIdentifier],
						nil];
	}
	
	NSString *toolbarItemIdentifier = [[self toolbar] selectedItemIdentifier];
	NSView *theView = [prefViewDict objectForKey:toolbarItemIdentifier];
	if (theView != nil)
	{
		[self switchContentView:theView];
		
		if ([toolbarItemIdentifier isEqualToString:[toolbarItemInput itemIdentifier]])
		{
			[window makeFirstResponder:theView];
		}
	}
}

- (IBAction) chooseRomForAutoload:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_SELECT_ROM_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_ROM_DS, @FILE_EXT_ROM_GBA, nil];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseRomForAutoloadDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseRomForAutoloadDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (void) chooseRomForAutoloadDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	// Temporarily set the autoload ROM option in user defaults to some neutral value first and synchronize.
	// When the user defaults are actually set later, this will force the proper state transitions to occur.
	[[NSUserDefaults standardUserDefaults] setInteger:ROMAUTOLOADOPTION_CHOOSE_ROM forKey:@"General_AutoloadROMOption"];
	[[NSUserDefaults standardUserDefaults] synchronize];
	
	if (returnCode == NSCancelButton)
	{
		[[NSUserDefaults standardUserDefaults] setInteger:ROMAUTOLOADOPTION_LOAD_NONE forKey:@"General_AutoloadROMOption"];
		return;
	}
	
	NSURL *selectedFileURL = [[sheet URLs] lastObject]; //hopefully also the first object
	if(selectedFileURL == nil)
	{
		[[NSUserDefaults standardUserDefaults] setInteger:ROMAUTOLOADOPTION_LOAD_NONE forKey:@"General_AutoloadROMOption"];
		return;
	}
	
	NSString *selectedFile = [selectedFileURL path];
	
	[[NSUserDefaults standardUserDefaults] setInteger:ROMAUTOLOADOPTION_LOAD_SELECTED forKey:@"General_AutoloadROMOption"];
	[[NSUserDefaults standardUserDefaults] setObject:selectedFile forKey:@"General_AutoloadROMSelectedPath"];
	[bindings setValue:[selectedFile lastPathComponent] forKey:@"AutoloadRomName"];
}

- (IBAction) chooseAdvansceneDatabase:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_SELECT_ADVANSCENE_DB_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_ADVANSCENE_DB, nil];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseAdvansceneDatabaseDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseAdvansceneDatabaseDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (void) chooseAdvansceneDatabaseDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
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
	
	NSString *selectedFile = [selectedFileURL path];
	
	[[NSUserDefaults standardUserDefaults] setObject:selectedFile forKey:@"Advanscene_DatabasePath"];
	[bindings setValue:[selectedFile lastPathComponent] forKey:@"AdvansceneDatabaseName"];
}

- (IBAction) chooseCheatDatabase:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_SELECT_R4_CHEAT_DB_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_R4_CHEAT_DB, nil];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseCheatDatabaseDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseCheatDatabaseDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (void) chooseCheatDatabaseDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
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
	
	NSString *selectedFile = [selectedFileURL path];
	
	[[NSUserDefaults standardUserDefaults] setObject:selectedFile forKey:@"R4Cheat_DatabasePath"];
	[bindings setValue:[selectedFile lastPathComponent] forKey:@"R4CheatDatabaseName"];
	
	const BOOL isRomLoaded = [(EmuControllerDelegate *)[emuController content] currentRom] != nil;
	NSMutableDictionary *cheatWindowBindings = (NSMutableDictionary *)[cheatWindowController content];
	CocoaDSCheatManager *cdsCheats = (CocoaDSCheatManager *)[cheatWindowBindings valueForKey:@"cheatList"];
	
	if (isRomLoaded == YES && cdsCheats != nil)
	{
		NSInteger error = 0;
		NSMutableArray *dbList = [cdsCheats cheatListFromDatabase:selectedFileURL errorCode:&error];
		if (dbList != nil)
		{
			[cheatDatabaseController setContent:dbList];
			
			NSString *titleString = cdsCheats.dbTitle;
			NSString *dateString = cdsCheats.dbDate;
			
			[cheatWindowBindings setValue:titleString forKey:@"cheatDBTitle"];
			[cheatWindowBindings setValue:dateString forKey:@"cheatDBDate"];
			[cheatWindowBindings setValue:[NSString stringWithFormat:@"%ld", (unsigned long)[dbList count]] forKey:@"cheatDBItemCount"];
		}
		else
		{
			// TODO: Display an error message here.
			[cheatWindowBindings setValue:@"---" forKey:@"cheatDBItemCount"];
			
			switch (error)
			{
				case CHEATEXPORT_ERROR_FILE_NOT_FOUND:
					NSLog(@"R4 Cheat Database read failed! Could not load the database file!");
					[cheatWindowBindings setValue:@"Database not loaded." forKey:@"cheatDBTitle"];
					[cheatWindowBindings setValue:@"CANNOT LOAD FILE" forKey:@"cheatDBDate"];
					break;
					
				case CHEATEXPORT_ERROR_WRONG_FILE_FORMAT:
					NSLog(@"R4 Cheat Database read failed! Wrong file format!");
					[cheatWindowBindings setValue:@"Database load error." forKey:@"cheatDBTitle"];
					[cheatWindowBindings setValue:@"FAILED TO LOAD FILE" forKey:@"cheatDBDate"];
					break;
					
				case CHEATEXPORT_ERROR_SERIAL_NOT_FOUND:
					NSLog(@"R4 Cheat Database read failed! Could not find the serial number for this game in the database!");
					[cheatWindowBindings setValue:@"ROM not found in database." forKey:@"cheatDBTitle"];
					[cheatWindowBindings setValue:@"ROM not found." forKey:@"cheatDBDate"];
					break;
					
				case CHEATEXPORT_ERROR_EXPORT_FAILED:
					NSLog(@"R4 Cheat Database read failed! Could not read the database file!");
					[cheatWindowBindings setValue:@"Database read error." forKey:@"cheatDBTitle"];
					[cheatWindowBindings setValue:@"CANNOT READ FILE" forKey:@"cheatDBDate"];
					break;
					
				default:
					break;
			}
		}
	}
}

- (IBAction) selectDisplayRotation:(id)sender
{
	const NSInteger displayRotation = [(NSMenuItem *)sender tag];
	if (displayRotation != -1)
	{
		[[NSUserDefaults standardUserDefaults] setDouble:displayRotation forKey:@"DisplayView_Rotation"];
	}
}

- (IBAction) updateFiltersPreferGPU:(id)sender
{
	const BOOL theState = [CocoaDSUtil getIBActionSenderButtonStateBool:sender];
	[[self previewView] setFiltersPreferGPU:theState];
	[previewView setNeedsDisplay:YES];
}

- (IBAction) updateSourceDeposterize:(id)sender
{
	const BOOL theState = [CocoaDSUtil getIBActionSenderButtonStateBool:sender];
	[[self previewView] setSourceDeposterize:theState];
	[previewView setNeedsDisplay:YES];
}

- (IBAction) selectOutputFilter:(id)sender
{
	const NSInteger filterID = [CocoaDSUtil getIBActionSenderTag:sender];
	[[NSUserDefaults standardUserDefaults] setInteger:filterID forKey:@"DisplayView_OutputFilter"];
	[[self previewView] setOutputFilter:filterID];
	[previewView setNeedsDisplay:YES];
}

- (IBAction) selectPixelScaler:(id)sender
{
	const NSInteger filterID = [CocoaDSUtil getIBActionSenderTag:sender];
	[[NSUserDefaults standardUserDefaults] setInteger:filterID forKey:@"DisplayView_VideoFilter"];
	[[self previewView] setPixelScaler:filterID];
	[previewView setNeedsDisplay:YES];
}

- (IBAction) updateVolumeIcon:(id)sender
{
	NSImage *iconImage = (NSImage *)[bindings objectForKey:@"volumeIconImage"];
	NSImage *newIconImage = nil;
	const float vol = [[NSUserDefaults standardUserDefaults] floatForKey:@"Sound_Volume"];
	
	if (vol <= 0.0f)
	{
		newIconImage = iconVolumeMute;
	}
	else if (vol > 0.0f && vol <= VOLUME_THRESHOLD_LOW)
	{
		newIconImage = iconVolumeOneThird;
	}
	else if (vol > VOLUME_THRESHOLD_LOW && vol <= VOLUME_THRESHOLD_HIGH)
	{
		newIconImage = iconVolumeTwoThird;
	}
	else
	{
		newIconImage = iconVolumeFull;
	}
	
	if (newIconImage == iconImage)
	{
		return;
	}
	
	[bindings setObject:newIconImage forKey:@"volumeIconImage"];
}

- (IBAction) selectSPUSyncMode:(id)sender
{
	const NSInteger spuSyncMode = [[NSUserDefaults standardUserDefaults] integerForKey:@"SPU_SyncMode"];
	[spuSyncMethodMenu setEnabled:(spuSyncMode == SPU_SYNC_MODE_DUAL_SYNC_ASYNC) ? NO : YES];
}

- (IBAction) selectSPUSyncMethod:(id)sender
{
	const NSInteger spuSyncMethod = [(NSMenuItem *)sender tag];
	[[NSUserDefaults standardUserDefaults] setInteger:spuSyncMethod forKey:@"SPU_SyncMethod"];
}

- (void) updateDisplayRotationMenu:(double)displayRotation
{
	if (displayRotation == 0.0f ||
		displayRotation == 90.0f ||
		displayRotation == 180.0f ||
		displayRotation == 270.0f)
	{
		[displayRotationMenu selectItemWithTag:(NSInteger)displayRotation];
	}
	else if (displayRotation < 0.0f || displayRotation >= 360.0f)
	{
		displayRotation = 0.0f;
		[displayRotationField setFloatValue:displayRotation];
		[displayRotationMenu selectItemWithTag:(NSInteger)displayRotation];
	}
	else
	{
		[displayRotationMenu selectItem:displayRotationMenuCustomItem];
	}
}

- (IBAction) chooseARM9BiosImage:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_SELECT_ARM9_IMAGE_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_HW_IMAGE_FILE, nil];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseArm9BiosImageDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseArm9BiosImageDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (IBAction) chooseARM7BiosImage:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_SELECT_ARM7_IMAGE_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_HW_IMAGE_FILE, nil];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseArm7BiosImageDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseArm7BiosImageDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (IBAction) chooseFirmwareImage:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_SELECT_FIRMWARE_IMAGE_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_HW_IMAGE_FILE, nil];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseFirmwareImageDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseFirmwareImageDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (void) chooseArm9BiosImageDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
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
	
	NSString *selectedFile = [selectedFileURL path];
	
	[[NSUserDefaults standardUserDefaults] setObject:selectedFile forKey:@"BIOS_ARM9ImagePath"];
	[bindings setValue:[selectedFile lastPathComponent] forKey:@"Arm9BiosImageName"];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setArm9ImageURL:selectedFileURL];
}

- (void) chooseArm7BiosImageDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
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
	
	NSString *selectedFile = [selectedFileURL path];
	
	[[NSUserDefaults standardUserDefaults] setObject:selectedFile forKey:@"BIOS_ARM7ImagePath"];
	[bindings setValue:[selectedFile lastPathComponent] forKey:@"Arm7BiosImageName"];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setArm7ImageURL:selectedFileURL];
}

- (void) chooseFirmwareImageDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
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
	
	NSString *selectedFile = [selectedFileURL path];
	
	[[NSUserDefaults standardUserDefaults] setObject:selectedFile forKey:@"Emulation_FirmwareImagePath"];
	[bindings setValue:[selectedFile lastPathComponent] forKey:@"FirmwareImageName"];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setFirmwareImageURL:selectedFileURL];
}

- (IBAction) configureInternalFirmware:(id)sender
{
	[NSApp beginSheet:firmwareConfigSheet
	   modalForWindow:window
		modalDelegate:self
	   didEndSelector:@selector(didEndFirmwareConfigSheet:returnCode:contextInfo:)
		  contextInfo:nil];
}

- (IBAction) closeFirmwareConfigSheet:(id)sender
{
	NSWindow *sheet = [(NSControl *)sender window];
	const NSInteger code = [CocoaDSUtil getIBActionSenderTag:sender];
	
	// Force end of editing of any text fields.
	[sheet makeFirstResponder:nil];
	
    [NSApp endSheet:sheet returnCode:code];
}

- (void) didEndFirmwareConfigSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
    [sheet orderOut:self];
}

- (IBAction) generateFirmwareMACAddress:(id)sender
{
	uint32_t randomMACAddressValue = 0;
	
	do
	{
		randomMACAddressValue = (uint32_t)random() & 0x00FFFFFF;
	} while (randomMACAddressValue == 0);
	
	randomMACAddressValue = (randomMACAddressValue << 8) | 0xBF;
	
	[[NSUserDefaults standardUserDefaults] setInteger:randomMACAddressValue forKey:@"FirmwareConfig_FirmwareMACAddress"];
	[[NSUserDefaults standardUserDefaults] synchronize];
	
	[self updateFirmwareMACAddressString:nil];
}

- (IBAction) updateFirmwareMACAddressString:(id)sender
{
	const uint32_t defaultMACAddressValue = (uint32_t)[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_FirmwareMACAddress"];
	const uint8_t mac4 = (defaultMACAddressValue >>  8) & 0x000000FF;
	const uint8_t mac5 = (defaultMACAddressValue >> 16) & 0x000000FF;
	const uint8_t mac6 = (defaultMACAddressValue >> 24) & 0x000000FF;
	
	NSString *theMACAddressString = [NSString stringWithFormat:@"00:09:BF:%02X:%02X:%02X", mac4, mac5, mac6];
	[self setFirmwareMACAddressString:theMACAddressString];
}

- (IBAction) updateSubnetMaskString_AP1:(id)sender
{
	const uint32_t defaultSubnetMask = (uint8_t)[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_SubnetMask_AP1"];
	const uint32_t subnetMaskValue = (defaultSubnetMask == 0) ? 0 : (0xFFFFFFFF << (32 - defaultSubnetMask));
	
	NSString *subnetMaskString = [NSString stringWithFormat:@"%d.%d.%d.%d",
								  (subnetMaskValue >> 24) & 0x000000FF,
								  (subnetMaskValue >> 16) & 0x000000FF,
								  (subnetMaskValue >>  8) & 0x000000FF,
								  (subnetMaskValue >>  0) & 0x000000FF];
	
	[self setSubnetMaskString_AP1:subnetMaskString];
}

- (IBAction) updateSubnetMaskString_AP2:(id)sender
{
	const uint32_t defaultSubnetMask = (uint8_t)[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_SubnetMask_AP2"];
	const uint32_t subnetMaskValue = (defaultSubnetMask == 0) ? 0 : (0xFFFFFFFF << (32 - defaultSubnetMask));
	
	NSString *subnetMaskString = [NSString stringWithFormat:@"%d.%d.%d.%d",
								  (subnetMaskValue >> 24) & 0x000000FF,
								  (subnetMaskValue >> 16) & 0x000000FF,
								  (subnetMaskValue >>  8) & 0x000000FF,
								  (subnetMaskValue >>  0) & 0x000000FF];
	
	[self setSubnetMaskString_AP2:subnetMaskString];
}

- (IBAction) updateSubnetMaskString_AP3:(id)sender
{
	const uint32_t defaultSubnetMask = (uint8_t)[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_SubnetMask_AP3"];
	const uint32_t subnetMaskValue = (defaultSubnetMask == 0) ? 0 : (0xFFFFFFFF << (32 - defaultSubnetMask));
	
	NSString *subnetMaskString = [NSString stringWithFormat:@"%d.%d.%d.%d",
								  (subnetMaskValue >> 24) & 0x000000FF,
								  (subnetMaskValue >> 16) & 0x000000FF,
								  (subnetMaskValue >>  8) & 0x000000FF,
								  (subnetMaskValue >>  0) & 0x000000FF];
	
	[self setSubnetMaskString_AP3:subnetMaskString];
}

- (void)controlTextDidEndEditing:(NSNotification *)aNotification
{
	[self updateDisplayRotationMenu:[displayRotationField floatValue]];
}

- (void) switchContentView:(NSView *)theView
{
	if ([window contentView] == theView)
	{
		return;
	}
	
	NSRect newFrame = [window frameRectForContentRect:[theView frame]];
	newFrame.origin.x = [window frame].origin.x;
	newFrame.origin.y = [window frame].origin.y + [[window contentView] frame].size.height - [theView frame].size.height;
	
	NSView *tempView = [[NSView alloc] initWithFrame:[[window contentView] frame]];
	[window setContentView:tempView];
	
	[window setFrame:newFrame display:YES animate:YES];
	[window setContentView:theView];
	
	[tempView release];
}

- (void) markUnsupportedOpenGLMSAAMenuItems
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	NSUInteger maxSamples = [[cdsCore cdsGPU] openglDeviceMaxMultisamples];
	size_t itemCount = [openglMSAAPopUpButton numberOfItems];
	BOOL needAddUnsupportedSeparator = YES;
	
	for (size_t i = 0; i < itemCount; i++)
	{
		NSMenuItem *menuItem = [openglMSAAPopUpButton itemAtIndex:i];
		if ([menuItem tag] > maxSamples)
		{
			if (needAddUnsupportedSeparator)
			{
				NSMenuItem *newSeparatorItem = [NSMenuItem separatorItem];
				[newSeparatorItem setTag:-1];
				[[openglMSAAPopUpButton menu] insertItem:newSeparatorItem atIndex:i];
				
				needAddUnsupportedSeparator = NO;
				itemCount++;
				continue;
			}
			
			[menuItem setTitle:[NSString stringWithFormat:@"%@ (unsupported on this GPU)", [menuItem title]]];
		}
	}
}

- (void) setupUserDefaults
{
	// Update input preferences.
	[viewInput loadSavedProfilesList];
	
	// Update the SPU Sync controls in the Preferences window.
	if ([[NSUserDefaults standardUserDefaults] integerForKey:@"SPU_SyncMode"] == SPU_SYNC_MODE_DUAL_SYNC_ASYNC)
	{
		[[self spuSyncMethodMenu] setEnabled:NO];
	}
	else
	{
		[[self spuSyncMethodMenu] setEnabled:YES];
	}
	
	// Set the menu for the display rotation.
	const double displayRotation = (double)[[NSUserDefaults standardUserDefaults] floatForKey:@"DisplayView_Rotation"];
	[self updateDisplayRotationMenu:displayRotation];
	
	// Set the default sound volume per user preferences.
	[self updateVolumeIcon:nil];
	
	// Set the default video settings per user preferences.
	const BOOL filtersPreferGPU = [[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_FiltersPreferGPU"];
	[[self previewView] setFiltersPreferGPU:filtersPreferGPU];
	
	const BOOL sourceDeposterize = [[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_Deposterize"];
	[[self previewView] setSourceDeposterize:sourceDeposterize];
	
	const NSInteger pixelScalerID = [[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_VideoFilter"];
	[[self previewView] setPixelScaler:pixelScalerID];
	
	const NSInteger outputFilterID = [[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_OutputFilter"];
	[[self previewView] setOutputFilter:outputFilterID];
	
	// Set up file paths.
	NSString *arm7BiosImagePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"BIOS_ARM7ImagePath"];
	if (arm7BiosImagePath != nil)
	{
		[bindings setValue:[arm7BiosImagePath lastPathComponent] forKey:@"Arm7BiosImageName"];
	}
	
	NSString *arm9BiosImagePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"BIOS_ARM9ImagePath"];
	if (arm9BiosImagePath != nil)
	{
		[bindings setValue:[arm9BiosImagePath lastPathComponent] forKey:@"Arm9BiosImageName"];
	}
	
	NSString *firmwareImagePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"Emulation_FirmwareImagePath"];
	if (firmwareImagePath != nil)
	{
		[bindings setValue:[firmwareImagePath lastPathComponent] forKey:@"FirmwareImageName"];
	}
	
	NSString *advansceneDatabasePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"Advanscene_DatabasePath"];
	if (advansceneDatabasePath != nil)
	{
		[bindings setValue:[advansceneDatabasePath lastPathComponent] forKey:@"AdvansceneDatabaseName"];
	}
	
	NSString *cheatDatabasePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"R4Cheat_DatabasePath"];
	if (cheatDatabasePath != nil)
	{
		[bindings setValue:[cheatDatabasePath lastPathComponent] forKey:@"R4CheatDatabaseName"];
	}
	
	NSString *autoloadRomPath = [[NSUserDefaults standardUserDefaults] stringForKey:@"General_AutoloadROMSelectedPath"];
	if (autoloadRomPath != nil)
	{
		[bindings setValue:[autoloadRomPath lastPathComponent] forKey:@"AutoloadRomName"];
	}
	else
	{
		[bindings setValue:NSSTRING_STATUS_NO_ROM_CHOSEN forKey:@"AutoloadRomName"];
	}
}

#pragma mark NSWindowDelegate Protocol

- (void)windowDidBecomeKey:(NSNotification *)notification
{
	[prefWindowController setContent:bindings];
	
	if ([viewInput configInputTargetID] != nil)
	{
		[[viewInput inputManager] setHidInputTarget:viewInput];
	}
}

- (void)windowWillClose:(NSNotification *)notification
{
	[viewInput setConfigInputTargetID:nil];
}

@end
