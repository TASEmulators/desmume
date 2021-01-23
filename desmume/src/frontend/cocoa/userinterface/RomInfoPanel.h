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

#import <Cocoa/Cocoa.h>

#define ROMINFO_PANEL_SECTION_HEADER_HEIGHT 22

@interface RomInfoPanelSectionView : NSView
{
	NSButton *disclosureButton;
	NSTextField *sectionLabel;
	
	CGFloat expandedHeight;
	CGFloat collapsedHeight;
	CGFloat contentHeight;
}

@property (readonly) IBOutlet NSButton *disclosureButton;
@property (readonly) IBOutlet NSTextField *sectionLabel;
@property (assign) BOOL isExpanded;
@property (readonly) CGFloat expandedHeight;
@property (readonly) CGFloat collapsedHeight;
@property (readonly) CGFloat contentHeight;

- (NSString *) stringFromSectionLabel;

@end

@interface RomInfoContentView : NSView
@end

@interface RomInfoPanel : NSPanel
{
	NSSize _panelMaxSize;
	NSSize _mainViewMaxSize;
	
	NSArray<RomInfoPanelSectionView*> *_sectionViewList;
}

@property (weak) IBOutlet RomInfoPanelSectionView *generalSectionView;
@property (weak) IBOutlet RomInfoPanelSectionView *titlesSectionView;
@property (weak) IBOutlet RomInfoPanelSectionView *armBinariesSectionView;
@property (weak) IBOutlet RomInfoPanelSectionView *fileSystemSectionView;
@property (weak) IBOutlet RomInfoPanelSectionView *miscSectionView;

- (IBAction) toggleViewState:(id)sender;

- (void) autoLayout;
- (void) setupUserDefaults;
- (void) writeDefaults;

@end
