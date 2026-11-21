/*
	Copyright (C) 2008-2026 DeSmuME team

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

// Frontend-independent DSM (.dsm) movie metadata. This is the logic that used
// to live inside windows/replay.cpp, minus the Win32 dialogs and globals.
//
// The host frontend owns everything platform specific: file dialogs, the
// widgets that display the fields, colour choices and preference persistence.
// It calls ReadHeader()/Assess() and formats the returned strings; the actual
// playback still goes through the core's FCEUI_LoadMovie().
//
// The core's FCEUI_MovieGetInfo() is a stub and MovieData::installRomChecksum()
// deliberately drops the checksum, so the header has to be parsed here. Only
// header lines are decoded - frame data is never touched - which makes a
// preview cheap enough to run on every file-selection change.
//
// This header must stay free of platform headers; in particular it does not
// include types.h (BOOL/TRUE/FALSE break Cocoa declarations). Strings are
// UTF-8, matching the DSM header itself.
//
// Frontend integration contract (Windows / Cocoa / GTK):
//   1. ReadHeader(path, hdr) on file selection or Refresh; bail out if false.
//   2. Assess(hdr, CurrentRomIdentity(), CurrentCoreVersion()) and render the
//      strings with the helpers below; colour the ROM/core fields red when
//      romMismatch/coreMismatch is set.
//   3. Let the user toggle read-only and, when writable, edit the description.
//   4. On OK: if descriptionChanged, call WriteComments(path, hdr, request.description);
//      then call FCEUI_LoadMovie(path, request.readOnly, false,
//                                ParsePauseFrame(request.pauseFrameText,
//                                                request.pauseEnabled)).
//   Path resolution, dialogs, widgets and preference persistence stay in the
//   host (INI / NSUserDefaults / GKeyFile). Nothing in this layer touches them.

#ifndef MOVIE_INFO_CORE_H
#define MOVIE_INFO_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace MovieInfo
{
	// Mirrors START_FROM from movie.h so frontends never have to include the
	// core header (types.h would break Cocoa declarations). Values are checked
	// against the real enum in movie_info_core.cpp.
	enum StartFrom
	{
		kStartBlank = 0,
		kStartSram = 1,
		kStartSavestate = 2
	};

	// All fields are kept as raw header strings so a host can display or parse
	// them without losing information. lines[] holds every header line verbatim
	// (LF-normalised) so rewriting comments never drops unknown keys.
	struct Header
	{
		bool valid;
		bool binary;
		bool hasSram;
		int64_t fileSize;
		int64_t frameDataOffset;  // byte offset of the first '|' frame line, or fileSize
		uint32_t frames;

		std::vector<std::string> lines;
		std::vector<std::string> comments;

		std::string version;
		std::string emuVersion;
		std::string rerecordCount;
		std::string romFilename;
		std::string romSerial;
		std::string romChecksum;
		std::string recordDate;
		std::string savestate;
		std::string useExtBios;
		std::string swiFromBios;
		std::string useExtFirmware;
		std::string bootFromFirmware;
		std::string firmNickname;
		std::string firmLanguage;

		Header();
	};

	// Identity of the ROM currently loaded in the emulator. The host may fill
	// this in itself, or use CurrentRomIdentity() for the usual case.
	struct RomIdentity
	{
		std::string serial;
		std::string internalName;
		uint32_t crc;
		bool loaded;

		RomIdentity();
	};

	// What a preview needs to know about compatibility. A missing serial or a
	// legacy base64 checksum is "unknown", never a mismatch.
	struct Assessment
	{
		bool crcComparable;     // romChecksum is an 8-digit hex CRC32, not legacy base64 MD5
		bool romMismatch;
		bool coreMismatch;
		bool coreKnown;         // emuVersion present and parses to a nonzero value
		bool hasCurrentRom;
		uint32_t recordedCore;
		uint32_t currentCore;

		Assessment();
	};

	// Reads only the header of path. Returns false for unreadable files,
	// unknown DSM versions and malformed headers. Frame data is never decoded.
	bool ReadHeader(const std::string &path, Header &out);

	// Splits "key value" the same way LoadFM2() does. Exposed so hosts that
	// rewrite the header can preserve keys this build does not know.
	void SplitKeyValue(const std::string &line, std::string &key, std::string &value);

	// Returns new header text (LF-terminated) with all non-author comment lines
	// replaced by description. Frame bytes are NOT included: the host appends
	// everything from header.frameDataOffset onwards, verbatim.
	std::string BuildHeaderWithComments(const Header &hdr, const std::string &description);

	Assessment Assess(const Header &hdr, const RomIdentity &rom, uint32_t currentCoreVersion);

	// The complete state a Play Movie dialog hands to the core. Hosts fill it
	// from their own widgets; GTK can use it unchanged. pauseFrameText is kept
	// as text so ParsePauseFrame() can apply the same validation everywhere.
	struct PlayRequest
	{
		bool readOnly;
		bool pauseEnabled;
		std::string pauseFrameText;
		std::string description;
		bool descriptionChanged;

		PlayRequest();
	};

	// Rewrites only the description comment lines in place, keeping every
	// unknown key, the author line and all frame bytes. Atomic on both Windows
	// and POSIX. Returns false (leaving the original file intact) on IO error.
	bool WriteComments(const std::string &path, const Header &hdr, const std::string &description);

	// -1 when playback should run to the end, otherwise the frame to pause at.
	int ParsePauseFrame(const std::string &text, bool enabled);

	// Convenience wrappers over gameInfo and EMU_DESMUME_VERSION_NUMERIC().
	// Implemented in the .cpp so no frontend has to include the core headers.
	RomIdentity CurrentRomIdentity();
	uint32_t CurrentCoreVersion();

	// --- Display helpers. Kept here so every frontend shows identical text. ---

	// "comment author X" becomes the author; every other comment line is joined
	// with "; " into the description.
	void SplitComments(const Header &hdr, std::string &author, std::string &description);

	std::string FormatLength(uint32_t frames);              // HH:MM:SS.hh
	std::string FormatRecordDate(const std::string &raw);  // local time, or "Unknown"
	std::string FormatHexCrc(uint32_t crc);                // 8 uppercase hex digits
	std::string CoreVersionText(const Header &hdr, const Assessment &a);
	std::string FormatRomLine(const char *prefix, const std::string &serial,
	                           const std::string &crc, const char *thirdLabel,
	                           const std::string &thirdValue);
	std::string BaseName(const std::string &path);
	std::string Shorten(const std::string &s, size_t maxChars);
	std::string DescribeFirmware(const std::string &raw);
	std::string DescribeLanguage(const std::string &raw);

	bool IsHexCrc(const std::string &s);
	uint32_t ParseUInt(const std::string &s);

	// --- Movie creation helpers (Record Movie dialog) ---

	// Appends .dsm only when the leaf name has no extension, matching the
	// Windows dialog's EnsureDsmExtension().
	std::string EnsureDsmExtension(const std::string &fileName);

	// Returns a leaf file name (no directory). With unique=true, "ROM.dsm"
	// becomes "ROM - 1.dsm", "ROM - 2.dsm" ... until one does not exist in
	// directory; playback never uses this (it must not guess a name).
	std::string DefaultMovieFileName(const std::string &baseName,
	                                  const std::string &directory, bool unique);

	struct EmulationFlags
	{
		bool useExtBios;
		bool swiFromBios;
		bool useExtFirmware;
		bool bootFromFirmware;
	};

	EmulationFlags CurrentEmulationFlags();

	struct RTCStamp
	{
		int year, month, day, hour, minute, second;
	};

	// The core's historical default (2009-01-01 00:00:00), used as the Start
	// RTC field of the Record dialog.
	RTCStamp DefaultRTCStamp();
}

#endif // MOVIE_INFO_CORE_H
