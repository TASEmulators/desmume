/*
	Copyright (C) 2012-2023 DeSmuME team

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

#import "troubleshootingWindowDelegate.h"
#import "EmuControllerDelegate.h"

#import "cocoa_util.h"
#import "cocoa_globals.h"
#import "cocoa_cheat.h"
#import "cocoa_core.h"
#import "cocoa_GPU.h"
#import "cocoa_output.h"

@implementation TroubleshootingWindowDelegate

@synthesize dummyObject;
@synthesize window;
@synthesize troubleshootingWindowController;
@synthesize romInfoController;
@synthesize emuControlController;
@synthesize cdsCoreController;
@synthesize viewSupportRequest;
@synthesize viewBugReport;
@synthesize viewFinishedForm;
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
		
	return self;
}

- (IBAction) copyRomInfoToTextFields:(id)sender
{
	NSMutableDictionary *romInfoBindings = (NSMutableDictionary *)[romInfoController content];
	NSString *romNameStr = (NSString *)[romInfoBindings valueForKey:@"romInternalName"];
	NSString *romSerialStr = (NSString *)[romInfoBindings valueForKey:@"romSerial"];
	
	[bindings setValue:romNameStr forKey:@"romName"];
	[bindings setValue:romSerialStr forKey:@"romSerial"];
}

- (IBAction) continueToFinalForm:(id)sender
{
	static NSString *unspecifiedStr = @"Unspecified"; // Do not expose localized version for this NSString -- we want this to be in English
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	// Force end of editing of any text fields.
	[window makeFirstResponder:nil];
	
	// Set final form text.
	NSString *romNameStr = (NSString *)[bindings valueForKey:@"romName"];
	if (romNameStr == nil)
	{
		romNameStr = unspecifiedStr;
	}
	
	NSString *romSerialStr = (NSString *)[bindings valueForKey:@"romSerial"];
	if (romSerialStr == nil)
	{
		romSerialStr = unspecifiedStr;
	}
	
	NSString *finalFormTextStr = @"[ BEGIN DESMUME TROUBLESHOOTING INFORMATION ]\n";
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nApp Version: "] stringByAppendingString:[[CocoaDSUtil appInternalVersionString] stringByAppendingString:[CocoaDSUtil appCompilerDetailString]]];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nOperating System: "] stringByAppendingString:[CocoaDSUtil operatingSystemString]];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nModel Identifier: "] stringByAppendingString:[CocoaDSUtil modelIdentifierString]];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nROM Name: "] stringByAppendingString:romNameStr];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nROM Serial: "] stringByAppendingString:romSerialStr];
	finalFormTextStr = [finalFormTextStr stringByAppendingString:@"\n"];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nEmulation Speed: "] stringByAppendingString:([cdsCore isSpeedLimitEnabled] ? [NSString stringWithFormat:@"%1.2fx", [emuControl lastSetSpeedScalar]] : @"Unlimited")];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nAuto Frame Skip: "] stringByAppendingString:([cdsCore isFrameSkipEnabled] ? @"YES" : @"NO")];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nSLOT-1 Device Type: "] stringByAppendingString:[cdsCore slot1DeviceTypeString]];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nSLOT-2 Device Type: "] stringByAppendingString:[cdsCore slot2DeviceTypeString]];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nAdvanced Bus-Level Timing: "] stringByAppendingString:([cdsCore emuFlagAdvancedBusLevelTiming] ? @"YES" : @"NO")];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nRigorous 3D Rendering Timing: "] stringByAppendingString:([cdsCore emuFlagRigorousTiming] ? @"YES" : @"NO")];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nCPU Emulation Engine: "] stringByAppendingString:([cdsCore cpuEmulationEngine] == CPUEmulationEngineID_DynamicRecompiler ? [NSString stringWithFormat:@"%@ (BlockSize=%li)", [cdsCore cpuEmulationEngineString], (long)[cdsCore maxJITBlockSize]] : [cdsCore cpuEmulationEngineString])];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nUse Game-Specific Hacks: "] stringByAppendingString:([cdsCore emuFlagUseGameSpecificHacks] ? @"YES" : @"NO")];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nExternal BIOS: "] stringByAppendingString:([cdsCore emuFlagUseExternalBios] ? @"YES" : @"NO")];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nExternal Firmware: "] stringByAppendingString:([cdsCore emuFlagUseExternalFirmware] ? @"YES" : @"NO")];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nGPU - Scaling Factor: "] stringByAppendingString:[NSString stringWithFormat:@"%ldx", (unsigned long)[[cdsCore cdsGPU] gpuScale]]];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nGPU - Color Depth: "] stringByAppendingString:[NSString stringWithFormat:@"%@", ([[cdsCore cdsGPU] gpuColorFormat] == NDSColorFormat_BGR555_Rev) ? @"15-bit" : (([[cdsCore cdsGPU] gpuColorFormat] == NDSColorFormat_BGR666_Rev) ? @"18-bit" : @"24-bit")]];
	
	NSString *render3DEngineDetails = [[cdsCore cdsGPU] render3DRenderingEngineString];
	switch ([[cdsCore cdsGPU] render3DRenderingEngine])
	{
		case CORE3DLIST_NULL:
			break;
			
		case CORE3DLIST_SWRASTERIZE:
			render3DEngineDetails = [NSString stringWithFormat:@"%@ (HighResColor=%@, LineHack=%@, FragmentSamplingHack=%@, ThreadCount=%@)",
									 [[cdsCore cdsGPU] render3DRenderingEngineString],
									 ([[cdsCore cdsGPU] render3DHighPrecisionColorInterpolation] ? @"YES" : @"NO"),
									 ([[cdsCore cdsGPU] render3DLineHack] ? @"YES" : @"NO"),
									 ([[cdsCore cdsGPU] render3DFragmentSamplingHack] ? @"YES" : @"NO"),
									 ([[cdsCore cdsGPU] render3DThreads] == 0 ? @"Automatic" : [NSString stringWithFormat:@"%ld", (unsigned long)[[cdsCore cdsGPU] render3DThreads]])];
			break;
			
		case CORE3DLIST_OPENGL:
			render3DEngineDetails = [NSString stringWithFormat:@"%@ (MSAA=%@, SmoothTextures=%@)",
									 [[cdsCore cdsGPU] render3DRenderingEngineString],
									 ([[cdsCore cdsGPU] render3DMultisampleSize] == 0) ? @"Off" : [NSString stringWithFormat:@"%ld", (unsigned long)[[cdsCore cdsGPU] render3DMultisampleSize]],
									 [[cdsCore cdsGPU] render3DTextureSmoothing] ? @"YES" : @"NO"];
			break;
			
		default:
			break;
	}
	
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\n3D Renderer - Engine: "] stringByAppendingString:render3DEngineDetails];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\n3D Renderer - Enable Textures: "] stringByAppendingString:([[cdsCore cdsGPU] render3DTextures] ? @"YES" : @"NO")];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\n3D Renderer - Texture Deposterize: "] stringByAppendingString:([[cdsCore cdsGPU] render3DTextureDeposterize] ? @"YES" : @"NO")];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\n3D Renderer - Texture Scaling Factor: "] stringByAppendingString:[NSString stringWithFormat:@"%ldx", (unsigned long)[[cdsCore cdsGPU] render3DTextureScalingFactor]]];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\n3D Renderer - Edge Marking: "] stringByAppendingString:([[cdsCore cdsGPU] render3DEdgeMarking] ? @"YES" : @"NO")];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\n3D Renderer - Fog: "] stringByAppendingString:([[cdsCore cdsGPU] render3DFog] ? @"YES" : @"NO")];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nVideo - Output Engine: "] stringByAppendingString:[NSString stringWithFormat:@"%s (%s)", [[cdsCore cdsGPU] fetchObject]->GetName(), [[cdsCore cdsGPU] fetchObject]->GetDescription()]];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nAudio - Output Engine: "] stringByAppendingString:[[emuControl cdsSpeaker] audioOutputEngineString]];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nAudio - Advanced SPU Logic: "] stringByAppendingString:([[emuControl cdsSpeaker] spuAdvancedLogic] ? @"YES" : @"NO")];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nAudio - Sound Interpolation Method: "] stringByAppendingString:[[emuControl cdsSpeaker] spuInterpolationModeString]];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nAudio - Sound Synchronization Method: "] stringByAppendingString:[[emuControl cdsSpeaker] spuSyncMethodString]];
	finalFormTextStr = [finalFormTextStr stringByAppendingString:@"\n"];
	finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nCheats: "] stringByAppendingString:(([cdsCore isCheatingEnabled] && ([[cdsCore cdsCheatManager] itemActiveCount] > 0)) ? [NSString stringWithFormat:@"YES (ActiveCheatCount=%ld)", (unsigned long)[[cdsCore cdsCheatManager] itemActiveCount]] : @"NO")];
	finalFormTextStr = [finalFormTextStr stringByAppendingString:@"\n"];
	
	if ([window contentView] == viewSupportRequest)
	{
		NSString *supportRequestTextStr = (NSString *)[bindings valueForKey:@"supportRequestText"];
		if (supportRequestTextStr == nil)
		{
			supportRequestTextStr = unspecifiedStr;
		}
		
		finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nSupport Request: "] stringByAppendingString:supportRequestTextStr];
		[bindings setValue:NSSTRING_HELP_COPY_PASTE_TECH_SUPPORT forKey:@"copyPasteHelpText"];
		[bindings setValue:NSSTRING_TITLE_GO_TECH_SUPPORT_WEBPAGE_TITLE forKey:@"goWebpageButtonTitle"];
	}
	else
	{
		NSString *bugReportObservedTextStr = (NSString *)[bindings valueForKey:@"bugReportObservedText"];
		if (bugReportObservedTextStr == nil)
		{
			bugReportObservedTextStr = unspecifiedStr;
		}
		
		NSString *bugReportExpectedTextStr = (NSString *)[bindings valueForKey:@"bugReportExpectedText"];
		if (bugReportExpectedTextStr == nil)
		{
			bugReportExpectedTextStr = unspecifiedStr;
		}
		
		finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\nObserved Behavior: "] stringByAppendingString:bugReportObservedTextStr];
		finalFormTextStr = [[finalFormTextStr stringByAppendingString:@"\n\nExpected Behavior: "] stringByAppendingString:bugReportExpectedTextStr];
		[bindings setValue:NSSTRING_HELP_COPY_PASTE_BUG_REPORT forKey:@"copyPasteHelpText"];
		[bindings setValue:NSSTRING_TITLE_GO_BUG_REPORT_WEBPAGE_TITLE forKey:@"goWebpageButtonTitle"];
	}
	
	finalFormTextStr = [finalFormTextStr stringByAppendingString:@"\n\n[ END DESMUME TROUBLESHOOTING INFORMATION ]"];
	
	NSDictionary *formTextAttr = [NSDictionary dictionaryWithObjectsAndKeys:
								  [NSColor controlTextColor], NSForegroundColorAttributeName,
								  nil];
	
	[bindings setValue:[[[NSAttributedString alloc] initWithString:finalFormTextStr attributes:formTextAttr] autorelease] forKey:@"finalFormText"];
	
	// Remember the current form and switch the window view.
	currentForm = [window contentView];
	[self switchContentView:viewFinishedForm];
}

- (IBAction) backForm:(id)sender
{
	[self switchContentView:currentForm];
}

- (IBAction) copyInfoToPasteboard:(id)sender
{
	NSPasteboard *pboard = [NSPasteboard generalPasteboard];
	[pboard declareTypes:[NSArray arrayWithObjects:PASTEBOARDTYPE_STRING, nil] owner:self];
	[pboard setString:[(NSAttributedString *)[bindings valueForKey:@"finalFormText"] string] forType:PASTEBOARDTYPE_STRING];
}

- (IBAction) goToWebpage:(id)sender
{
	if (currentForm == viewSupportRequest)
	{
		[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@STRING_DESMUME_TECH_SUPPORT_SITE]];
	}
	else
	{
		[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@STRING_DESMUME_BUG_REPORT_SITE]];
	}
}

- (void) switchViewByID:(TroubleshootingViewID)viewID
{
	NSView *viewToSwitch = nil;
	
	switch (viewID)
	{
		case TROUBLESHOOTING_TECH_SUPPORT_VIEW_ID:
			viewToSwitch = viewSupportRequest;
			break;
		
		case TROUBLESHOOTING_BUG_REPORT_VIEW_ID:
			viewToSwitch = viewBugReport;
			break;
			
		default:
			break;
	}
	
	if (viewToSwitch == nil)
	{
		return;
	}
	
	[self clearAllText];
	[self switchContentView:viewToSwitch];
}

- (void) clearAllText
{
	[bindings removeObjectForKey:@"romName"];
	[bindings removeObjectForKey:@"romSerial"];
	[bindings removeObjectForKey:@"supportRequestText"];
	[bindings removeObjectForKey:@"bugReportObservedText"];
	[bindings removeObjectForKey:@"bugReportExpectedText"];
	[bindings removeObjectForKey:@"finalFormText"];
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

- (void)windowDidBecomeKey:(NSNotification *)notification
{
	[troubleshootingWindowController setContent:bindings];
}

@end
