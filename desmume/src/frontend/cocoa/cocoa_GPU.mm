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

#import "cocoa_GPU.h"

#import "cocoa_globals.h"
#include "utilities.h"

#include <set>


static void* RunFetchThread(void *arg)
{
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
	if (kCFCoreFoundationVersionNumber >= kCFCoreFoundationVersionNumber10_6)
	{
		pthread_setname_np("Video Fetch");
	}
#endif
	
	MacGPUFetchObjectAsync *asyncFetchObj = (MacGPUFetchObjectAsync *)arg;
	asyncFetchObj->RunFetchLoop();
	
	return NULL;
}

MacGPUFetchObjectAsync::MacGPUFetchObjectAsync()
{
	_threadFetch = NULL;
	_pauseState = true;
	_threadMessageID = MESSAGE_NONE;
	_fetchIndex = 0;
	pthread_cond_init(&_condSignalFetch, NULL);
	pthread_mutex_init(&_mutexFetchExecute, NULL);
	
	_id = GPUClientFetchObjectID_GenericAsync;
	
	memset(_name, 0, sizeof(_name));
	strncpy(_name, "Generic Asynchronous Video", sizeof(_name) - 1);
	
	memset(_description, 0, sizeof(_description));
	strncpy(_description, "No description.", sizeof(_description) - 1);
	
	_taskEmulationLoop = 0;
	
	for (size_t i = 0; i < MAX_FRAMEBUFFER_PAGES; i++)
	{
		_semFramebuffer[i] = 0;
		_framebufferState[i] = ClientDisplayBufferState_Idle;
		_unfairlockFramebufferStates[i] = apple_unfairlock_create();
	}
}

MacGPUFetchObjectAsync::~MacGPUFetchObjectAsync()
{
	pthread_cancel(this->_threadFetch);
	pthread_join(this->_threadFetch, NULL);
	this->_threadFetch = NULL;
	
	pthread_cond_destroy(&this->_condSignalFetch);
	pthread_mutex_destroy(&this->_mutexFetchExecute);
}

void MacGPUFetchObjectAsync::Init()
{
	if (CommonSettings.num_cores > 1)
	{
		pthread_attr_t threadAttr;
		pthread_attr_init(&threadAttr);
		pthread_attr_setschedpolicy(&threadAttr, SCHED_RR);
		
		struct sched_param sp;
		memset(&sp, 0, sizeof(struct sched_param));
		sp.sched_priority = 44;
		pthread_attr_setschedparam(&threadAttr, &sp);
		
		pthread_create(&_threadFetch, &threadAttr, &RunFetchThread, this);
		pthread_attr_destroy(&threadAttr);
	}
	else
	{
		pthread_create(&_threadFetch, NULL, &RunFetchThread, this);
	}
}

void MacGPUFetchObjectAsync::SetPauseState(bool theState)
{
	this->_pauseState = theState;
}

bool MacGPUFetchObjectAsync::GetPauseState()
{
	return this->_pauseState;
}

void MacGPUFetchObjectAsync::SemaphoreFramebufferCreate()
{
	this->_taskEmulationLoop = mach_task_self();
	
	for (size_t i = 0; i < MAX_FRAMEBUFFER_PAGES; i++)
	{
		semaphore_create(this->_taskEmulationLoop, &this->_semFramebuffer[i], SYNC_POLICY_FIFO, 1);
	}
}

void MacGPUFetchObjectAsync::SemaphoreFramebufferDestroy()
{
	for (size_t i = MAX_FRAMEBUFFER_PAGES - 1; i < MAX_FRAMEBUFFER_PAGES; i--)
	{
		if (this->_semFramebuffer[i] != 0)
		{
			semaphore_destroy(this->_taskEmulationLoop, this->_semFramebuffer[i]);
			this->_semFramebuffer[i] = 0;
		}
	}
}

uint8_t MacGPUFetchObjectAsync::SelectBufferIndex(const uint8_t currentIndex, size_t pageCount)
{
	uint8_t selectedIndex = currentIndex;
	bool stillSearching = true;
	
	// First, search for an idle buffer along with its corresponding semaphore.
	if (stillSearching)
	{
		selectedIndex = (selectedIndex + 1) % pageCount;
		for (; selectedIndex != currentIndex; selectedIndex = (selectedIndex + 1) % pageCount)
		{
			if (this->FramebufferStateAtIndex(selectedIndex) == ClientDisplayBufferState_Idle)
			{
				stillSearching = false;
				break;
			}
		}
	}
	
	// Next, search for either an idle or a ready buffer along with its corresponding semaphore.
	if (stillSearching)
	{
		selectedIndex = (selectedIndex + 1) % pageCount;
		for (size_t spin = 0; spin < 100ULL * pageCount; selectedIndex = (selectedIndex + 1) % pageCount, spin++)
		{
			if ( (this->FramebufferStateAtIndex(selectedIndex) == ClientDisplayBufferState_Idle) ||
				((this->FramebufferStateAtIndex(selectedIndex) == ClientDisplayBufferState_Ready) && (selectedIndex != currentIndex)) )
			{
				stillSearching = false;
				break;
			}
		}
	}
	
	// Since the most available buffers couldn't be taken, we're going to spin for some finite
	// period of time until an idle buffer emerges. If that happens, then force wait on the
	// buffer's corresponding semaphore.
	if (stillSearching)
	{
		selectedIndex = (selectedIndex + 1) % pageCount;
		for (size_t spin = 0; spin < 10000ULL * pageCount; selectedIndex = (selectedIndex + 1) % pageCount, spin++)
		{
			if (this->FramebufferStateAtIndex(selectedIndex) == ClientDisplayBufferState_Idle)
			{
				stillSearching = false;
				break;
			}
		}
	}
	
	// In an effort to find something that is likely to be available shortly in the future,
	// search for any idle, ready or reading buffer, and then force wait on its corresponding
	// semaphore.
	if (stillSearching)
	{
		selectedIndex = (selectedIndex + 1) % pageCount;
		for (; selectedIndex != currentIndex; selectedIndex = (selectedIndex + 1) % pageCount)
		{
			if ( (this->FramebufferStateAtIndex(selectedIndex) == ClientDisplayBufferState_Idle) ||
				 (this->FramebufferStateAtIndex(selectedIndex) == ClientDisplayBufferState_Ready) ||
				 (this->FramebufferStateAtIndex(selectedIndex) == ClientDisplayBufferState_Reading) )
			{
				stillSearching = false;
				break;
			}
		}
	}
	
	return selectedIndex;
}

semaphore_t MacGPUFetchObjectAsync::SemaphoreFramebufferPageAtIndex(const u8 bufferIndex)
{
	assert(bufferIndex < MAX_FRAMEBUFFER_PAGES);
	return this->_semFramebuffer[bufferIndex];
}

ClientDisplayBufferState MacGPUFetchObjectAsync::FramebufferStateAtIndex(uint8_t index)
{
	apple_unfairlock_lock(this->_unfairlockFramebufferStates[index]);
	const ClientDisplayBufferState bufferState = this->_framebufferState[index];
	apple_unfairlock_unlock(this->_unfairlockFramebufferStates[index]);
	
	return bufferState;
}

void MacGPUFetchObjectAsync::SetFramebufferState(ClientDisplayBufferState bufferState, uint8_t index)
{
	apple_unfairlock_lock(this->_unfairlockFramebufferStates[index]);
	this->_framebufferState[index] = bufferState;
	apple_unfairlock_unlock(this->_unfairlockFramebufferStates[index]);
}

void MacGPUFetchObjectAsync::FetchSynchronousAtIndex(uint8_t index)
{
	this->FetchFromBufferIndex(index);
}

void MacGPUFetchObjectAsync::SignalFetchAtIndex(uint8_t index, int32_t messageID)
{
	pthread_mutex_lock(&this->_mutexFetchExecute);
	
	this->_fetchIndex = index;
	this->_threadMessageID = messageID;
	pthread_cond_signal(&this->_condSignalFetch);
	
	pthread_mutex_unlock(&this->_mutexFetchExecute);
}

void MacGPUFetchObjectAsync::RunFetchLoop()
{
	NSAutoreleasePool *tempPool = nil;
	pthread_mutex_lock(&this->_mutexFetchExecute);
	
	do
	{
		tempPool = [[NSAutoreleasePool alloc] init];
		
		while (this->_threadMessageID == MESSAGE_NONE)
		{
			pthread_cond_wait(&this->_condSignalFetch, &this->_mutexFetchExecute);
		}
		
		const uint32_t lastMessageID = this->_threadMessageID;
		this->FetchFromBufferIndex(this->_fetchIndex);
		
		if (lastMessageID == MESSAGE_FETCH_AND_PERFORM_ACTIONS)
		{
			this->DoPostFetchActions();
		}
		
		this->_threadMessageID = MESSAGE_NONE;
		
		[tempPool release];
	} while(true);
}

void MacGPUFetchObjectAsync::DoPostFetchActions()
{
	// Do nothing.
}

#pragma mark -

static void ScreenChangeCallback(CFNotificationCenterRef center,
                                 void *observer,
                                 CFStringRef name,
                                 const void *object,
                                 CFDictionaryRef userInfo)
{
	((MacGPUFetchObjectDisplayLink *)observer)->DisplayLinkListUpdate();
}

static CVReturn MacDisplayLinkCallback(CVDisplayLinkRef displayLinkRef,
                                       const CVTimeStamp *inNow,
                                       const CVTimeStamp *inOutputTime,
                                       CVOptionFlags flagsIn,
                                       CVOptionFlags *flagsOut,
                                       void *displayLinkContext)
{
	MacGPUFetchObjectDisplayLink *fetchObj = (MacGPUFetchObjectDisplayLink *)displayLinkContext;
	
	NSAutoreleasePool *tempPool = [[NSAutoreleasePool alloc] init];
	fetchObj->FlushAllViewsOnDisplayLink(displayLinkRef, inNow, inOutputTime);
	[tempPool release];
	
	return kCVReturnSuccess;
}

MacGPUFetchObjectDisplayLink::MacGPUFetchObjectDisplayLink()
{
	_id = GPUClientFetchObjectID_MacDisplayLink;
	
	memset(_name, 0, sizeof(_name));
	strncpy(_name, "macOS Display Link GPU Fetch", sizeof(_name) - 1);
	
	memset(_description, 0, sizeof(_description));
	strncpy(_description, "No description.", sizeof(_description) - 1);
	
	pthread_mutex_init(&_mutexDisplayLinkLists, NULL);
	
	_displayLinksActiveList.clear();
	_displayLinksStartedList.clear();
	_displayLinkFlushTimeList.clear();
	DisplayLinkListUpdate();
	
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(),
	                                this,
	                                ScreenChangeCallback,
	                                CFSTR("NSApplicationDidChangeScreenParametersNotification"),
	                                NULL,
	                                CFNotificationSuspensionBehaviorDeliverImmediately);
}

MacGPUFetchObjectDisplayLink::~MacGPUFetchObjectDisplayLink()
{
	CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(),
	                                   this,
	                                   CFSTR("NSApplicationDidChangeScreenParametersNotification"),
	                                   NULL);
	
	pthread_mutex_lock(&this->_mutexDisplayLinkLists);
	
	while (this->_displayLinksActiveList.size() > 0)
	{
		DisplayLinksActiveMap::iterator it = this->_displayLinksActiveList.begin();
		CGDirectDisplayID displayID = it->first;
		CVDisplayLinkRef displayLinkRef = it->second;
		
		if (CVDisplayLinkIsRunning(displayLinkRef))
		{
			CVDisplayLinkStop(displayLinkRef);
			this->_displayLinksStartedList.erase(displayID);
		}
		
		CVDisplayLinkRelease(displayLinkRef);
		
		this->_displayLinksActiveList.erase(displayID);
		this->_displayLinkFlushTimeList.erase(displayID);
	}
	
	pthread_mutex_unlock(&this->_mutexDisplayLinkLists);
	pthread_mutex_destroy(&this->_mutexDisplayLinkLists);
}

void MacGPUFetchObjectDisplayLink::DisplayLinkStartUsingID(CGDirectDisplayID displayID)
{
	CVDisplayLinkRef displayLinkRef = NULL;
	
	pthread_mutex_lock(&this->_mutexDisplayLinkLists);
	
	if (this->_displayLinksActiveList.find(displayID) != this->_displayLinksActiveList.end())
	{
		displayLinkRef = this->_displayLinksActiveList[displayID];
	}
	
	if ( (displayLinkRef != NULL) && !CVDisplayLinkIsRunning(displayLinkRef) )
	{
		CVDisplayLinkStart(displayLinkRef);
		this->_displayLinksStartedList[displayID] = displayLinkRef;
	}
	
	pthread_mutex_unlock(&this->_mutexDisplayLinkLists);
}

void MacGPUFetchObjectDisplayLink::DisplayLinkListUpdate()
{
	// Set up the display links
	NSArray *screenList = [NSScreen screens];
	std::set<CGDirectDisplayID> screenActiveDisplayIDsList;
	
	pthread_mutex_lock(&this->_mutexDisplayLinkLists);
	
	// Add new CGDirectDisplayIDs for new screens
	for (size_t i = 0; i < [screenList count]; i++)
	{
		NSScreen *screen = [screenList objectAtIndex:i];
		NSDictionary *deviceDescription = [screen deviceDescription];
		NSNumber *idNumber = (NSNumber *)[deviceDescription valueForKey:@"NSScreenNumber"];
		
		CGDirectDisplayID displayID = [idNumber unsignedIntValue];
		bool isDisplayLinkStillActive = (this->_displayLinksActiveList.find(displayID) != this->_displayLinksActiveList.end());
		
		if (!isDisplayLinkStillActive)
		{
			CVDisplayLinkRef newDisplayLink;
			CVDisplayLinkCreateWithCGDisplay(displayID, &newDisplayLink);
			CVDisplayLinkSetOutputCallback(newDisplayLink, &MacDisplayLinkCallback, this);
			
			this->_displayLinksActiveList[displayID] = newDisplayLink;
			this->_displayLinkFlushTimeList[displayID] = 0;
		}
		
		// While we're iterating through NSScreens, save the CGDirectDisplayID to a temporary list for later use.
		screenActiveDisplayIDsList.insert(displayID);
	}
	
	// Remove old CGDirectDisplayIDs for screens that no longer exist
	for (DisplayLinksActiveMap::iterator it = this->_displayLinksActiveList.begin(); it != this->_displayLinksActiveList.end(); )
	{
		CGDirectDisplayID displayID = it->first;
		CVDisplayLinkRef displayLinkRef = it->second;
		
		if (screenActiveDisplayIDsList.find(displayID) == screenActiveDisplayIDsList.end())
		{
			if (CVDisplayLinkIsRunning(displayLinkRef))
			{
				CVDisplayLinkStop(displayLinkRef);
				this->_displayLinksStartedList.erase(displayID);
			}
			
			CVDisplayLinkRelease(displayLinkRef);
			
			this->_displayLinksActiveList.erase(displayID);
			this->_displayLinkFlushTimeList.erase(displayID);
			
			if (this->_displayLinksActiveList.empty())
			{
				break;
			}
			else
			{
				it = this->_displayLinksActiveList.begin();
				continue;
			}
		}
		
		++it;
	}
	
	pthread_mutex_unlock(&this->_mutexDisplayLinkLists);
}

void MacGPUFetchObjectDisplayLink::FlushAllViewsOnDisplayLink(CVDisplayLinkRef displayLinkRef, const CVTimeStamp *timeStampNow, const CVTimeStamp *timeStampOutput)
{
	CGDirectDisplayID displayID = CVDisplayLinkGetCurrentCGDisplay(displayLinkRef);
	bool didFlushOccur = false;
	
	std::vector<ClientDisplayViewInterface *> cdvFlushList;
	this->_outputManager->GenerateFlushListForDisplay((int32_t)displayID, cdvFlushList);
	
	const size_t listSize = cdvFlushList.size();
	if (listSize > 0)
	{
		this->FlushMultipleViews(cdvFlushList, (uint64_t)timeStampNow->videoTime, timeStampOutput->hostTime);
		didFlushOccur = true;
	}
	
	if (didFlushOccur)
	{
		// Set the new time limit to 8 seconds after the current time.
		this->_displayLinkFlushTimeList[displayID] = timeStampNow->videoTime + (timeStampNow->videoTimeScale * VIDEO_FLUSH_TIME_LIMIT_OFFSET);
	}
	else if (timeStampNow->videoTime > this->_displayLinkFlushTimeList[displayID])
	{
		CVDisplayLinkStop(displayLinkRef);
		this->_displayLinksStartedList.erase(displayID);
	}
}

void MacGPUFetchObjectDisplayLink::SetPauseState(bool theState)
{
	if (theState == this->_pauseState)
	{
		return;
	}
	
	this->_pauseState = theState;
	
	if (theState)
	{
		for (DisplayLinksActiveMap::iterator it = this->_displayLinksStartedList.begin(); it != this->_displayLinksStartedList.end(); it++)
		{
			CVDisplayLinkRef displayLinkRef = it->second;
			if ( (displayLinkRef != NULL) && CVDisplayLinkIsRunning(displayLinkRef) )
			{
				CVDisplayLinkStop(displayLinkRef);
			}
		}
		
		this->FinalizeAllViews();
	}
	else
	{
		for (DisplayLinksActiveMap::iterator it = this->_displayLinksStartedList.begin(); it != this->_displayLinksStartedList.end(); it++)
		{
			CVDisplayLinkRef displayLinkRef = it->second;
			if ( (displayLinkRef != NULL) && !CVDisplayLinkIsRunning(displayLinkRef) )
			{
				CVDisplayLinkStart(displayLinkRef);
			}
		}
	}
}

void MacGPUFetchObjectDisplayLink::DoPostFetchActions()
{
	this->PushVideoDataToAllDisplayViews();
}
