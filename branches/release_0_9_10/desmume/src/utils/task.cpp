/*
	Copyright (C) 2009-2013 DeSmuME team

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

#include "types.h"
#include "task.h"
#include <stdio.h>

#ifdef HOST_WINDOWS
#include <windows.h>
#else
#include <pthread.h>
#if defined HOST_LINUX || defined HOST_DARWIN
#include <unistd.h>
#elif defined HOST_BSD
#include <sys/sysctl.h>
#endif
#endif // HOST_WINDOWS

// http://stackoverflow.com/questions/150355/programmatically-find-the-number-of-cores-on-a-machine
int getOnlineCores (void)
{
#ifdef HOST_WINDOWS
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
#elif defined HOST_LINUX || defined HOST_DARWIN
	return sysconf(_SC_NPROCESSORS_ONLN);
#elif defined HOST_BSD
	int cores;
	const int mib[4] = { CTL_HW, HW_NCPU, 0, 0 };
	const size_t len = sizeof(cores);
	sysctl(mib, 2, &cores, &len, NULL, 0);
	return (cores < 1) ? 1 : cores;
#else
	return 1;
#endif
}

#ifdef HOST_WINDOWS
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
	TWork workFunc;
	void* workFuncParam;

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
	: workFunc(NULL)
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
		workFuncParam = workFunc(workFuncParam);
		//signal completion
		bWorkDone = true;
		if(!spinlock) SetEvent(workDone);
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
	this->workFunc = work;
	this->workFuncParam = param;
	bWorkDone = false;
	//signal it to start
	if(!spinlock) SetEvent(incomingWork); 
	bIncomingWork = true;
}

void* Task::Impl::finish()
{
	//just wait for the work to be done
	if(spinlock)
	{
		while(!bWorkDone)
			Sleep(0);
	}
	else
	{
		while(!bWorkDone)
			WaitForSingleObject(workDone, INFINITE);
	}
	
	return workFuncParam;
}

#else

class Task::Impl {
private:
	pthread_t _thread;
	bool _isThreadRunning;
	
public:
	Impl();
	~Impl();

	void start(bool spinlock);
	void execute(const TWork &work, void *param);
	void* finish();
	void shutdown();

	pthread_mutex_t mutex;
	pthread_cond_t condWork;
	TWork workFunc;
	void *workFuncParam;
	void *ret;
	bool exitThread;
};

static void* taskProc(void *arg)
{
	Task::Impl *ctx = (Task::Impl *)arg;

	do {
		pthread_mutex_lock(&ctx->mutex);

		while (ctx->workFunc == NULL && !ctx->exitThread) {
			pthread_cond_wait(&ctx->condWork, &ctx->mutex);
		}

		if (ctx->workFunc != NULL) {
			ctx->ret = ctx->workFunc(ctx->workFuncParam);
		} else {
			ctx->ret = NULL;
		}

		ctx->workFunc = NULL;
		pthread_cond_signal(&ctx->condWork);

		pthread_mutex_unlock(&ctx->mutex);

	} while(!ctx->exitThread);

	return NULL;
}

Task::Impl::Impl()
{
	_isThreadRunning = false;
	workFunc = NULL;
	workFuncParam = NULL;
	ret = NULL;
	exitThread = false;

	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&condWork, NULL);
}

Task::Impl::~Impl()
{
	shutdown();
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&condWork);
}

void Task::Impl::start(bool spinlock)
{
	pthread_mutex_lock(&this->mutex);

	if (this->_isThreadRunning) {
		pthread_mutex_unlock(&this->mutex);
		return;
	}

	this->workFunc = NULL;
	this->workFuncParam = NULL;
	this->ret = NULL;
	this->exitThread = false;
	pthread_create(&this->_thread, NULL, &taskProc, this);
	this->_isThreadRunning = true;

	pthread_mutex_unlock(&this->mutex);
}

void Task::Impl::execute(const TWork &work, void *param)
{
	pthread_mutex_lock(&this->mutex);

	if (work == NULL || !this->_isThreadRunning) {
		pthread_mutex_unlock(&this->mutex);
		return;
	}

	this->workFunc = work;
	this->workFuncParam = param;
	pthread_cond_signal(&this->condWork);

	pthread_mutex_unlock(&this->mutex);
}

void* Task::Impl::finish()
{
	void *returnValue = NULL;

	pthread_mutex_lock(&this->mutex);

	if (!this->_isThreadRunning) {
		pthread_mutex_unlock(&this->mutex);
		return returnValue;
	}

	while (this->workFunc != NULL) {
		pthread_cond_wait(&this->condWork, &this->mutex);
	}

	returnValue = this->ret;

	pthread_mutex_unlock(&this->mutex);

	return returnValue;
}

void Task::Impl::shutdown()
{
	pthread_mutex_lock(&this->mutex);

	if (!this->_isThreadRunning) {
		pthread_mutex_unlock(&this->mutex);
		return;
	}

	this->workFunc = NULL;
	this->exitThread = true;
	pthread_cond_signal(&this->condWork);

	pthread_mutex_unlock(&this->mutex);

	pthread_join(this->_thread, NULL);

	pthread_mutex_lock(&this->mutex);
	this->_isThreadRunning = false;
	pthread_mutex_unlock(&this->mutex);
}
#endif

void Task::start(bool spinlock) { impl->start(spinlock); }
void Task::shutdown() { impl->shutdown(); }
Task::Task() : impl(new Task::Impl()) {}
Task::~Task() { delete impl; }
void Task::execute(const TWork &work, void* param) { impl->execute(work,param); }
void* Task::finish() { return impl->finish(); }


