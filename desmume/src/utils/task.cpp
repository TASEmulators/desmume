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
#include <assert.h>

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

static void thunkTaskProc(void *arg);

class Task::Impl {
private:
	sthread_t* thread;
	friend void thunkTaskProc(void* arg);
	void taskProc();

public:
	Impl();
	~Impl();

	void start(bool spinlock);
	void execute(const TWork &work, void *param);
	void* finish();
	void shutdown();
	void initialize();

	slock_t *mutex;
	scond_t *workCond;
	volatile bool workFlag, finishFlag, exitFlag;
	volatile TWork workFunc;
	void * volatile workFuncParam;
	void * volatile ret;
	bool started;
};

static void thunkTaskProc(void *arg)
{
	Task::Impl *ctx = (Task::Impl *)arg;
	ctx->taskProc();
}

void Task::Impl::taskProc()
{
	for(;;)
	{
		slock_lock(mutex);

		if(!workFlag)
			scond_wait(workCond, mutex);
		workFlag = false;

		ret = workFunc(workFuncParam);

		finishFlag = true;
		scond_signal(workCond);

		slock_unlock(mutex);

		if(exitFlag)
			break;
	}
}

static void* killTask(void* task)
{
	((Task::Impl*)task)->exitFlag = true;
	return NULL;
}

Task::Impl::Impl()
	: started(false)
{
}

Task::Impl::~Impl()
{
	shutdown();
}

void Task::Impl::initialize()
{
	thread = NULL;
	workFunc = NULL;
	workCond = NULL;
	workFunc = NULL;
	workFuncParam = NULL;
	workFlag = finishFlag = exitFlag = false;
	ret = NULL;
	started = false;
}

void Task::Impl::start(bool spinlock)
{
	//check user error
	assert(!started);

	if(started) shutdown();

	initialize();
	mutex = slock_new();
	workCond = scond_new();

	slock_lock(mutex);

	thread = sthread_create(&thunkTaskProc,this);
	started = true;

	slock_unlock(mutex);
}

void Task::Impl::shutdown()
{
	if(!started) return;

	//nobody should shutdown while a task is still running;
	//it would imply that we're in some kind of shutdown process, and datastructures might be getting freed while a worker is still working on it.
	//nonetheless, _troublingly_, it seems like we do that now, so for now let's try to let that work finish instead of blowing up when it isn't finished.
	//assert(!workFunc);
	finish();

	//a new task which sets the kill flag
	execute(killTask,this);
	finish();
	
	started = false;

	sthread_join(thread);
	scond_free(workCond);
	slock_free(mutex);
}

void* Task::Impl::finish()
{
	//no work running; nothing to do
	if(!workFunc)
		return NULL;

	slock_lock(mutex);

	if(!finishFlag)
		scond_wait(workCond, mutex);
	finishFlag = false;

	slock_unlock(this->mutex);

	workFunc = NULL;

	return ret;
}

void Task::Impl::execute(const TWork &work, void *param)
{
	slock_lock(this->mutex);

	workFunc = work;
	workFuncParam = param;
	workFlag = true;
	scond_signal(workCond);

	slock_unlock(this->mutex);
}

void Task::start(bool spinlock) { impl->start(spinlock); }
void Task::shutdown() { impl->shutdown(); }
Task::Task() : impl(new Task::Impl()) {}
Task::~Task() { delete impl; }
void Task::execute(const TWork &work, void* param) { impl->execute(work,param); }
void* Task::finish() { return impl->finish(); }


