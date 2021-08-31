/*
	Copyright (C) 2009-2021 DeSmuME team

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

#include <stdio.h>
#include <string.h>

#include "types.h"
#include "task.h"
#include <rthreads/rthreads.h>

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>
#endif

class Task::Impl {
private:
	sthread_t* _thread;
	bool _isThreadRunning;
	
public:
	Impl();
	~Impl();

	void start(bool spinlock, int threadPriority, const char *name);
	void execute(const TWork &work, void *param);
	void* finish();
	void shutdown();

	bool needSetThreadName;
	char threadName[16]; // pthread_setname_np() assumes a max character length of 16.
	
	slock_t *mutex;
	scond_t *condWork;
	TWork workFunc;
	void *workFuncParam;
	void *ret;
	bool exitThread;
};

static void taskProc(void *arg)
{
	Task::Impl *ctx = (Task::Impl *)arg;
	
	// The pthread_setname_np() function is gimped on macOS because it can't set thread
	// names from any thread other than the current thread. That's why we can only set the
	// thread name right here. We are not modifying rthreads, which is meant to be
	// cross-platform, due to the substantially different nature of macOS's gimped version
	// of pthread_setname_np().
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
	if (ctx->needSetThreadName)
	{
		pthread_setname_np(ctx->threadName);
		ctx->needSetThreadName = false;
	}
#endif

	do {
		slock_lock(ctx->mutex);

		while (ctx->workFunc == NULL && !ctx->exitThread) {
			scond_wait(ctx->condWork, ctx->mutex);
		}

		if (ctx->workFunc != NULL) {
			ctx->ret = ctx->workFunc(ctx->workFuncParam);
		} else {
			ctx->ret = NULL;
		}

		ctx->workFunc = NULL;
		scond_signal(ctx->condWork);

		slock_unlock(ctx->mutex);

	} while(!ctx->exitThread);
}

Task::Impl::Impl()
{
	_isThreadRunning = false;
	workFunc = NULL;
	workFuncParam = NULL;
	ret = NULL;
	exitThread = false;
	
	memset(threadName, 0, sizeof(threadName));
	needSetThreadName = false;

	mutex = slock_new();
	condWork = scond_new();
}

Task::Impl::~Impl()
{
	shutdown();
	slock_free(mutex);
	scond_free(condWork);
}

void Task::Impl::start(bool spinlock, int threadPriority, const char *name)
{
	slock_lock(this->mutex);
	
	if (this->_isThreadRunning) {
		slock_unlock(this->mutex);
		return;
	}
	
	this->workFunc = NULL;
	this->workFuncParam = NULL;
	this->ret = NULL;
	this->exitThread = false;
	this->_thread = sthread_create_with_priority(&taskProc, this, threadPriority);
	this->_isThreadRunning = true;
	
#if !defined(USE_WIN32_THREADS) && !defined(__APPLE__)
	sthread_setname(this->_thread, name);
#else
	
#if defined(DESMUME_COCOA) && defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
	// Setting the thread name on macOS uses pthread_setname_np(), but this requires macOS v10.6 or later.
	this->needSetThreadName = (kCFCoreFoundationVersionNumber >= kCFCoreFoundationVersionNumber10_6) && (name != NULL);
#else
	this->needSetThreadName = (name != NULL);
#endif
	
	if (this->needSetThreadName)
	{
		strncpy(this->threadName, name, sizeof(this->threadName));
	}
#endif
	
	slock_unlock(this->mutex);
}

void Task::Impl::execute(const TWork &work, void *param)
{
	slock_lock(this->mutex);

	if ((work == NULL) || (this->workFunc != NULL) || !this->_isThreadRunning)
	{
		slock_unlock(this->mutex);
		return;
	}

	this->workFunc = work;
	this->workFuncParam = param;
	scond_signal(this->condWork);

	slock_unlock(this->mutex);
}

void* Task::Impl::finish()
{
	void *returnValue = NULL;

	slock_lock(this->mutex);

	if ((this->workFunc == NULL) || !this->_isThreadRunning) {
		slock_unlock(this->mutex);
		return returnValue;
	}

	while (this->workFunc != NULL)
	{
		scond_wait(this->condWork, this->mutex);
	}

	returnValue = this->ret;

	slock_unlock(this->mutex);

	return returnValue;
}

void Task::Impl::shutdown()
{
	slock_lock(this->mutex);

	if (!this->_isThreadRunning) {
		slock_unlock(this->mutex);
		return;
	}

	this->workFunc = NULL;
	this->exitThread = true;
	scond_signal(this->condWork);

	slock_unlock(this->mutex);

	sthread_join(this->_thread);

	slock_lock(this->mutex);
	this->_isThreadRunning = false;
	slock_unlock(this->mutex);
}

void Task::start(bool spinlock) { impl->start(spinlock, 0, NULL); }
void Task::start(bool spinlock, int threadPriority, const char *name) { impl->start(spinlock, threadPriority, name); }
void Task::shutdown() { impl->shutdown(); }
Task::Task() : impl(new Task::Impl()) {}
Task::~Task() { delete impl; }
void Task::execute(const TWork &work, void* param) { impl->execute(work,param); }
void* Task::finish() { return impl->finish(); }


