#ifndef _PFC_PROFILER_H_
#define _PFC_PROFILER_H_

#ifdef _WINDOWS

#include <intrin.h>
namespace pfc {
class profiler_static {
public:
	profiler_static(const char * p_name);
	~profiler_static();
	void add_time(t_int64 delta) {total_time+=delta;num_called++;}
private:
	const char * name;
	t_uint64 total_time,num_called;
};

class profiler_local {
public:
	profiler_local(profiler_static * p_owner) {
		owner = p_owner;
		start = __rdtsc();
	}
	~profiler_local() {
		t_int64 end = __rdtsc();
		owner->add_time(end-start);
	}
private:
	t_int64 start;
	profiler_static * owner;
};

#define profiler(name) \
	static pfc::profiler_static profiler_static_##name(#name); \
	pfc::profiler_local profiler_local_##name(&profiler_static_##name);
	

class hires_timer {
public:
	void start() {
		m_start = g_query();
	}
	double query() const {
		return _query( g_query() );
	}
	double query_reset() {
		t_uint64 current = g_query();
		double ret = _query(current);
		m_start = current;
		return ret;
	}
private:
	double _query(t_uint64 p_val) const {
		return (double)( p_val - m_start ) / (double) g_query_freq();
	}
	static t_uint64 g_query() {
		LARGE_INTEGER val;
		if (!QueryPerformanceCounter(&val)) throw pfc::exception_not_implemented();
		return val.QuadPart;
	}
	static t_uint64 g_query_freq() {
		LARGE_INTEGER val;
		if (!QueryPerformanceFrequency(&val)) throw pfc::exception_not_implemented();
		return val.QuadPart;
	}
	t_uint64 m_start;
};

class lores_timer {
public:
	void start() {
		_start(GetTickCount());
	}

	double query() const {
		return _query(GetTickCount());
	}
	double query_reset() {
		t_uint32 time = GetTickCount();
		double ret = _query(time);
		_start(time);
		return ret;
	}
private:
	void _start(t_uint32 p_time) {m_last_seen = m_start = p_time;}
	double _query(t_uint32 p_time) const {
		t_uint64 time = p_time;
		if (time < (m_last_seen & 0xFFFFFFFF)) time += 0x100000000;
		m_last_seen = (m_last_seen & 0xFFFFFFFF00000000) + time;
		return (double)(m_last_seen - m_start) / 1000.0;
	}
	t_uint64 m_start;
	mutable t_uint64 m_last_seen;
};
}
#else 
//PORTME
#endif

#endif
