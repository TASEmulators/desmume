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

//-------------------------
//this file contains the METASPU system
//which is designed to handle the task of audio synchronization
//and is designed to be as portable between multiple emulators
//-------------------------


#ifndef _METASPU_H_
#define _METASPU_H_

#include <algorithm>

template< typename T >
static FORCEINLINE void Clampify( T& src, T min, T max )
{
	src = std::min( std::max( src, min ), max );
}

template< typename T >
static FORCEINLINE T GetClamped( T src, T min, T max )
{
	return std::min( std::max( src, min ), max );
}

class ISynchronizingAudioBuffer
{
public:
	virtual void enqueue_samples(s16* buf, int samples_provided) = 0;

	//returns the number of samples actually supplied, which may not match the number requested
	virtual int output_samples(s16* buf, int samples_requested) = 0;
};

enum ESynchMode
{
	ESynchMode_DualSynchAsynch,
	ESynchMode_Synchronous
};

enum ESynchMethod
{
	ESynchMethod_N, //nitsuja's
	ESynchMethod_Z, //zero's
	ESynchMethod_P, //PCSX2 spu2-x
};

ISynchronizingAudioBuffer* metaspu_construct(ESynchMethod method);

#endif
