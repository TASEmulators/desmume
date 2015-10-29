/*
	Copyright (C) 2015 DeSmuME Team

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

#import "RomInfoPanel.h"
#import "cocoa_util.h"

@implementation RomInfoPanelSectionView

@synthesize disclosureButton;
@synthesize sectionLabel;
@dynamic isExpanded;
@synthesize expandedHeight;
@synthesize collapsedHeight;
@synthesize contentHeight;

- (id)initWithFrame:(NSRect)frame
{
	self = [super initWithFrame:frame];
	if (self == nil)
	{
		return self;
	}
	
	expandedHeight = frame.size.height;
	collapsedHeight = ROMINFO_PANEL_SECTION_HEADER_HEIGHT;
	contentHeight = expandedHeight - collapsedHeight;
	
	return self;
}

- (void) setIsExpanded:(BOOL)theState
{
	[disclosureButton setState:(theState) ? NSOnState : NSOffState];
	
	NSRect newFrame = [self frame];
	newFrame.size.height = (theState) ?  expandedHeight : collapsedHeight;
	[self setFrame:newFrame];
}

- (BOOL) isExpanded
{
	return ([disclosureButton state] == NSOnState);
}

- (NSString *) stringFromSectionLabel
{
	if (sectionLabel != nil)
	{
		return [sectionLabel stringValue];
	}
	
	return @"";
}

@end

@implementation RomInfoContentView

// Override this method here to prevent all forms of horizontal scrolling.
// This even includes scrolling by means of gestures.
- (NSRect)adjustScroll:(NSRect)proposedVisibleRect
{
	proposedVisibleRect.origin.x = 0.0f;
	return proposedVisibleRect;
}

@end

@implementation RomInfoPanel

@synthesize generalSectionView;
@synthesize titlesSectionView;
@synthesize armBinariesSectionView;
@synthesize fileSystemSectionView;
@synthesize miscSectionView;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	_sectionViewList = nil;
	
	_panelMaxSize.width = 1.0f;
	_panelMaxSize.height = 1.0f;
	_mainViewMaxSize.width = 1.0f;
	_mainViewMaxSize.height = 1.0f;
	
	return self;
}

- (void)dealloc
{
	[_sectionViewList release];
	
	[super dealloc];
}

- (IBAction) toggleViewState:(id)sender
{
	NSButton *disclosureTriangle = (NSButton *)sender;
	RomInfoPanelSectionView *sv = (RomInfoPanelSectionView *)[disclosureTriangle superview];
	const BOOL isSectionExpanded = [CocoaDSUtil getIBActionSenderButtonStateBool:disclosureTriangle];
	[sv setIsExpanded:isSectionExpanded];
	
	[self autoLayout];
}

// Since we want to maintain backwards compatibility with OS X v10.5, we need to roll our
// own custom autolayout method.
- (void) autoLayout
{
	// Calculate the heights of the superviews.
	NSSize newMainViewSize = _mainViewMaxSize;
	NSSize newPanelMaxSize = _panelMaxSize;
	for (RomInfoPanelSectionView *sv in _sectionViewList)
	{
		const BOOL isExpanded = [sv isExpanded];
		const CGFloat svContentHeight = [sv contentHeight];
		newMainViewSize.height -= ((isExpanded) ? 0.0f : svContentHeight);
		newPanelMaxSize.height -= ((isExpanded) ? 0.0f : svContentHeight);
	}
	
	// Set the superview heights.
	NSView *mainView = [[_sectionViewList objectAtIndex:0] superview];
	[mainView setFrameSize:newMainViewSize];
	[self setContentMaxSize:newPanelMaxSize];
	
	// Place all of the views in their final locations by enumerating through each section
	// view and stacking them on top of one another. This requires that the enumeration is
	// done in a specific order, so we'll enumerate manually in this case.
	CGFloat originY = 0.0f; // Keeps track of the current origin location as we enumerate through each section view.
	
	for (size_t j = 0; j < [_sectionViewList count]; j++)
	{
		RomInfoPanelSectionView *sv = (RomInfoPanelSectionView *)[_sectionViewList objectAtIndex:j];
		NSPoint svOrigin = [sv frame].origin;
		svOrigin.y = originY;
		
		[sv setFrameOrigin:svOrigin];
		originY += [sv frame].size.height;
	}
	
	// If the panel is currently larger than the new max size, automatically reduce its size
	// to match the new max size.
	CGFloat heightDiff = [[self contentView] frame].size.height - [self contentMaxSize].height;
	if (heightDiff > 0.0f)
	{
		NSRect newFrameRect = [self frame];
		newFrameRect.size.height -= heightDiff;
		newFrameRect.origin.y += heightDiff;
		[self setFrame:newFrameRect display:YES animate:NO];
	}
}

- (void) setupUserDefaults
{
	// Add each section view to this list as they would appear in the panel
	// from bottom to top order.
	_sectionViewList = [[NSArray alloc] initWithObjects:
						miscSectionView,
						fileSystemSectionView,
						armBinariesSectionView,
						titlesSectionView,
						generalSectionView,
						nil];
	
	NSView *mainView = [[_sectionViewList objectAtIndex:0] superview];
	_panelMaxSize = [self contentMaxSize];
	_mainViewMaxSize = [mainView frame].size;
	
	// Toggle each view state per user preferences.
	NSDictionary *viewStatesDict = [[NSUserDefaults standardUserDefaults] objectForKey:@"RomInfoPanel_SectionViewState"];
	if (viewStatesDict != nil)
	{
		for (RomInfoPanelSectionView *sv in _sectionViewList)
		{
			NSNumber *theStateObj = (NSNumber *)[viewStatesDict objectForKey:[sv stringFromSectionLabel]];
			[sv setIsExpanded:(theStateObj != nil) ? [theStateObj boolValue] : YES];
		}
	}
	else
	{
		for (RomInfoPanelSectionView *sv in _sectionViewList)
		{
			[sv setIsExpanded:YES];
		}
	}
	
	// Perform an autolayout to reflect possible changes to the panel states.
	[self autoLayout];
}

- (void) writeDefaults
{
	NSMutableDictionary *viewStatesDict = [NSMutableDictionary dictionaryWithCapacity:[_sectionViewList count]];
	for (RomInfoPanelSectionView *sv in _sectionViewList)
	{
		[viewStatesDict setObject:[NSNumber numberWithBool:[sv isExpanded]] forKey:[sv stringFromSectionLabel]];
	}
	
	[[NSUserDefaults standardUserDefaults] setObject:viewStatesDict forKey:@"RomInfoPanel_SectionViewState"];
	[[NSUserDefaults standardUserDefaults] synchronize];
}

#pragma mark NSPanel Methods

- (BOOL)becomesKeyOnlyIfNeeded
{
	return YES;
}

@end
