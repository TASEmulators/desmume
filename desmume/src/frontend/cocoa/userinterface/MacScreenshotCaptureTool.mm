/*
	Copyright (C) 2017 DeSmuME team
 
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

#include <pthread.h>

#import "../cocoa_file.h"
#import "../cocoa_GPU.h"
#import "../cocoa_globals.h"
#import "MacScreenshotCaptureTool.h"
#import "MacOGLDisplayView.h"

#ifdef ENABLE_APPLE_METAL
#include "MacMetalDisplayView.h"
#endif

@implementation MacScreenshotCaptureToolDelegate

@synthesize window;
@synthesize sharedData;
@synthesize saveDirectoryPath;
@synthesize romName;
@synthesize fileFormat;
@synthesize displayMode;
@synthesize displayLayout;
@synthesize displayOrder;
@synthesize displaySeparation;
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
	
	sharedData = nil;
	saveDirectoryPath = nil;
	romName = @"No_ROM_loaded";
	
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

- (IBAction) takeScreenshot:(id)sender
{
	NSString *savePath = [self saveDirectoryPath];
	
	if ( (savePath != nil) && ([savePath length] > 0) )
	{
		savePath = [savePath stringByExpandingTildeInPath];
	}
	else
	{
		[self chooseDirectoryPath:self];
	}
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	BOOL isDirectoryFound = [fileManager createDirectoryAtPath:savePath withIntermediateDirectories:YES attributes:nil error:nil];
	[fileManager release];
	
	if (!isDirectoryFound)
	{
		[self chooseDirectoryPath:self];
		
		isDirectoryFound = [fileManager createDirectoryAtPath:savePath withIntermediateDirectories:YES attributes:nil error:nil];
		if (!isDirectoryFound)
		{
			// This was the last chance for the user to try to get a working writable directory.
			// If the directory is still invalid, then just bail.
			return;
		}
	}
	
	// Note: We're allocating the parameter's memory block here, but we will be freeing it once we copy it in the detached thread.
	MacScreenshotCaptureToolParams *param = new MacScreenshotCaptureToolParams;
	param->sharedData				= [self sharedData];
	param->fileFormat				= (NSBitmapImageFileType)[self fileFormat];
	param->savePath					= std::string([savePath cStringUsingEncoding:NSUTF8StringEncoding]);
	param->romName					= std::string([romName cStringUsingEncoding:NSUTF8StringEncoding]);
	param->useDeposterize			= [self useDeposterize] ? true : false;
	param->outputFilterID			= (OutputFilterTypeID)[self outputFilterID];
	param->pixelScalerID			= (VideoFilterTypeID)[self pixelScalerID];
	
	param->cdpProperty.mode			= (ClientDisplayMode)[self displayMode];
	param->cdpProperty.layout		= (ClientDisplayLayout)[self displayLayout];
	param->cdpProperty.order		= (ClientDisplayOrder)[self displayOrder];
	param->cdpProperty.gapScale		= (float)[self displaySeparation] / 100.0f;
	param->cdpProperty.rotation		= (float)[self displayRotation];
	
	pthread_t fileWriteThread;
	pthread_attr_t fileWriteThreadAttr;
	
	pthread_attr_init(&fileWriteThreadAttr);
	pthread_attr_setdetachstate(&fileWriteThreadAttr, PTHREAD_CREATE_DETACHED);
	pthread_create(&fileWriteThread, &fileWriteThreadAttr, &RunFileWriteThread, param);
	pthread_attr_destroy(&fileWriteThreadAttr);
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

- (void) readUserDefaults
{
	[self setSaveDirectoryPath:[[NSUserDefaults standardUserDefaults] stringForKey:@"ScreenshotCaptureTool_DirectoryPath"]];
	[self setFileFormat:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_FileFormat"]];
	[self setUseDeposterize:[[NSUserDefaults standardUserDefaults] boolForKey:@"ScreenshotCaptureTool_Deposterize"]];
	[self setOutputFilterID:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_OutputFilter"]];
	[self setPixelScalerID:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_PixelScaler"]];
	[self setDisplayMode:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_DisplayMode"]];
	[self setDisplayLayout:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_DisplayLayout"]];
	[self setDisplayOrder:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_DisplayOrder"]];
	[self setDisplaySeparation:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_DisplaySeparation"]];
	[self setDisplayRotation:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_DisplayRotation"]];
}

- (void) writeUserDefaults
{
	[[NSUserDefaults standardUserDefaults] setObject:[self saveDirectoryPath] forKey:@"ScreenshotCaptureTool_DirectoryPath"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self fileFormat] forKey:@"ScreenshotCaptureTool_FileFormat"];
	[[NSUserDefaults standardUserDefaults] setBool:[self useDeposterize] forKey:@"ScreenshotCaptureTool_Deposterize"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self outputFilterID] forKey:@"ScreenshotCaptureTool_OutputFilter"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self pixelScalerID] forKey:@"ScreenshotCaptureTool_PixelScaler"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displayMode] forKey:@"ScreenshotCaptureTool_DisplayMode"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displayLayout] forKey:@"ScreenshotCaptureTool_DisplayLayout"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displayOrder] forKey:@"ScreenshotCaptureTool_DisplayOrder"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displaySeparation] forKey:@"ScreenshotCaptureTool_DisplaySeparation"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displayRotation] forKey:@"ScreenshotCaptureTool_DisplayRotation"];
}

@end

static void* RunFileWriteThread(void *arg)
{
	// Copy the rendering properties from the calling thread.
	MacScreenshotCaptureToolParams *inParams = (MacScreenshotCaptureToolParams *)arg;
	MacScreenshotCaptureToolParams param;
	
	param.sharedData				= inParams->sharedData;
	param.fileFormat				= inParams->fileFormat;
	param.savePath					= inParams->savePath;
	param.romName					= inParams->romName;
	param.useDeposterize			= inParams->useDeposterize;
	param.outputFilterID			= inParams->outputFilterID;
	param.pixelScalerID				= inParams->pixelScalerID;
	
	param.cdpProperty.mode			= inParams->cdpProperty.mode;
	param.cdpProperty.layout		= inParams->cdpProperty.layout;
	param.cdpProperty.order			= inParams->cdpProperty.order;
	param.cdpProperty.gapScale		= inParams->cdpProperty.gapScale;
	param.cdpProperty.rotation		= inParams->cdpProperty.rotation;
	
	delete inParams;
	inParams = NULL;
	
	// Do a few sanity checks before proceeding.
	if (param.sharedData == nil)
	{
		return NULL;
	}
	
	GPUClientFetchObject *fetchObject = [param.sharedData GPUFetchObject];
	if (fetchObject == NULL)
	{
		return NULL;
	}
	
	const NDSDisplayInfo &displayInfo = fetchObject->GetFetchDisplayInfoForBufferIndex( fetchObject->GetLastFetchIndex() );
	
	if ( (displayInfo.renderedWidth[NDSDisplayID_Main]  == 0) || (displayInfo.renderedHeight[NDSDisplayID_Main]  == 0) ||
		 (displayInfo.renderedWidth[NDSDisplayID_Touch] == 0) || (displayInfo.renderedHeight[NDSDisplayID_Touch] == 0) )
	{
		return NULL;
	}
	
	// Set up the rendering properties.
	ClientDisplayPresenter::CalculateNormalSize(param.cdpProperty.mode, param.cdpProperty.layout, param.cdpProperty.gapScale, param.cdpProperty.normalWidth, param.cdpProperty.normalHeight);
	double transformedWidth = param.cdpProperty.normalWidth;
	double transformedHeight = param.cdpProperty.normalHeight;
	
	param.cdpProperty.viewScale		= (double)displayInfo.customWidth / (double)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	ClientDisplayPresenter::ConvertNormalToTransformedBounds(param.cdpProperty.viewScale, param.cdpProperty.rotation, transformedWidth, transformedHeight);
	
	param.cdpProperty.clientWidth	= transformedWidth;
	param.cdpProperty.clientHeight	= transformedHeight;
	param.cdpProperty.gapDistance	= (double)DS_DISPLAY_UNSCALED_GAP * param.cdpProperty.gapScale;
	
	// Render the frame to an NSBitmapImageRep.
	NSAutoreleasePool *autoreleasePool = [[NSAutoreleasePool alloc] init];
	
	NSUInteger w = param.cdpProperty.clientWidth;
	NSUInteger h = param.cdpProperty.clientHeight;
	
	NSBitmapImageRep *newImageRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																			pixelsWide:w
																			pixelsHigh:h
																		 bitsPerSample:8
																	   samplesPerPixel:4
																			  hasAlpha:YES
																			  isPlanar:NO
																		colorSpaceName:NSCalibratedRGBColorSpace
																		   bytesPerRow:w * 4
																		  bitsPerPixel:32];
	if (newImageRep == nil)
	{
		[autoreleasePool release];
		return NULL;
	}
	
	ClientDisplay3DPresenter *cdp = NULL;
	bool filtersPreferGPU = true;
	
#ifdef ENABLE_APPLE_METAL
	if ([param.sharedData isKindOfClass:[MetalDisplayViewSharedData class]])
	{
		if ([(MetalDisplayViewSharedData *)param.sharedData device] == nil)
		{
			[newImageRep release];
			[autoreleasePool release];
			return NULL;
		}
		
		cdp = new MacMetalDisplayPresenter(param.sharedData);
	}
	else
#endif
	{
		cdp = new MacOGLDisplayPresenter(param.sharedData);
		filtersPreferGPU = false; // Just in case we're capturing the screenshot on an older GPU, perform the filtering on the CPU to avoid potential issues.
	}
	
	cdp->Init();
	cdp->SetFiltersPreferGPU(filtersPreferGPU);
	cdp->SetHUDVisibility(false);
	cdp->CommitPresenterProperties(param.cdpProperty);
	cdp->SetupPresenterProperties();
	cdp->SetSourceDeposterize(param.useDeposterize);
	
	if ( (cdp->GetViewScale() > 0.999) && (cdp->GetViewScale() < 1.001) )
	{
		// Special case stuff for when capturing the screenshot at the native resolution.
		if ( (param.cdpProperty.layout == ClientDisplayLayout_Vertical) || (param.cdpProperty.layout == ClientDisplayLayout_Horizontal) )
		{
			cdp->SetOutputFilter(OutputFilterTypeID_NearestNeighbor);
		}
		else
		{
			cdp->SetOutputFilter(param.outputFilterID);
		}
	}
	else
	{
		// For custom-sized resolutions, apply all the filters as normal.
		cdp->SetPixelScaler(param.pixelScalerID);
		cdp->SetOutputFilter(param.outputFilterID);
	}
	
	cdp->LoadDisplays();
	cdp->ProcessDisplays();
	cdp->UpdateLayout();
	cdp->CopyFrameToBuffer((uint32_t *)[newImageRep bitmapData]);
	
	// Write the file.
	NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
	[dateFormatter setDateFormat:@"yyyyMMdd_HH-mm-ss.SSS "];
	NSString *fileName = [[dateFormatter stringFromDate:[NSDate date]] stringByAppendingString:[NSString stringWithCString:param.romName.c_str() encoding:NSUTF8StringEncoding]];
	[dateFormatter release];
	
	NSString *savePath = [NSString stringWithCString:param.savePath.c_str() encoding:NSUTF8StringEncoding];
	NSURL *fileURL = [NSURL fileURLWithPath:[savePath stringByAppendingPathComponent:fileName]];
	[CocoaDSFile saveScreenshot:fileURL bitmapData:newImageRep fileType:param.fileFormat];
	
	// Clean up.
	delete cdp;
	
	[newImageRep release];
	[autoreleasePool release];
	
	return NULL;
}
