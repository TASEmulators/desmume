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

// VideoEncoders.cpp : what the recording settings dialog can offer.
//
// Enumerates the Media Foundation encoders installed on this machine, works
// out which of them are hardware ones, and decides which container a chosen
// pair of codecs ends up in.
//
// MFTEnumEx is used rather than MFTEnum2: the list needs every MFT plus the
// answer to "is this one hardware", which is one pass here.  MFTEnum2 only
// filters on the way in, so it would take two passes and a set difference,
// and it needs Windows 10 1709.

// Declarations only: _WIN32_WINNT decides which SDK prototypes this file can
// see, not what the exe links against.
#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00

#include "mflite.h"
#include "videoencoders.h"

namespace
{
	const wchar_t kNone[] = L"none";

	std::wstring attributeString(IMFActivate *activate, REFGUID key)
	{
		LPWSTR value  = NULL;
		UINT32 length = 0;

		if (FAILED(activate->GetAllocatedString(key, &value, &length)) || value == NULL)
			return std::wstring();

		std::wstring result(value);
		CoTaskMemFree(value);
		return result;
	}

	bool isHardwareEncoder(IMFActivate *activate)
	{
		UINT32 flags = 0;
		if (SUCCEEDED(activate->GetUINT32(MF_TRANSFORM_FLAGS_Attribute, &flags)) &&
		    (flags & MFT_ENUM_FLAG_HARDWARE) != 0)
			return true;

		// A few third party MFTs register without the flag.  The vendor id
		// attribute is only written for hardware ones, so it works as a
		// fallback without guessing from the friendly name.
		return !attributeString(activate, MFT_ENUM_HARDWARE_VENDOR_ID_Attribute).empty();
	}

	const wchar_t *vendorName(const std::wstring &id)
	{
		if (id == L"VEN_10DE")
			return L"NVIDIA";
		if (id == L"VEN_1002")
			return L"AMD";
		if (id == L"VEN_8086")
			return L"Intel";
		if (id == L"VEN_1414")
			return L"Microsoft";
		if (id == L"VEN_5143")
			return L"Qualcomm";
		return L"";
	}

	// One codec's worth of findings: the first hardware MFT and the first
	// software MFT that can produce it, if any.
	struct CodecRow
	{
		bool		 hardwareFound;
		std::wstring hardwareName;
		std::wstring hardwareVendor;

		bool		 softwareFound;
		std::wstring softwareName;

		CodecRow() : hardwareFound(false), softwareFound(false) {}
	};

	void collectVideoCodec(const GUID &codec, CodecRow &row)
	{
		MFT_REGISTER_TYPE_INFO output;
		output.guidMajorType = MFMediaType_Video;
		output.guidSubtype	 = codec;

		IMFActivate **activates = NULL;
		UINT32		  count		= 0;

		if (FAILED(mf::enumTransforms(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_ALL, NULL, &output,
		                              &activates, &count)))
			return;

		for (UINT32 i = 0; i < count; ++i)
		{
			const bool		   hardware = isHardwareEncoder(activates[i]);
			const std::wstring name		= attributeString(activates[i], MFT_FRIENDLY_NAME_Attribute);

			// Only the first MFT of each kind is shown: the sink writer picks
			// one itself, so a machine with two GPUs still gets one row per
			// codec and kind rather than two rows that do the same thing.
			if (hardware)
			{
				if (!row.hardwareFound)
				{
					row.hardwareFound	= true;
					row.hardwareName	= name;
					row.hardwareVendor = vendorName(
						attributeString(activates[i], MFT_ENUM_HARDWARE_VENDOR_ID_Attribute));
				}
			}
			else if (!row.softwareFound)
			{
				row.softwareFound = true;
				row.softwareName  = name;
			}

			activates[i]->Release();
		}

		CoTaskMemFree(activates);
	}

	void collectAudioCodec(const GUID &codec, AudioEncoderChoice *choice)
	{
		MFT_REGISTER_TYPE_INFO output;
		output.guidMajorType = MFMediaType_Audio;
		output.guidSubtype	 = codec;

		IMFActivate **activates = NULL;
		UINT32		  count		= 0;

		if (FAILED(mf::enumTransforms(MFT_CATEGORY_AUDIO_ENCODER, MFT_ENUM_FLAG_ALL, NULL, &output,
		                              &activates, &count)))
			return;

		if (count > 0)
		{
			choice->subtype	  = codec;
			choice->available = true;
			choice->name	  = attributeString(activates[0], MFT_FRIENDLY_NAME_Attribute);
		}

		for (UINT32 i = 0; i < count; ++i)
			activates[i]->Release();

		CoTaskMemFree(activates);
	}
}

namespace videoEncoders
{
	bool available()
	{
		if (!mf::startup())
			return false;

		mf::shutdown();
		return true;
	}

	void enumerate(std::vector<VideoEncoderChoice> &video, std::vector<AudioEncoderChoice> &audio)
	{
		video.clear();
		audio.clear();

		if (!mf::startup())
		{
			VideoEncoderChoice noneVideo;
			ZeroMemory(&noneVideo.subtype, sizeof(GUID));
			noneVideo.hardware	= false;
			noneVideo.available = false;
			video.push_back(noneVideo);

			AudioEncoderChoice noneAudio;
			ZeroMemory(&noneAudio.subtype, sizeof(GUID));
			noneAudio.available = false;
			audio.push_back(noneAudio);
			return;
		}

		mf::ComApartment com;

		static const GUID kVideoCodecs[] = { MFVideoFormat_H264, MFVideoFormat_HEVC, MFVideoFormat_AV1 };

		// Everything Windows can encode audio to.  A codec only reaches the
		// dialog when an encoder MFT for it is installed, which is what makes
		// this list follow the operating system instead of a build switch.
		static const GUID kAudioCodecs[] = {
			MFAudioFormat_AAC,	MFAudioFormat_MP3,	MFAudioFormat_FLAC,
			MFAudioFormat_ALAC, MFAudioFormat_Opus, MFAudioFormat_Dolby_AC3,
		};

		for (size_t i = 0; i < _countof(kVideoCodecs); ++i)
		{
			CodecRow row;
			collectVideoCodec(kVideoCodecs[i], row);

			if (row.hardwareFound)
			{
				VideoEncoderChoice choice;
				choice.subtype	 = kVideoCodecs[i];
				choice.hardware	 = true;
				choice.available = true;
				choice.name		 = row.hardwareName;
				choice.vendor	 = row.hardwareVendor;
				video.push_back(choice);
			}

			if (row.softwareFound)
			{
				VideoEncoderChoice choice;
				choice.subtype	 = kVideoCodecs[i];
				choice.hardware	 = false;
				choice.available = true;
				choice.name		 = row.softwareName;
				video.push_back(choice);
			}
		}

		{
			VideoEncoderChoice choice;
			ZeroMemory(&choice.subtype, sizeof(GUID));
			choice.hardware	 = false;
			choice.available = false;
			video.push_back(choice);
		}

		for (size_t i = 0; i < _countof(kAudioCodecs); ++i)
		{
			AudioEncoderChoice choice;
			ZeroMemory(&choice.subtype, sizeof(GUID));
			choice.available = false;
			collectAudioCodec(kAudioCodecs[i], &choice);

			if (choice.available)
				audio.push_back(choice);
		}

		{
			AudioEncoderChoice choice;
			ZeroMemory(&choice.subtype, sizeof(GUID));
			choice.available = false;
			audio.push_back(choice);
		}

		mf::shutdown();
	}

	const RecordContainer *containerFor(const GUID &video, const GUID &audio)
	{
		static const RecordContainer kMp4  = { L".mp4", "MP4 Video" };
		static const RecordContainer kM4a  = { L".m4a", "MPEG-4 Audio" };
		static const RecordContainer kMp3  = { L".mp3", "MP3 Audio" };
		static const RecordContainer kFlac = { L".flac", "FLAC Audio" };

		const bool hasVideo = !isNullGuid(video);
		const bool hasAudio = !isNullGuid(audio);

		if (!hasVideo && !hasAudio)
			return NULL;

		// With a picture the container is always MPEG-4; audio-only picks the
		// native container of the codec where there is one, and falls back to
		// MPEG-4 audio (.m4a) for the codecs that only exist inside it.
		if (hasVideo)
			return &kMp4;

		if (IsEqualGUID(audio, MFAudioFormat_MP3))
			return &kMp3;

		if (IsEqualGUID(audio, MFAudioFormat_FLAC))
			return &kFlac;

		return &kM4a;
	}

	const wchar_t *codecName(const GUID &subtype)
	{
		if (IsEqualGUID(subtype, MFVideoFormat_H264))
			return L"H.264";
		if (IsEqualGUID(subtype, MFVideoFormat_HEVC))
			return L"HEVC";
		if (IsEqualGUID(subtype, MFVideoFormat_AV1))
			return L"AV1";
		if (IsEqualGUID(subtype, MFAudioFormat_AAC))
			return L"AAC";
		if (IsEqualGUID(subtype, MFAudioFormat_MP3))
			return L"MP3";
		if (IsEqualGUID(subtype, MFAudioFormat_FLAC))
			return L"FLAC";
		if (IsEqualGUID(subtype, MFAudioFormat_ALAC))
			return L"ALAC";
		if (IsEqualGUID(subtype, MFAudioFormat_Opus))
			return L"Opus";
		if (IsEqualGUID(subtype, MFAudioFormat_Dolby_AC3))
			return L"AC-3";
		return L"";
	}

	bool isLosslessAudio(const GUID &subtype)
	{
		return IsEqualGUID(subtype, MFAudioFormat_FLAC) || IsEqualGUID(subtype, MFAudioFormat_ALAC);
	}

	bool isH264(const GUID &subtype)
	{
		return IsEqualGUID(subtype, MFVideoFormat_H264) != FALSE;
	}

	bool isHevc(const GUID &subtype)
	{
		return IsEqualGUID(subtype, MFVideoFormat_HEVC) != FALSE;
	}

	bool isAv1(const GUID &subtype)
	{
		return IsEqualGUID(subtype, MFVideoFormat_AV1) != FALSE;
	}

	int videoFrameStep(double nativeFps, int targetFps)
	{
		if (targetFps <= 0 || nativeFps <= 0)
			return 1;

		int step = (int)(nativeFps / targetFps + 0.5);
		return step < 1 ? 1 : step;
	}

	GUID defaultVideoCodec()
	{
		return MFVideoFormat_H264;
	}

	GUID defaultAudioCodec()
	{
		return MFAudioFormat_AAC;
	}

	bool isNullGuid(const GUID &guid)
	{
		static const GUID kNull = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
		return IsEqualGUID(guid, kNull) != FALSE;
	}

	std::wstring guidToString(const GUID &guid)
	{
		wchar_t buffer[64];
		swprintf(buffer, _countof(buffer), L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		         (unsigned)guid.Data1, (unsigned)guid.Data2, (unsigned)guid.Data3,
		         (unsigned)guid.Data4[0], (unsigned)guid.Data4[1], (unsigned)guid.Data4[2],
		         (unsigned)guid.Data4[3], (unsigned)guid.Data4[4], (unsigned)guid.Data4[5],
		         (unsigned)guid.Data4[6], (unsigned)guid.Data4[7]);
		return std::wstring(buffer);
	}

	bool guidFromString(const std::wstring &text, GUID *guid)
	{
		if (text.empty() || text == kNone)
			return false;

		return SUCCEEDED(CLSIDFromString(text.c_str(), (LPCLSID)guid));
	}
}
