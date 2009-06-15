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

#import "globals.h"

const CGFloat inner_padding = 3;
const CGFloat outer_padding = 3;
const CGFloat tab_view_height = 240;
const CGFloat icon_size = 130;
const CGFloat window_width = 465;

NSTextField *about_name;
NSTextField *about_description;
NSTextField *about_version;
NSTextField *about_date;
NSTextField *about_website;

@interface AboutDelegate : NSObject
{
}
- (void)windowDidResize:(NSNotification*)notification;
- (void)windowWillClose:(NSNotification*)notification;
@end

@implementation AboutDelegate
- (void)windowDidResize:(NSNotification*)notification
{
	NSWindow *window = [notification object];
	CGFloat new_width = [window frame].size.width;

	NSRect temp = [about_name frame];
	temp.origin.x = (new_width - outer_padding*2-temp.size.width) / 2 + outer_padding; //center
	if(temp.origin.x < outer_padding + icon_size)temp.origin.x = outer_padding + icon_size; //move left if overlapping icon
	[about_name setFrame:temp];	

	temp = [about_description frame];
	temp.origin.x = (new_width - outer_padding*2-temp.size.width) / 2 + inner_padding;
	if(temp.origin.x < outer_padding + icon_size)temp.origin.x = outer_padding + icon_size;
	[about_description setFrame:temp];	

	temp = [about_version frame];
	temp.origin.x = (new_width - outer_padding*2-temp.size.width) / 2 + inner_padding;
	if(temp.origin.x < outer_padding + icon_size)temp.origin.x = outer_padding + icon_size;
	[about_version setFrame:temp];	

	temp = [about_date frame];
	temp.origin.x = (new_width - outer_padding*2-temp.size.width) / 2 + inner_padding;
	if(temp.origin.x < outer_padding + icon_size)temp.origin.x = outer_padding + icon_size;
	[about_date setFrame:temp];	

	temp = [about_website frame];
	temp.origin.x = (new_width - outer_padding*2-temp.size.width) / 2 + inner_padding;
	if(temp.origin.x < outer_padding + icon_size)temp.origin.x = outer_padding + icon_size;
	[about_website setFrame:temp];	
	
	[[window contentView] setNeedsDisplay:YES];
}

- (void)windowWillClose:(NSNotification*)notification
{
	//end our modal session if the about window closses
	[NSApp stopModal];
}
@end

@implementation NSApplication (helpmenu)

- (void)launchWebsite
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://desmume.sourceforge.net"]];
}

- (void)launchForums
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://forums.desmume.org/index.php"]];
}

- (void)bugReport
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://sourceforge.net/tracker/?func=add&group_id=164579&atid=832291"]];
}

@end

@implementation NSApplication (aboutdesmume)

- (void)orderFrontStandardAboutPanel:(id)sender
{
	NSWindow *about_window = [[NSWindow alloc] initWithContentRect:NSMakeRect(500, 285, window_width-1, outer_padding*2+tab_view_height+inner_padding+icon_size) styleMask:NSClosableWindowMask|NSTitledWindowMask|NSResizableWindowMask backing:NSBackingStoreBuffered defer:NO];
	if(about_window == nil)return;

	[about_window setTitle:NSLocalizedString([sender title], nil)];

	AboutDelegate *delegate = [[AboutDelegate alloc] init];
	[about_window setDelegate:delegate];

	NSBundle *main_bundle = [NSBundle mainBundle];

	//desmume icon preview
	NSImageView *image = [[NSImageView alloc] initWithFrame:NSMakeRect(outer_padding, outer_padding+tab_view_height+inner_padding, icon_size, icon_size)];
	[image setImageFrameStyle:NSImageFrameNone];
	[image setImageScaling:NSScaleToFit];
	[image setEditable:NO];
	[image setAllowsCutCopyPaste:NO];
	[image setImage:[NSApp applicationIconImage]];
	[image setAutoresizingMask:NSViewMinYMargin];
	[[about_window contentView] addSubview:image];
	[image release];

	//
	const CGFloat width = 296;
	CGFloat content_min_width = window_width - outer_padding*2;

	//
	about_name = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 348 - 240 + tab_view_height + inner_padding, width, 17)];
	[about_name setStringValue:NSLocalizedString(@"DeSmuME", nil)];
	[about_name setEditable:NO];
	[about_name setDrawsBackground:NO];
	[about_name setBordered:NO];
	[about_name setBezeled:NO];
	[about_name setSelectable:YES];
	[about_name sizeToFit];
	[about_name setAutoresizingMask:NSViewMinYMargin];
	NSRect temp = [about_name frame];
	temp.origin.x += (width - temp.size.width) / 2;
	[about_name setFrame:temp];
	[[about_window contentView] addSubview:about_name];
	
	if(content_min_width < icon_size + inner_padding + [about_name frame].size.width)
	content_min_width = icon_size + inner_padding + [about_name frame].size.width;

	//
	about_description = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 83 + tab_view_height + inner_padding, width, 17)];
	[about_description setStringValue:NSLocalizedString(@"Nintendo DS Emulator", nil)];
	[about_description setEditable:NO];
	[about_description setDrawsBackground:NO];
	[about_description setBordered:NO];
	[about_description setBezeled:NO];
	[about_description setSelectable:YES];
	[about_description sizeToFit];
	[about_description setAutoresizingMask:NSViewMinYMargin];
	temp = [about_description frame];
	temp.origin.x += (width - temp.size.width) / 2;
	[about_description setFrame:temp];
	[[about_window contentView] addSubview:about_description];
	
	if(content_min_width < icon_size + inner_padding + [about_description frame].size.width)
	content_min_width = icon_size + inner_padding + [about_description frame].size.width;

	//
	about_version = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 58 + tab_view_height + inner_padding, width, 17)];
	[about_version setStringValue:[NSString stringWithFormat:NSLocalizedString(@"Version %@", nil), [[main_bundle infoDictionary] objectForKey:@"CFBundleVersion"]]];
	[about_version setEditable:NO];
	[about_version setDrawsBackground:NO];
	[about_version setBordered:NO];
	[about_version setBezeled:NO];
	[about_version setSelectable:YES];
	[about_version sizeToFit];
	[about_version setAutoresizingMask:NSViewMinYMargin];
	temp = [about_version frame];
	temp.origin.x += (width - temp.size.width) / 2;
	[about_version setFrame:temp];
	[[about_window contentView] addSubview:about_version];
	
	if(content_min_width < icon_size + inner_padding + [about_version frame].size.width)
	content_min_width = icon_size + inner_padding + [about_version frame].size.width;

	//
	about_date = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 33 + tab_view_height + inner_padding, width, 17)];
	[about_date setStringValue:@__DATE__];//fixme localize
	[about_date setEditable:NO];
	[about_date setDrawsBackground:NO];
	[about_date setBordered:NO];
	[about_date setBezeled:NO];
	[about_date setSelectable:YES];
	[about_date sizeToFit];
	[about_date setAutoresizingMask:NSViewMinYMargin];
	temp = [about_date frame];
	temp.origin.x += (width - temp.size.width) / 2;
	[about_date setFrame:temp];
	[[about_window contentView] addSubview:about_date];
	
	if(content_min_width < icon_size + inner_padding + [about_date frame].size.width)
	content_min_width = icon_size + inner_padding + [about_date frame].size.width;

	//
	about_website = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 8 + tab_view_height + inner_padding, width, 17)];
	[about_website setStringValue:@"http://www.desmume.org"];//fixme linkize
	[about_website setEditable:NO];
	[about_website setDrawsBackground:NO];
	[about_website setBordered:NO];
	[about_website setBezeled:NO];
	[about_website setSelectable:YES];
	[about_website sizeToFit];
	[about_website setAutoresizingMask:NSViewMinYMargin];
	temp = [about_website frame];
	temp.origin.x += (width - temp.size.width) / 2;
	[about_website setFrame:temp];
	[[about_window contentView] addSubview:about_website];
	
	if(content_min_width < icon_size + inner_padding + [about_website frame].size.width)
	content_min_width = icon_size + inner_padding + [about_website frame].size.width;

	//tab view
	NSTabView *tab_view = [[NSTabView alloc] initWithFrame:NSMakeRect(outer_padding, outer_padding, window_width - outer_padding*2, tab_view_height)];
	[tab_view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
	[[about_window contentView] addSubview:tab_view];

	NSRect rect;
	rect.origin.x = rect.origin.y = 0;

	//fixme if one of the files we read from is missing the entire panel doesn't show

	NSTabViewItem *tab_view_item;
	NSScrollView *scroll_view;
	NSTextView *text_view;
	
	//README
	NSString *datapath = [main_bundle pathForResource:@"README" ofType:@""];
	if(datapath != nil)
	{
		tab_view_item = [[NSTabViewItem alloc] initWithIdentifier:nil];
		[tab_view_item setLabel:NSLocalizedString(@"Readme", nil)];
		[tab_view addTabViewItem:tab_view_item];

		scroll_view = [[NSScrollView alloc] initWithFrame:NSMakeRect(0,0,0,0)];
		[scroll_view setDrawsBackground:NO];
		[scroll_view setHasVerticalScroller:YES];
		[scroll_view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
		[tab_view_item setView:scroll_view];

		rect.size = [[scroll_view contentView] frame].size;
		text_view = [[NSTextView alloc] initWithFrame:rect];
		[text_view insertText:[NSString stringWithContentsOfFile:datapath]];
		[text_view setDrawsBackground:NO];
		[text_view setEditable:NO];
		[text_view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
		[scroll_view setDocumentView:text_view];
		[text_view release];

		[scroll_view release];

		[tab_view_item release];
	}

	//LICENSE
	datapath = [main_bundle pathForResource:@"COPYING" ofType:@""];
	if(datapath != nil)
	{
		tab_view_item = [[NSTabViewItem alloc] initWithIdentifier:nil];
		[tab_view_item setLabel:NSLocalizedString(@"License", nil)];
		[tab_view addTabViewItem:tab_view_item];

		scroll_view = [[NSScrollView alloc] initWithFrame:NSMakeRect(0,0,0,0)];
		[scroll_view setDrawsBackground:NO];
		[scroll_view setHasVerticalScroller:YES];
		[scroll_view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
		[tab_view_item setView:scroll_view];

		rect.size = [[scroll_view contentView] frame].size;
		text_view = [[NSTextView alloc] initWithFrame:rect];
		[text_view insertText:[NSString stringWithContentsOfFile:datapath]];
		[text_view setDrawsBackground:NO];
		[text_view setEditable:NO];
		[text_view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
		[scroll_view setDocumentView:text_view];
		[text_view release];

		[scroll_view release];

		[tab_view_item release];
	}

	//AUTHORS
	datapath = [main_bundle pathForResource:@"AUTHORS" ofType:@""];
	if(datapath != nil)
	{
		tab_view_item = [[NSTabViewItem alloc] initWithIdentifier:nil];
		[tab_view_item setLabel:NSLocalizedString(@"Authors", nil)];
		[tab_view addTabViewItem:tab_view_item];

		scroll_view = [[NSScrollView alloc] initWithFrame:NSMakeRect(0,0,0,0)];
		[scroll_view setDrawsBackground:NO];
		[scroll_view setHasVerticalScroller:YES];
		[scroll_view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
		[tab_view_item setView:scroll_view];

		rect.size = [[scroll_view contentView] frame].size;
		text_view = [[NSTextView alloc] initWithFrame:rect];
		[text_view insertText:[NSString stringWithContentsOfFile:datapath]];
		[text_view setDrawsBackground:NO];
		[text_view setEditable:NO];
		[text_view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
		[scroll_view setDocumentView:text_view];
		[text_view release];

		[scroll_view release];

		[tab_view_item release];
	}

	//CHANGE LOG
	datapath = [main_bundle pathForResource:@"ChangeLog" ofType:@""];
	if(datapath != nil)
	{
		tab_view_item = [[NSTabViewItem alloc] initWithIdentifier:nil];
		[tab_view_item setLabel:NSLocalizedString(@"Change Log", nil)];
		[tab_view addTabViewItem:tab_view_item];

		scroll_view = [[NSScrollView alloc] initWithFrame:NSMakeRect(0,0,0,0)];
		[scroll_view setDrawsBackground:NO];
		[scroll_view setHasVerticalScroller:YES];
		[scroll_view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
		[tab_view_item setView:scroll_view];

		rect.size = [[scroll_view contentView] frame].size;
		text_view = [[NSTextView alloc] initWithFrame:rect];
		[text_view insertText:[NSString stringWithContentsOfFile:datapath]];
		[text_view setDrawsBackground:NO];
		[text_view setEditable:NO];
		[text_view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
		[scroll_view setDocumentView:text_view];
		[text_view release];

		[scroll_view release];

		[tab_view_item release];
	}

	//
	if(content_min_width < [tab_view minimumSize].width)
	content_min_width = [tab_view minimumSize].width;
	
	[tab_view release];

	//show the window
	rect = [about_window frame];
	if(rect.size.width < content_min_width + outer_padding*2)
		rect.size.width = content_min_width + outer_padding*2;
	[about_window setFrame:rect display:NO];
	[about_window setMinSize:rect.size];
	NSWindowController *about_window_controller = [[NSWindowController alloc] initWithWindow:about_window];
	[about_window_controller showWindow:nil];

	//run the window
	[NSApp runModalForWindow:about_window];

	[about_window release];
	[about_window_controller release];
	[delegate release];
	[about_name release];
	[about_description release];
	[about_version release];
	[about_date release];
	[about_website release];
}
@end
