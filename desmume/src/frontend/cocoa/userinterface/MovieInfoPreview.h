/*
	Copyright (C) 2008-2026 DeSmuME team

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

// Metadata area of the Play Movie dialog. It owns only the fields; the dialog
// controller decides when to refresh them and what to do on OK. The shared
// MovieInfo core supplies every value and the desync assessment.

#import <Cocoa/Cocoa.h>

@interface MovieInfoPreview : NSObject
{
	NSView *view;
	NSTextField *movieROMField;
	NSTextField *yourROMField;
	NSTextField *authorField;
	NSTextField *recordDateField;
	NSTextField *coreField;
	NSTextField *framesField;
	NSTextField *lengthField;
	NSTextField *rerecordField;
	NSTextView *commentsView;
	NSTextField *statusField;
	NSString *lastPath;
}

- (NSView *)view;

// Read a movie's header and fill the fields. The dialog controller calls this
// on path changes and on Refresh.
- (void)previewFileURL:(NSURL *)url;

// The Comment field mirrors the Windows dialog: editable only for a movie
// opened read-write, and only its comment lines are rewritten on OK.
- (NSString *)comments;
- (void)setCommentsEditable:(BOOL)editable;

@end
