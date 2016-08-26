/*
	Copyright (C) 2009-2016 DeSmuME team

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

#include "types.h"
#include "task.h"
#include <rthreads/rthreads.h>

#ifdef HOST_WINDOWS
	#include <windows.h>
#else
	#if defined HOST_LINUX
		#include <unistd.h>
	#elif defined HOST_BSD || defined HOST_DARWIN
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
#elif defined HOST_LINUX
	return sysconf(_SC_NPROCESSORS_ONLN);
#elif defined HOST_BSD || defined HOST_DARWIN
	int cores;
	int mib[4] = { CTL_HW, HW_NCPU, 0, 0 };
	size_t len = sizeof(cores); //don't make this const, i guess sysctl can't take a const *
	sysctl(mib, 2, &cores, &len, NULL, 0);
	return (cores < 1) ? 1 : cores;
#else
	return 1;
#endif
}

class Task::Impl {
private:
	sthread_t* _thread;
	bool _isThreadRunning;
	
public:
	Impl();
	~Impl();

	void start(bool spinlock);
	void execute(const TWork &work, void *param);
	void* finish();
	void shutdown();

	slock_t *mutex;
	scond_t *condWork;
	TWork workFunc;
	void *workFuncParam;
	void *ret;
	bool exitThread;
	bool isTaskWaiting;
};

static void taskProc(void *arg)
{
	Task::Impl *ctx = (Task::Impl *)arg;

	do {
		slock_lock(ctx->mutex);

		while (ctx->workFunc == NULL && !ctx->exitThread) {
			ctx->isTaskWaiting = true;
			scond_wait(ctx->condWork, ctx->mutex);
			ctx->isTaskWaiting = false;
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
	isTaskWaiting = false;
	workFunc = NULL;
	workFuncParam = NULL;
	ret = NULL;
	exitThread = false;

	mutex = slock_new();
	condWork = scond_new();
}

Task::Impl::~Impl()
{
	shutdown();
	slock_free(mutex);
	scond_free(condWork);
}

void Task::Impl::start(bool spinlock)
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
	this->isTaskWaiting = false;
	this->_thread = sthread_create(&taskProc,this);
	this->_isThreadRunning = true;

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

	// As a last resort, we need to ensure that taskProc() actually executed, and if
	// it didn't, do something about it right now.
	//
	// Normally, calling execute() will wake up taskProc(), but on certain systems,
	// the signal from execute() might get missed by taskProc(). If this signal is
	// missed, then this method's scond_wait() will hang, since taskProc() will never
	// clear workFunc and signal back when its finished (taskProc() was never woken
	// up in the first place).
	//
	// This situation is only possible on systems where scond_wait() does not have
	// immediate lock/unlock mechanics with the wait state, such as on Windows.
	// Signals can get lost in scond_wait() since a thread's wait state might start
	// at a much later time from releasing the mutex, causing the signalling thread
	// to send its signal before the wait state is set. All of this is possible
	// because of the fact that switching the wait state and switching the mutex
	// state are performed as two separate operations. In common parlance, this is
	// known as the "lost wakeup problem".
	//
	// On systems that do have immediate lock/unlock mechanics with the wait state,
	// such as systems that natively support pthread_cond_wait(), it is impossible
	// for this situation to occur since both the thread wait state and the mutex
	// state will switch simultaneously, thus never missing a signal due to the
	// constant protection of the mutex.
#if defined(WIN32)
	if (this->isTaskWaiting)
	{
		// In the event where the signal was missed by taskProc(), just do the work
		// right now in this thread. Hopefully, signal misses don't happen to often,
		// because if they do, it would completely defeat the purpose of having the
		// extra task thread in the first place.
		this->ret = this->workFunc(workFuncParam);
		this->workFunc = NULL;
	}
#endif

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

void Task::start(bool spinlock) { impl->start(spinlock); }
void Task::shutdown() { impl->shutdown(); }
Task::Task() : impl(new Task::Impl()) {}
Task::~Task() { delete impl; }
void Task::execute(const TWork &work, void* param) { impl->execute(work,param); }
void* Task::finish() { return impl->finish(); }


