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

#ifndef _COCOA_AUDIO_CONTROLLER_H_
#define _COCOA_AUDIO_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "ClientAudioOutput.h"

#ifdef BOOL
#undef BOOL
#endif


class MacCoreAudioOutput : public ClientAudioOutput
{
public:
	MacCoreAudioOutput() {};
	~MacCoreAudioOutput() {};
	
	// ClientEmulationOutput methods
	virtual void SetIdle(bool theState);
};

@interface CocoaAudioController : NSObjectController
{
	MacCoreAudioOutput *_audioOutput;

	NSNumber *_engineID;
	NSNumber *_volume;
	NSNumber *_spuInterpolationModeID;
	NSNumber *_spuSyncModeID;
	NSNumber *_spuSyncMethodID;
	
	NSString *_engineString;
	NSString *_spuInterpolationModeString;
	NSString *_spuSyncMethodString;
	
	BOOL _darkMode;
	
	NSImage *_speakerVolumeIcon;
	NSImage *_iconSpeakerVolumeFull;
	NSImage *_iconSpeakerVolumeTwoThird;
	NSImage *_iconSpeakerVolumeOneThird;
	NSImage *_iconSpeakerVolumeMute;
	NSImage *_iconSpeakerVolumeFullDM;
	NSImage *_iconSpeakerVolumeTwoThirdDM;
	NSImage *_iconSpeakerVolumeOneThirdDM;
	NSImage *_iconSpeakerVolumeMuteDM;
}

@property (assign, nonatomic, getter=darkMode, setter=setDarkMode:) BOOL _darkMode;
@property (readonly, nonatomic, getter=audioOutput) MacCoreAudioOutput *_audioOutput;
@property (assign, nonatomic, getter=engineID, setter=setEngineID:) NSNumber *_engineID;
@property (assign, nonatomic, getter=volume, setter=setVolume:) NSNumber *_volume;
@property (assign, getter=speakerVolumeIcon, setter=setSpeakerVolumeIcon:) NSImage *_speakerVolumeIcon;
@property (assign, nonatomic, getter=spuAdvancedLogic, setter=setSpuAdvancedLogic:) BOOL _spuAdvancedLogic;
@property (assign, nonatomic, getter=spuInterpolationModeID, setter=setSpuInterpolationModeID:) NSNumber *_spuInterpolationModeID;
@property (assign, nonatomic, getter=spuSyncModeID, setter=setSpuSyncModeID:) NSNumber *_spuSyncModeID;
@property (assign, nonatomic, getter=spuSyncMethodID, setter=setSpuSyncMethodID:) NSNumber *_spuSyncMethodID;
@property (assign, nonatomic, getter=isMuted, setter=setMute:) BOOL _isMuted;

@property (assign, nonatomic, getter=engineString, setter=setEngineString:) NSString *_engineString;
@property (assign, nonatomic, getter=spuInterpolationModeString, setter=setSpuInterpolationModeString:) NSString *_spuInterpolationModeString;
@property (assign, nonatomic, getter=spuSyncMethodString, setter=setSpuSyncMethodString:) NSString *_spuSyncMethodString;

- (void) setEngineIDValue:(int)newEngineID;
- (int) engineIDValue;

- (void) setVolumeValue:(float)vol;
- (float) volumeValue;
- (void) updateSpeakerVolumeIcon;

- (void) setSpuInterpolationModeIDValue:(SPUInterpolationMode)modeID;
- (SPUInterpolationMode) spuInterpolationModeIDValue;

- (void) setSpuSyncModeIDValue:(ESynchMode)modeID;
- (ESynchMode) spuSyncModeIDValue;

- (void) setSpuSyncMethodIDValue:(ESynchMethod)methodID;
- (ESynchMethod) spuSyncMethodIDValue;

@end

#endif // _COCOA_AUDIO_CONTROLLER_H_
