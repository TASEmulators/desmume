/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2017 DeSmuME team

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


static void RomIconToRGBA8888(uint32_t *bitmapData);

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
						  @1, @"eeprom - 4 kbit",
						  @2, @"eeprom - 64 kbit",
						  @3, @"eeprom - 512 kbit",
						  @5, @"flash - 2 mbit",
						  @6, @"flash - 4 mbit",
						  @7, @"flash - 8 mbit",
						  @10, @"flash - 64 mbit",
						  @0, @"none",
						  @0, @"tbc",
						  nil];
	}
	
	header = [[NSMutableDictionary alloc] initWithCapacity:32];
	if (header == nil)
	{
		return nil;
	}
	
	bindings = [CocoaDSRom romNotLoadedBindings];
	if (bindings == nil)
	{
		return nil;
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
		[self.bindings setValue:[CocoaDSRom byteSizeStringWithLargerUnit:(128*1024) << [[self.header objectForKey:@"romSize"] integerValue]] forKey:@"romSize"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"arm9BinaryOffset"] intValue]] forKey:@"arm9BinaryOffset"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"arm9BinaryEntryAddress"] intValue]] forKey:@"arm9BinaryEntryAddress"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"arm9BinaryStartAddress"] intValue]] forKey:@"arm9BinaryStartAddress"];
		[self.bindings setValue:[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, (long)[[self.header objectForKey:@"arm9BinarySize"] integerValue]] forKey:@"arm9BinarySize"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"arm7BinaryOffset"] intValue]] forKey:@"arm7BinaryOffset"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"arm7BinaryEntryAddress"] intValue]] forKey:@"arm7BinaryEntryAddress"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"arm7BinaryStartAddress"] intValue]] forKey:@"arm7BinaryStartAddress"];
		[self.bindings setValue:[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, (long)[[self.header objectForKey:@"arm7BinarySize"] integerValue]] forKey:@"arm7BinarySize"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"fntOffset"] intValue]] forKey:@"fntOffset"];
		[self.bindings setValue:[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, (long)[[self.header objectForKey:@"fntTableSize"] integerValue]] forKey:@"fntTableSize"];
		[self.bindings setValue:[NSString stringWithFormat:@"0x%08X", [[self.header objectForKey:@"fatOffset"] intValue]] forKey:@"fatOffset"];
		[self.bindings setValue:[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, (long)[[self.header objectForKey:@"fatSize"] integerValue]] forKey:@"fatSize"];
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
		return result;
	}
	
	fileURL = [theURL copy];
	
	NSURL *advscDBPath = [[NSUserDefaults standardUserDefaults] URLForKey:@"Advanscene_DatabasePath"];
	if (advscDBPath != nil)
	{
		NSXMLParser *advscDB = [[NSXMLParser alloc] initWithContentsOfURL:advscDBPath];
		[advscDB setDelegate:self];
		[advscDB parse];
	}
	
	NSDictionary *userInfo = [[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithBool:YES], @"DidLoad", self.fileURL, @"URL", nil];
	[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadName:@"org.desmume.DeSmuME.loadRomDidFinish" object:self userInfo:userInfo];
	
	return result;
}

- (void) loadDataOnThread:(id)object
{
	__strong CocoaDSRom *strongSelf = self;
	
	NSURL *theURL = [(NSURL *)object copy];
	@autoreleasepool {
		[strongSelf loadData:theURL];
	}
	strongSelf = nil;
}

- (NSString *) title
{
	NDS_header *ndsRomHeader = NDS_getROMHeader();
	if (ndsRomHeader == nil)
	{
		return nil;
	}
	
	return [[NSString alloc] initWithBytes:ndsRomHeader->gameTile length:ROMINFO_GAME_TITLE_LENGTH encoding:NSUTF8StringEncoding];
}

- (NSString *) code
{
	NDS_header *ndsRomHeader = NDS_getROMHeader();
	if (ndsRomHeader == nil)
	{
		return nil;
	}
	
	return [[NSString alloc] initWithBytes:ndsRomHeader->gameCode length:ROMINFO_GAME_CODE_LENGTH encoding:NSUTF8StringEncoding];
}

- (NSString *) banner:(const UInt16 *)UTF16TextBuffer
{
	NSUInteger bannerLength = ROMINFO_GAME_BANNER_LENGTH * sizeof(*UTF16TextBuffer);
	
	return [[NSString alloc] initWithBytes:UTF16TextBuffer length:bannerLength encoding:NSUTF16LittleEndianStringEncoding];
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
	
	[newImage addRepresentation:imageRep];
	
	return newImage;
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
	NSImage *iconImage = [NSImage imageNamed:NSImageNameApplicationIcon];
	
	NSString *noRom = NSSTRING_STATUS_NO_ROM_LOADED;
	NSMutableString *romNameAndSerialInfoString = [NSMutableString stringWithString:@"Name: "];
	[romNameAndSerialInfoString appendString:noRom];
	[romNameAndSerialInfoString appendString:@"\nSerial: "];
	[romNameAndSerialInfoString appendString:noRom];
	
	return [NSMutableDictionary dictionaryWithObjectsAndKeys:
			noRom, @"romInternalName",
			noRom, @"romSerial",
			[romNameAndSerialInfoString copy], @"romNameAndSerialInfo",
			noRom, @"bannerJapanese",
			noRom, @"bannerEnglish",
			noRom, @"bannerFrench",
			noRom, @"bannerGerman",
			noRom, @"bannerItalian",
			noRom, @"bannerSpanish",
			noRom, @"gameTitle",
			noRom, @"gameCode",
			noRom, @"gameDeveloper",
			noRom, @"gameDeveloperWithCode",
			noRom, @"unitCode",
			noRom, @"makerCode",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0l], @"romSize",
			@"----------", @"arm9BinaryOffset",
			@"----------", @"arm9BinaryEntryAddress",
			@"----------", @"arm9BinaryStartAddress",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0l], @"arm9BinarySize",
			@"----------", @"arm7BinaryOffset",
			@"----------", @"arm7BinaryEntryAddress",
			@"----------", @"arm7BinaryStartAddress",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0l], @"arm7BinarySize",
			@"----------", @"fntOffset",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0l], @"fntTableSize",
			@"----------", @"fatOffset",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0l], @"fatSize",
			@"----------", @"iconOffset",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0l], @"usedRomSize",
			[NSString stringWithFormat:NSSTRING_STATUS_SIZE_BYTES, 0l], @"unusedCapacity",
			iconImage, @"iconImage",
			nil];
}

+ (NSString *) byteSizeStringWithLargerUnit:(NSUInteger)byteSize
{
	static NSByteCountFormatter * formatter;
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		formatter = [[NSByteCountFormatter alloc] init];
		formatter.includesActualByteCount = YES;
		formatter.formattingContext = NSFormattingContextListItem;
	});
	return [formatter stringFromByteCount:byteSize];
}

@end

/**
	@function RomIconToRGBA8888()

	@brief Reads the icon image data from a ROM and converts it to an RGBA8888 formatted bitmap.

	@param bitmapData Write pointer for the icon's pixel data.

	@Discussion
		- If bitmapData is NULL, then this function immediately returns and does nothing.
 
		- If no ROM is loaded, then bitmapData will have a black square icon.
 
		- The caller is responsible for ensuring that bitmapData points to a valid
		  memory location and that the memory block is large enough to hold the pixel
		  data. The written data will be 32x32 pixels in 32-bit color. Therefore, a size
		  of at least 4096 bytes must be allocated.
 **/
void RomIconToRGBA8888(uint32_t *bitmapData)
{
	const RomBanner &ndsRomBanner = gameInfo.getRomBanner(); // Contains the memory addresses we need to get our read pointer locations.
	const uint16_t *iconClutPtr;	// Read pointer for the icon's CLUT.
	const uint32_t *iconPixPtr;		// Read pointer for the icon's pixel data.
	
	uint32_t clut[16];				// 4-bit indexed CLUT, storing RGBA8888 values for each color.
	
	uint32_t pixRowColors;			// Temp location for storing an 8 pixel row of 4-bit indexed color values from the icon's pixel data.
	unsigned int pixRowIndex;		// Temp location for tracking which pixel row of an 8x8 square that we are reading.
	unsigned int x;					// Temp location for tracking which of the 8x8 pixel squares that we are reading (x-dimension).
	unsigned int y;					// Temp location for tracking which of the 8x8 pixel squares that we are reading (y-dimension).
	
	uint32_t *bitmapPixPtr;			// Write pointer for the RGBA8888 bitmap pixel data, relative to the passed in *bitmapData pointer.
	
	if (bitmapData == NULL)
	{
		return;
	}
	
	// Set all of our icon read pointers.
	iconClutPtr = (uint16_t *)ndsRomBanner.palette + 1;
	iconPixPtr = (uint32_t *)ndsRomBanner.bitmap;
	
	// Setup the 4-bit CLUT.
	//
	// The actual color values are stored with the ROM icon data in RGB555 format.
	// We convert these color values and store them in the CLUT as RGBA8888 values.
	//
	// The first entry always represents the alpha, so we can just ignore it.
	clut[0] = 0x00000000;
	ColorspaceConvertBuffer555To8888Opaque<false, true>((u16 *)iconClutPtr, &clut[1], 15);
	
	// Load the image from the icon pixel data.
	//
	// ROM icons are stored in 4-bit indexed color and have dimensions of 32x32 pixels.
	// Also, ROM icons are split into 16 separate 8x8 pixel squares arranged in a 4x4
	// array. Here, we sequentially read from the ROM data, and adjust our write
	// location appropriately within the bitmap memory block.
	for(y = 0; y < 4; y++)
	{
		for(x = 0; x < 4; x++)
		{
			for(pixRowIndex = 0; pixRowIndex < 8; pixRowIndex++, iconPixPtr++)
			{
				// Load the entire row of pixels as a single 32-bit chunk.
				pixRowColors = *iconPixPtr;
				
				// Set the write location. The formula below calculates the proper write
				// location depending on the position of the read pointer. We use a more
				// optimized version of this formula in practice.
				//
				// bitmapPixPtr = bitmapData + ( ((y * 8) + pixRowIndex) * 32 ) + (x * 8);
				bitmapPixPtr = bitmapData + ( ((y << 3) + pixRowIndex) << 5 ) + (x << 3);
				
				// Set the RGBA8888 bitmap pixels using our CLUT from earlier.
				
#ifdef MSB_FIRST
				*bitmapPixPtr = LOCAL_TO_LE_32(clut[(pixRowColors & 0x0F000000) >> 24]);
				
				bitmapPixPtr++;
				*bitmapPixPtr = LOCAL_TO_LE_32(clut[(pixRowColors & 0xF0000000) >> 28]);
				
				bitmapPixPtr++;
				*bitmapPixPtr = LOCAL_TO_LE_32(clut[(pixRowColors & 0x000F0000) >> 16]);
				
				bitmapPixPtr++;
				*bitmapPixPtr = LOCAL_TO_LE_32(clut[(pixRowColors & 0x00F00000) >> 20]);
				
				bitmapPixPtr++;
				*bitmapPixPtr = LOCAL_TO_LE_32(clut[(pixRowColors & 0x00000F00) >> 8]);
				
				bitmapPixPtr++;
				*bitmapPixPtr = LOCAL_TO_LE_32(clut[(pixRowColors & 0x0000F000) >> 12]);
				
				bitmapPixPtr++;
				*bitmapPixPtr = LOCAL_TO_LE_32(clut[(pixRowColors & 0x0000000F)]);
				
				bitmapPixPtr++;
				*bitmapPixPtr = LOCAL_TO_LE_32(clut[(pixRowColors & 0x000000F0) >> 4]);
				
#else
				
				*bitmapPixPtr = clut[(pixRowColors & 0x0000000F)];
				
				bitmapPixPtr++;
				*bitmapPixPtr = clut[(pixRowColors & 0x000000F0) >> 4];
				
				bitmapPixPtr++;
				*bitmapPixPtr = clut[(pixRowColors & 0x00000F00) >> 8];
				
				bitmapPixPtr++;
				*bitmapPixPtr = clut[(pixRowColors & 0x0000F000) >> 12];
				
				bitmapPixPtr++;
				*bitmapPixPtr = clut[(pixRowColors & 0x000F0000) >> 16];
				
				bitmapPixPtr++;
				*bitmapPixPtr = clut[(pixRowColors & 0x00F00000) >> 20];
				
				bitmapPixPtr++;
				*bitmapPixPtr = clut[(pixRowColors & 0x0F000000) >> 24];
				
				bitmapPixPtr++;
				*bitmapPixPtr = clut[(pixRowColors & 0xF0000000) >> 28];
#endif
			}
		}
	}
}
