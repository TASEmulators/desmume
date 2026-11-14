/*
	Copyright (C) 2026 DeSmuME team

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

#ifndef _MFLITE_H_
#define _MFLITE_H_

// The Media Foundation entry points the recorder uses, resolved at run time.
//
// Nothing here is imported: mfplat.dll and mfreadwrite.dll are opened with
// LoadLibrary, so an exe built from an XP-level source tree still starts on a
// machine that has neither, and the MinGW build does not need an import
// library that its headers do not provide.  The same rule covers D3D11,
// DirectDraw, DirectSound, XAudio2 and wgpu.
//
// Only the declarations matter here, and they come from the SDK headers at
// whatever _WIN32_WINNT the including translation unit asked for -- which is
// why every .cpp that pulls this in defines _WIN32_WINNT 0x0A00 itself before
// its first include.

#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mfreadwrite.h>
#include <mferror.h>

// mfuuid.lib is a static library of GUID constants only: it puts nothing in
// the import table, so linking it does not tie the exe to mfplat.dll.
#ifdef _MSC_VER
#pragma comment(lib, "mfuuid.lib")
#endif

namespace mf
{
	// Loads mfplat.dll and mfreadwrite.dll and calls MFStartup once.
	// Reference counted, so enumerating while a recording is open does not
	// tear the platform down.  False when the DLLs are missing or the
	// platform refuses to start.
	bool startup();
	void shutdown();

	// NULL until startup() succeeded.  The pointer types are the SDK
	// prototypes themselves, so they cannot drift: MFTEnumEx takes its
	// category GUID by value, and a hand written REFGUID here shifted every
	// later argument by one register.
	extern decltype(&MFTEnumEx) enumTransforms;
	extern decltype(&MFCreateAttributes) createAttributes;
	extern decltype(&MFCreateMediaType) createMediaType;
	extern decltype(&MFCreateMemoryBuffer) createMemoryBuffer;
	extern decltype(&MFCreateSample) createSample;
	extern decltype(&MFCreateSinkWriterFromURL) createSinkWriterFromURL;

	// A COM apartment for the calling thread, released on destruction.  The
	// emulator does not otherwise hold one; videoencoders.cpp initialises COM
	// the same way around its own calls.
	//
	// multiThreaded picks COINIT_MULTITHREADED, which is what a worker thread
	// that only ever talks to Media Foundation wants: an STA would need a
	// message pump that such a thread does not have.  The sink writer is
	// created on the thread that calls it, so this is what keeps the encoder
	// off the emulation thread.
	class ComApartment
	{
	public:
		explicit ComApartment(bool multiThreaded = false);
		~ComApartment();

		bool ok() const { return m_ok; }

	private:
		bool m_ok;
		bool m_uninit;

		ComApartment(const ComApartment &);
		ComApartment &operator=(const ComApartment &);
	};
}

#endif // _MFLITE_H_
