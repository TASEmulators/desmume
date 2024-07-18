/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2024 DeSmuME team

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

#import "cocoa_rom.h"
#import "cocoa_file.h"
#import "cocoa_globals.h"
#import "cocoa_util.h"

#include "../../NDSSystem.h"
#include "../../GPU.h"
#include "../../Database.h"
#include "../../mc.h"
#undef BOOL


@implementation CocoaDSRom

@synthesize header;
@synthesize bindings;
@synthesize fileURL;
@dynamic willStreamLoadData;
@dynamic isDataLoaded;
@synthesize saveType;

static NSMutableDictionary *saveTypeValues = nil;

- (id)init
{
	return [self initWithURL:nil];
}

- (id) initWithURL:(NSURL *)theURL
{
	return [self initWithURL:theURL saveType:ROMSAVETYPE_AUTOMATIC];
}

- (id) initWithURL:(NSURL *)theURL saveType:(NSInteger)saveTypeID
{
	return [self initWithURL:theURL saveType:ROMSAVETYPE_AUTOMATIC streamLoadData:NO];
}

- (id) initWithURL:(NSURL *)theURL saveType:(NSInteger)saveTypeID streamLoadData:(BOOL)willStreamLoad
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	if (saveTypeValues == nil)
	{
		saveTypeValues = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
						  [NSNumber numberWithInteger:1], @"eeprom - 4 kbit",
						  [NSNumber numberWithInteger:2], @"eeprom - 64 kbit",
						  [NSNumber numberWithInteger:3], @"eeprom - 512 kbit",
						  [NSNumber numberWithInteger:5], @"flash - 2 mbit",
						  [NSNumber numberWithInteger:6], @"flash - 4 mbit",
						  [NSNumber numberWithInteger:7], @"flash - 8 mbit",
						  [NSNumber numberWithInteger:10], @"flash - 64 mbit",
						  [NSNumber numberWithInteger:0], @"none",
						  [NSNumber numberWithInteger:0], @"tbc",
						  nil];
	}
	
	header = [[NSMutableDictionary alloc] initWithCapacity:32];
	if (header == nil)
	{
		[self release];
		self = nil;
		return self;
	}
	
	bindings = [[CocoaDSRom romNotLoadedBindings] retain];
	if (bindings == nil)
	{
		[header release];
		[self release];
		self = nil;
		return self;
	}
	
	fileURL = nil;
	saveType = saveTypeID;
	
	xmlCurrentRom = nil;
	xmlElementStack = [[NSMutableArray alloc] initWithCapacity:32];
	xmlCharacterStack = [[NSMutableArray alloc] initWithCapacity:32];
	
	[self setWillStreamLoadData:willStreamLoad];
	
	if (theURL != nil)
	{
		[self loadData:theURL];
	}
	
	return self;
}

- (void)dealloc
{
	if ([self isDataLoaded])
	{
		NDS_FreeROM();
	}
	
	[xmlElementStack release];
	[xmlCharacterStack release];
	[header release];
	[bindings release];
	[fileURL release];
	
	[super dealloc];
}

- (void) setWillStreamLoadData:(BOOL)theState
{
	CommonSettings.loadToMemory = (theState) ? false : true;
}

- (BOOL) willStreamLoadData
{
	return (CommonSettings.loadToMemory ? NO : YES);
}

- (BOOL) isDataLoaded
{
	return (gameInfo.romdataForReader != NULL);
}

- (BOOL) initHeader
{
	BOOL result = NO;
	
	const NDS_header *ndsRomHeader = NDS_getROMHeader();
	const RomBanner &ndsRomBanner = gameInfo.getRomBanner();
	
	if (self.header == nil || self.bindings == nil)
	{
		return result;
	}
	
	[self.header setValue:[self banner:ndsRomBanner.titles[0]] forKey:@"bannerJapanese"];
	[self.header setValue:[self banner:ndsRomBanner.titles[1]] forKey:@"bannerEnglish"];
	[self.header setValue:[self banner:ndsRomBanner.titles[2]] forKey:@"bannerFrench"];
	[self.header setValue:[self banner:ndsRomBanner.titles[3]] forKey:@"bannerGerman"];
	[self.header setValue:[self banner:ndsRomBanner.titles[4]] forKey:@"bannerItalian"];
	[self.header setValue:[self banner:ndsRomBanner.titles[5]] forKey:@"bannerSpanish"];
	
	[self.bindings setValue:[self.header objectForKey:@"bannerJapanese"] forKey:@"bannerJapanese"];
	[self.bindings setValue:[self.header objectForKey:@"bannerEnglish"] forKey:@"bannerEnglish"];
	[self.bindings setValue:[self.header objectForKey:@"bannerFrench"] forKey:@"bannerFrench"];
	[self.bindings setValue:[self.header objectForKey:@"bannerGerman"] forKey:@"bannerGerman"];
	[self.bindings setValue:[self.header objectForKey:@"bannerItalian"] forKey:@"bannerItalian"];
	[self.bindings setValue:[self.header objectForKey:@"bannerSpanish"] forKey:@"bannerSpanish"];
	
	if (ndsRomHeader != NULL)
	{
		[self.header setValue:[self title] forKey:@"gameTitle"];
		[self.header setValue:[self code] forKey:@"gameCode"];
		[self.header setValue:[self developerName] forKey:@"gameDeveloper"];
		[self.header setValue:[self developerNameAndCode] forKey:@"gameDeveloperWithCode"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->makerCode] forKey:@"makerCode"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->unitCode] forKey:@"unitCode"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->cardSize] forKey:@"romSize"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->ARM9src] forKey:@"arm9BinaryOffset"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->ARM9exe] forKey:@"arm9BinaryEntryAddress"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->ARM9cpy] forKey:@"arm9BinaryStartAddress"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->ARM9binSize] forKey:@"arm9BinarySize"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->ARM7src] forKey:@"arm7BinaryOffset"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->ARM7exe] forKey:@"arm7BinaryEntryAddress"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->ARM7cpy] forKey:@"arm7BinaryStartAddress"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->ARM7binSize] forKey:@"arm7BinarySize"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->FNameTblOff] forKey:@"fntOffset"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->FNameTblSize] forKey:@"fntTableSize"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->FATOff] forKey:@"fatOffset"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->FATSize] forKey:@"fatSize"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->IconOff] forKey:@"iconOffset"];
		[self.header setValue:[NSNumber numberWithInteger:ndsRomHeader->endROMoffset] forKey:@"usedRomSize"];
		
		[self.bindings setValue:[self.header objectForKey:@"gameTitle"] forKey:@"gameTitle"];
		[self.bindings setValue:[self.header objectForKey:@"gameCode"] forKey:@"gameCode"];
		[self.bindings setValue:[self.header objectForKey:@"gameDeveloper"] forKey:@"gameDeveloper"];
		[self.bindings setValue:[self.header objectForKey:@"gameDeveloperWithCode"] forKey:@"gameDeveloperWithCode"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%04X", [[self.header objectForKey:@"makerCode"] intValue]] forKey:@"makerCode"];
		[self.bindings setValue:[self unitCodeStringUsingID:[[self.header objectForKey:@"unitCode"] intValue]] forKey:@"unitCode"];
		[self.bindings setValue:[CocoaDSRom byteSizeStringWithLargerUnit:(128*1024) << [[self.header objectForKey:@"romSize"] intValue]] forKey:@"romSize"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"arm9BinaryOffset"] intValue]] forKey:@"arm9BinaryOffset"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"arm9BinaryEntryAddress"] intValue]] forKey:@"arm9BinaryEntryAddress"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"arm9BinaryStartAddress"] intValue]] forKey:@"arm9BinaryStartAddress"];
		[self.bindings setValue:[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, (unsigned long long)[[self.header objectForKey:@"arm9BinarySize"] intValue]] forKey:@"arm9BinarySize"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"arm7BinaryOffset"] intValue]] forKey:@"arm7BinaryOffset"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"arm7BinaryEntryAddress"] intValue]] forKey:@"arm7BinaryEntryAddress"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"arm7BinaryStartAddress"] intValue]] forKey:@"arm7BinaryStartAddress"];
		[self.bindings setValue:[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, (unsigned long long)[[self.header objectForKey:@"arm7BinarySize"] intValue]] forKey:@"arm7BinarySize"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"fntOffset"] intValue]] forKey:@"fntOffset"];
		[self.bindings setValue:[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, (unsigned long long)[[self.header objectForKey:@"fntTableSize"] intValue]] forKey:@"fntTableSize"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"fatOffset"] intValue]] forKey:@"fatOffset"];
		[self.bindings setValue:[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, (unsigned long long)[[self.header objectForKey:@"fatSize"] intValue]] forKey:@"fatSize"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"fatOffset"] intValue]] forKey:@"fatOffset"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"iconOffset"] intValue]] forKey:@"iconOffset"];
		[self.bindings setValue:[CocoaDSRom byteSizeStringWithLargerUnit:[[self.header objectForKey:@"usedRomSize"] intValue]] forKey:@"usedRomSize"];
		[self.bindings setValue:[CocoaDSRom byteSizeStringWithLargerUnit:(((128*1024) << [[self.header objectForKey:@"romSize"] intValue]) - [[self.header objectForKey:@"usedRomSize"] intValue])] forKey:@"unusedCapacity"];
	}
	
	// Get ROM image
	NSImage *iconImage = [self icon];
	if (iconImage != nil)
	{
		[self.header setObject:iconImage forKey:@"iconImage"];
		[self.bindings setObject:(NSImage *)[self.header objectForKey:@"iconImage"] forKey:@"iconImage"];
	}
	
	NSString *internalNameString = [self internalName];
	NSString *serialString = [self serial];
	
	if (internalNameString == nil || serialString == nil)
	{
		return result;
	}
	else
	{
		[self.header setValue:internalNameString forKey:@"romInternalName"];
		[self.header setValue:serialString forKey:@"romSerial"];
		
		[self.bindings setValue:[self.header objectForKey:@"romInternalName"] forKey:@"romInternalName"];
		[self.bindings setValue:[self.header objectForKey:@"romSerial"] forKey:@"romSerial"];
		
		NSString *romNameAndSerialInfoString = @"Name: ";
		romNameAndSerialInfoString = [romNameAndSerialInfoString stringByAppendingString:[self.header objectForKey:@"romInternalName"]];
		romNameAndSerialInfoString = [[romNameAndSerialInfoString stringByAppendingString:@"\nSerial: "] stringByAppendingString:[self.header objectForKey:@"romSerial"]];
		[self.bindings setValue:romNameAndSerialInfoString forKey:@"romNameAndSerialInfo"];
	}
	
	result = YES;
	return result;
}

- (BOOL) loadData:(NSURL *)theURL
{
	[CocoaDSRom changeRomSaveType:saveType];
	
	BOOL result = [CocoaDSFile loadRom:theURL];
	
	if (result)
	{
		result = [self initHeader];
	}
	
	if (!result)
	{
		NSDictionary *userInfo = [[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithBool:NO], @"DidLoad", nil];
		[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadName:@"org.desmume.DeSmuME.loadRomDidFinish" object:self userInfo:userInfo];
		[userInfo release];
		return result;
	}
	
	fileURL = [theURL copy];
	
	NSString *advscDBPath = [[NSUserDefaults standardUserDefaults] stringForKey:@"Advanscene_DatabasePath"];
	if (advscDBPath != nil)
	{
		NSXMLParser *advscDB = [[NSXMLParser alloc] initWithContentsOfURL:[NSURL fileURLWithPath:advscDBPath]];
		[advscDB setDelegate:self];
		[advscDB parse];
		[advscDB release];
	}
	
	NSDictionary *userInfo = [[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithBool:YES], @"DidLoad", self.fileURL, @"URL", nil];
	[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadName:@"org.desmume.DeSmuME.loadRomDidFinish" object:self userInfo:userInfo];
	[userInfo release];
	
	return result;
}

- (void) loadDataOnThread:(id)object
{
	[self retain];
	
	NSURL *theURL = [(NSURL *)object copy];
	NSAutoreleasePool *threadPool = [[NSAutoreleasePool alloc] init];
	
	[self loadData:theURL];
	
	[threadPool release];
	[theURL release];
	
	[self release];
}

- (NSString *) title
{
	NDS_header *ndsRomHeader = NDS_getROMHeader();
	if (ndsRomHeader == nil)
	{
		return nil;
	}
	
	return [[[NSString alloc] initWithBytes:ndsRomHeader->gameTile length:ROMINFO_GAME_TITLE_LENGTH encoding:NSUTF8StringEncoding] autorelease];
}

- (NSString *) code
{
	NDS_header *ndsRomHeader = NDS_getROMHeader();
	if (ndsRomHeader == nil)
	{
		return nil;
	}
	
	return [[[NSString alloc] initWithBytes:ndsRomHeader->gameCode length:ROMINFO_GAME_CODE_LENGTH encoding:NSUTF8StringEncoding] autorelease];
}

- (NSString *) banner:(const UInt16 *)UTF16TextBuffer
{
	NSUInteger bannerLength = ROMINFO_GAME_BANNER_LENGTH * sizeof(*UTF16TextBuffer);
	
	return [[[NSString alloc] initWithBytes:UTF16TextBuffer length:bannerLength encoding:NSUTF16LittleEndianStringEncoding] autorelease];
}

- (NSString *) internalName
{
	return [NSString stringWithCString:gameInfo.ROMname encoding:NSUTF8StringEncoding];
}

- (NSString *) serial
{
	return [NSString stringWithCString:gameInfo.ROMserial encoding:NSUTF8StringEncoding];
}

- (NSString *) developerName
{
	NDS_header *ndsRomHeader = NDS_getROMHeader();
	if (ndsRomHeader == nil)
	{
		return nil;
	}
	
	return [NSString stringWithCString:(Database::MakerNameForMakerCode(ndsRomHeader->makerCode, true)) encoding:NSUTF8StringEncoding];
}

- (NSString *) developerNameAndCode
{
	NDS_header *ndsRomHeader = NDS_getROMHeader();
	if (ndsRomHeader == nil)
	{
		return nil;
	}
	
	return [NSString stringWithFormat:@"%s [0x%04X]", Database::MakerNameForMakerCode(ndsRomHeader->makerCode, true), ndsRomHeader->makerCode];
}

- (NSString *) unitCodeStringUsingID:(NSInteger)unitCodeID
{
	switch (unitCodeID)
	{
		case 0:
			return @"NDS";
			break;
			
		case 1:
			return @"DSi (Invalid ID)";
			break;
			
		case 2:
			return @"NDS + DSi";
			break;
			
		case 3:
			return @"DSi";
			break;
			
		default:
			break;
	}
	
	return @"Unknown";
}

- (NSImage *) icon
{
	NSImage *newImage = nil;
	
	NDS_header *ndsRomHeader = NDS_getROMHeader();
	if (ndsRomHeader == nil)
	{
		return newImage;
	}
	
	NSUInteger iconOffset = ndsRomHeader->IconOff;
	if(iconOffset == 0)
	{
		return newImage;
	}
	
	newImage = [[NSImage alloc] initWithSize:NSMakeSize(32, 32)];
	if(newImage == nil)
	{
		return newImage;
	}
	
	NSBitmapImageRep *imageRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																		 pixelsWide:ROM_ICON_WIDTH
																		 pixelsHigh:ROM_ICON_HEIGHT
																	  bitsPerSample:8
																	samplesPerPixel:4
																		   hasAlpha:YES
																		   isPlanar:NO
																	 colorSpaceName:NSCalibratedRGBColorSpace
																		bytesPerRow:ROM_ICON_WIDTH * 4
																	   bitsPerPixel:32];
	
	if(imageRep == nil)
	{
		[newImage release];
		newImage = nil;
		return newImage;
	}
	
	uint32_t *bitmapData = (uint32_t *)[imageRep bitmapData];
	RomIconToRGBA8888(bitmapData);
	
#ifdef MSB_FIRST
	for (size_t i = 0; i < ROM_ICON_WIDTH * ROM_ICON_HEIGHT; i++)
	{
		bitmapData[i] = LE_TO_LOCAL_32(bitmapData[i]);
	}
#endif
	
	[imageRep autorelease];
	[newImage addRepresentation:imageRep];
	
	return [newImage autorelease];
}

- (void) handleAdvansceneDatabaseInfo
{
	if (xmlCurrentRom == nil)
	{
		return;
	}
	
	// Set the ROM save type.
	BOOL useAdvscForRomSave = [[NSUserDefaults standardUserDefaults] boolForKey:@"Advanscene_AutoDetectRomSaveType"];
	if (useAdvscForRomSave && (saveType == ROMSAVETYPE_AUTOMATIC))
	{
		NSString *saveTypeString = (NSString *)[xmlCurrentRom valueForKey:@"saveType"];
		NSInteger saveTypeID = [CocoaDSRom saveTypeByString:saveTypeString];
		[CocoaDSRom changeRomSaveType:saveTypeID];
		
		if (saveTypeID != ROMSAVETYPE_AUTOMATIC)
		{
			NSLog(@"Set the ROM save type using the ADVANsCEne database: %s.", [saveTypeString cStringUsingEncoding:NSUTF8StringEncoding]);
		}
	}
}

- (void)parser:(NSXMLParser *)parser didStartElement:(NSString *)elementName namespaceURI:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName attributes:(NSDictionary *)attributeDict
{
	NSMutableString *xmlCharacters = [NSMutableString stringWithCapacity:1024];
	[xmlCharacters setString:@""];
	
	[xmlCharacterStack addObject:xmlCharacters];
	[xmlElementStack addObject:elementName];
	
	if ([elementName isEqualToString:@"game"])
	{
		if (xmlCurrentRom == nil)
		{
			xmlCurrentRom = [[NSMutableDictionary alloc] initWithCapacity:32];
		}
	}
}

- (void)parser:(NSXMLParser *)parser didEndElement:(NSString *)elementName namespaceURI:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName
{
	NSString *xmlElement = (NSString *)[xmlElementStack lastObject];
	if (xmlElement == nil || ![xmlElement isEqualToString:elementName])
	{
		return;
	}
	
	NSMutableString *xmlCharacters = (NSMutableString *)[xmlCharacterStack lastObject];
	[xmlCharacters setString:[xmlCharacters stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];
	
	if ([elementName isEqualToString:@"game"])
	{
		NSString *romHeaderSerial = (NSString *)[self.header valueForKey:@"romSerial"];
		NSString *xmlSerial = (NSString *)[xmlCurrentRom valueForKey:@"serial"];
		if ([xmlSerial isEqualToString:romHeaderSerial])
		{
			[self handleAdvansceneDatabaseInfo];
			[parser abortParsing];
		}
		
		[xmlCurrentRom release];
		xmlCurrentRom = nil;
	}
	else
	{
		if (xmlCurrentRom != nil)
		{
			[xmlCurrentRom setValue:[NSString stringWithString:xmlCharacters] forKey:elementName];
		}
	}
	
	[xmlCharacterStack removeLastObject];
	[xmlElementStack removeLastObject];
}

- (void)parser:(NSXMLParser *)parser foundCharacters:(NSString *)string
{
	NSMutableString *xmlCharacters = (NSMutableString *)[xmlCharacterStack lastObject];
	if (xmlCharacters != nil)
	{
		[xmlCharacters appendString:string];
	}
}

- (void)parser:(NSXMLParser *)parser parseErrorOccurred:(NSError *)parseError
{
	NSInteger errorCode = [parseError code];
	if (errorCode == NSXMLParserDelegateAbortedParseError)
	{
		return;
	}
}

+ (void) changeRomSaveType:(NSInteger)saveTypeID
{
	CommonSettings.manualBackupType = (int)saveTypeID;
	if (saveTypeID != ROMSAVETYPE_AUTOMATIC)
	{
		backup_forceManualBackupType();
	}
}

+ (NSInteger) saveTypeByString:(NSString *)saveTypeString
{
	NSInteger saveTypeID = 0;
	
	if (saveTypeValues == nil)
	{
		return saveTypeID;
	}
	
	NSNumber *saveTypeNumber = (NSNumber *)[saveTypeValues valueForKey:[saveTypeString lowercaseString]];
	if (saveTypeNumber == nil)
	{
		return saveTypeID;
	}
	
	saveTypeID = [saveTypeNumber integerValue];
	
	return saveTypeID;
}

+ (NSMutableDictionary *) romNotLoadedBindings
{
	NSImage *iconImage = [NSImage imageNamed:@"NSApplicationIcon"];
	
	NSString *romNameAndSerialInfoString = @"Name: ";
	romNameAndSerialInfoString = [romNameAndSerialInfoString stringByAppendingString:NSSTRING_STATUS_NO_ROM_LOADED];
	romNameAndSerialInfoString = [[romNameAndSerialInfoString stringByAppendingString:@"\nSerial: "] stringByAppendingString:NSSTRING_STATUS_NO_ROM_LOADED];
	
	return [NSMutableDictionary dictionaryWithObjectsAndKeys:
			NSSTRING_STATUS_NO_ROM_LOADED, @"romInternalName",
			NSSTRING_STATUS_NO_ROM_LOADED, @"romSerial",
			romNameAndSerialInfoString, @"romNameAndSerialInfo",
			NSSTRING_STATUS_NO_ROM_LOADED, @"bannerJapanese",
			NSSTRING_STATUS_NO_ROM_LOADED, @"bannerEnglish",
			NSSTRING_STATUS_NO_ROM_LOADED, @"bannerFrench",
			NSSTRING_STATUS_NO_ROM_LOADED, @"bannerGerman",
			NSSTRING_STATUS_NO_ROM_LOADED, @"bannerItalian",
			NSSTRING_STATUS_NO_ROM_LOADED, @"bannerSpanish",
			NSSTRING_STATUS_NO_ROM_LOADED, @"gameTitle",
			NSSTRING_STATUS_NO_ROM_LOADED, @"gameCode",
			NSSTRING_STATUS_NO_ROM_LOADED, @"gameDeveloper",
			NSSTRING_STATUS_NO_ROM_LOADED, @"gameDeveloperWithCode",
			NSSTRING_STATUS_NO_ROM_LOADED, @"unitCode",
			NSSTRING_STATUS_NO_ROM_LOADED, @"makerCode",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0ull], @"romSize",
			@"----------", @"arm9BinaryOffset",
			@"----------", @"arm9BinaryEntryAddress",
			@"----------", @"arm9BinaryStartAddress",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0ull], @"arm9BinarySize",
			@"----------", @"arm7BinaryOffset",
			@"----------", @"arm7BinaryEntryAddress",
			@"----------", @"arm7BinaryStartAddress",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0ull], @"arm7BinarySize",
			@"----------", @"fntOffset",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0ull], @"fntTableSize",
			@"----------", @"fatOffset",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0ull], @"fatSize",
			@"----------", @"iconOffset",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0ull], @"usedRomSize",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0ull], @"unusedCapacity",
			iconImage, @"iconImage",
			nil];
}

+ (NSString *) byteSizeStringWithLargerUnit:(NSUInteger)byteSize
{
	const float kilobyteSize = byteSize / 1024.0f;
	const float megabyteSize = byteSize / 1024.0f / 1024.0f;
	const float gigabyteSize = byteSize / 1024.0f / 1024.0f / 1024.0f;
	
	NSString *byteString = [NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, (unsigned long long)byteSize];
	NSString *unitString = byteString;
	
	if (gigabyteSize > 1.0f)
	{
		unitString = [NSString stringWithFormat:@"%@ (%1.1f GB)", byteString, gigabyteSize];
	}
	else if (megabyteSize > 1.0f)
	{
		unitString = [NSString stringWithFormat:@"%@ (%1.1f MB)", byteString, megabyteSize];
	}
	else if (kilobyteSize > 1.0f)
	{
		unitString = [NSString stringWithFormat:@"%@ (%1.1f KB)", byteString, kilobyteSize];
	}
	
	return unitString;
}

@end

/********************************************************************************************
	@function RomIconToRGBA8888()

	@brief Reads the icon image data from a ROM and converts it to an RGBA8888 formatted bitmap.

	@param bitmapData Write pointer for the icon's pixel data.

	@return Nothing

	@discussion
		- If bitmapData is NULL, then this function immediately returns and does nothing.
		- If no ROM is loaded, then bitmapData will have a black square icon.
		- The caller is responsible for ensuring that bitmapData points to a valid
		  memory location and that the memory block is large enough to hold the pixel
		  data. The written data will be 32x32 pixels in 32-bit color. Therefore, a size
		  of at least 4096 bytes must be allocated.
 ********************************************************************************************/
void RomIconToRGBA8888(uint32_t *bitmapData)
{
	if (bitmapData == NULL)
	{
		return;
	}
	
	const RomBanner &ndsRomBanner = gameInfo.getRomBanner(); // Contains the memory addresses we need to get our read pointer locations.
	const uint32_t *iconPixPtr = (uint32_t *)ndsRomBanner.bitmap; // Read pointer for the icon's pixel data.
	
	// Setup the 4-bit CLUT.
	//
	// The actual color values are stored with the ROM icon data in RGB555 format.
	// We convert these color values and store them in the CLUT as RGBA8888 values.
	//
	// The first entry always represents the alpha, so just set it to 0.
	const uint16_t *clut4 = (uint16_t *)ndsRomBanner.palette;
	CACHE_ALIGN uint32_t clut32[16];
	ColorspaceConvertBuffer555xTo8888Opaque<false, true, BESwapNone>(clut4, clut32, 16);
	clut32[0] = 0x00000000;
	
	// Load the image from the icon pixel data.
	//
	// ROM icons are stored in 4-bit indexed color and have dimensions of 32x32 pixels.
	// Also, ROM icons are split into 16 separate 8x8 pixel tiles arranged in a 4x4
	// array. Here, we sequentially read from the ROM data, and adjust our write
	// location appropriately within the bitmap memory block.
	for (size_t y = 0; y < 4; y++)
	{
		for (size_t x = 0; x < 4; x++)
		{
			for (size_t p = 0; p < 8; p++, iconPixPtr++)
			{
				// Load an entire row of palette color indices as a single 32-bit chunk.
				const uint32_t palIdx = LE_TO_LOCAL_32(*iconPixPtr);
				
				// Set the write location. The formula below calculates the proper write
				// location depending on the position of the read pointer. We use a more
				// optimized version of this formula in practice.
				//
				// bitmapOutPtr = bitmapData + ( ((y * 8) + palIdx) * 32 ) + (x * 8);
				uint32_t *bitmapOutPtr = bitmapData + ( ((y << 3) + p) << 5 ) + (x << 3);
				*bitmapOutPtr = clut32[(palIdx & 0x0000000F) >> 0];
				
				bitmapOutPtr++;
				*bitmapOutPtr = clut32[(palIdx & 0x000000F0) >> 4];
				
				bitmapOutPtr++;
				*bitmapOutPtr = clut32[(palIdx & 0x00000F00) >> 8];
				
				bitmapOutPtr++;
				*bitmapOutPtr = clut32[(palIdx & 0x0000F000) >> 12];
				
				bitmapOutPtr++;
				*bitmapOutPtr = clut32[(palIdx & 0x000F0000) >> 16];
				
				bitmapOutPtr++;
				*bitmapOutPtr = clut32[(palIdx & 0x00F00000) >> 20];
				
				bitmapOutPtr++;
				*bitmapOutPtr = clut32[(palIdx & 0x0F000000) >> 24];
				
				bitmapOutPtr++;
				*bitmapOutPtr = clut32[(palIdx & 0xF0000000) >> 28];
			}
		}
	}
}
