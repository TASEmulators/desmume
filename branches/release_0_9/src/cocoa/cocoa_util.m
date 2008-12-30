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
#import "globals.h"

////////////////////////////////////////////////////////////////
//Dialog Boxes-------------------------------------------------
////////////////////////////////////////////////////////////////

void messageDialog(NSString *title, NSString *text)
{
	NSRunAlertPanel(title, text, nil/*OK*/, nil, nil);
}

BOOL messageDialogYN(NSString *title, NSString *text)
{
	return NSRunAlertPanel(title, text, NSLocalizedString(@"Yes", nil), NSLocalizedString(@"No", nil), nil) != 0;
}

//does an open dialog to choose an NDS file
//returns the filename or NULL if the user selected cancel
NSOpenPanel* panel;
NSString* openDialog(NSArray *file_types)
{
	panel = [NSOpenPanel openPanel];

	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setAllowsMultipleSelection:NO];

	if([panel runModalForDirectory:nil file:nil types:file_types] == NSOKButton)
	{
		//a file was selected

		id selected_file = [[panel filenames] lastObject]; //hopefully also the first object

		return selected_file;
	}

	//a file wasn't selected
	return NULL;
}

