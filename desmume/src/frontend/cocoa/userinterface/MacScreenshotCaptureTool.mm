/*
	Copyright (C) 2017-2018 DeSmuME team
 
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

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	formatID = NSTIFFFileType;
	
	return self;
}

- (IBAction) takeScreenshot:(id)sender
{
	NSString *savePath = [self saveDirectoryPath];
	
	// Check for the existence of the target writable directory. Cancel the take screenshot operation
	// if the directory does not exist or is not writable.
	if ( (savePath != nil) && ([savePath length] > 0) )
	{
		savePath = [savePath stringByExpandingTildeInPath];
	}
	else
	{
		[self chooseDirectoryPath:self];
		return;
	}
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	BOOL isDirectoryFound = [fileManager createDirectoryAtPath:savePath withIntermediateDirectories:YES attributes:nil error:nil];
	
	if (!isDirectoryFound)
	{
		[self chooseDirectoryPath:self];
		[fileManager release];
		return;
	}
	
	[fileManager release];
	
	// Note: We're allocating the parameter's memory block here, but we will be freeing it once we copy it in the detached thread.
	MacCaptureToolParams *param = new MacCaptureToolParams;
	param->refObject				= NULL;
	param->sharedData				= [self sharedData];
	param->formatID					= [self formatID];
	param->savePath					= std::string([savePath cStringUsingEncoding:NSUTF8StringEncoding]);
	param->romName					= std::string([romName cStringUsingEncoding:NSUTF8StringEncoding]);
	param->useDeposterize			= [self useDeposterize] ? true : false;
	param->outputFilterID			= (OutputFilterTypeID)[self outputFilterID];
	param->pixelScalerID			= (VideoFilterTypeID)[self pixelScalerID];
	
	param->cdpProperty.mode			= (ClientDisplayMode)[self displayMode];
	param->cdpProperty.layout		= (ClientDisplayLayout)[self displayLayout];
	param->cdpProperty.order		= (ClientDisplayOrder)[self displayOrder];
	param->cdpProperty.gapScale		= (double)[self displaySeparation] / 100.0;
	param->cdpProperty.rotation		= (double)[self displayRotation];
	param->cdpProperty.viewScale	= (double)[self displayScale] / 100.0;
	
	pthread_t fileWriteThread;
	pthread_attr_t fileWriteThreadAttr;
	
	pthread_attr_init(&fileWriteThreadAttr);
	pthread_attr_setdetachstate(&fileWriteThreadAttr, PTHREAD_CREATE_DETACHED);
	pthread_create(&fileWriteThread, &fileWriteThreadAttr, &RunFileWriteThread, param);
	pthread_attr_destroy(&fileWriteThreadAttr);
}

- (void) readUserDefaults
{
	[self setSaveDirectoryPath:[[NSUserDefaults standardUserDefaults] stringForKey:@"ScreenshotCaptureTool_DirectoryPath"]];
	[self setFormatID:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_FileFormat"]];
	[self setUseDeposterize:[[NSUserDefaults standardUserDefaults] boolForKey:@"ScreenshotCaptureTool_Deposterize"]];
	[self setOutputFilterID:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_OutputFilter"]];
	[self setPixelScalerID:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_PixelScaler"]];
	[self setDisplayMode:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_DisplayMode"]];
	[self setDisplayLayout:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_DisplayLayout"]];
	[self setDisplayOrder:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_DisplayOrder"]];
	[self setDisplaySeparation:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_DisplaySeparation"]];
	[self setDisplayScale:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_DisplayScale"]];
	[self setDisplayRotation:[[NSUserDefaults standardUserDefaults] integerForKey:@"ScreenshotCaptureTool_DisplayRotation"]];
}

- (void) writeUserDefaults
{
	[[NSUserDefaults standardUserDefaults] setObject:[self saveDirectoryPath] forKey:@"ScreenshotCaptureTool_DirectoryPath"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self formatID] forKey:@"ScreenshotCaptureTool_FileFormat"];
	[[NSUserDefaults standardUserDefaults] setBool:[self useDeposterize] forKey:@"ScreenshotCaptureTool_Deposterize"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self outputFilterID] forKey:@"ScreenshotCaptureTool_OutputFilter"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self pixelScalerID] forKey:@"ScreenshotCaptureTool_PixelScaler"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displayMode] forKey:@"ScreenshotCaptureTool_DisplayMode"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displayLayout] forKey:@"ScreenshotCaptureTool_DisplayLayout"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displayOrder] forKey:@"ScreenshotCaptureTool_DisplayOrder"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displaySeparation] forKey:@"ScreenshotCaptureTool_DisplaySeparation"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displayScale] forKey:@"ScreenshotCaptureTool_DisplayScale"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displayRotation] forKey:@"ScreenshotCaptureTool_DisplayRotation"];
}

@end

static void* RunFileWriteThread(void *arg)
{
	// Copy the rendering properties from the calling thread.
	MacCaptureToolParams *inParams = (MacCaptureToolParams *)arg;
	MacCaptureToolParams param;
	
	param.refObject					= inParams->refObject;
	param.sharedData				= inParams->sharedData;
	param.formatID					= inParams->formatID;
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
	param.cdpProperty.viewScale		= inParams->cdpProperty.viewScale;
	
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
	
	const u8 lastBufferIndex = fetchObject->GetLastFetchIndex();
	const NDSDisplayInfo &displayInfo = fetchObject->GetFetchDisplayInfoForBufferIndex(lastBufferIndex);
	
	if ( (displayInfo.renderedWidth[NDSDisplayID_Main]  == 0) || (displayInfo.renderedHeight[NDSDisplayID_Main]  == 0) ||
		 (displayInfo.renderedWidth[NDSDisplayID_Touch] == 0) || (displayInfo.renderedHeight[NDSDisplayID_Touch] == 0) )
	{
		return NULL;
	}
	
	// Set up the rendering properties.
	ClientDisplayPresenter::CalculateNormalSize(param.cdpProperty.mode, param.cdpProperty.layout, param.cdpProperty.gapScale, param.cdpProperty.normalWidth, param.cdpProperty.normalHeight);
	double transformedWidth = param.cdpProperty.normalWidth;
	double transformedHeight = param.cdpProperty.normalHeight;
	double framebufferScale = (double)displayInfo.customWidth / (double)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	
	// If the scaling is 0, then calculate the scaling based on the incoming framebuffer size.
	if (param.cdpProperty.viewScale == 0)
	{
		param.cdpProperty.viewScale = framebufferScale;
	}
	
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
	bool isUsingMetal = false;
	
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
		isUsingMetal = true;
	}
	else
#endif
	{
		cdp = new MacOGLDisplayPresenter(param.sharedData);
	}
	
	cdp->Init();
	cdp->SetFiltersPreferGPU(filtersPreferGPU);
	cdp->SetHUDVisibility(false);
	cdp->CommitPresenterProperties(param.cdpProperty);
	cdp->SetupPresenterProperties();
	cdp->SetSourceDeposterize(param.useDeposterize);
	
	if ( (cdp->GetViewScale() > 0.999) && (cdp->GetViewScale() < 1.001) )
	{
		// In the special case of capturing a native-size framebuffer at its native size, don't use any filters for the sake of accuracy.
		if ( (param.cdpProperty.layout == ClientDisplayLayout_Vertical) || (param.cdpProperty.layout == ClientDisplayLayout_Horizontal) ||
			 ((framebufferScale > 0.999) && (framebufferScale < 1.001)) )
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
		cdp->SetOutputFilter(param.outputFilterID);
		cdp->SetPixelScaler(param.pixelScalerID);
		
		// Metal presenters are always assumed to filter on the GPU, so only do this for OpenGL.
		if (!isUsingMetal && !cdp->WillFilterOnGPU() && (param.pixelScalerID != VideoFilterTypeID_None))
		{
			if ( (param.cdpProperty.mode == ClientDisplayMode_Main) || (param.cdpProperty.mode == ClientDisplayMode_Dual) )
			{
				((OGLClientFetchObject *)fetchObject)->FetchNativeDisplayToSrcClone(NDSDisplayID_Main, lastBufferIndex, true);
			}
			
			if ( (param.cdpProperty.mode == ClientDisplayMode_Touch) || (param.cdpProperty.mode == ClientDisplayMode_Dual) )
			{
				((OGLClientFetchObject *)fetchObject)->FetchNativeDisplayToSrcClone(NDSDisplayID_Touch, lastBufferIndex, true);
			}
		}
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
	[CocoaDSFile saveScreenshot:fileURL bitmapData:newImageRep fileType:param.formatID];
	
	// Clean up.
	delete cdp;
	
	[newImageRep release];
	[autoreleasePool release];
	
	return NULL;
}
