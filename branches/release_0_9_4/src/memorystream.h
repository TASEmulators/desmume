#ifndef _memorystream_h_
#define _memorystream_h_

#include <iostream>
#include <string.h>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <algorithm>

template<typename T>
class memory_streambuf: public std::streambuf {
private:

	friend class memorystream;

	//the current buffer
	T* buf;

	//the current allocated capacity of the buffer
	size_t capacity;

	//whether the sequence is owned by the stringbuf
	bool myBuf;

	//the logical length of the buffer
	size_t length;

	//the current 'write window' starting position within the buffer for writing.
	size_t ww;

	//a vector that we have been told to use
	std::vector<T>* usevec;


public:

	memory_streambuf(int _capacity)
		: buf(new T[capacity=_capacity])
		, myBuf(true)
		, length(_capacity)
		, ww(0)
		, usevec(0)
	{
		sync();
	}


	memory_streambuf()
		: buf(new T[capacity = 128])
		, myBuf(true)
		, length(0)
		, ww(0)
		, usevec(0)
	{
		sync();
	}

	//constructs a non-expandable streambuf around the provided buffer
	memory_streambuf(T* usebuf, int buflength)
		: buf(usebuf)
		, myBuf(false)
		, length(buflength)
		, ww(0)
		, usevec(0)
	{
		sync();
	}

	//constructs an expandable streambuf around the provided buffer
	memory_streambuf(std::vector<T>* _usevec)
		: capacity(_usevec->size())
		, myBuf(false)
		, length(_usevec->size())
		, ww(0)
		, usevec(_usevec)
	{
		if(length>0)
			buf = &(*_usevec)[0];
		else buf = 0;

		sync();
	}

	~memory_streambuf()
	{
		//only cleanup if we own the seq
		if(myBuf) delete[] buf;
	}

	//the logical length of the buffer
	size_t size()
	{
		sync();
		return length;
	}

	//to avoid copying, rebuilds the provided vector and copies the streambuf contents into it
	void toVector(std::vector<T>& out)
	{
		out.resize(length);
		memcpy(&out[0],buf,length);
	}

	//maybe the compiler can avoid copying, but maybe not: returns a vector representing the current streambuf
	std::vector<T> toVector()
	{
		return std::vector<T>(buf,buf+length);
	}

	//if the memorystream wraps a vector, the vector will be trimmed to the correct size,.
	//you probably need to use this if you are using the vector wrapper
	void trim()
	{
		if(!usevec) return;
		usevec->resize(size());
	}

	//tells the current read or write position
	std::streampos tell(std::ios::openmode which)
	{
		if(which == std::ios::in)
			return tellRead();
		else if(which == std::ios::out)
			return tellWrite();
		else return -1;
	}

	//tells the current read position
	std::streampos tellRead()
	{
		return gptr()-eback();
	}

	//tells the current write position
	std::streampos tellWrite()
	{
		return pptr()-pbase() + ww;
	}

	int sync()
	{
		dosync(-1);
		return 0;
	}

	T* getbuf()
	{
		sync();
		return buf;
	}

	//if we were provided a buffer, then calling this gives us ownership of it
	void giveBuf() {
		myBuf = true;
	}

private:

	void dosync(int c)
	{
		size_t wp = tellWrite();
		size_t rp = tellRead();

		//if we are supposed to insert a character..
		if(c != -1)
		{
			buf[wp] = c;
			wp++;
		}

		//the length is determined by the highest character that was ever inserted
		length = std::max(length,wp);

		//the write window advances to begin at the current write insertion point
		ww = wp;

		//set the new write and read windows
		setp(buf+ww, buf + capacity);
		setg(buf, buf+rp, buf + length);
	}

	void expand(size_t upto)
	{
		if(!myBuf && !usevec)
			throw new std::runtime_error("memory_streambuf is not expandable");

		size_t newcapacity;
		if(upto == (size_t)-1)
			newcapacity = capacity + capacity/2 + 2;
		else
			newcapacity = std::max(upto,capacity);

		if(newcapacity == capacity) return;

		//if we are supposed to use the vector, then do it now
		if(usevec)
		{
			usevec->resize(newcapacity);
			capacity = usevec->size();
			buf = &(*usevec)[0];
		}
		else
		{
			//otherwise, manage our own buffer
			T* newbuf = new T[newcapacity];
			memcpy(newbuf,buf,capacity);
			delete[] buf;
			capacity = newcapacity;
			buf = newbuf;
		}
	}

protected:

	int overflow(int c)
	{
		expand((size_t)-1);
		dosync(c);
		return 1;
	}

	std::streambuf::pos_type seekpos(pos_type pos, std::ios::openmode which)
	{
		//extend if we are seeking the write cursor
		if(which & std::ios_base::out)
			expand(pos);

		sync();

		if(which & std::ios_base::in)
			setg(buf, buf+pos, buf + length);
		if(which & std::ios_base::out)
		{
			ww = pos;
			setp(buf+pos, buf + capacity);
		}

		return pos;
	}

	pos_type seekoff(off_type off, std::ios::seekdir way, std::ios::openmode which)
	{
		switch(way) {
		case std::ios::beg:
			return seekpos(off, which);
		case std::ios::cur:
			return seekpos(tell(which)+off, which);
		case std::ios::end:
			return seekpos(length+off, which);
		default:
			return -1;
		}
	}

};

//an iostream that uses the memory_streambuf to effectively act much like a c# memorystream
//please call sync() after writing data if you want to read it back
class memorystream : public std::basic_iostream<char, std::char_traits<char> >
{
public:
	memorystream()
		: std::basic_iostream<char, std::char_traits<char> >(&streambuf)
	{}

	memorystream(int sz)
		: std::basic_iostream<char, std::char_traits<char> >(&streambuf)
		, streambuf(sz)
	{}

	memorystream(char* usebuf, int buflength)
		: std::basic_iostream<char, std::char_traits<char> >(&streambuf)
		, streambuf(usebuf, buflength)
	{}

	memorystream(std::vector<char>* usevec)
		: std::basic_iostream<char, std::char_traits<char> >(&streambuf)
		, streambuf(usevec)
	{}

	//the underlying memory_streambuf
	memory_streambuf<char> streambuf;


public:

	size_t size() { return streambuf.size(); }
	char* buf() { return streambuf.getbuf(); }
	//flushes all the writing state and ensures the stream is ready for reading
	void sync() { streambuf.sync(); }
	//rewinds the cursors to offset 0
	void rewind() { streambuf.seekpos(0,std::ios::in | std::ios::out); }

	//if the memorystream wraps a vector, the vector will be trimmed to the correct size,.
	//you probably need to use this if you are using the vector wrapper
	void trim() { streambuf.trim(); }

	void giveBuf() { streambuf.giveBuf(); }
};


#endif
