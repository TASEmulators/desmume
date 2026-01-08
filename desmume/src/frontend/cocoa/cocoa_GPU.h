/*
	Copyright (C) 2013-2026 DeSmuME team

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

#include <pthread.h>
#include <mach/task.h>
#include <mach/semaphore.h>
#include <mach/sync_policy.h>

// This symbol only exists in the kernel headers, but not in the user headers.
// Manually define the symbol here, since we will be Mach semaphores in the user-space.
#ifndef SYNC_POLICY_PREPOST
	#define SYNC_POLICY_PREPOST 0x4
#endif

#import <CoreVideo/CoreVideo.h>

#import <Foundation/Foundation.h>
#include <map>
#include <vector>
#include "utilities.h"

#include "ClientEmulationOutput.h"
#include "ClientVideoOutput.h"

#import "cocoa_util.h"

#define VIDEO_FLUSH_TIME_LIMIT_OFFSET	8	// The amount of time, in seconds, to wait for a flush to occur on a given CVDisplayLink before stopping it.

enum ClientDisplayBufferState
{
	ClientDisplayBufferState_Idle			= 0,	// The buffer has already been read and is currently idle. It is a candidate for a read or write operation.
	ClientDisplayBufferState_Writing		= 1,	// The buffer is currently being written. It cannot be accessed.
	ClientDisplayBufferState_Ready			= 2,	// The buffer was just written to, but has not been read yet. It is a candidate for a read or write operation.
	ClientDisplayBufferState_PendingRead	= 3,	// The buffer has been marked that it will be read. It must not be accessed.
	ClientDisplayBufferState_Reading		= 4		// The buffer is currently being read. It cannot be accessed.
};

class MacGPUFetchObjectAsync : public GPUClientFetchObject
{
protected:
	task_t _taskEmulationLoop;
	
	apple_unfairlock_t _unfairlockFramebufferStates[MAX_FRAMEBUFFER_PAGES];
	semaphore_t _semFramebuffer[MAX_FRAMEBUFFER_PAGES];
	volatile ClientDisplayBufferState _framebufferState[MAX_FRAMEBUFFER_PAGES];
	
	bool _pauseState;
	uint32_t _threadMessageID;
	uint8_t _fetchIndex;
	pthread_t _threadFetch;
	pthread_cond_t _condSignalFetch;
	pthread_mutex_t _mutexFetchExecute;
	
public:
	MacGPUFetchObjectAsync();
	~MacGPUFetchObjectAsync();
	
	virtual void Init();
	virtual void SetPauseState(bool theState);
	bool GetPauseState();
	
	void SemaphoreFramebufferCreate();
	void SemaphoreFramebufferDestroy();
	uint8_t SelectBufferIndex(const uint8_t currentIndex, size_t pageCount);
	semaphore_t SemaphoreFramebufferPageAtIndex(const u8 bufferIndex);
	ClientDisplayBufferState FramebufferStateAtIndex(uint8_t index);
	void SetFramebufferState(ClientDisplayBufferState bufferState, uint8_t index);
	
	void FetchSynchronousAtIndex(uint8_t index);
	void SignalFetchAtIndex(uint8_t index, int32_t messageID);
	void RunFetchLoop();
	virtual void DoPostFetchActions();
};

typedef std::map<CGDirectDisplayID, CVDisplayLinkRef> DisplayLinksActiveMap;
typedef std::map<CGDirectDisplayID, int64_t> DisplayLinkFlushTimeLimitMap;

class MacGPUFetchObjectDisplayLink : public MacGPUFetchObjectAsync, public ClientGPUFetchObjectMultiDisplayView
{
protected:
	pthread_mutex_t _mutexDisplayLinkLists;
	DisplayLinksActiveMap _displayLinksActiveList;
	DisplayLinksActiveMap _displayLinksStartedList;
	DisplayLinkFlushTimeLimitMap _displayLinkFlushTimeList;
	
public:
	MacGPUFetchObjectDisplayLink();
	~MacGPUFetchObjectDisplayLink();
	
	void DisplayLinkStartUsingID(CGDirectDisplayID displayID);
	void DisplayLinkListUpdate();
	
	virtual void FlushAllViewsOnDisplayLink(CVDisplayLinkRef displayLinkRef, const CVTimeStamp *timeStampNow, const CVTimeStamp *timeStampOutput);
	
	// MacGPUFetchObjectAsync methods
	virtual void SetPauseState(bool theState);
	virtual void DoPostFetchActions();
};
