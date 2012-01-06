/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012 DeSmuME team

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

#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
	#include "macosx_10_4_compat.h"
#endif


@interface CocoaDSFile : NSDocument
{

}

+ (BOOL) loadState:(NSURL *)saveStateURL;
+ (BOOL) saveState:(NSURL *)saveStateURL;
+ (BOOL) loadRom:(NSURL *)romURL;
+ (BOOL) importRomSave:(NSURL *)romSaveURL;
+ (BOOL) exportRomSave:(NSURL *)romSaveURL;
+ (NSURL *) romSaveURLFromRomURL:(NSURL *)romURL;
+ (NSURL *) cheatsURLFromRomURL:(NSURL *)romURL;
+ (BOOL) romSaveExists:(NSURL *)romURL;
+ (BOOL) romSaveExistsWithRom:(NSURL *)romURL;
+ (void) setupAllFilePaths;
+ (BOOL) setupAllAppDirectories;
+ (NSURL *) saveStateURL;
+ (BOOL) saveStateExistsForSlot:(NSURL *)romURL slotNumber:(NSUInteger)slotNumber;
+ (BOOL) isSaveStateSlotExtension:(NSString *)extension;
+ (NSString *) getSaveSlotFileName:(NSURL *)romURL slotNumber:(NSUInteger)slotNumber;
+ (NSString *) fileKind:(NSURL *)fileURL;
+ (NSString *) fileVersion:(NSURL *)fileURL;
+ (BOOL) fileExistsForCurrent:(NSURL *)fileURL;
+ (NSURL *) getURLUserAppSupportByKind:(NSString *)fileKind;
+ (NSURL *) getBaseURLUserAppSupport;
+ (NSURL *) getURLUserAppSupport:(NSString *)directoryName appVersion:(NSString *)appVersion;
+ (BOOL) createUserAppSupportDirectory:(NSString *)directoryName;
+ (NSURL *) lastLoadedRomURL;

#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_4
+ (BOOL) moveFileToCurrentDirectory:(NSURL *)fileURL;
+ (BOOL) copyFileToCurrentDirectory:(NSURL *)fileURL;
+ (BOOL) moveFileListToCurrent:(NSMutableArray *)fileList;
+ (BOOL) copyFileListToCurrent:(NSMutableArray *)fileList;
+ (NSMutableArray *) completeFileList;
+ (NSMutableArray *) appFileList:(NSURL *)directoryURL;
+ (NSMutableArray *) appFileList:(NSURL *)directoryURL fileKind:(NSString *)theFileKind;
#endif

@end
