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

#ifndef NDS_CONTROL_H_INCLUDED
#define NDS_CONTROL_H_INCLUDED

#include <Cocoa/Cocoa.h>

//This class manages emulation
//dont instanciate more than once!
@interface NintendoDS : NSObject {}

//creation
- (id)init;

//ROM control
- (void)pickROM;
- (BOOL)loadROM:(NSString*)filename;
- (void)closeROM;
- (void)askAndCloseROM;

//execution control
- (void)execute;
- (void)pause;
- (void)reset;
- (void)setFrameSkip:(id)sender;

//run function (should be called at the start of the program)
- (void)run;

//save states
- (void)saveStateAs;
- (void)loadStateFrom;
- (void)saveToSlot:(id)sender;
- (void)loadFromSlot:(id)sender;
//- (void)askAndClearStates;

//
- (void)showRomInfo;
@end


#endif // NDS_CONTROL_H_INCLUDED
