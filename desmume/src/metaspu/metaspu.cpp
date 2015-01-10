/*  Copyright 2009-2015 DeSmuME team

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

#include "metaspu.h"

#include <queue>
#include <vector>
#include <assert.h>

//for pcsx2 method
#if defined(_MSC_VER) || defined(HAVE_LIBSOUNDTOUCH) || defined(DESMUME_COCOA) || defined(DESMUME_QT)
#include "SndOut.h"
#endif


template<typename T> inline T _abs(T val)
{
	if(val<0) return -val;
	else return val;
}

template<typename T> inline T moveValueTowards(T val, T target, T incr)
{
	incr = _abs(incr);
	T delta = _abs(target-val);
	if(val<target) val += incr;
	else if(val>target) val -= incr;
	T newDelta = _abs(target-val);
	if(newDelta >= delta)
		val = target;
	return val;
}


class ZeromusSynchronizer : public ISynchronizingAudioBuffer
{
public:
	ZeromusSynchronizer()
		: mixqueue_go(false)
		,
		#ifdef NDEBUG
		adjustobuf(200,1000)
		#else
		adjustobuf(22000,44000)
		#endif
	{

	}

	bool mixqueue_go;

	virtual void enqueue_samples(s16* buf, int samples_provided)
	{
		for(int i=0;i<samples_provided;i++) {
			s16 left = *buf++;
			s16 right = *buf++;
			adjustobuf.enqueue(left,right);
		}
	}

	//returns the number of samples actually supplied, which may not match the number requested
	virtual int output_samples(s16* buf, int samples_requested)
	{
		int done = 0;
		if(!mixqueue_go) {
			if(adjustobuf.size > 200)
				mixqueue_go = true;
		}
		else
		{
			for(int i=0;i<samples_requested;i++) {
				if(adjustobuf.size==0) {
					mixqueue_go = false;
					break;
				}
				done++;
				s16 left, right;
				adjustobuf.dequeue(left,right);
				*buf++ = left;
				*buf++ = right;
			}
		}
		
		return done;
	}

private:
	class Adjustobuf
	{
	public:
		Adjustobuf(int _minLatency, int _maxLatency)
			: size(0)
			, minLatency(_minLatency)
			, maxLatency(_maxLatency)
		{
			rollingTotalSize = 0;
			targetLatency = (maxLatency + minLatency)/2;
			rate = 1.0f;
			cursor = 0.0f;
			curr[0] = curr[1] = 0;
			kAverageSize = 80000;
		}

		float rate, cursor;
		int minLatency, targetLatency, maxLatency;
		std::queue<s16> buffer;
		int size;
		s16 curr[2];

		std::queue<int> statsHistory;

		void enqueue(s16 left, s16 right) 
		{
			buffer.push(left);
			buffer.push(right);
			size++;
		}

		s64 rollingTotalSize;

		u32 kAverageSize;

		void addStatistic()
		{
			statsHistory.push(size);
			rollingTotalSize += size;
			if(statsHistory.size()>kAverageSize)
			{
				rollingTotalSize -= statsHistory.front();
				statsHistory.pop();

				float averageSize = (float)(rollingTotalSize / kAverageSize);
				//static int ctr=0;  ctr++; if((ctr&127)==0) printf("avg size: %f curr size: %d rate: %f\n",averageSize,size,rate);
				{
					float targetRate;
					if(averageSize < targetLatency)
					{
						targetRate = 1.0f - (targetLatency-averageSize)/kAverageSize;
					}
					else if(averageSize > targetLatency) {
						targetRate = 1.0f + (averageSize-targetLatency)/kAverageSize;
					} else targetRate = 1.0f;
				
					//rate = moveValueTowards(rate,targetRate,0.001f);
					rate = targetRate;
				}

			}


		}

		void dequeue(s16& left, s16& right)
		{
			left = right = 0; 
			addStatistic();
			if(size==0) { return; }
			cursor += rate;
			while(cursor>1.0f) {
				cursor -= 1.0f;
				if(size>0) {
					curr[0] = buffer.front(); buffer.pop();
					curr[1] = buffer.front(); buffer.pop();
					size--;
				}
			}
			left = curr[0]; 
			right = curr[1];
		}
	} adjustobuf;
};

class NitsujaSynchronizer : public ISynchronizingAudioBuffer
{
private:
	struct ssamp
	{
		s16 l, r;
		ssamp() {}
		ssamp(s16 ll, s16 rr) : l(ll), r(rr) {}
	};

	std::vector<ssamp> sampleQueue;

	// returns values going between 0 and y-1 in a saw wave pattern, based on x
	static FORCEINLINE int pingpong(int x, int y)
	{
		x %= 2*y;
		if(x >= y)
			x = 2*y - x - 1;
		return x;

		// in case we want to switch to odd buffer sizes for more sharpness
		//x %= 2*(y-1);
		//if(x >= y)
		//	x = 2*(y-1) - x;
		//return x;
	}

	static FORCEINLINE ssamp crossfade (ssamp lhs, ssamp rhs,  int cur, int start, int end)
	{
		if(cur <= start)
			return lhs;
		if(cur >= end)
			return rhs;

		// in case we want sine wave interpolation instead of linear here
		//float ang = 3.14159f * (float)(cur - start) / (float)(end - start);
		//cur = start + (int)((1-cosf(ang))*0.5f * (end - start));

		int inNum = cur - start;
		int outNum = end - cur;
		int denom = end - start;

		int lrv = ((int)lhs.l * outNum + (int)rhs.l * inNum) / denom;
		int rrv = ((int)lhs.r * outNum + (int)rhs.r * inNum) / denom;

		return ssamp(lrv,rrv);
	}

	static FORCEINLINE void emit_sample(s16*& outbuf, ssamp sample)
	{
		*outbuf++ = sample.l;
		*outbuf++ = sample.r;
	}

	static FORCEINLINE void emit_samples(s16*& outbuf, const ssamp* samplebuf, int samples)
	{
		for(int i=0;i<samples;i++)
			emit_sample(outbuf,samplebuf[i]);
	}

public:
	NitsujaSynchronizer()
	{}

	virtual void enqueue_samples(s16* buf, int samples_provided)
	{
		for(int i=0;i<samples_provided;i++)
		{
			sampleQueue.push_back(ssamp(buf[0],buf[1]));
			buf += 2;
		}
	}

	virtual int output_samples(s16* buf, int samples_requested)
	{
		int audiosize = samples_requested;
		int queued = sampleQueue.size();

		// I am too lazy to deal with odd numbers
		audiosize &= ~1;
		queued &= ~1;

		if(queued > 0x200 && audiosize > 0) // is there any work to do?
		{
			// are we going at normal speed?
			// or more precisely, are the input and output queues/buffers of similar size?
			if(queued > 900 || audiosize > queued * 2)
			{
				// not normal speed. we have to resample it somehow in this case.
				if(audiosize <= queued)
				{
					// fast forward speed
					// this is the easy case, just crossfade it and it sounds ok
					for(int i = 0; i < audiosize; i++)
					{
						int j = i + queued - audiosize;
						ssamp outsamp = crossfade(sampleQueue[i],sampleQueue[j], i,0,audiosize);
						emit_sample(buf,outsamp);
					}
				}
				else
				{
					// slow motion speed
					// here we take a very different approach,
					// instead of crossfading it, we select a single sample from the queue
					// and make sure that the index we use to select a sample is constantly moving
					// and that it starts at the first sample in the queue and ends on the last one.
					//
					// hopefully the index doesn't move discontinuously or we'll get slight crackling
					// (there might still be a minor bug here that causes this occasionally)
					//
					// here's a diagram of how the index we sample from moves:
					//
					// queued (this axis represents the index we sample from. the top means the end of the queue)
					// ^
					// |   --> audiosize (this axis represents the output index we write to, right meaning forward in output time/position)
					// |   A           C       C  end
					//    A A     B   C C     C
					//   A   A   A B C   C   C
					//  A     A A   B     C C
					// A       A           C
					// start
					//
					// yes, this means we are spending some stretches of time playing the sound backwards,
					// but the stretches are short enough that this doesn't sound weird.
					// this lets us avoid most crackling problems due to the endpoints matching up.

					// first calculate a shorter-than-full window
					// that has minimal slope at the endpoints
					// (to further reduce crackling, especially in sine waves)
					int beststart = 0, extraAtEnd = 0;
					{
						int bestend = queued;
						static const int worstdiff = 99999999;
						int beststartdiff = worstdiff;
						int bestenddiff = worstdiff;
						for(int i = 0; i < 128; i+=2)
						{
							int diff = abs(sampleQueue[i].l - sampleQueue[i+1].l) + abs(sampleQueue[i].r - sampleQueue[i+1].r);
							if(diff < beststartdiff)
							{
								beststartdiff = diff;
								beststart = i;
							}
						}
						for(int i = queued-3; i > queued-3-128; i-=2)
						{
							int diff = abs(sampleQueue[i].l - sampleQueue[i+1].l) + abs(sampleQueue[i].r - sampleQueue[i+1].r);
							if(diff < bestenddiff)
							{
								bestenddiff = diff;
								bestend = i+1;
							}
						}

						extraAtEnd = queued - bestend;
						queued = bestend - beststart;

						int oksize = queued;
						while(oksize + queued*2 + beststart + extraAtEnd <= samples_requested)
							oksize += queued*2;
						audiosize = oksize;

						for(int x = 0; x < beststart; x++)
						{
							emit_sample(buf,sampleQueue[x]);
						}
						sampleQueue.erase(sampleQueue.begin(), sampleQueue.begin() + beststart);
					}


					int midpointX = audiosize >> 1;
					int midpointY = queued >> 1;

					// all we need to do here is calculate the X position of the leftmost "B" in the above diagram.
					// TODO: we should calculate it with a simple equation like
					//   midpointXOffset = min(something,somethingElse);
					// but it's a little difficult to work it out exactly
					// so here's a stupid search for the value for now:

					int prevA = 999999;
					int midpointXOffset = queued/2;
					while(true)
					{
						int a = abs(pingpong(midpointX - midpointXOffset, queued) - midpointY) - midpointXOffset;
						if(((a > 0) != (prevA > 0) || (a < 0) != (prevA < 0)) && prevA != 999999)
						{
							if((a + prevA)&1) // there's some sort of off-by-one problem with this search since we're moving diagonally...
								midpointXOffset++; // but this fixes it most of the time...
							break; // found it
						}
						prevA = a;
						midpointXOffset--;
						if(midpointXOffset < 0)
						{
							midpointXOffset = 0;
							break; // failed to find it. the two sides probably meet exactly in the center.
						}
					}

					int leftMidpointX = midpointX - midpointXOffset;
					int rightMidpointX = midpointX + midpointXOffset;
					int leftMidpointY = pingpong(leftMidpointX, queued);
					int rightMidpointY = (queued-1) - pingpong((int)audiosize-1 - rightMidpointX + queued*2, queued);

					// output the left almost-half of the sound (section "A")
					for(int x = 0; x < leftMidpointX; x++)
					{
						int i = pingpong(x, queued);
						emit_sample(buf,sampleQueue[i]);
					}

					// output the middle stretch (section "B")
					int y = leftMidpointY;
					int dyMidLeft  = (leftMidpointY  < midpointY) ? 1 : -1;
					int dyMidRight = (rightMidpointY > midpointY) ? 1 : -1;
					for(int x = leftMidpointX; x < midpointX; x++, y+=dyMidLeft)
						emit_sample(buf,sampleQueue[y]);
					for(int x = midpointX; x < rightMidpointX; x++, y+=dyMidRight)
						emit_sample(buf,sampleQueue[y]);

					// output the end of the queued sound (section "C")
					for(int x = rightMidpointX; x < audiosize; x++)
					{
						int i = (queued-1) - pingpong((int)audiosize-1 - x + queued*2, queued);
						emit_sample(buf,sampleQueue[i]);
					}

					for(int x = 0; x < extraAtEnd; x++)
					{
						int i = queued + x;
						emit_sample(buf,sampleQueue[i]);
					}
					queued += extraAtEnd;
					audiosize += beststart + extraAtEnd;
				} //end else

				sampleQueue.erase(sampleQueue.begin(), sampleQueue.begin() + queued);
				return audiosize;
			}
			else
			{
				// normal speed
				// just output the samples straightforwardly.
				//
				// at almost-full speeds (like 50/60 FPS)
				// what will happen is that we rapidly fluctuate between entering this branch
				// and entering the "slow motion speed" branch above.
				// but that's ok! because all of these branches sound similar enough that we can get away with it.
				// so the two cases actually complement each other.

				if(audiosize >= queued)
				{
					emit_samples(buf,&sampleQueue[0],queued);
					sampleQueue.erase(sampleQueue.begin(), sampleQueue.begin() + queued);
					return queued;
				}
				else
				{
					emit_samples(buf,&sampleQueue[0],audiosize);
					sampleQueue.erase(sampleQueue.begin(), sampleQueue.begin()+audiosize);
					return audiosize;
				}

			} //end normal speed

		} //end if there is any work to do
		else
		{
			return 0;
		}

	} //output_samples

private:

}; //NitsujaSynchronizer


#if defined(_MSC_VER) || defined(HAVE_LIBSOUNDTOUCH) || defined(DESMUME_COCOA) || defined(DESMUME_QT)
class PCSX2Synchronizer : public ISynchronizingAudioBuffer
{
public:
	std::queue<s16> readySamples;
	PCSX2Synchronizer()
	{
		SndBuffer::Init();
	}
	virtual void enqueue_samples(s16* buf, int samples_provided)
	{
		for(int i=0;i<samples_provided;i++)
		{
			StereoOut32 so32(buf[0],buf[1]);
			SndBuffer::Write(so32);
			buf++;
			buf++;
		}
	}

	virtual int output_samples(s16* buf, int samples_requested)
	{
		for(int i=0;i<samples_requested;i++) {
			if(readySamples.size()==0) {
				//SndOutPacketSize
				StereoOut16 temp[SndOutPacketSize*2];
				SndBuffer::ReadSamples( temp );
				for(int i=0;i<SndOutPacketSize;i++) {
					readySamples.push(temp[i].Left);
					readySamples.push(temp[i].Right);
				}
			}
			*buf++ = readySamples.front(); readySamples.pop();
			*buf++ = readySamples.front(); readySamples.pop();
		}
		return samples_requested;
	}
};
#endif


ISynchronizingAudioBuffer* metaspu_construct(ESynchMethod method)
{
	switch(method)
	{
	case ESynchMethod_N: return new NitsujaSynchronizer();
	case ESynchMethod_Z: return new ZeromusSynchronizer();
#if defined(_MSC_VER) || defined(HAVE_LIBSOUNDTOUCH) || defined(DESMUME_COCOA) || defined(DESMUME_QT)
	case ESynchMethod_P: return new PCSX2Synchronizer();
#endif
	default: return NULL;
	}
}
