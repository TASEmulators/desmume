/*
	Copyright (C) 2012-2026 DeSmuME team

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

#ifndef __OE_SOUND_INTERFACE_H__
#define __OE_SOUND_INTERFACE_H__

#include "../ClientAudioOutput.h"

#ifdef BOOL
#undef BOOL
#endif

#define SNDCORE_OPENEMU 58326

@class OERingBuffer;

class OEAudioOutputEngine : public ClientAudioOutputEngine
{
private:
	void __InstanceInit(OERingBuffer *oeBuffer, bool useOldAPI);
	
protected:
	OERingBuffer *_buffer;
	bool _willUseOldAPI;
	
public:
	OEAudioOutputEngine();
	OEAudioOutputEngine(OERingBuffer *oeBuffer, bool useOldAPI);
	
	// ClientAudioOutputEngine methods
	virtual size_t GetAvailableSamples() const;
	virtual void WriteToBuffer(const void *inSamples, size_t numberSampleFrames);
};

#endif // __OE_SOUND_INTERFACE_H__
