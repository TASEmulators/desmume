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

#ifndef _CLIENT_EMULATION_OUTPUT_H_
#define _CLIENT_EMULATION_OUTPUT_H_

#include <pthread.h>
#include <string>
#include <vector>

enum PlatformTypeID
{
	PlatformTypeID_Generic   = 0x00000001,
	
	PlatformTypeID_Mac       = 0x00000002,
	
	PlatformTypeID_Any       = 0xFFFFFFFF
};

enum OutputTypeID
{
	OutputTypeID_Generic     = 0x00000001,
	
	OutputTypeID_Video       = 0x00000010, // A single set of fully rendered video framebuffers
	OutputTypeID_Audio       = 0x00000020, // A single fully processed audio frame
	OutputTypeID_BGLayer     = 0x00000040, // A single fully rendered BG layer
	OutputTypeID_Sprite      = 0x00000080, // A single fully rendered sprite
	
	OutputTypeID_TCM         = 0x00000100, // ARM9 ITCM / DTCM
	OutputTypeID_RAM         = 0x00000200, // A RAM block in main memory
	OutputTypeID_WRAM        = 0x00000400, // ARM9 / ARM7 shared WRAM
	OutputTypeID_WRAM7       = 0x00000800, // ARM7-only WRAM
	
	OutputTypeID_Palette     = 0x00001000, // Palette RAM
	OutputTypeID_VRAM        = 0x00002000, // A video RAM block
	OutputTypeID_OAM         = 0x00004000, // 2D sprite memory
	OutputTypeID_Firmware    = 0x00008000, // Firmware
	
	OutputTypeID_Register    = 0x00010000, // I/O register (can be single register or a block)
	
	OutputTypeID_HostDisplay = 0x00100000, // Video intended for presentation on a host display
	OutputTypeID_HostSpeaker = 0x00200000, // Audio intended to be played through the host speakers
	
	OutputTypeID_Any         = 0xFFFFFFFF
};

class ClientEmulationOutput
{
protected:
	void *_clientObject;
	bool _isIdle;
	
	int32_t _platformId;
	int32_t _typeId;
	
	pthread_rwlock_t *_rwlockProducer;
	
	virtual void _HandleDispatch(int32_t messageID);
	
public:
	ClientEmulationOutput();
	~ClientEmulationOutput();
	
	void* GetClientObject();
	void SetClientObject(void *theObject);
	
	virtual void SetIdle(bool theState);
	virtual bool IsIdle();
	
	void SetPlatformId(int32_t platformId);
	int32_t GetPlatformId();
	
	void SetTypeId(int32_t typeId);
	int32_t GetTypeId();
	
	pthread_rwlock_t* GetProducerRWLock();
	void SetProducerRWLock(pthread_rwlock_t *rwlockProducer);
	
	virtual void Dispatch(int32_t messageID);
	virtual void HandleEmuFrameProcessed();
};

class ClientEmulationThreadedOutput : public ClientEmulationOutput
{
protected:
	std::string _threadName;
	bool _isPThreadNameSet;
	int32_t _threadMessageID;
	pthread_mutex_t _mutexMessageLoop;
	pthread_cond_t _condSignalMessage;
	pthread_t _pthread;
	
public:
	ClientEmulationThreadedOutput();
	~ClientEmulationThreadedOutput();
	
	const char* GetThreadName() const;
	void SetThreadName(const char *theString);
	void SetFlagPThreadName(bool isFlagSet);
	bool IsPThreadNameFlagSet() const;
	
	void CreateThread();
	void ExitThread();
	
	virtual void RunLoop();
	
	virtual void Dispatch(int32_t messageID);
};

class ClientEmulationOutputManager
{
protected:
	std::vector<ClientEmulationOutput *> _runList;
	std::vector<ClientEmulationOutput *> _pendingRegisterList;
	std::vector<ClientEmulationOutput *> _pendingUnregisterList;
	
	pthread_mutex_t _pendingRegisterListMutex;
	pthread_mutex_t _pendingUnregisterListMutex;
	
	void _RegisterPending();
	void _UnregisterPending();
	
public:
	ClientEmulationOutputManager();
	~ClientEmulationOutputManager();
	
	void SetIdleStatePlatformAndType(int32_t platformId, int32_t typeId, bool idleState);
	void SetIdleStateAll(bool idleState);
	
	void Register(ClientEmulationOutput *theOutput, pthread_rwlock_t *rwlockProducer);
	void Unregister(ClientEmulationOutput *theOutput);
	void UnregisterAtIndex(size_t index);
	void UnregisterAll();
	
	void DispatchAllOfPlatformAndType(int32_t platformId, int32_t typeId, int32_t messageID);
	void DispatchAll(int32_t messageID);
};

#endif //_CLIENT_EMULATION_OUTPUT_H_
