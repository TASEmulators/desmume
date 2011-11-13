/*  Copyright 2009,2011 DeSmuME team

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

#include "types.h"
#include "task.h"
#include <stdio.h>

#ifdef _WINDOWS
#include <windows.h>
#else
#include <pthread.h>
#if !defined(__APPLE__)
#include <semaphore.h>
#endif
#endif

#ifdef _MSC_VER
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

#elif defined(__APPLE__)

class Task::Impl {
public:
	Impl();
	~Impl();

	void start(bool spinlock);
	void execute(const TWork &work, void *param);
	void* finish();
	void shutdown();

	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t condWork;

	TWork work;
	void *param;
	void *ret;
	bool exitThread;
};

void* taskProc(void *arg)
{
	Task::Impl *ctx = (Task::Impl *)arg;

	do {
		pthread_mutex_lock(&ctx->mutex);

		while (ctx->work == NULL && !ctx->exitThread) {
			pthread_cond_wait(&ctx->condWork, &ctx->mutex);
		}

		if (ctx->work != NULL) {
			ctx->ret = ctx->work(ctx->param);
		} else {
			ctx->ret = NULL;
		}

		ctx->work = NULL;
		pthread_cond_signal(&ctx->condWork);

		pthread_mutex_unlock(&ctx->mutex);

	} while(!ctx->exitThread);

	return NULL;
}

Task::Impl::Impl()
{
	thread = -1;
	work = NULL;
	param = NULL;
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

	if (this->thread != -1) {
		pthread_mutex_unlock(&this->mutex);
		return;
	}

	this->work = NULL;
	this->param = NULL;
	this->ret = NULL;
	this->exitThread = false;
	pthread_create(&this->thread, NULL, &taskProc, this);

	pthread_mutex_unlock(&this->mutex);
}

void Task::Impl::execute(const TWork &work, void *param)
{
	pthread_mutex_lock(&this->mutex);

	if (work == NULL || this->thread == -1) {
		pthread_mutex_unlock(&this->mutex);
		return;
	}

	this->work = work;
	this->param = param;
	pthread_cond_signal(&this->condWork);

	pthread_mutex_unlock(&this->mutex);
}

void* Task::Impl::finish()
{
	void *returnValue = NULL;

	pthread_mutex_lock(&this->mutex);

	if (this->thread == -1) {
		pthread_mutex_unlock(&this->mutex);
		return returnValue;
	}

	while (this->work != NULL) {
		pthread_cond_wait(&this->condWork, &this->mutex);
	}

	returnValue = this->ret;

	pthread_mutex_unlock(&this->mutex);

	return returnValue;
}

void Task::Impl::shutdown()
{
	pthread_mutex_lock(&this->mutex);

	if (this->thread == -1) {
		pthread_mutex_unlock(&this->mutex);
		return;
	}

	this->work = NULL;
	this->exitThread = true;
	pthread_cond_signal(&this->condWork);

	pthread_mutex_unlock(&this->mutex);

	pthread_join(this->thread, NULL);

	pthread_mutex_lock(&this->mutex);
	this->thread = -1;
	pthread_mutex_unlock(&this->mutex);
}

#else

class Task::Impl {
public:
	Impl();
	~Impl() {}

	void start(bool spinlock);
	void execute(const TWork &work, void* param);
	void* finish();
	void shutdown();

	pthread_t thread;
	sem_t in, out;
	TWork work;
	void *param, *ret;
	bool bStarted, bKill;
};

void *taskProc(void *arg)
{
	Task::Impl *ctx = (Task::Impl *)arg;
	while(1) {
		while(sem_wait(&ctx->in) == -1)
			;
		if(ctx->bKill)
			break;
		ctx->ret = ctx->work(ctx->param);
		sem_post(&ctx->out);
	}
	return NULL;
}

Task::Impl::Impl()
{
	work = NULL;
	param = NULL;
	ret = NULL;
	bKill = false;
	bStarted = false;
}

void Task::Impl::start(bool spinlock)
{
	sem_init(&in, 0, 0);
	sem_init(&out, 0, 0);
	pthread_create(&thread, NULL, &taskProc, this);
	bStarted = 1;
}

void Task::Impl::execute(const TWork &work, void* param)
{
	this->work = work;
	this->param = param;
	sem_post(&in);
}

void *Task::Impl::finish()
{
	while(sem_wait(&out) == -1)
		;
	return ret;
}

void Task::Impl::shutdown()
{
	if(!bStarted)
		return;
	bKill = 1;
	sem_post(&in);
	pthread_join(thread, NULL);
	sem_destroy(&in);
	sem_destroy(&out);
}
#endif

void Task::start(bool spinlock) { impl->start(spinlock); }
void Task::shutdown() { impl->shutdown(); }
Task::Task() : impl(new Task::Impl()) {}
Task::~Task() { delete impl; }
void Task::execute(const TWork &work, void* param) { impl->execute(work,param); }
void* Task::finish() { return impl->finish(); }


