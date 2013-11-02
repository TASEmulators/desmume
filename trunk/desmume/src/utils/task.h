/*
	Copyright (C) 2009 DeSmuME team

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

#ifndef _TASK_H_
#define _TASK_H_

//Sort of like a single-thread thread pool.
//You hand it a worker function and then call finish() to synch with its completion
class Task
{
public:
	Task();
	~Task();
	
	typedef void * (*TWork)(void *);

	// initialize task runner
	void start(bool spinlock);

	//execute some work
	void execute(const TWork &work, void* param);

	//wait for the work to complete
	void* finish();

	// does the opposite of start
	void shutdown();

	class Impl;
	Impl *impl;

};

int getOnlineCores (void);

#endif
