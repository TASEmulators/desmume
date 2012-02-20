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

+ (BOOL) exportRomSaveToURL:(NSURL *)destinationURL romSaveURL:(NSURL *)romSaveURL fileType:(NSInteger)fileTypeID
{
	BOOL result = NO;
	
	switch (fileTypeID)
	{
		case ROMSAVEFORMAT_DESMUME:
		{
			NSString *destinationPath = [[destinationURL path] stringByAppendingPathExtension:@FILE_EXT_ROM_SAVE];
			NSFileManager *fileManager = [[NSFileManager alloc] init];
			result = [fileManager copyItemAtPath:[romSaveURL path] toPath:destinationPath error:nil];
			[fileManager release];
			break;
		}
			
		case ROMSAVEFORMAT_NOGBA:
		{
			const char *destinationPath = [[[destinationURL path] stringByAppendingPathExtension:@FILE_EXT_ROM_SAVE_NOGBA] cStringUsingEncoding:NSUTF8StringEncoding];
			bool resultCode = NDS_ExportSave(destinationPath);
			if (resultCode)
			{
				result = YES;
			}
			break;
		}
			
		case ROMSAVEFORMAT_RAW:
		{
			const char *destinationPath = [[[destinationURL path] stringByAppendingPathExtension:@FILE_EXT_ROM_SAVE_RAW] cStringUsingEncoding:NSUTF8StringEncoding];
			bool resultCode = NDS_ExportSave(destinationPath);
			if (resultCode)
			{
				result = YES;
			}
			break;
		}
			
		default:
			break;
	}
	
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
	
	NSString *romSavePath = [[CocoaDSFile fileURLFromRomURL:romURL toKind:@"ROM Save"] path];
	
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
	NSURL *romURL = [CocoaDSFile directoryURLByKind:@"ROM"];
	if (romURL != nil)
	{
		strlcpy(path.pathToRoms, [[romURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	}
	
	NSURL *romSaveURL = [CocoaDSFile directoryURLByKind:@"ROM Save"];
	if (romSaveURL != nil)
	{
		strlcpy(path.pathToBattery, [[romSaveURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	}
	
	NSURL *saveStateURL = [CocoaDSFile directoryURLByKind:@"Save State"];
	if (saveStateURL != nil)
	{
		strlcpy(path.pathToStates, [[saveStateURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	}
	
	NSURL *screenshotURL = [CocoaDSFile directoryURLByKind:@"Screenshot"];
	if (screenshotURL != nil)
	{
		strlcpy(path.pathToScreenshots, [[screenshotURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	}
	
	NSURL *aviURL = [CocoaDSFile directoryURLByKind:@"Video"];
	if (aviURL != nil)
	{
		strlcpy(path.pathToAviFiles, [[aviURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	}
	
	NSURL *cheatURL = [CocoaDSFile directoryURLByKind:@"Cheat"];
	if (cheatURL != nil)
	{
		strlcpy(path.pathToCheats, [[cheatURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	}
	
	NSURL *soundSamplesURL = [CocoaDSFile directoryURLByKind:@"Sound Sample"];
	if (soundSamplesURL != nil)
	{
		strlcpy(path.pathToSounds, [[soundSamplesURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	}
	
	NSURL *firmwareURL = [CocoaDSFile directoryURLByKind:@"Firmware Configuration"];
	if (firmwareURL != nil)
	{
		strlcpy(path.pathToFirmware, [[firmwareURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	}
	
	NSURL *luaURL = [CocoaDSFile directoryURLByKind:@"Lua Script"];
	if (luaURL != nil)
	{
		strlcpy(path.pathToLua, [[luaURL path] cStringUsingEncoding:NSUTF8StringEncoding], MAX_PATH);
	}
}

+ (BOOL) setupAllAppDirectories
{
	BOOL result = YES;
	NSString *currentVersion = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleVersion"];
	NSDictionary *fileTypeInfoRootDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"FileTypeInfo" ofType:@"plist"]];
	
	NSDictionary *pathDict = (NSDictionary *)[fileTypeInfoRootDict valueForKey:@"DefaultPaths"];
	NSDictionary *pathVersionDict = (NSDictionary *)[pathDict valueForKey:currentVersion];
	NSDictionary *pathPortDict = (NSDictionary *)[pathVersionDict valueForKey:@PORT_VERSION];
	
	NSDictionary *dirNameDict = (NSDictionary *)[fileTypeInfoRootDict valueForKey:@"DirectoryNames"];
	NSDictionary *dirNameVersionDict = (NSDictionary *)[dirNameDict valueForKey:currentVersion];
	NSDictionary *dirNamePortDict = (NSDictionary *)[dirNameVersionDict valueForKey:@PORT_VERSION];
	
	NSArray *fileKindList = [pathPortDict allKeys];
	
#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4 // Mac OS X v10.4 and earlier.
	NSEnumerator *enumerator = [fileKindList objectEnumerator];
	NSString *fileKind;
	
	while ((fileKind = (NSString *)[enumerator nextObject]) != nil)
#else // Mac OS X v10.5 and later.
	for (NSString *fileKind in fileKindList)
#endif
	{
		NSString *dirPath = (NSString *)[pathPortDict valueForKey:fileKind];
		NSString *dirName = (NSString *)[dirNamePortDict valueForKey:fileKind];
		
		if ([dirPath isEqualToString:@PATH_USER_APP_SUPPORT])
		{
			if (![CocoaDSFile createUserAppSupportDirectory:dirName])
			{
				result = NO;
			}
		}
		else if ([dirPath isEqualToString:@PATH_WITH_ROM])
		{
			continue;
		}
		else
		{
			// TODO: Setup directory when using an absolute path.
		}
	}
	
	return result;
}

+ (NSURL *) saveStateURL
{
	return [CocoaDSFile directoryURLByKind:@"Save State"];
}

+ (BOOL) saveScreenshot:(NSURL *)fileURL bitmapData:(NSBitmapImageRep *)bitmapImageRep fileType:(NSBitmapImageFileType)fileType
{
	BOOL result = NO;
	
	if (fileURL == nil || bitmapImageRep == nil)
	{
		return result;
	}
	
	NSString *fileExt = nil;
	switch (fileType)
	{
		case NSTIFFFileType:
			fileExt = @"tiff";
			break;
			
		case NSBMPFileType:
			fileExt = @"bmp";
			break;
			
		case NSGIFFileType:
			fileExt = @"gif";
			break;
			
		case NSJPEGFileType:
			fileExt = @"jpg";
			break;
			
		case NSPNGFileType:
			fileExt = @"png";
			break;
			
		case NSJPEG2000FileType:
			fileExt = @"jp2";
			break;
			
		default:
			break;
	}
	
	NSURL *saveFileURL = [NSURL fileURLWithPath:[[fileURL path] stringByAppendingPathExtension:fileExt]];
	
	result = [[bitmapImageRep representationUsingType:fileType properties:[NSDictionary dictionary]] writeToURL:saveFileURL atomically:NO];
	
	return result;
}

/********************************************************************************************
	fileKindByURL:

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
+ (NSString *) fileKindByURL:(NSURL *)fileURL
{
	return [CocoaDSFile fileKindByURL:fileURL version:nil port:nil];
}

+ (NSString *) fileKindByURL:(NSURL *)fileURL version:(NSString *)versionString port:(NSString *)portString
{
	NSString *fileKind = nil;
	
	if (fileURL == nil)
	{
		return fileKind;
	}
	
	NSString *lookupVersionStr = versionString;
	NSString *lookupPortStr = portString;
	
	if (lookupVersionStr == nil)
	{
		lookupVersionStr = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleVersion"];
	}
	
	if (lookupPortStr == nil)
	{
		lookupPortStr = @PORT_VERSION;
	}
	
	NSDictionary *fileTypeInfoRootDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"FileTypeInfo" ofType:@"plist"]];
	NSDictionary *fileTypeExtDict = (NSDictionary *)[fileTypeInfoRootDict valueForKey:@"FileTypeByExtensions"];
	NSDictionary *versionDict = (NSDictionary *)[fileTypeExtDict valueForKey:lookupVersionStr];
	NSDictionary *portDict = (NSDictionary *)[versionDict valueForKey:lookupPortStr];
	
	NSString *fileExt = [[fileURL path] pathExtension];
	
	fileKind = (NSString *)[portDict valueForKey:fileExt];
	if (fileKind == nil)
	{
		if ([CocoaDSFile isSaveStateSlotExtension:fileExt])
		{
			fileKind = (NSString *)[portDict valueForKey:@FILE_EXT_SAVE_STATE];
		}
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
	
	versionURL = [CocoaDSFile userAppSupportURL:nil version:@"0.9.7"];
	versionPath = [versionURL path];
	if (versionPath != nil && [[[filePath stringByDeletingLastPathComponent] stringByDeletingLastPathComponent] isEqualToString:versionPath])
	{
		versionStr = @"0.9.7";
		portStr = @"Cocoa";
	}
	
	versionURL = [CocoaDSFile userAppSupportURL:nil version:@"0.9.8"];
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
	
	if (fileURL == nil)
	{
		return result;
	}
	
	NSString *filePath = [[CocoaDSFile directoryURLByKind:[CocoaDSFile fileKindByURL:fileURL]] path];
	filePath = [filePath stringByAppendingPathComponent:[[fileURL path] lastPathComponent]];
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	result = [fileManager fileExistsAtPath:filePath];
	[fileManager release];
	
	return result;
}

+ (NSURL *) fileURLFromRomURL:(NSURL *)romURL toKind:(NSString *)fileKind
{
	return [CocoaDSFile fileURLFromRomURL:romURL toKind:fileKind version:nil port:nil];
}

+ (NSURL *) fileURLFromRomURL:(NSURL *)romURL toKind:(NSString *)fileKind version:(NSString *)versionString port:(NSString *)portString
{
	NSURL *fileURL = nil;
	
	NSString *fileExt = [CocoaDSFile fileExtensionByKind:fileKind version:versionString port:portString];
	if (fileExt == nil)
	{
		return fileURL;
	}
	
	if ([CocoaDSFile isFileKindWithRom:fileKind])
	{
		fileURL = [NSURL fileURLWithPath:[[[romURL path] stringByDeletingPathExtension] stringByAppendingPathExtension:fileExt]];
	}
	else
	{
		NSURL *dirURL = [CocoaDSFile directoryURLByKind:fileKind version:versionString port:portString];
		if (dirURL != nil)
		{
			NSString *newFileName = [[[[romURL path] lastPathComponent] stringByDeletingPathExtension] stringByAppendingPathExtension:fileExt];
			fileURL = [NSURL fileURLWithPath:[[dirURL path] stringByAppendingPathComponent:newFileName]];
		}
	}
	
	return fileURL;
}

+ (NSString *) fileExtensionByKind:(NSString *)fileKind
{
	return [CocoaDSFile fileExtensionByKind:fileKind version:nil port:nil];
}

+ (NSString *) fileExtensionByKind:(NSString *)fileKind version:(NSString *)versionString port:(NSString *)portString
{
	NSString *fileExt = nil;
	
	if (fileKind == nil)
	{
		return fileExt;
	}
	
	NSString *lookupVersionStr = versionString;
	NSString *lookupPortStr = portString;
	
	if (lookupVersionStr == nil)
	{
		lookupVersionStr = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleVersion"];
	}
	
	if (lookupPortStr == nil)
	{
		lookupPortStr = @PORT_VERSION;
	}
	
	NSDictionary *fileTypeInfoRootDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"FileTypeInfo" ofType:@"plist"]];
	NSDictionary *fileExtDict = (NSDictionary *)[fileTypeInfoRootDict valueForKey:@"FileExtensionByTypes"];
	NSDictionary *versionDict = (NSDictionary *)[fileExtDict valueForKey:lookupVersionStr];
	NSDictionary *portDict = (NSDictionary *)[versionDict valueForKey:lookupPortStr];
	
	fileExt = (NSString *)[portDict valueForKey:fileKind];
	
	return fileExt;
}

+ (NSURL *) directoryURLByKind:(NSString *)fileKind
{
	return [CocoaDSFile directoryURLByKind:fileKind version:nil port:nil];
}

+ (NSURL *) directoryURLByKind:(NSString *)fileKind version:(NSString *)versionString port:(NSString *)portString
{
	NSURL *fileURL = nil;
	NSString *lookupVersionStr = versionString;
	NSString *lookupPortStr = portString;
	NSDictionary *fileTypeInfoRootDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"FileTypeInfo" ofType:@"plist"]];
	
	if (lookupVersionStr == nil)
	{
		lookupVersionStr = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleVersion"];
	}
	
	if (lookupPortStr == nil)
	{
		lookupPortStr = @PORT_VERSION;
	}
	
	NSDictionary *pathDict = (NSDictionary *)[fileTypeInfoRootDict valueForKey:@"DefaultPaths"];
	NSDictionary *pathVersionDict = (NSDictionary *)[pathDict valueForKey:lookupVersionStr];
	NSDictionary *pathPortDict = (NSDictionary *)[pathVersionDict valueForKey:lookupPortStr];
	
	NSDictionary *dirNameDict = (NSDictionary *)[fileTypeInfoRootDict valueForKey:@"DirectoryNames"];
	NSDictionary *dirNameVersionDict = (NSDictionary *)[dirNameDict valueForKey:lookupVersionStr];
	NSDictionary *dirNamePortDict = (NSDictionary *)[dirNameVersionDict valueForKey:lookupPortStr];
	
	NSString *dirPath = (NSString *)[pathPortDict valueForKey:fileKind];
	if (dirPath != nil)
	{
		NSString *dirName = (NSString *)[dirNamePortDict valueForKey:fileKind];
		
		if ([dirPath isEqualToString:@PATH_USER_APP_SUPPORT])
		{
			fileURL = [CocoaDSFile userAppSupportURL:dirName version:lookupVersionStr];
		}
		else if ([dirPath isEqualToString:@PATH_WITH_ROM])
		{
			return fileURL;
		}
		else
		{
			fileURL = [NSURL fileURLWithPath:[dirPath stringByExpandingTildeInPath]];
		}
	}
	
	return fileURL;
}

+ (BOOL) isFileKindWithRom:(NSString *)fileKind
{
	return [CocoaDSFile isFileKindWithRom:fileKind version:nil port:nil];
}

+ (BOOL) isFileKindWithRom:(NSString *)fileKind version:(NSString *)versionString port:(NSString *)portString
{
	BOOL result = NO;
	NSString *lookupVersionStr = versionString;
	NSString *lookupPortStr = portString;
	NSDictionary *fileTypeInfoRootDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"FileTypeInfo" ofType:@"plist"]];
	
	if (lookupVersionStr == nil)
	{
		lookupVersionStr = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleVersion"];
	}
	
	if (lookupPortStr == nil)
	{
		lookupPortStr = @PORT_VERSION;
	}
	
	NSDictionary *pathDict = (NSDictionary *)[fileTypeInfoRootDict valueForKey:@"DefaultPaths"];
	NSDictionary *pathVersionDict = (NSDictionary *)[pathDict valueForKey:lookupVersionStr];
	NSDictionary *pathPortDict = (NSDictionary *)[pathVersionDict valueForKey:lookupPortStr];
	NSString *dirPath = (NSString *)[pathPortDict valueForKey:fileKind];
	
	if ([dirPath isEqualToString:@PATH_WITH_ROM])
	{
		result = YES;
	}
	
	return result;
}

+ (BOOL) saveStateExistsForSlot:(NSURL *)romURL slotNumber:(NSUInteger)slotNumber
{
	BOOL exists = NO;
	NSString *fileKind = @"Save State";
	NSString *saveStateFilePath = nil;
	
	if (romURL == nil)
	{
		return exists;
	}
	
	if (slotNumber == 10)
	{
		slotNumber = 0;
	}
	
	NSString *saveStateFileName = [CocoaDSFile saveSlotFileName:romURL slotNumber:slotNumber];
	if (saveStateFileName == nil)
	{
		return exists;
	}
	
	if ([CocoaDSFile isFileKindWithRom:fileKind])
	{
		saveStateFilePath = [[[romURL path] stringByDeletingLastPathComponent] stringByAppendingPathComponent:saveStateFileName];
	}
	else
	{
		NSURL *dirURL = [CocoaDSFile directoryURLByKind:fileKind];
		if (dirURL != nil)
		{
			saveStateFilePath = [[dirURL path] stringByAppendingPathComponent:saveStateFileName];
		}
	}
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	exists = [fileManager isReadableFileAtPath:saveStateFilePath];
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

+ (NSString *) saveSlotFileName:(NSURL *)romURL slotNumber:(NSUInteger)slotNumber
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

+ (NSURL *) userAppSupportBaseURL
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

+ (NSURL *) userAppSupportURL:(NSString *)directoryName version:(NSString *)versionString
{
	NSURL *userAppSupportURL = [CocoaDSFile userAppSupportBaseURL];
	if (userAppSupportURL == nil)
	{
		return userAppSupportURL;
	}
	
	NSString *userAppSupportPath = [userAppSupportURL path];
	NSString *versionStringForURL = versionString;
	
	// If nil is passed in for appVersion, then just use the current app version.
	if (versionStringForURL == nil)
	{
		versionStringForURL = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleVersion"];
	}
	
	userAppSupportPath = [userAppSupportPath stringByAppendingPathComponent:versionStringForURL];
	
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
	
#else // Mac OS X v10.5 and later. Yes, the code is this simple...
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
	
	NSString *newLocationPath = [[CocoaDSFile directoryURLByKind:[CocoaDSFile fileKindByURL:fileURL]] path];
	if (newLocationPath == nil)
	{
		return result;
	}
	
	NSString *filePath = [fileURL path];
	newLocationPath = [newLocationPath stringByAppendingPathComponent:[filePath lastPathComponent]];
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
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
	
	NSString *newLocationPath = [[CocoaDSFile directoryURLByKind:[CocoaDSFile fileKindByURL:fileURL]] path];
	if (newLocationPath == nil)
	{
		return result;
	}
	
	NSString *filePath = [fileURL path];
	newLocationPath = [newLocationPath stringByAppendingPathComponent:[filePath lastPathComponent]];
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
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
	NSDictionary *fileTypeInfoRootDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"FileTypeInfo" ofType:@"plist"]];
	NSDictionary *filePathsDict = (NSDictionary *)[fileTypeInfoRootDict valueForKey:@"DefaultPaths"];
	NSArray *versionArray = [filePathsDict allKeys];
	
	for (NSString *versionKey in versionArray)
	{
		if ([currentVersion isEqualToString:versionKey])
		{
			continue;
		}
		
		NSDictionary *versionDict = (NSDictionary *)[filePathsDict valueForKey:versionKey];
		NSArray *portArray = [versionDict allKeys];
		
		for (NSString *portKey in portArray)
		{
			NSDictionary *portDict = (NSDictionary *)[versionDict valueForKey:portKey];
			NSArray *fileKindList = [portDict allKeys];
			
			for (NSString *fileKind in fileKindList)
			{
				NSURL *dirURL = [CocoaDSFile directoryURLByKind:fileKind version:versionKey port:portKey];
				[fileList addObjectsFromArray:[CocoaDSFile appFileList:dirURL fileKind:fileKind]];
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
		NSString *fileKind = [CocoaDSFile fileKindByURL:fileURL];
		
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
