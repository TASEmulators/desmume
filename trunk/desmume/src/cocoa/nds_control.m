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

//DeSmuME Cocoa includes
#import "globals.h"
#import "nds_control.h"
#import "main_window.h"
#import "preferences.h"

//DeSmuME general includes
#define OBJ_C
#include "../NDSSystem.h"
#include "../saves.h"
#undef BOOL

//ds screens are 59.8 frames per sec, so 1/59.8 seconds per frame
//times one million for microseconds per frame
#define DS_MICROSECONDS_PER_FRAME (1.0 / 59.8) * 1000000.0

//fixme bug when hitting apple+Q during the open dialog

@interface TableHelper : NSObject
{
	NSTableView *table;
	NSTableColumn *type_column;
	NSTableColumn *value_column;
	NDS_header *header;
}
//init
- (id)initWithWindow:(NSWindow*)window;

//table data protocol
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (BOOL)tableView:(NSTableView *)aTableView acceptDrop:(id <NSDraggingInfo>)info row:(int)row dropOperation:(NSTableViewDropOperation)operation;
- (NSArray *)tableView:(NSTableView *)aTableView namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination forDraggedRowsWithIndexes:(NSIndexSet *)indexSet;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;
- (void)tableView:(NSTableView *)aTableView setObjectValue:(id)anObject forTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;
- (void)tableView:(NSTableView *)aTableView sortDescriptorsDidChange:(NSArray *)oldDescriptors;
- (NSDragOperation)tableView:(NSTableView *)aTableView validateDrop:(id <NSDraggingInfo>)info proposedRow:(int)row proposedDropOperation:(NSTableViewDropOperation)operation;
- (BOOL)tableView:(NSTableView *)aTableView writeRowsWithIndexes:(NSIndexSet *)rowIndexes toPasteboard:(NSPasteboard*)pboard;

//window delegate stuff
- (void)windowWillClose:(NSNotification *)aNotification;
- (void)windowDidResize:(NSNotification *)aNotification;
@end

/////////////////////////

NSMenuItem *close_rom_item;
NSMenuItem *execute_item;
NSMenuItem *pause_item;
NSMenuItem *reset_item;
NSMenuItem *save_state_as_item;
NSMenuItem *load_state_from_item;
NSMenuItem *saveSlot_item[SAVE_SLOTS];
NSMenuItem *loadSlot_item[SAVE_SLOTS];
//NSMenuItem *clear_all_saves_item;
NSMenuItem *rom_info_item;
NSMenuItem *frame_skip_auto_item;
NSMenuItem *frame_skip_item[MAX_FRAME_SKIP];

volatile u8 frame_skip = 0; //this is one more than the acutal frame skip, a value of 0 signifies auto frame skip

static int backupmemorytype=MC_TYPE_AUTODETECT;
static u32 backupmemorysize=1;

struct NDS_fw_config_data firmware;

NSString *current_file;

@implementation NintendoDS
- (id)init
{
	struct armcpu_memory_iface *arm9_memio = &arm9_base_memory_iface;
	struct armcpu_memory_iface *arm7_memio = &arm7_base_memory_iface;
	struct armcpu_ctrl_iface *arm9_ctrl_iface;
	struct armcpu_ctrl_iface *arm7_ctrl_iface;
	//struct configured_features my_config;

	NDS_Init( arm9_memio, &arm9_ctrl_iface,
		arm7_memio, &arm7_ctrl_iface);

	NDS_FillDefaultFirmwareConfigData(&firmware);
[self setPlayerName:@"Joe"];
	NDS_CreateDummyFirmware(&firmware);


	return self;
}

- (void)setPlayerName:(NSString*)player_name
{
	//first we convert to UTF-16 which the DS uses to store the nickname
	NSData *string_chars = [player_name dataUsingEncoding:NSUnicodeStringEncoding];

	//copy the bytes
	firmware.nickname_len = MIN([string_chars length],MAX_FW_NICKNAME_LENGTH);
	[string_chars getBytes:firmware.nickname length:firmware.nickname_len];
	firmware.nickname[firmware.nickname_len / 2] = 0;

	//set the firmware
	//NDS_CreateDummyFirmware(&firmware);
}

- (void)pickROM
{
	BOOL was_paused = paused;
	[self pause];

	NSString *temp = openDialog([NSArray arrayWithObjects:@"NDS", @"ds.GBA", nil]);

	if(temp)
	{
		[self loadROM:temp];
		return;
	}

	if(!was_paused)[self execute];
}

- (BOOL)loadROM:(NSString*)filename
{
	//pause if not already paused
	bool was_paused = paused;
	[self pause];

	//load the rom
	if(!NDS_LoadROM([filename cStringUsingEncoding:NSASCIIStringEncoding], backupmemorytype, backupmemorysize, "/Users/gecko/AAAA.sav") > 0)
	{
		//if it didn't work give an error and dont unpause
		messageDialog(localizedString(@"Error", nil), @"Could not open file");

		//continue playing if load didn't work
		if(!was_paused)[self execute];

		return FALSE;
	}

	//add the rom to the recent documents list
	[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:filename]];

	//set current file var
	current_file = filename;

	//enable execution control menu
	[execute_item setEnabled:YES];
	[pause_item setEnabled:YES];
	[reset_item setEnabled:YES];
	[close_rom_item setEnabled:YES];
	[rom_info_item setEnabled:YES];
	[save_state_as_item setEnabled:YES];
	[load_state_from_item setEnabled:YES];

	//
	bool atleast_one_save_exists = false;
	scan_savestates();
	int i;
	for(i = 0; i < SAVE_SLOTS; i++)
	{
		[saveSlot_item[i] setEnabled:YES];

		if(savestates[i].exists == desmume_TRUE)
		{
			atleast_one_save_exists = true;
			[saveSlot_item[i] setState:NSOnState];
			[loadSlot_item[i] setEnabled:YES];
		} else
		{
			[saveSlot_item[i] setState:NSOffState];
			[loadSlot_item[i] setEnabled:NO];
		}
	}

	//if(atleast_one_save_exists)
		//[clear_all_saves_item setEnabled:YES];

	//layers apparently get reset on rom load?
	//[set topBG2_item fixme

	//if it worked, check the execute upon load option
	if([[NSUserDefaults standardUserDefaults] boolForKey:PREF_EXECUTE_UPON_LOAD])
		[self execute];
	else
		[main_window clearScreenWhite];

	return true;
}

- (void)closeROM
{
	[self pause];

	NDS_FreeROM();

	//set menu item states
	[execute_item setEnabled:NO];
	[pause_item setEnabled:NO];
	[reset_item setEnabled:NO];
	[close_rom_item setEnabled:NO];
	[rom_info_item setEnabled:NO];
	[save_state_as_item setEnabled:NO];
	[load_state_from_item setEnabled:NO];

	//disable all save slots
	int i;
	for(i = 0; i < SAVE_SLOTS; i++)
	{
		[saveSlot_item[i] setEnabled:NO];
		[saveSlot_item[i] setState:NSOffState];
		[loadSlot_item[i] setEnabled:NO];
	}

	//[clear_all_saves_item setEnabled:NO];
}

- (void)askAndCloseROM
{
	bool was_paused = paused;
	[NDS pause];

	if(messageDialogYN(localizedString(@"DeSmuME Emulator", nil), localizedString(@"Are you sure you want to close the ROM?", nil)))
	{
		[self closeROM];

		[main_window clearScreenBlack];
	}
	else if(!was_paused)[NDS execute];
}

- (void)execute
{
	paused = FALSE;
	execute = TRUE;
	//SPU_Pause(0);

 	[pause_item setState:NSOffState];
 	[execute_item setState:NSOnState];
}

- (void)pause
{
	if(paused)return;

	execute = FALSE;
	//SPU_Pause(1);

#ifndef MULTITHREADED
	paused = TRUE;
#else
	//wait for the other thread to stop execution
	while (!paused) {}
#endif

 	[pause_item setState:NSOnState];
 	[execute_item setState:NSOffState];
}

- (void)reset
{
	NDS_Reset();
	[self execute];
}

- (void)setFrameSkip:(id)sender
{
	int i;

	//see if this was sent from the autoskip menu item
	if(sender == frame_skip_auto_item)
	{
		//set the frame skip to auto
		frame_skip = 0;

		//check auto
		[frame_skip_auto_item setState:NSOnState];

		//uncheck the others
		for(i = 0; i < MAX_FRAME_SKIP; i++)[frame_skip_item[i] setState:NSOffState];
		return;
	}

	//search through the frame skip menu items to find out which one called this function
	for(i = 0; i < MAX_FRAME_SKIP; i++)
		if(sender == frame_skip_item[i])break;

	if(i == MAX_FRAME_SKIP)return; //not a known sender

	//set the frame skip value
	frame_skip = i + 1;

	//check the selected frame skip value
	[frame_skip_item[i] setState:NSOnState];

	//uncheck the others
	int i2;
	for(i2 = 0; i2 < MAX_FRAME_SKIP; i2++)
		if(i2 != i)
			[frame_skip_item[i2] setState:NSOffState];
	[frame_skip_auto_item setState:NSOffState];
}

- (void)run
{
	u32 cycles = 0;

	//this flag is so we can set the pause state to true
	//ONCE after the inner run loop breaks (ie when you pause)
	//to avoid some slight chance that the other thread sets pause to false
	//and this thread re
	bool was_running = false; //initialized here to avoid a warning

	unsigned long long frame_start_time, frame_end_time;
	unsigned long long microseconds_per_frame;

	int frames_to_skip = 0;

	//program main loop
	while(!finished)
	{

#ifndef MULTITHREADED
		//keep the GUI responsive even while not running a gme
		if(!execute)clearEvents(true);
#endif
		//run the emulator
		while(execute && !finished)
		{
			was_running = true;

//clear the event queue so the GUI elements are responsive when the game is running
//needs to happen before frame_start_time is recieved incase the user does a live resize
//or something that will pause everything
#ifndef MULTITHREADED
			if(frames_to_skip == 0)
				clearEvents(false);
#endif

			Microseconds((struct UnsignedWide*)&frame_start_time);

			cycles = NDS_exec((560190<<1)-cycles, FALSE);

			if(frames_to_skip != 0)
				frames_to_skip--;

			else
			{

				Microseconds((struct UnsignedWide*)&frame_end_time);

				if(frame_skip == 0)
				{ //automatic

					//i don't know if theres a standard strategy, but here we calculate
					//how much longer the frame took than it should have, then set it to skip
					//that many frames.
					frames_to_skip = ((double)(frame_end_time - frame_start_time)) / ((double)DS_MICROSECONDS_PER_FRAME);

				} else
				{

					frames_to_skip = frame_skip;

				}

#ifndef MULTITHREADED
				if(!paused) //so the screen doesnt update after being cleared to white (when closing rom)
#endif
				[main_window updateScreen];

			}

		}

		//
		if(was_running)
		{
			paused = TRUE; //for the multithreaded version (avoid forever loop in the pause method)
			was_running = false; //

			//set some kind of checkmarks or something?
		}

	}
}

- (void)saveStateAs
{//dst
	BOOL was_paused = paused;
	[NDS pause];

	NSSavePanel *panel = [NSSavePanel savePanel];

	[panel setTitle:localizedString(@"Save State to File...", nil)];
	[panel setAllowedFileTypes:[NSArray arrayWithObjects:@"dst",nil]];

	if([panel runModal] == NSOKButton)
	{

		NSString *filename = [panel filename];

		if(filename)
			savestate_save([filename cStringUsingEncoding:NSASCIIStringEncoding]);

	}

	if(!was_paused)[NDS execute];
}

- (void)loadStateFrom
{
	NSString *file = openDialog([NSArray arrayWithObject:@"DST"]);

	if(file)
	{
		[NDS pause];

		savestate_load([file cStringUsingEncoding:NSASCIIStringEncoding]);

		[NDS execute];
	}
}

- (void)saveToSlot:(id)sender
{
	//get the save slot by detecting which menu item sent the message
	int i;
	for(i = 0; i < SAVE_SLOTS; i++)
		if(sender == saveSlot_item[i])break;

	//quit if the message wasn't sent by a save state menu item
	if(i == SAVE_SLOTS)return;

	bool was_paused = paused;
	[NDS pause];

	savestate_slot(i + 1);

	[saveSlot_item[i] setState:NSOnState];
	[loadSlot_item[i] setEnabled:YES];
	//[clear_all_saves_item setEnabled:YES];

	if(!was_paused)[NDS execute];

}

- (void)loadFromSlot:(id)sender
{
	//get the load slot
	int i;
	for(i = 0; i < SAVE_SLOTS; i++)
		if(sender == loadSlot_item[i])break;

	//quit if the message wasn't sent by a save state menu item
	if(i == SAVE_SLOTS)return;

	bool was_paused = paused;
	[NDS pause];

	loadstate_slot(i + 1);

	if(!was_paused)[NDS execute];
}
/*
- (void)askAndClearStates
{
	BOOL was_paused = paused;
	[NDS pause];

	if(messageDialogYN(localizedString(@"DeSmuME Emulator", nil),
	localizedString(@"Are you sure you want to clear all save slots?", nil)))
	{


		to be implemented after saves.h provides a way to delete saves...

		int i;
		for(i = 0; i < SAVE_SLOTS; i++)
		{
			[saveSlot_item[i] setState:NSOffState];
			[loadSlot_item[i] setEnabled:NO];
		}

		[clear_all_saves_item setEnabled:NO];

	}

	if(!was_paused)[NDS execute];
}
*/

- (void)showRomInfo
{
	BOOL was_paused = paused;
	[self pause];

	//create a window
	NSRect rect;
	rect.size.width = 400;
	rect.size.height = 200; //fixme;
	rect.origin.x = 200;
	rect.origin.y = 200;
	NSWindow *rom_info_window = [[NSWindow alloc] initWithContentRect:rect styleMask:
	NSTitledWindowMask|NSClosableWindowMask|NSResizableWindowMask backing:NSBackingStoreBuffered defer:NO screen:nil];

	//set the window title
	[rom_info_window setTitle:localizedString(@"ROM Info", nil)];

	//create an NSTableView to display the stuff
	TableHelper *helper = [[TableHelper alloc] initWithWindow:rom_info_window];

	//set the window delegate
	[rom_info_window setDelegate:helper];

	//min size is starting size
	[rom_info_window setMinSize:[rom_info_window frame].size];

	//show the window
	[[[NSWindowController alloc] initWithWindow:rom_info_window] showWindow:nil];

	//run the window
	[NSApp runModalForWindow:rom_info_window];

	if(!was_paused)[self execute];
}

@end
#define ROM_INFO_ROWS 9
#define ROM_INFO_WIDTH 400
@implementation TableHelper
- (id)initWithWindow:(NSWindow*)window
{
	self = [super init];

	type_column = [[NSTableColumn alloc] initWithIdentifier:@""];
	[type_column setEditable:NO];
	[value_column setResizable:YES];
	[[type_column headerCell] setStringValue:@"Attribute"];
	[type_column setMinWidth: 1];

	value_column = [[NSTableColumn alloc] initWithIdentifier:@""];
	[value_column setEditable:NO];
	[value_column setResizable:YES];
	[[value_column headerCell] setStringValue:@"Value"];
	[value_column setMinWidth: 1];

	NSRect rect;
	rect.size.width = rect.size.height = rect.origin.x = rect.origin.y = 0;
	table = [[NSTableView alloc] initWithFrame:rect];
	[table setDrawsGrid:NO];
	[table setAllowsColumnSelection:YES];
	[table setAllowsColumnReordering:YES];
	[table setAllowsEmptySelection:YES];
	[table setAllowsMultipleSelection:NO];
	[table addTableColumn:type_column];
	[table addTableColumn:value_column];
	[table setDataSource:self];
	//[table setDelegate:self];
	//[table setTarget: _parent];
	//[table setDoubleAction: @selector(editClicked:)];
	[table setUsesAlternatingRowBackgroundColors:YES];

	//
	//[table setHeaderView:[[NSTableHeaderView alloc] initWithFrame:NSMakeRect(0,0,0,0)]];
if([table headerView] == nil)messageDialogBlank();

	//resize the window to fit enough rows
	rect.size.width = ROM_INFO_WIDTH;
	rect.size.height = [table rowHeight] * ROM_INFO_ROWS + [table intercellSpacing].height * (ROM_INFO_ROWS - 1) + 2 /*dunno what the 2 is for*/;
	[window setContentSize:rect.size];

	//resize the left column to take up only as much space as needed
	//[type_column sizeToFit];

	//resize the right column to take up the rest of the window
	[value_column setWidth:ROM_INFO_WIDTH - [type_column width]];

	//
	[window setContentView:table];

	//grab the header to read data from
	header = NDS_getROMHeader();

	return self;
}
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
{
	return ROM_INFO_ROWS;
}

- (BOOL)tableView:(NSTableView *)aTableView acceptDrop:(id <NSDraggingInfo>)info row:(int)row dropOperation:(NSTableViewDropOperation)operation
{
	return NO;
}

- (NSArray *)tableView:(NSTableView *)aTableView namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination forDraggedRowsWithIndexes:(NSIndexSet *)indexSet
{
	return [NSArray array];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
	if(aTableColumn == type_column)
	{
		if(rowIndex == 0)
			return localizedString(@"File on Disc", @" ROM Info ");

		if(rowIndex == 1)
			return localizedString(@"ROM Title", @" ROM Info ");

		if(rowIndex == 2)
			return localizedString(@"Maker Code", @" ROM Info ");

		if(rowIndex == 3)
			return localizedString(@"Unit Code", @" ROM Info ");

		if(rowIndex == 4)
			return localizedString(@"Card Size", @" ROM Info ");

		if(rowIndex == 5)
			return localizedString(@"Flags", @" ROM Info ");

		if(rowIndex == 6)
			return localizedString(@"Size of ARM9 Binary", @" ROM Info ");

		if(rowIndex == 7)
			return localizedString(@"Size of ARM7 Size", @" ROM Info ");

		if(rowIndex == 8)
			return localizedString(@"Size of Data", @" ROM Info ");
	} else
	{
		if(rowIndex == 0)return current_file;

		if(rowIndex == 1)
			return NSSTRc(header->gameTile);

		if(rowIndex == 2)
		{
			return [NSString localizedStringWithFormat:@"%u", header->makerCode];
		}

		if(rowIndex == 3)
		{
			return [NSString localizedStringWithFormat:@"%u", header->unitCode];
		}

		if(rowIndex == 4)
		{//fixe: should show units?
			return [NSString localizedStringWithFormat:@"%u", header->cardSize];
		}

		if(rowIndex == 5)
		{//always seems to be empty?
			return [NSString localizedStringWithFormat:@"%u%u%u%u%u%u%u%u",
			((header->flags) & 0x01) >> 0,
			((header->flags) & 0x02) >> 1,
			((header->flags) & 0x04) >> 2,
			((header->flags) & 0x08) >> 3,
			((header->flags) & 0x10) >> 4,
			((header->flags) & 0x20) >> 5,
			((header->flags) & 0x40) >> 6,
			((header->flags) & 0x80) >> 7];
		}

		if(rowIndex == 6)
		{//fixe: should show units?
			return [NSString localizedStringWithFormat:@"%u", header->ARM9binSize];
		}

		if(rowIndex == 7)
		{//fixe: should show units?
			return [NSString localizedStringWithFormat:@"%u", header->ARM7binSize];
		}

		if(rowIndex == 8)
		{//fixe: should show units?
			return [NSString localizedStringWithFormat:@"%u", header->ARM7binSize + header->ARM7src];
		}

	}

	return @"If you see this, there is a bug";
}

- (void)tableView:(NSTableView *)aTableView setObjectValue:(id)anObject forTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;
{
}

- (void)tableView:(NSTableView *)aTableView sortDescriptorsDidChange:(NSArray *)oldDescriptors;
{
}

- (NSDragOperation)tableView:(NSTableView *)aTableView validateDrop:(id <NSDraggingInfo>)info proposedRow:(int)row proposedDropOperation:(NSTableViewDropOperation)operation
{
	return NSDragOperationNone;
}

- (BOOL)tableView:(NSTableView *)aTableView writeRowsWithIndexes:(NSIndexSet *)rowIndexes toPasteboard:(NSPasteboard*)pboard;
{
	return NO;
}

- (void)windowWillClose:(NSNotification *)aNotification
{
	[NSApp stopModal];
}

- (void)windowDidResize:(NSNotification *)aNotification
{
	//we have this because the tableview doesn't change clipped text size

	int i;
	for(i = 0; i < ROM_INFO_ROWS; i++)
	{
		[table drawRow:i clipRect:[table frame]];
	}
}
@end
