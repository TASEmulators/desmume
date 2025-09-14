/*
	Copyright (C) 2025 DeSmuME team

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

#import "CocoaAudioController.h"
#import "cocoa_util.h"

#include "MacCoreAudioOutputEngine.h"
#include "cocoa_globals.h"


@implementation CocoaAudioController

@dynamic _darkMode;
@synthesize _audioOutput;
@dynamic _engineID;
@dynamic _volume;
@synthesize _speakerVolumeIcon;
@dynamic _spuAdvancedLogic;
@dynamic _spuInterpolationModeID;
@dynamic _spuSyncModeID;
@dynamic _spuSyncMethodID;
@dynamic _isMuted;
@dynamic _engineString;
@dynamic _spuInterpolationModeString;
@dynamic _spuSyncMethodString;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	_darkMode = [CocoaDSUtil determineDarkModeAppearance];
	
	_iconSpeakerVolumeFull       = [[NSImage imageNamed:@"Icon_VolumeFull_16x16"] retain];
	_iconSpeakerVolumeTwoThird   = [[NSImage imageNamed:@"Icon_VolumeTwoThird_16x16"] retain];
	_iconSpeakerVolumeOneThird   = [[NSImage imageNamed:@"Icon_VolumeOneThird_16x16"] retain];
	_iconSpeakerVolumeMute       = [[NSImage imageNamed:@"Icon_VolumeMute_16x16"] retain];
	
	_iconSpeakerVolumeFullDM     = [[NSImage imageNamed:@"Icon_VolumeFull_DarkMode_16x16"] retain];
	_iconSpeakerVolumeTwoThirdDM = [[NSImage imageNamed:@"Icon_VolumeTwoThird_DarkMode_16x16"] retain];
	_iconSpeakerVolumeOneThirdDM = [[NSImage imageNamed:@"Icon_VolumeOneThird_DarkMode_16x16"] retain];
	_iconSpeakerVolumeMuteDM     = [[NSImage imageNamed:@"Icon_VolumeMute_DarkMode_16x16"] retain];
	
	_speakerVolumeIcon = (_darkMode) ? _iconSpeakerVolumeFullDM : _iconSpeakerVolumeFull;
	
	_audioEngineMacCoreAudio = NULL;
	_audioOutput = new ClientAudioOutput;
	
	_engineID = [[NSNumber numberWithInt:_audioOutput->GetEngineByID()] retain];
	_volume = [[NSNumber numberWithFloat:_audioOutput->GetVolume()] retain];
	_spuInterpolationModeID = [[NSNumber numberWithInt:_audioOutput->GetSPUInterpolationModeByID()] retain];
	_spuSyncModeID = [[NSNumber numberWithInt:_audioOutput->GetSPUSyncModeByID()] retain];
	_spuSyncMethodID = [[NSNumber numberWithInt:_audioOutput->GetSPUSyncMethodByID()] retain];
	
	_engineString = [[NSString stringWithCString:_audioOutput->GetEngineString() encoding:NSUTF8StringEncoding] retain];
	_spuInterpolationModeString = [[NSString stringWithCString:_audioOutput->GetSPUInterpolationModeString() encoding:NSUTF8StringEncoding] retain];
	_spuSyncMethodString = [[NSString stringWithCString:_audioOutput->GetSPUSyncMethodString() encoding:NSUTF8StringEncoding] retain];
	
	return self;
}

- (void)dealloc
{
	delete _audioOutput;
	_audioOutput = NULL;
	
	delete _audioEngineMacCoreAudio;
	_audioEngineMacCoreAudio = NULL;
	
	[_engineID release];
	[_volume release];
	[_spuInterpolationModeID release];
	[_spuSyncModeID release];
	[_spuSyncMethodID release];
	
	[_engineString release];
	[_spuInterpolationModeString release];
	[_spuSyncMethodString release];
	
	[_iconSpeakerVolumeFull release];
	[_iconSpeakerVolumeTwoThird release];
	[_iconSpeakerVolumeOneThird release];
	[_iconSpeakerVolumeMute release];
	
	[_iconSpeakerVolumeFullDM release];
	[_iconSpeakerVolumeTwoThirdDM release];
	[_iconSpeakerVolumeOneThirdDM release];
	[_iconSpeakerVolumeMuteDM release];
	
	[super dealloc];
}

- (void) setDarkMode:(BOOL)theState
{
	if (_darkMode == theState)
	{
		return;
	}
	
	_darkMode = theState;
	[self updateSpeakerVolumeIcon];
}

- (BOOL) darkMode
{
	return _darkMode;
}

- (void) setEngineID:(NSNumber *)idNumber
{
	int newEngineID = [idNumber intValue];
	ClientAudioOutputEngine *newEngineObject = NULL;
	
	if (newEngineID == _audioOutput->GetEngineByID())
	{
		return;
	}
	
	switch (newEngineID)
	{
		case SNDCORE_MAC_COREAUDIO:
		{
			if (_audioEngineMacCoreAudio == NULL)
			{
				_audioEngineMacCoreAudio = new MacCoreAudioOutputEngine;
				newEngineObject = _audioEngineMacCoreAudio;
			}
			break;
		}
			
		default:
			break;
	}
	
	if (newEngineID != SNDCORE_MAC_COREAUDIO)
	{
		delete _audioEngineMacCoreAudio;
		_audioEngineMacCoreAudio = NULL;
	}
	
	_audioOutput->SetEngine(newEngineObject);
	[self setEngineString:_engineString];
}

- (NSNumber *) engineID
{
	int currEngineID = _audioOutput->GetEngineByID();
	if (currEngineID != [_engineID intValue])
	{
		[_engineID release];
		_engineID = [[NSNumber numberWithInt:currEngineID] retain];
	}
	
	return _engineID;
}

- (void) setEngineIDValue:(int)newEngineID
{
	[self setEngineID:[NSNumber numberWithInt:newEngineID]];
}

- (int) engineIDValue
{
	return [[self engineID] intValue];
}

- (void) setVolume:(NSNumber *)volNumber
{
	float vol = [volNumber floatValue];
	_audioOutput->SetVolume(vol);
	vol = _audioOutput->GetVolume();
	
	if (vol > 0.0f)
	{
		[self setMute:NO];
	}
	
	[self updateSpeakerVolumeIcon];
}

- (NSNumber *) volume
{
	float currVolume = _audioOutput->GetVolume();
	if (currVolume != [_volume floatValue])
	{
		[_volume release];
		_volume = [[NSNumber numberWithFloat:currVolume] retain];
	}
	
	return _volume;
}

- (void) setVolumeValue:(float)vol
{
	[self setVolume:[NSNumber numberWithFloat:vol]];
}

- (float) volumeValue
{
	return [[self volume] floatValue];
}

- (void) updateSpeakerVolumeIcon
{
	const BOOL currentDarkModeState = _darkMode;
	const float currentVolume = _audioOutput->GetVolume();
	NSImage *currIcon = _speakerVolumeIcon;
	NSImage *newIcon = nil;
	
	if ( _audioOutput->IsMuted() || (currentVolume <= 0.0f) )
	{
		newIcon = (currentDarkModeState) ? _iconSpeakerVolumeMuteDM : _iconSpeakerVolumeMute;
	}
	else if ( (currentVolume > 0.0f) && (currentVolume <= VOLUME_THRESHOLD_LOW) )
	{
		newIcon = (currentDarkModeState) ? _iconSpeakerVolumeOneThirdDM : _iconSpeakerVolumeOneThird;
	}
	else if ( (currentVolume > VOLUME_THRESHOLD_LOW) && (currentVolume <= VOLUME_THRESHOLD_HIGH) )
	{
		newIcon = (currentDarkModeState) ? _iconSpeakerVolumeTwoThirdDM : _iconSpeakerVolumeTwoThird;
	}
	else
	{
		newIcon = (currentDarkModeState) ? _iconSpeakerVolumeFullDM : _iconSpeakerVolumeFull;
	}
	
	if (newIcon != currIcon)
	{
		[self setSpeakerVolumeIcon:newIcon];
	}
}

- (void) setSpuAdvancedLogic:(BOOL)theState
{
	_audioOutput->SetSPUAdvancedLogic((theState) ? true : false);
}

- (BOOL) spuAdvancedLogic
{
	return _audioOutput->GetSPUAdvancedLogic() ? YES : NO;
}

- (void) setSpuInterpolationModeID:(NSNumber *)idNumber
{
	SPUInterpolationMode newInterpModeID = (SPUInterpolationMode)[idNumber intValue];
	_audioOutput->SetSPUInterpolationModeByID(newInterpModeID);
	[self setSpuInterpolationModeString:_spuInterpolationModeString];
}

- (NSNumber *) spuInterpolationModeID
{
	SPUInterpolationMode currInterpModeID = _audioOutput->GetSPUInterpolationModeByID();
	if (currInterpModeID != [_spuInterpolationModeID intValue])
	{
		[_spuInterpolationModeID release];
		_spuInterpolationModeID = [[NSNumber numberWithInt:currInterpModeID] retain];
	}
	
	return _spuInterpolationModeID;
}

- (void) setSpuInterpolationModeIDValue:(SPUInterpolationMode)modeID
{
	[self setSpuInterpolationModeID:[NSNumber numberWithInt:modeID]];
}

- (SPUInterpolationMode) spuInterpolationModeIDValue
{
	return (SPUInterpolationMode)[[self spuInterpolationModeID] intValue];
}

- (void) setSpuSyncModeID:(NSNumber *)idNumber
{
	ESynchMode newSyncModeID = (ESynchMode)[idNumber intValue];
	_audioOutput->SetSPUSyncModeByID(newSyncModeID);
	[self setSpuSyncMethodString:_spuSyncMethodString];
}

- (NSNumber *) spuSyncModeID
{
	ESynchMode currSyncModeID = _audioOutput->GetSPUSyncModeByID();
	if (currSyncModeID != [_spuSyncModeID intValue])
	{
		[_spuSyncModeID release];
		_spuSyncModeID = [[NSNumber numberWithInt:currSyncModeID] retain];
	}
	
	return _spuSyncModeID;
}

- (void) setSpuSyncModeIDValue:(ESynchMode)modeID
{
	[self setSpuSyncModeID:[NSNumber numberWithInt:modeID]];
}

- (ESynchMode) spuSyncModeIDValue
{
	return (ESynchMode)[[self spuSyncModeID] intValue];
}

- (void) setSpuSyncMethodID:(NSNumber *)idNumber
{
	ESynchMethod newSyncMethodID = (ESynchMethod)[idNumber intValue];
	_audioOutput->SetSPUSyncMethodByID(newSyncMethodID);
	[self setSpuSyncMethodString:_spuSyncMethodString];
}

- (NSNumber *) spuSyncMethodID
{
	ESynchMethod currSyncMethodID = _audioOutput->GetSPUSyncMethodByID();
	if (currSyncMethodID != [_spuSyncMethodID intValue])
	{
		[_spuSyncMethodID release];
		_spuSyncMethodID = [[NSNumber numberWithInt:currSyncMethodID] retain];
	}
	
	return _spuSyncMethodID;
}

- (void) setSpuSyncMethodIDValue:(ESynchMethod)methodID
{
	[self setSpuSyncMethodID:[NSNumber numberWithInt:methodID]];
}

- (ESynchMethod) spuSyncMethodIDValue
{
	return (ESynchMethod)[[self spuSyncMethodID] intValue];
}

- (void) setMute:(BOOL)theState
{
	_audioOutput->SetMute((theState) ? true : false);
	[self updateSpeakerVolumeIcon];
}

- (BOOL) isMuted
{
	return _audioOutput->IsMuted() ? YES : NO;
}

- (void) setEngineString:(NSString *)theString
{
	// Do nothing. This exists for KVO-compliance only.
}

- (NSString *) engineString
{
	NSString *currString = [NSString stringWithCString:_audioOutput->GetEngineString() encoding:NSUTF8StringEncoding];
	if (![_engineString isEqualToString:currString])
	{
		[_engineString release];
		_engineString = [currString retain];
	}
	
	return _engineString;
}

- (void) setSpuInterpolationModeString:(NSString *)theString
{
	// Do nothing. This exists for KVO-compliance only.
}

- (NSString *) spuInterpolationModeString
{
	NSString *currString = [NSString stringWithCString:_audioOutput->GetSPUInterpolationModeString() encoding:NSUTF8StringEncoding];
	if (![_spuInterpolationModeString isEqualToString:currString])
	{
		[_spuInterpolationModeString release];
		_spuInterpolationModeString = [currString retain];
	}
	
	return _spuInterpolationModeString;
}

- (void) setSpuSyncMethodString:(NSString *)theString
{
	// Do nothing. This exists for KVO-compliance only.
}

- (NSString *) spuSyncMethodString
{
	NSString *currString = [NSString stringWithCString:_audioOutput->GetSPUSyncMethodString() encoding:NSUTF8StringEncoding];
	if (![_spuSyncMethodString isEqualToString:currString])
	{
		[_spuSyncMethodString release];
		_spuSyncMethodString = [currString retain];
	}
	
	return _spuSyncMethodString;
}

@end
