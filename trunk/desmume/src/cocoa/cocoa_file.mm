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

#import "cocoa_file.h"
#include <libkern/OSAtomic.h>
#import "cocoa_globals.h"
#import "cocoa_util.h"

#include "../NDSSystem.h"
#include "../path.h"
#include "../saves.h"
#undef BOOL

@implementation CocoaDSFile

+ (BOOL) loadState:(NSURL *)saveStateURL
{
	BOOL result = NO;
	
	if (saveStateURL == nil)
	{
		return result;
	}
	
	const char *statePath = [[saveStateURL path] cStringUsingEncoding:NSUTF8StringEncoding];
	bool cResult = savestate_load(statePath);
	if(cResult)
	{
		result = YES;
	}
	
	return result;
}

+ (BOOL) saveState:(NSURL *)saveStateURL
{
	BOOL result = NO;
	
	if (saveStateURL == nil)
	{
		return result;
	}
	
	const char *statePath = [[saveStateURL path] cStringUsingEncoding:NSUTF8StringEncoding];
	bool cResult = savestate_save(statePath);
	if(cResult)
	{
		result = YES;
	}
	
	return result;
}

+ (BOOL) loadRom:(NSURL *)romURL
{
	BOOL result = NO;
	
	if (romURL == nil)
	{
		return result;
	}
	
	const char *romPath = [[romURL path] cStringUsingEncoding:NSUTF8StringEncoding];
	NSInteger resultCode = NDS_LoadROM(romPath, nil);
	if (resultCode > 0)
	{
		result = YES;
	}
	
	return result;
}

+ (BOOL) importRomSave:(NSURL *)romSaveURL
{
	BOOL result = NO;
	const char *romSavePath = [[romSaveURL path] cStringUsingEncoding:NSUTF8StringEncoding];
	
	NSInteger resultCode = NDS_ImportSave(romSavePath);
	if (resultCode == 0)
	{
		return result;
	}
	
	result = YES;
	
	return result;
}

+ (BOOL) exportRomSave:(NSURL *)romSaveURL
{
	BOOL result = NO;
	const char *romSavePath = [[romSaveURL path] cStringUsingEncoding:NSUTF8StringEncoding];
	
	NSInteger resultCode = NDS_ExportSave(romSavePath);
	if (resultCode == 0)
	{
		return result;
	}
	
	result = YES;
	
	return result;
}

+ (NSURL *) romSaveURLFromRomURL:(NSURL *)romURL
{
	return [NSURL fileURLWithPath:[[[romURL path] stringByDeletingPathExtension] stringByAppendingPathExtension:@FILE_EXT_ROM_SAVE]];
}

+ (NSURL *) cheatsURLFromRomURL:(NSURL *)romURL
{
	return [NSURL fileURLWithPath:[[[romURL path] stringByDeletingPathExtension] stringByAppendingPathExtension:@FILE_EXT_CHEAT]];
}

+ (BOOL) romSaveExists:(NSURL *)romURL
{
	BOOL exists = NO;
	
	if (romURL == nil)
	{
		return exists;
	}
	
	NSString *romSaveFileName = [[[[romURL path] stringByDeletingPathExtension] stringByAppendingPathExtension:@FILE_EXT_ROM_SAVE] lastPathComponent];
	NSURL *romSaveURL = [CocoaDSFile getURLUserAppSupport:@BATTERYKEY appVersion:nil];
	NSString *romSavePath = [romSaveURL path];
	romSavePath = [romSavePath stringByAppendingPathComponent:romSaveFileName];
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	
	exists = [fileManager isReadableFileAtPath:romSavePath];
	
	[fileManager release];
	
	return exists;
}

+ (BOOL) romSaveExistsWithRom:(NSURL *)romURL
{
	BOOL exists = NO;
	
	if (romURL == nil)
	{
		return exists;
	}
	
	NSString *romSavePath = [[CocoaDSFile romSaveURLFromRomURL:romURL] path];
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	
	exists = [fileManager isReadableFileAtPath:romSavePath];
	
	[fileManager release];
	
	return exists;
}

+ (void) setupAllFilePaths
{
	NSURL *romURL			= [CocoaDSFile getURLUserAppSupport:@ROMKEY appVersion:nil];
	NSURL *romSaveURL		= [CocoaDSFile getURLUserAppSupport:@BATTERYKEY appVersion:nil];
	NSURL *saveStateURL		= [CocoaDSFile getURLUserAppSupport:@STATEKEY appVersion:nil];
	NSURL *screenshotURL	= [CocoaDSFile getURLUserAppSupport:@SCREENSHOTKEY appVersion:nil];
	NSURL *aviURL			= [CocoaDSFile getURLUserAppSupport:@AVIKEY appVersion:nil];
	NSURL *cheatURL			= [CocoaDSFile getURLUserAppSupport:@CHEATKEY appVersion:nil];
	NSURL *soundSamplesURL	= [CocoaDSFile getURLUserAppSupport:@SOUNDKEY appVersion:nil];
	NSURL *firmwareURL		= [CocoaDSFile getURLUserAppSupport:@FIRMWAREKEY appVersion:nil];
	NSURL *luaURL			= [CocoaDSFile getURLUserAppSupport:@LUAKEY appVersion:nil];
	
	strlcpy(path.pathToRoms, [[romURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	strlcpy(path.pathToBattery, [[romSaveURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	strlcpy(path.pathToStates, [[saveStateURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	strlcpy(path.pathToScreenshots, [[screenshotURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	strlcpy(path.pathToAviFiles, [[aviURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	strlcpy(path.pathToCheats, [[cheatURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	strlcpy(path.pathToSounds, [[soundSamplesURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	strlcpy(path.pathToFirmware, [[firmwareURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	strlcpy(path.pathToLua, [[luaURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
}

+ (BOOL) setupAllAppDirectories
{
	BOOL result = NO;
	
	if (![CocoaDSFile createUserAppSupportDirectory:@BATTERYKEY])
	{
		return result;
	}
	
	if (![CocoaDSFile createUserAppSupportDirectory:@STATEKEY])
	{
		return result;
	}
	
	if (![CocoaDSFile createUserAppSupportDirectory:@CHEATKEY])
	{
		return result;
	}
	
	result = YES;
	
	return result;
}

+ (NSURL *) saveStateURL
{
	return [CocoaDSFile getURLUserAppSupport:@STATEKEY appVersion:nil];
}

/********************************************************************************************
	fileKind:

	Determines a DeSmuME file's type, and returns a description as a string.

	Takes:
		fileURL - An NSURL that points to a file.

	Returns:
		An NSString representing the file type. The NSString will be nil if the file
		type is not recognized as a DeSmuME file.

	Details:
		This is not the most reliable method for determining a file's type. The current
		implementation simply checks the file extension to determine the file type.
		Future implementations could be made more reliable by actually opening the file
		and validating some header info. 
 ********************************************************************************************/
+ (NSString *) fileKind:(NSURL *)fileURL
{
	NSString *fileKind = nil;
	
	if (fileURL == nil)
	{
		return fileKind;
	}
	
	NSString *fileExt = [[fileURL path] pathExtension];
	
	if ([fileExt isEqualToString:@FILE_EXT_ROM_SAVE])
	{
		fileKind = @"ROM Save";
	}
	else if ([fileExt isEqualToString:@FILE_EXT_CHEAT])
	{
		fileKind = @"Cheat File";
	}
	else if ([fileExt isEqualToString:@FILE_EXT_FIRMWARE_CONFIG])
	{
		fileKind = @"Firmware Configuration";
	}
	else if ([CocoaDSFile isSaveStateSlotExtension:fileExt])
	{
		fileKind = @"Save State";
	}
	else if ([fileExt isEqualToString:@FILE_EXT_ROM_DS])
	{
		fileKind = @"DS ROM";
	}
	else if ([fileExt isEqualToString:@FILE_EXT_ROM_GBA])
	{
		fileKind = @"GBA ROM";
	}
	
	return fileKind;
}

+ (NSString *) fileVersion:(NSURL *)fileURL
{
	NSString *fileVersion = @"Unknown Version";
	
	if (fileURL == nil)
	{
		return fileVersion;
	}
	
	NSString *versionStr = nil;
	NSString *portStr = nil;
	
	NSString *filePath = [fileURL path];
	NSURL *versionURL = nil;
	NSString *versionPath = @PATH_CONFIG_DIRECTORY_0_9_6;
	versionPath = [versionPath stringByExpandingTildeInPath];
	
	if ([[filePath stringByDeletingLastPathComponent] isEqualToString:versionPath])
	{
		versionStr = @"0.9.6";
		portStr = @"GTK";
	}
	
	versionURL = [CocoaDSFile getURLUserAppSupport:nil appVersion:@"0.9.7"];
	versionPath = [versionURL path];
	if (versionPath != nil && [[[filePath stringByDeletingLastPathComponent] stringByDeletingLastPathComponent] isEqualToString:versionPath])
	{
		versionStr = @"0.9.7";
		portStr = @"Cocoa";
	}
	
	versionURL = [CocoaDSFile getURLUserAppSupport:nil appVersion:@"0.9.8"];
	versionPath = [versionURL path];
	if (versionPath != nil && [[[filePath stringByDeletingLastPathComponent] stringByDeletingLastPathComponent] isEqualToString:versionPath])
	{
		versionStr = @"0.9.8";
		portStr = @"Cocoa";
	}
	
	fileVersion = [[versionStr stringByAppendingString:@" "] stringByAppendingString:portStr];
	
	return fileVersion;
}

+ (BOOL) fileExistsForCurrent:(NSURL *)fileURL
{
	BOOL result = NO;
	NSString *currentVersion = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleVersion"];
	NSDictionary *versionSearchDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"VersionPaths" ofType:@"plist"]];
	
	if (fileURL == nil)
	{
		return result;
	}
	else if (currentVersion == nil)
	{
		NSLog(@"Could not read the main application bundle!");
		return result;
	}
	else if (versionSearchDict == nil)
	{
		NSLog(@"Could not read the version plist file!");
		return result;
	}
	
	NSDictionary *versionDict = [versionSearchDict valueForKey:currentVersion];
	NSDictionary *portDict = [versionDict valueForKey:@PORT_VERSION];
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	
	NSString *existsPath = [[CocoaDSFile getURLUserAppSupportByKind:[CocoaDSFile fileKind:fileURL]] path];
	if (existsPath == nil)
	{
		return result;
	}
	
	existsPath = [existsPath stringByAppendingPathComponent:[[fileURL path] lastPathComponent]];
	result = [fileManager fileExistsAtPath:existsPath];
	
	[fileManager release];
	
	return result;
}

+ (NSURL *) getURLUserAppSupportByKind:(NSString *)fileKind
{
	NSURL *fileURL = nil;
	
	if ([fileKind isEqualToString:@"ROM Save"])
	{
		fileURL = [CocoaDSFile getURLUserAppSupport:@BATTERYKEY appVersion:nil];
	}
	else if ([fileKind isEqualToString:@"Cheat File"])
	{
		fileURL = [CocoaDSFile getURLUserAppSupport:@CHEATKEY appVersion:nil];
	}
	else if ([fileKind isEqualToString:@"Firmware Configuration"])
	{
		fileURL = [CocoaDSFile getURLUserAppSupport:@FIRMWAREKEY appVersion:nil];
	}
	else if ([fileKind isEqualToString:@"Save State"])
	{
		fileURL = [CocoaDSFile getURLUserAppSupport:@STATEKEY appVersion:nil];
	}
	
	return fileURL;
}

+ (BOOL) saveStateExistsForSlot:(NSURL *)romURL slotNumber:(NSUInteger)slotNumber
{
	BOOL exists = NO;
	
	if (romURL == nil)
	{
		return exists;
	}
	
	if (slotNumber == 10)
	{
		slotNumber = 0;
	}
	
	NSURL *searchURL = [CocoaDSFile getURLUserAppSupport:@STATEKEY appVersion:nil];
	NSString *searchFileName = [CocoaDSFile getSaveSlotFileName:romURL slotNumber:slotNumber];
	if (searchURL == nil || searchFileName == nil)
	{
		return exists;
	}
	
	NSString *searchFullPath = [[searchURL path] stringByAppendingPathComponent:searchFileName];
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	
	exists = [fileManager isReadableFileAtPath:searchFullPath];
	
	[fileManager release];
	
	return exists;
}

/********************************************************************************************
	isSaveStateSlotExtension:

	Determines if a given extension represents a save state file type.

	Takes:
		extension - An NSString representing the file extension.

	Returns:
		A BOOL indicating if extension represents a save state file type.

	Details:
		Save state file extensions are represented by .ds#, where # is an integer that
		is greater than or equal to 0.
 ********************************************************************************************/
+ (BOOL) isSaveStateSlotExtension:(NSString *)extension
{
	BOOL result = NO;
	
	if ([extension length] < 3 || ![extension hasPrefix:@"ds"])
	{
		return result;
	}
	
	NSString *slotNum = [extension substringFromIndex:2];
	if ([slotNum isEqualToString:@"0"])
	{
		result = YES;
	}
	else if ([slotNum intValue] != 0)
	{
		result = YES;
	}
	
	return result;
}

+ (NSString *) getSaveSlotFileName:(NSURL *)romURL slotNumber:(NSUInteger)slotNumber
{
	if (romURL == nil)
	{
		return nil;
	}
	
	if (slotNumber == 10)
	{
		slotNumber = 0;
	}
	
	NSString *romFileName = [[romURL path] lastPathComponent];
	NSString *fileExt = [NSString stringWithFormat:@"ds%d", slotNumber];
	
	return [[romFileName stringByDeletingPathExtension] stringByAppendingPathExtension:fileExt];
}

+ (NSURL *) getBaseURLUserAppSupport
{
	NSURL *userAppSupportURL = nil;
	NSString *userAppSupportPath = nil;
	NSString *appName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];
	
	NSArray *savePaths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
	if ([savePaths count] == 0)
	{
		return userAppSupportURL;
	}
	
	userAppSupportPath = [[savePaths objectAtIndex:0] stringByAppendingPathComponent:appName];
	userAppSupportURL = [NSURL fileURLWithPath:userAppSupportPath];
	
	return userAppSupportURL;
}

+ (NSURL *) getURLUserAppSupport:(NSString *)directoryName appVersion:(NSString *)appVersion
{
	NSURL *userAppSupportURL = [CocoaDSFile getBaseURLUserAppSupport];
	if (userAppSupportURL == nil)
	{
		return userAppSupportURL;
	}
	
	NSString *userAppSupportPath = [userAppSupportURL path];
	NSString *appVersionForURL = appVersion;
	
	// If nil is passed in for appVersion, then just use the current app version.
	if (appVersionForURL == nil)
	{
		appVersionForURL = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleVersion"];
	}
	
	userAppSupportPath = [userAppSupportPath stringByAppendingPathComponent:appVersionForURL];
	
	if (directoryName != nil)
	{
		userAppSupportPath = [userAppSupportPath stringByAppendingPathComponent:directoryName];
	}
	
	userAppSupportURL = [NSURL fileURLWithPath:userAppSupportPath];
	
	return userAppSupportURL;
}

+ (BOOL) createUserAppSupportDirectory:(NSString *)directoryName
{
	BOOL result = NO;
	
	NSString *tempPath = nil;
	NSString *appName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];
	NSString *appVersion = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleVersion"];
	
	if (directoryName == nil)
	{
		return result;
	}
	
	NSArray *savePaths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
	if ([savePaths count] > 0)
	{
		tempPath = [savePaths objectAtIndex:0];
	}
	else
	{
		return result;
	}
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	
#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4 // Mac OS X v10.4 and earlier.
	BOOL isDir = YES;
	
	tempPath = [tempPath stringByAppendingPathComponent:appName];
	result = [fileManager createDirectoryAtPath:tempPath attributes:nil];
	if (!result)
	{
		if(![fileManager fileExistsAtPath:tempPath isDirectory:&isDir])
		{
			[fileManager release];
			return result;
		}
	}
	
	tempPath = [tempPath stringByAppendingPathComponent:appVersion];
	result = [fileManager createDirectoryAtPath:tempPath attributes:nil];
	if (!result)
	{
		if(![fileManager fileExistsAtPath:tempPath isDirectory:&isDir])
		{
			[fileManager release];
			return result;
		}
	}
	
	tempPath = [tempPath stringByAppendingPathComponent:directoryName];
	result = [fileManager createDirectoryAtPath:tempPath attributes:nil];
	if (!result)
	{
		if(![fileManager fileExistsAtPath:tempPath isDirectory:&isDir])
		{
			[fileManager release];
			return result;
		}
	}
	
	/*
	 In Mac OS X v10.4 and earlier, having the File Manager create new directories where they already
	 exist returns NO. Note that this behavior is not per Apple's own documentation. Therefore, we
	 manually set result=YES at the end to make sure the function returns the right result.
	 */
	result = YES;
	
#else // Mac OS X v10.5 and later. Yes, it's that simple...
	tempPath = [tempPath stringByAppendingPathComponent:appName];
	tempPath = [tempPath stringByAppendingPathComponent:appVersion];
	tempPath = [tempPath stringByAppendingPathComponent:directoryName];
	result = [fileManager createDirectoryAtPath:tempPath withIntermediateDirectories:YES attributes:nil error:NULL];
#endif
	
	[fileManager release];
	return result;
}

+ (NSURL *) lastLoadedRomURL
{
	return [[[NSDocumentController sharedDocumentController] recentDocumentURLs] objectAtIndex:0];
}

#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_4

+ (BOOL) moveFileToCurrentDirectory:(NSURL *)fileURL
{
	BOOL result = NO;
	
	if (fileURL == nil)
	{
		return result;
	}
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	
	NSString *newLocationPath = [[CocoaDSFile getURLUserAppSupportByKind:[CocoaDSFile fileKind:fileURL]] path];
	if (newLocationPath == nil)
	{
		return result;
	}
	
	NSString *filePath = [fileURL path];
	newLocationPath = [newLocationPath stringByAppendingPathComponent:[filePath lastPathComponent]];
	result = [fileManager moveItemAtPath:filePath toPath:newLocationPath error:nil];
	
	[fileManager release];
	
	return result;
}

+ (BOOL) copyFileToCurrentDirectory:(NSURL *)fileURL
{
	BOOL result = NO;
	
	if (fileURL == nil)
	{
		return result;
	}
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	
	NSString *newLocationPath = [[CocoaDSFile getURLUserAppSupportByKind:[CocoaDSFile fileKind:fileURL]] path];
	if (newLocationPath == nil)
	{
		return result;
	}
	
	NSString *filePath = [fileURL path];
	newLocationPath = [newLocationPath stringByAppendingPathComponent:[filePath lastPathComponent]];
	result = [fileManager copyItemAtPath:filePath toPath:newLocationPath error:nil];
	
	[fileManager release];
	
	return result;
}

+ (BOOL) moveFileListToCurrent:(NSMutableArray *)fileList
{
	BOOL result = NO;
	
	if (fileList == nil)
	{
		return result;
	}
	
	for (NSDictionary *fileDict in fileList)
	{
		BOOL willMigrate = [[fileDict valueForKey:@"willMigrate"] boolValue];
		if (!willMigrate)
		{
			continue;
		}
		
		NSURL *fileURL = [NSURL URLWithString:[fileDict valueForKey:@"URL"]];
		[CocoaDSFile moveFileToCurrentDirectory:fileURL];
	}
	
	result = YES;
	
	return result;
}

+ (BOOL) copyFileListToCurrent:(NSMutableArray *)fileList
{
	BOOL result = NO;
	
	if (fileList == nil)
	{
		return result;
	}
	
	for (NSDictionary *fileDict in fileList)
	{
		BOOL willMigrate = [[fileDict valueForKey:@"willMigrate"] boolValue];
		if (!willMigrate)
		{
			continue;
		}
		
		NSURL *fileURL = [NSURL URLWithString:[fileDict valueForKey:@"URL"]];
		[CocoaDSFile copyFileToCurrentDirectory:fileURL];
	}
	
	result = YES;
	
	return result;
}

+ (NSMutableArray *) completeFileList
{
	NSMutableArray *fileList = [NSMutableArray arrayWithCapacity:100];
	if (fileList == nil)
	{
		return fileList;
	}
	
	NSString *currentVersion = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleVersion"];
	NSDictionary *versionTopDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"VersionPaths" ofType:@"plist"]];
	
	NSDictionary *versionDirectoriesDict = (NSDictionary *)[versionTopDict valueForKey:@"VersionDirectories"];
	NSArray *versionArray = [versionDirectoriesDict allKeys];
	
	for (NSString *versionKey in versionArray)
	{
		if ([currentVersion isEqualToString:versionKey])
		{
			continue;
		}
		
		NSDictionary *versionDict = (NSDictionary *)[versionDirectoriesDict valueForKey:versionKey];
		NSArray *portArray = [versionDict allKeys];
		
		for (NSString *portKey in portArray)
		{
			NSDictionary *portDict = (NSDictionary *)[versionDict valueForKey:portKey];
			NSArray *typeArray = [portDict allKeys];
			
			for (NSString *typeKey in typeArray)
			{
				NSString *typePath = [portDict valueForKey:typeKey];
				NSURL *typeURL = nil;
				
				if ([typePath isEqualToString:@"${WITHROM}"])
				{
					continue;
				}
				else if ([typePath isEqualToString:@"${APPSUPPORT}"])
				{
					typeURL = [CocoaDSFile getURLUserAppSupportByKind:typeKey];
					[fileList addObjectsFromArray:[CocoaDSFile appFileList:typeURL fileKind:typeKey]];
				}
				else
				{
					typeURL = [NSURL fileURLWithPath:[typePath stringByExpandingTildeInPath]];
					[fileList addObjectsFromArray:[CocoaDSFile appFileList:typeURL fileKind:typeKey]];
				}
			}
		}
	}
	
	return fileList;
}

+ (NSMutableArray *) appFileList:(NSURL *)directoryURL
{
	return [self appFileList:directoryURL fileKind:nil];
}

+ (NSMutableArray *) appFileList:(NSURL *)directoryURL fileKind:(NSString *)theFileKind
{
	NSMutableArray *outArray = nil;
	
	if (directoryURL == nil)
	{
		return outArray;
	}
	
	NSString *directoryPath = [directoryURL path];
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	
	NSArray *fileList = [fileManager contentsOfDirectoryAtPath:directoryPath error:nil];
	if (fileList == nil)
	{
		[fileManager release];
		return outArray;
	}
	
	outArray = [NSMutableArray arrayWithCapacity:100];
	if (outArray == nil)
	{
		[fileManager release];
		return outArray;
	}
	
	for (NSString *fileName in fileList)
	{
		NSNumber *willMigrate = [NSNumber numberWithBool:YES];
		NSString *filePath = [directoryPath stringByAppendingPathComponent:fileName];
		NSURL *fileURL = [NSURL fileURLWithPath:filePath];
		NSString *fileVersion = [CocoaDSFile fileVersion:fileURL];
		NSString *fileKind = [CocoaDSFile fileKind:fileURL];
		
		if (fileKind == nil ||
			(theFileKind != nil && ![theFileKind isEqualToString:fileKind]) ||
			[CocoaDSFile fileExistsForCurrent:fileURL])
		{
			continue;
		}
		
		NSDictionary *fileAttributes = [fileManager attributesOfItemAtPath:filePath error:nil];
		NSMutableDictionary *finalDict = [NSMutableDictionary dictionaryWithObjectsAndKeys:
										  willMigrate, @"willMigrate",
										  fileName, @"name",
										  fileVersion, @"version",
										  fileKind, @"kind",
										  [fileAttributes fileModificationDate], @"dateModified",
										  [fileURL absoluteString], @"URL",
										  nil];
		
		[outArray addObject:finalDict];
	}
	
	[fileManager release];
	
	return outArray;
}

#endif

@end
