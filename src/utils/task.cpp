/*  Copyright 2009 DeSmuME team

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "task.h"

#ifdef _WIN32

#include <windows.h>
#include <stdio.h>

class Task::Impl {
public:
	Impl();
	~Impl();

	bool spinlock;

	void start(bool spinlock);
	void shutdown();

	//execute some work
	void execute(const TWork &work, void* param);

	//wait for the work to complete
	void* finish();

	static DWORD __stdcall s_taskProc(void *ptr);
	void taskProc();
	void init();

	//the work function that shall be executed
	TWork work;
	void* param;

	HANDLE incomingWork, workDone, hThread;
	volatile bool bIncomingWork, bWorkDone, bKill;
	bool bStarted;
};

static void* killTask(void* task)
{
	((Task::Impl*)task)->bKill = true;
	return 0;
}

Task::Impl::~Impl()
{
	shutdown();
}

Task::Impl::Impl()
	: work(NULL)
	, bIncomingWork(false)
	, bWorkDone(true)
	, bKill(false)
	, bStarted(false)
	, incomingWork(INVALID_HANDLE_VALUE)
	, workDone(INVALID_HANDLE_VALUE)
	, hThread(INVALID_HANDLE_VALUE)
{
}

DWORD __stdcall Task::Impl::s_taskProc(void *ptr)
{
	//just past the buck to the instance method
	((Task::Impl*)ptr)->taskProc();
	return 0;
}

void Task::Impl::taskProc()
{
	for(;;) {
		if(bKill) break;
		
		//wait for a chunk of work
		if(spinlock) while(!bIncomingWork) Sleep(0); 
		else WaitForSingleObject(incomingWork,INFINITE); 
		
		bIncomingWork = false; 
		//execute the work
		param = work(param);
		//signal completion
		if(!spinlock) SetEvent(workDone); 
		bWorkDone = true;
	}
}

void Task::Impl::start(bool spinlock)
{
	bIncomingWork = false;
	bWorkDone = true;
	bKill = false;
	bStarted = true;
	this->spinlock = spinlock;
	incomingWork = CreateEvent(NULL,FALSE,FALSE,NULL);
	workDone = CreateEvent(NULL,FALSE,FALSE,NULL);
	hThread = CreateThread(NULL,0,Task::Impl::s_taskProc,(void*)this, 0, NULL);
}
void Task::Impl::shutdown()
{
	if(!bStarted) return;
	bStarted = false;

	execute(killTask,this);
	finish();

	CloseHandle(incomingWork);
	CloseHandle(workDone);
	CloseHandle(hThread);

	incomingWork = INVALID_HANDLE_VALUE;
	workDone = INVALID_HANDLE_VALUE;
	hThread = INVALID_HANDLE_VALUE;
}

void Task::Impl::execute(const TWork &work, void* param) 
{
	//setup the work
	this->work = work;
	this->param = param;
	bWorkDone = false;
	//signal it to start
	if(!spinlock) SetEvent(incomingWork); 
	bIncomingWork = true;
}

void* Task::Impl::finish()
{
	//just wait for the work to be done
	if(spinlock) 
		while(!bWorkDone) 
			Sleep(0);
	else WaitForSingleObject(workDone,INFINITE); 
	return param;
}

#else

//just a stub impl that doesnt actually do any threading.
//somebody needs to update the pthread implementation below
class Task::Impl {
public:
	Impl() {}
	~Impl() {}

	void start(bool spinlock) {}
	void shutdown() {}

	void* ret;
	void execute(const TWork &work, void* param) { ret = work(param); }
	
	void* finish() { return ret; }
};


/*
#include <pthread.h>

class Task::Impl {
public:
	Impl();

	//execute some work
	void execute(const TWork &work, void* param);

	//wait for the work to complete
	void* finish();

	pthread_t thread;
	static void* s_taskProc(void *ptr);
	void taskProc();
	void start();
	void shutdown();

	//the work function that shall be executed
	TWork work;
	void* param;

	bool initialized;

	struct WaitEvent
	{
		WaitEvent() 
			: condition(PTHREAD_COND_INITIALIZER)
			, mutex(PTHREAD_MUTEX_INITIALIZER)
			, value(false)
		{}
		pthread_mutex_t mutex;
		pthread_cond_t condition;
		bool value;

		//waits for the WaitEvent to be set
		void waitAndClear()
		{ 
			lock();
			if(!value)
				pthread_cond_wait( &condition, &mutex );
			value = false;
			unlock();
		}

		//sets the WaitEvent
		void signal()
		{
			lock();
			if(!value) {
				value = true;
				pthread_cond_signal( &condition );
			}
			unlock();
		}

		//locks the condition's mutex
		void lock() { pthread_mutex_lock(&mutex); }
		
		//unlocks the condition's mutex
		void unlock() { pthread_mutex_unlock( &mutex ); }

	} incomingWork, workDone;

};

Task::Impl::Impl()
	: work(NULL)
	, initialized(false)
{
}

void* Task::Impl::s_taskProc(void *ptr)
{
	//just past the buck to the instance method
	((Task::Impl*)ptr)->taskProc();
	return 0;
}

void Task::Impl::taskProc()
{
	for(;;) {
		//wait for a chunk of work
		incomingWork.waitAndClear();
		//execute the work
		param = work(param);
		//signal completion
		workDone.signal();
	}
}

void Task::Impl::start()
{
	pthread_create( &thread, NULL, Task::Impl::s_taskProc, (void*)this );     
	initialized = true;
}
void Task::Impl::shutdown()
{
	if(!initialized)
		return;
	// pthread_join or something, NYI, this code is all disabled anyway at the time of this writing
	initialized = false;
}

void Task::Impl::execute(const TWork &work, void* param) 
{
	//initialization is deferred to the first execute to give win32 time to startup
	if(!initialized) init();
	//setup the work
	this->work = work;
	this->param = param;
	//signal it to start
	incomingWork.signal();
}

void* Task::Impl::finish()
{
	//just wait for the work to be done
	workDone.waitAndClear();
	return param;
}
*/

#endif

void Task::start(bool spinlock) { impl->start(spinlock); }
void Task::shutdown() { impl->shutdown(); }
Task::Task() : impl(new Task::Impl()) {}
Task::~Task() { delete impl; }
void Task::execute(const TWork &work, void* param) { impl->execute(work,param); }
void* Task::finish() { return impl->finish(); }


