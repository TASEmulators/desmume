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

// MFLite.cpp : the Media Foundation entry points the recorder uses.
//
// Nothing here is imported: mfplat.dll and mfreadwrite.dll are opened with
// LoadLibrary, and the emulator keeps starting on a system that has neither.
//
// Every pointer below is the SDK prototype through decltype, so the call
// sites and the resolved address cannot disagree about the argument list.
//
// This translation unit sets the target level itself: declarations only,
// which is what lets MFAudioFormat_FLAC / _ALAC / _Opus and the Windows 8
// attributes be named.  Nothing here adds an entry to the import table.

#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00

#include "mflite.h"

namespace mf
{
	decltype(&MFTEnumEx) enumTransforms							  = NULL;
	decltype(&MFCreateAttributes) createAttributes				  = NULL;
	decltype(&MFCreateMediaType) createMediaType				  = NULL;
	decltype(&MFCreateMemoryBuffer) createMemoryBuffer			  = NULL;
	decltype(&MFCreateSample) createSample						  = NULL;
	decltype(&MFCreateSinkWriterFromURL) createSinkWriterFromURL = NULL;

	namespace
	{
		HMODULE s_platform	= NULL;
		HMODULE s_readWrite = NULL;
		int		s_refs		= 0;
		bool	s_started	= false;

		decltype(&MFStartup) s_startupPlatform	= NULL;
		decltype(&MFShutdown) s_shutdownPlatform = NULL;

		// One GetProcAddress per entry point, all or nothing: a partially
		// resolved table would only move the failure further down.
		bool resolveAll()
		{
			FARPROC startup		= GetProcAddress(s_platform, "MFStartup");
			FARPROC shutdown	= GetProcAddress(s_platform, "MFShutdown");
			FARPROC enumerate	= GetProcAddress(s_platform, "MFTEnumEx");
			FARPROC attributes	= GetProcAddress(s_platform, "MFCreateAttributes");
			FARPROC mediaType	= GetProcAddress(s_platform, "MFCreateMediaType");
			FARPROC buffer		= GetProcAddress(s_platform, "MFCreateMemoryBuffer");
			FARPROC sample		= GetProcAddress(s_platform, "MFCreateSample");
			FARPROC sinkWriter	= GetProcAddress(s_readWrite, "MFCreateSinkWriterFromURL");

			if (!startup || !shutdown || !enumerate || !attributes || !mediaType || !buffer ||
			    !sample || !sinkWriter)
				return false;

			s_startupPlatform  = reinterpret_cast<decltype(&MFStartup)>(startup);
			s_shutdownPlatform = reinterpret_cast<decltype(&MFShutdown)>(shutdown);
			enumTransforms	   = reinterpret_cast<decltype(&MFTEnumEx)>(enumerate);
			createAttributes   = reinterpret_cast<decltype(&MFCreateAttributes)>(attributes);
			createMediaType	   = reinterpret_cast<decltype(&MFCreateMediaType)>(mediaType);
			createMemoryBuffer = reinterpret_cast<decltype(&MFCreateMemoryBuffer)>(buffer);
			createSample	   = reinterpret_cast<decltype(&MFCreateSample)>(sample);
			createSinkWriterFromURL =
				reinterpret_cast<decltype(&MFCreateSinkWriterFromURL)>(sinkWriter);
			return true;
		}
	}

	bool startup()
	{
		if (s_started)
		{
			++s_refs;
			return true;
		}

		// The modules stay loaded once opened: a second recording session
		// would only open them again, and unloading them while a Media
		// Foundation object is still alive would be worse.
		if (s_platform == NULL)
			s_platform = LoadLibraryW(L"mfplat.dll");
		if (s_readWrite == NULL)
			s_readWrite = LoadLibraryW(L"mfreadwrite.dll");

		if (s_platform == NULL || s_readWrite == NULL || !resolveAll())
			return false;

		if (FAILED(s_startupPlatform(MF_VERSION, MFSTARTUP_FULL)))
			return false;

		s_started = true;
		s_refs	  = 1;
		return true;
	}

	void shutdown()
	{
		if (!s_started || s_refs <= 0)
			return;

		if (--s_refs > 0)
			return;

		s_shutdownPlatform();
		s_started = false;
	}

	// Tolerate a caller that already initialised COM, and only balance the
	// call that succeeded here.
	ComApartment::ComApartment(bool multiThreaded)
	{
		m_ok	 = false;
		m_uninit = false;

		const DWORD model = multiThreaded ? COINIT_MULTITHREADED : COINIT_APARTMENTTHREADED;
		const HRESULT hr  = CoInitializeEx(NULL, model | COINIT_DISABLE_OLE1DDE);
		if (hr == S_OK || hr == S_FALSE)
		{
			m_uninit = true;
			m_ok	 = true;
			return;
		}

		// The thread is already in another apartment (an MTA, typically).
		// Media Foundation works there too, so this is not a failure.
		m_ok = hr == RPC_E_CHANGED_MODE;
	}

	ComApartment::~ComApartment()
	{
		if (m_uninit)
			CoUninitialize();
	}
}
