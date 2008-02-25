/*  Copyright (C) 2007 Jeff Bland

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#import <Cocoa/Cocoa.h>

//DeSmuME Cocoa includes
#import "globals.h"
#import "screenshot.h"
#import "main_window.h"
#import "nds_control.h"

//DeSmuME general includes
#define OBJ_C
#include "../MMU.h"
#include "../GPU.h"
#undef BOOL

#define BUTTON_HEIGHT 26.
#define RESIZE_CONTROL_WIDTH 12.
#define NUM_BUTTONS 2.

@interface Screenshot (delegate)
- (void)windowDidResize:(NSNotification*)aNotification;
- (void)saveButtonPressed;
- (void)updateButtonPressed;
- (BOOL)panel:(id)sender isValidFilename:(NSString*)filename;
- (BOOL)panel:(id)sender shouldShowFilename:(NSString *)filename;
@end

@implementation Screenshot (delegate)
- (void)windowDidResize:(NSNotification*)aNotification
{
	NSRect content_frame = [[window contentView] frame];
	[image_view setFrame:NSMakeRect(0, BUTTON_HEIGHT, content_frame.size.width, content_frame.size.height - BUTTON_HEIGHT)];

	float button_width = (content_frame.size.width - RESIZE_CONTROL_WIDTH) / NUM_BUTTONS;
	[save_button setFrame:NSMakeRect(0, 0, button_width, BUTTON_HEIGHT)];
	[update_button setFrame:NSMakeRect(button_width, 0, button_width, BUTTON_HEIGHT)];
}

- (void)saveButtonPressed
{//localize
	NSSavePanel *panel = [NSSavePanel savePanel];

	//Create format selection list
	NSRect rect;
	rect.size.width = 130;
	rect.size.height = 26;
	rect.origin.x = 230;
	rect.origin.y = 200;

	[panel setTitle:NSLocalizedString(@"Save Screenshot to File", nil)];
	//[panel setAllowedFileTypes:[NSArray arrayWithObjects:@"bmp",@"gif",nil]];

	if(!format_selection)
	{
		NSFont *font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSRegularControlSize]];

		NSTextView *format_text = [[NSTextView alloc] initWithFrame:NSMakeRect(0,0,500,500)];
		[format_text setFont:font];
		[format_text setEditable:NO];
		[format_text setDrawsBackground:NO];
		[format_text setMaxSize:NSMakeSize(500,26)];
		[format_text setMinSize:NSMakeSize(1,1)];
		[format_text setHorizontallyResizable:YES];
		[format_text setVerticallyResizable:YES];
		[format_text setString:NSLocalizedString(@"Select Image Format: ", nil)];
		[format_text sizeToFit];

		//center vertically
		NSRect temp = [format_text frame];
		temp.origin.y = 26.0 / 2.0 - (float)[format_text frame].size.height / 2.0;
		[format_text setFrame:temp];

		format_button = [[NSPopUpButton alloc] initWithFrame:NSMakeRect([format_text frame].size.width,0,200,26) pullsDown:NO];
		[format_button addItemWithTitle:NSLocalizedString(@"Pick by Extension", nil)];
		[format_button addItemWithTitle:NSLocalizedString(@"BMP", nil)];
		[format_button addItemWithTitle:NSLocalizedString(@"GIF", nil)];
		[format_button addItemWithTitle:NSLocalizedString(@"JPG", nil)];
		[format_button addItemWithTitle:NSLocalizedString(@"PNG", nil)];
		[format_button addItemWithTitle:NSLocalizedString(@"TIFF", nil)];
		//[format_button setAction:@selector(??)];
		//[format_button setTarget:self];

		format_selection = [[NSView alloc] initWithFrame:NSMakeRect(0,0,[format_text frame].size.width + [format_button frame].size.width,26)];
		[format_selection addSubview:format_button];
		[format_selection addSubview:format_text];

	}

	[panel setAccessoryView:format_selection];

	[panel setDelegate:self];

	if(window)
		[panel beginSheetForDirectory:@"" file:@"Screenshot.bmp" modalForWindow:window modalDelegate:self
		didEndSelector:nil contextInfo:nil];
	else
		[panel runModal];

}

- (void)updateButtonPressed
{
	BOOL was_paused = paused;
	[NDS pause];

	u8 *bitmap_data = [image_rep bitmapData];

	const u16 *buffer_16 = (const u16*)[main_window getBuffer];

	int i;
	for(i = 0; i <  [image_rep size].width * [image_rep size].height; i++)
	{ //this loop we go through pixel by pixel and convert from 16bit to 24bit for the NSImage
		*(bitmap_data++) = (*buffer_16 & 0x001F) <<  3;
		*(bitmap_data++) = (*buffer_16 & 0x03E0) >> 5 << 3;
		*(bitmap_data++) = (*buffer_16 & 0x7C00) >> 10 << 3;
		buffer_16++;
	}

	//there seems to be issues updating the image when
	//it is scaled, probably due to some internal caching of the scaled version.
	//resetting the image here to ensure that it updates properly.
	[image_view setImage:nil];
	[image_view setImage:image];

	//tell the image to redraw [soon]
	[image_view setNeedsDisplay:YES];

	if(!was_paused)[NDS execute];
}

- (BOOL)panel:(id)sender isValidFilename:(NSString*)filename
{
	NSBitmapImageFileType type = NSBMPFileType;

	if([format_button indexOfSelectedItem] == 0)
	{
		NSString *ext = [filename pathExtension];

		if([ext caseInsensitiveCompare:@"gif"] == NSOrderedSame)
			type = NSGIFFileType;
		else if([ext caseInsensitiveCompare:@"jpg"] == NSOrderedSame)
			type = NSJPEGFileType;
		else if([ext caseInsensitiveCompare:@"jpeg"] == NSOrderedSame)
			type = NSJPEGFileType;
		else if([ext caseInsensitiveCompare:@"png"] == NSOrderedSame)
			type = NSPNGFileType;
		else if([ext caseInsensitiveCompare:@"tiff"] == NSOrderedSame)
			type = NSTIFFFileType;

	}
	if([format_button indexOfSelectedItem] == 2)
		type = NSGIFFileType;
	if([format_button indexOfSelectedItem] == 3)
		type = NSJPEGFileType;
	if([format_button indexOfSelectedItem] == 4)
		type = NSPNGFileType;
	if([format_button indexOfSelectedItem] == 5)
		type = NSTIFFFileType;

	[[image_rep representationUsingType:type properties:[NSDictionary dictionary]]
	writeToFile:filename atomically:NO];

	return YES;
}

- (BOOL)panel:(id)sender shouldShowFilename:(NSString *)filename
{
	NSString *ext = [filename pathExtension];
	int index = [format_button indexOfSelectedItem];

	if((index == 1) || (index == 0))
		if([ext caseInsensitiveCompare:@"bmp"] == NSOrderedSame)
			return YES;

	if((index == 2) || (index == 0))
		if([ext caseInsensitiveCompare:@"gif"] == NSOrderedSame)
			return YES;

	if((index == 3) || (index == 0))
		if(([ext caseInsensitiveCompare:@"jpg"] == NSOrderedSame) || ([ext caseInsensitiveCompare:@"jpeg"] == NSOrderedSame))
			return YES;

	if((index == 4) || (index == 0))
		if([ext caseInsensitiveCompare:@"png"] == NSOrderedSame)
			return YES;

	if((index == 5) || (index == 0))
		if([ext caseInsensitiveCompare:@"tiff"] == NSOrderedSame)
			return YES;

	return NO;

}

@end

@implementation Screenshot

- (id)initWithBuffer:(volatile const u8*)buffer rotation:(u8)rotation saveOnly:(BOOL)save_only
{
	self = [super init];

	//this gets set upon the first save panel
	format_selection = nil;
	window = nil;

	NSRect rect;
	if(rotation == ROTATION_0 || rotation == ROTATION_180)
	{
		rect.size.width = DS_SCREEN_WIDTH;
		rect.size.height = DS_SCREEN_HEIGHT_COMBINED;
	} else
	{
		rect.size.width = DS_SCREEN_HEIGHT_COMBINED;
		rect.size.height = DS_SCREEN_WIDTH;
	}
	rect.origin.x = 200;
	rect.origin.y = 200;

	//create the image
	image_rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
	pixelsWide:rect.size.width
	pixelsHigh:rect.size.height
	bitsPerSample:8
	samplesPerPixel:3
	hasAlpha:NO
	isPlanar:NO
	colorSpaceName:NSCalibratedRGBColorSpace
	bytesPerRow:rect.size.width * 3
	bitsPerPixel:24];

	if(!image_rep)
	{
		messageDialog(NSLocalizedString(@"Error", nil), @"Could not create NSBitmapImageRep for screenshot");
		return nil;
	}

	u8 *bitmap_data = [image_rep bitmapData];

	u16 *buffer_16 = (u16*)buffer;
	int i;
	for(i = 0; i < rect.size.width * rect.size.height; i++)
	{ //this loop we go through pixel by pixel and convert from 16bit to 24bit for the NSImage
		*(bitmap_data++) = (*buffer_16 & 0x001F) <<  3;
		*(bitmap_data++) = (*buffer_16 & 0x03E0) >> 5 << 3;
		*(bitmap_data++) = (*buffer_16 & 0x7C00) >> 10 << 3;
		buffer_16++;
	}

	if(save_only)
	{
		[self saveButtonPressed];

	} else
	{

		//create the image
		image = [[NSImage alloc] initWithSize:NSMakeSize(rect.size.width, rect.size.height)];

		if(!image)
		{
			messageDialog(NSLocalizedString(@"Error", nil), @"Could not create NSImage for screenshot window");
			return nil;
		}

		[image setBackgroundColor:[NSColor whiteColor]];
		[image addRepresentation:image_rep];

		//create the image view
		image_view = [[NSImageView alloc] initWithFrame:rect];

		if(!image_view)
		{
			messageDialog(NSLocalizedString(@"Error", nil), @"Could not create NSImageView for screenshot window");
			return nil;
		}

		[image_view setImage:image];
		[image_view setImageScaling:NSScaleToFit];

		//create the save button
		save_button = [[NSButton alloc] initWithFrame:rect];

		if(!save_button)
		{
			messageDialog(NSLocalizedString(@"Error", nil), @"Could not create save button for screenshot window");
			return nil;
		}

		[save_button setBezelStyle:NSRoundedBezelStyle];
		[save_button setTitle:NSLocalizedString(@"Save Screenshot", nil)];
		[save_button setAction:@selector(saveButtonPressed)];
		[save_button setTarget:self];

		//update
		update_button = [[NSButton alloc] initWithFrame:rect];

		if(!update_button)
		{
			messageDialog(NSLocalizedString(@"Error", nil), @"Could not create update button for screenshot window");
			return nil;
		}

		[update_button setBezelStyle:NSRoundedBezelStyle];
		[update_button setTitle:NSLocalizedString(@"Update Screenshot", nil)];
		[update_button setAction:@selector(updateButtonPressed)];
		[update_button setTarget:self];

		//create a window
		rect.size.height += BUTTON_HEIGHT;
		window = [[NSWindow alloc] initWithContentRect:rect styleMask:
		NSTitledWindowMask|NSClosableWindowMask|NSResizableWindowMask backing:NSBackingStoreBuffered defer:NO screen:nil];

		//set the window title
		[window setTitle:NSLocalizedString(@"DeSmuME Screenshot", nil)];

		//set the window delegate
		[window setDelegate:self];

		//add the items to the window
		[[window contentView] addSubview:image_view];
		[[window contentView] addSubview:save_button];
		[[window contentView] addSubview:update_button];

		//size the stuff (by invoking our resize callback)
		[self windowDidResize:nil];

		//show the window
		controller = [[NSWindowController alloc] initWithWindow:window];
		[controller showWindow:nil];
	}

	return self;
}

- (void)dealloc
{
	[image_rep release];
	[window release];
	[controller release];

/*
	NSImage *image;
	NSImageView *image_view;
	NSButton *save_button;
	NSButton *update_button;
	NSBitmapImageRep *image_rep;
	NSView *format_selection;
	NSPopUpButton *format_button;
*/
	[super dealloc];
}

@end
