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

#import "screenshot.h"
#import "nds_control.h"
#import "screen_state.h"

@interface ScreenshotSaveDelegate : NSObject
{
	NSSavePanel *my_panel;
}
- (id)init;
- (void)setPanel:(NSSavePanel*)panel;
- (void)dealloc;
- (void)buttonPressed:(id)sender;
@end

@implementation ScreenshotSaveDelegate
- (id)init
{
	self = [super init];
	if(self == nil)return self;

	my_panel = nil;

	return self;
}

- (void)setPanel:(NSSavePanel*)panel;
{
	[my_panel release];
	my_panel = panel;
	[my_panel retain];
}

- (void)dealloc
{
	[my_panel release];
	[super dealloc];
}

- (void)buttonPressed:(id)sender
{
	if([sender indexOfSelectedItem] == 0)
	{
		[my_panel setAllowsOtherFileTypes:NO];
		[my_panel setAllowedFileTypes:[NSArray arrayWithObjects:@"bmp", @"gif", @"jpg", @"jpeg", @"png", @"tiff", nil]];
	} else if([sender indexOfSelectedItem] == 2)
	{
		[my_panel setAllowsOtherFileTypes:YES];
		[my_panel setAllowedFileTypes:[NSArray arrayWithObjects:@"gif", nil]];
	} else if([sender indexOfSelectedItem] == 3)
	{
		[my_panel setAllowsOtherFileTypes:YES];
		[my_panel setAllowedFileTypes:[NSArray arrayWithObjects:@"jpg", @"jpeg", nil]];
	} else if([sender indexOfSelectedItem] == 4)
	{
		[my_panel setAllowsOtherFileTypes:YES];
		[my_panel setAllowedFileTypes:[NSArray arrayWithObjects:@"png", nil]];
	} else if([sender indexOfSelectedItem] == 5)
	{
		[my_panel setAllowsOtherFileTypes:YES];
		[my_panel setAllowedFileTypes:[NSArray arrayWithObjects:@"tiff", nil]];
	} else
	{
		[my_panel setAllowsOtherFileTypes:YES];
		[my_panel setAllowedFileTypes:[NSArray arrayWithObjects:@"bmp", nil]];
	}

	[my_panel validateVisibleColumns];
}

@end

@implementation Screenshot
+ (void)saveScreenshotAs:(const ScreenState*)screen
{
	[screen retain];

	//create a save panel ------------------------------------

	ScreenshotSaveDelegate *save_delegate = [[ScreenshotSaveDelegate alloc] init];

	NSSavePanel *panel = [NSSavePanel savePanel];
	[panel retain];

	[panel setTitle:NSLocalizedString(@"Save Screenshot to File...", nil)];
	//[panel setCanSelectHiddenExtension:YES];

	//create the accessory view ------------------------------

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
	NSRect temp = [format_text frame]; //center
	temp.origin.y = 26.0 / 2.0 - (float)[format_text frame].size.height / 2.0;
	[format_text setFrame:temp];

	NSPopUpButton *format_button = [[NSPopUpButton alloc] initWithFrame:NSMakeRect([format_text frame].size.width,0,200,26) pullsDown:NO];
	[format_button addItemWithTitle:NSLocalizedString(@"Pick by Extension", nil)];
	[format_button addItemWithTitle:NSLocalizedString(@"BMP", nil)];
	[format_button addItemWithTitle:NSLocalizedString(@"GIF", nil)];
	[format_button addItemWithTitle:NSLocalizedString(@"JPG", nil)];
	[format_button addItemWithTitle:NSLocalizedString(@"PNG", nil)];
	[format_button addItemWithTitle:NSLocalizedString(@"TIFF", nil)];
	[format_button setAction:@selector(buttonPressed:)];
	[format_button setTarget:save_delegate];

	NSView *format_selection = [[NSView alloc] initWithFrame:NSMakeRect(0,0,[format_text frame].size.width + [format_button frame].size.width,26)];

	[format_selection addSubview:format_text];
	[format_text release];

	[format_selection addSubview:format_button];

	[panel setAccessoryView:format_selection];
	[format_selection release];

	[save_delegate setPanel:panel];

	//run the save panel -------------------------------------

	if([panel runModalForDirectory:nil file:nil] == NSFileHandlingPanelOKButton)
	{
		//save the image -------------------------------------

		//get the file type

		NSBitmapImageFileType type = NSBMPFileType;

		if([format_button indexOfSelectedItem] == 0)
		{
			NSString *ext = [[panel filename] pathExtension];

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

		//tell cocoa save the file
		NSBitmapImageRep *image = [screen imageRep];
		if(image == nil)
			messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't create the screenshot image");
		else
			[[image representationUsingType:type properties:[NSDictionary dictionary]]
			writeToFile:[panel filename] atomically:NO];

	}

	//
	[format_button release];
	[panel release];
	[save_delegate release];
	[screen release];
}

@end
