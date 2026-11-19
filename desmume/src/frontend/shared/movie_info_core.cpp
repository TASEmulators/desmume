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

#include "movie_info_core.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "../../NDSSystem.h"
#include "../../version.h"

namespace MovieInfo
{

Header::Header()
	: valid(false)
	, binary(false)
	, hasSram(false)
	, fileSize(0)
	, frameDataOffset(0)
	, frames(0)
{
}

RomIdentity::RomIdentity()
	: crc(0)
	, loaded(false)
{
}

Assessment::Assessment()
	: crcComparable(false)
	, romMismatch(false)
	, coreMismatch(false)
	, coreKnown(false)
	, hasCurrentRom(false)
	, recordedCore(0)
	, currentCore(0)
{
}

void SplitKeyValue(const std::string &line, std::string &key, std::string &value)
{
	const size_t i = line.find_first_not_of(" \t");
	if (i == std::string::npos)
	{
		key.clear();
		value.clear();
		return;
	}
	const size_t j = line.find_first_of(" \t", i);
	key = line.substr(i, j - i);
	if (j == std::string::npos)
	{
		value.clear();
		return;
	}
	const size_t k = line.find_first_not_of(" \t", j);
	value = (k == std::string::npos) ? "" : line.substr(k);
}

bool ReadHeader(const std::string &path, Header &out)
{
	out = Header();

	FILE *fp = fopen(path.c_str(), "rb");
	if (!fp)
		return false;

	if (fseek(fp, 0, SEEK_END) != 0)
	{
		fclose(fp);
		return false;
	}
	out.fileSize = (int64_t)ftell(fp);
	if (out.fileSize < 0)
	{
		fclose(fp);
		return false;
	}
	fseek(fp, 0, SEEK_SET);

	std::string line;
	long lineOffset = 0;
	bool lineStart = true;
	bool foundFrames = false;
	int c;

	// The header ends at the first line that starts with '|'. CR bytes are
	// dropped here so lines[] is LF-normalised, just as DeSmuME writes them.
	while ((c = fgetc(fp)) != EOF)
	{
		if (c == '\n')
		{
			out.lines.push_back(line);
			line.clear();
			lineStart = true;
			lineOffset = ftell(fp);
			continue;
		}
		if (c == '\r')
			continue;
		if (lineStart && c == '|')
		{
			out.frameDataOffset = (int64_t)lineOffset;
			foundFrames = true;
			break;
		}
		line += (char)c;
		if (c != ' ' && c != '\t')
			lineStart = false;
	}
	if (!line.empty())
		out.lines.push_back(line);
	if (!foundFrames)
		out.frameDataOffset = out.fileSize;

	for (size_t i = 0; i < out.lines.size(); i++)
	{
		std::string key, value;
		SplitKeyValue(out.lines[i], key, value);
		if (key.empty() || key[0] == '|')
			continue;

		if (key == "version") out.version = value;
		else if (key == "emuVersion") out.emuVersion = value;
		else if (key == "rerecordCount") out.rerecordCount = value;
		else if (key == "romFilename") out.romFilename = value;
		else if (key == "romSerial") out.romSerial = value;
		else if (key == "romChecksum") out.romChecksum = value;
		else if (key == "recordDate") out.recordDate = value;
		else if (key == "savestate") out.savestate = value;
		else if (key == "useExtBios") out.useExtBios = value;
		else if (key == "swiFromBios") out.swiFromBios = value;
		else if (key == "useExtFirmware") out.useExtFirmware = value;
		else if (key == "bootFromFirmware") out.bootFromFirmware = value;
		else if (key == "firmNickname") out.firmNickname = value;
		else if (key == "firmLanguage") out.firmLanguage = value;
		else if (key == "binary") out.binary = atoi(value.c_str()) != 0;
		else if (key == "sram") out.hasSram = !value.empty();
		else if (key == "comment") out.comments.push_back(value);
		// Unknown keys are ignored here just as LoadFM2() ignores them.
	}

	// LoadFM2() only accepts "version 1" and "version 2".
	if (out.version != "1" && out.version != "2")
	{
		fclose(fp);
		return false;
	}

	if (out.binary)
	{
		// One '|' byte starts the chunk; each binary record is 6 bytes.
		const int64_t dataStart = out.frameDataOffset + 1;
		if (out.frameDataOffset < out.fileSize && out.fileSize > dataStart)
			out.frames = (uint32_t)((out.fileSize - dataStart) / 6);
	}
	else if (out.frameDataOffset < out.fileSize)
	{
		if (fseek(fp, (long)out.frameDataOffset, SEEK_SET) == 0)
		{
			bool bol = true;
			while ((c = fgetc(fp)) != EOF)
			{
				if (bol && c == '|')
				{
					out.frames++;
					bol = false;
				}
				else if (c == '\n') bol = true;
				else if (c == '\r') { /* keep the line-start state */ }
				else if (c != ' ' && c != '\t') bol = false;
			}
		}
	}

	fclose(fp);
	out.valid = true;
	return true;
}

std::string BuildHeaderWithComments(const Header &hdr, const std::string &description)
{
	std::string text;
	for (size_t i = 0; i < hdr.lines.size(); i++)
	{
		std::string key, value;
		SplitKeyValue(hdr.lines[i], key, value);
		if (key == "comment" && value.compare(0, 7, "author ") != 0)
			continue;  // a description line; replaced below
		text += hdr.lines[i];
		text += '\n';
	}
	if (!description.empty())
	{
		text += "comment ";
		text += description;
		text += '\n';
	}
	return text;
}

bool IsHexCrc(const std::string &s)
{
	if (s.size() != 8)
		return false;
	for (size_t i = 0; i < s.size(); i++)
	{
		if (!isxdigit((unsigned char)s[i]))
			return false;
	}
	return true;
}

uint32_t ParseUInt(const std::string &s)
{
	return (uint32_t)strtoul(s.c_str(), NULL, 10);
}

Assessment Assess(const Header &hdr, const RomIdentity &rom, uint32_t currentCoreVersion)
{
	Assessment a;
	a.currentCore = currentCoreVersion;
	a.hasCurrentRom = rom.loaded;
	a.recordedCore = ParseUInt(hdr.emuVersion);
	a.coreKnown = !hdr.emuVersion.empty() && a.recordedCore != 0;
	a.coreMismatch = a.coreKnown && a.recordedCore != currentCoreVersion;

	// Old DSM files stored a base64 MD5 blob in romChecksum instead of the
	// CRC32 that current builds write. It cannot be compared with gameInfo.crc,
	// so it must not be reported as a ROM mismatch.
	a.crcComparable = IsHexCrc(hdr.romChecksum);
	const uint32_t movieCrc = a.crcComparable
	                            ? (uint32_t)strtoul(hdr.romChecksum.c_str(), NULL, 16)
	                            : 0;

	// Without a loaded ROM there is nothing to compare against. Comparing with
	// a stale/zeroed gameInfo would only produce a false desync warning.
	if (rom.loaded)
	{
		a.romMismatch = (a.crcComparable && movieCrc != rom.crc)
		             || (!hdr.romSerial.empty() && hdr.romSerial != rom.serial);
	}
	return a;
}

RomIdentity CurrentRomIdentity()
{
	RomIdentity rom;
	rom.loaded = (gameInfo.fROM != NULL);
	rom.serial = gameInfo.ROMserial;
	rom.internalName = gameInfo.ROMname;
	rom.crc = (uint32_t)gameInfo.crc;
	return rom;
}

uint32_t CurrentCoreVersion()
{
	return (uint32_t)EMU_DESMUME_VERSION_NUMERIC();
}

PlayRequest::PlayRequest()
	: readOnly(true)
	, pauseEnabled(false)
	, descriptionChanged(false)
{
}

bool WriteComments(const std::string &path, const Header &hdr, const std::string &description)
{
	const std::string headerText = BuildHeaderWithComments(hdr, description);
	const std::string tmpPath = path + ".dsmtmp";

	FILE *out = fopen(tmpPath.c_str(), "wb");
	if (!out)
		return false;
	bool ok = (fwrite(headerText.data(), 1, headerText.size(), out) == headerText.size());
	if (fclose(out) != 0)
		ok = false;
	if (!ok)
	{
		remove(tmpPath.c_str());
		return false;
	}

	FILE *in = fopen(path.c_str(), "rb");
	if (!in)
	{
		remove(tmpPath.c_str());
		return false;
	}
	if (fseek(in, (long)hdr.frameDataOffset, SEEK_SET) != 0)
		ok = false;
	if (ok)
	{
		out = fopen(tmpPath.c_str(), "ab");
		if (!out)
			ok = false;
		else
		{
			char buf[32768];
			size_t n;
			while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
			{
				if (fwrite(buf, 1, n, out) != n)
				{
					ok = false;
					break;
				}
			}
			if (fclose(out) != 0)
				ok = false;
		}
	}
	fclose(in);

	if (!ok)
	{
		remove(tmpPath.c_str());
		return false;
	}

#if defined(_WIN32)
	// std::rename cannot replace an existing file on Windows.
	if (!MoveFileExA(tmpPath.c_str(), path.c_str(),
	                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		remove(tmpPath.c_str());
		return false;
	}
#else
	// POSIX rename is an atomic replace on the same filesystem.
	if (rename(tmpPath.c_str(), path.c_str()) != 0)
	{
		remove(tmpPath.c_str());
		return false;
	}
#endif
	return true;
}

int ParsePauseFrame(const std::string &text, bool enabled)
{
	if (!enabled || text.empty())
		return -1;
	char *end = NULL;
	const unsigned long value = strtoul(text.c_str(), &end, 10);
	if (end == text.c_str() || value > (unsigned long)INT_MAX)
		return -1;
	return (int)value;
}

void SplitComments(const Header &hdr, std::string &author, std::string &description)
{
	author.clear();
	description.clear();
	for (size_t i = 0; i < hdr.comments.size(); i++)
	{
		const std::string &c = hdr.comments[i];
		if (c.compare(0, 7, "author ") == 0)
			author = c.substr(7);
		else
		{
			if (!description.empty())
				description += "; ";
			description += c;
		}
	}
}

std::string FormatLength(uint32_t frames)
{
	const double tempCount = (frames / (33513982.0 / 6 / 355 / 263)) + 0.005;
	const int numSeconds = (int)tempCount;
	const int fraction = (int)((tempCount - numSeconds) * 100);
	const int seconds = numSeconds % 60;
	const int minutes = (numSeconds / 60) % 60;
	const int hours = (numSeconds / 60 / 60) % 60;
	char buf[64];
	snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%02d", hours, minutes, seconds, fraction);
	return buf;
}

std::string FormatRecordDate(const std::string &raw)
{
	if (raw.empty())
		return "Unknown";

	char *end = NULL;
	const long long t = strtoll(raw.c_str(), &end, 10);
	if (end == raw.c_str() || t <= 0)
		return "Unknown";

	const time_t tt = (time_t)t;
	struct tm tmBuf;
#if defined(_MSC_VER)
	if (localtime_s(&tmBuf, &tt) != 0)
		return "Unknown";
#else
	if (localtime_r(&tt, &tmBuf) == NULL)
		return "Unknown";
#endif
	char buf[64];
	if (strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", &tmBuf) == 0)
		return "Unknown";
	return buf;
}

std::string FormatHexCrc(uint32_t crc)
{
	char buf[16];
	snprintf(buf, sizeof(buf), "%08X", crc);
	return buf;
}

std::string CoreVersionText(const Header &hdr, const Assessment &a)
{
	char buf[128];
	if (hdr.emuVersion.empty())
	{
		snprintf(buf, sizeof(buf), "-");
	}
	else if (a.coreMismatch)
	{
		snprintf(buf, sizeof(buf), "%u (current %u - desync expected)",
		         a.recordedCore, a.currentCore);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%u", a.recordedCore);
	}
	return buf;
}

std::string FormatRomLine(const char *prefix, const std::string &serial,
                           const std::string &crc, const char *thirdLabel,
                           const std::string &thirdValue)
{
	// Same columns as the Windows dialog, but assembled with std::string so a
	// long ROM name cannot truncate the line.
	std::string text = prefix;
	text += " serial=";
	text += serial;
	if (serial.size() < 18)
		text.append(18 - serial.size(), ' ');
	text += " crc=";
	text += crc;
	if (crc.size() < 12)
		text.append(12 - crc.size(), ' ');
	text += " ";
	text += thirdLabel;
	text += thirdValue;
	return text;
}

std::string BaseName(const std::string &path)
{
	const size_t p = path.find_last_of("\\/");
	return p == std::string::npos ? path : path.substr(p + 1);
}

std::string Shorten(const std::string &s, size_t maxChars)
{
	if (s.size() <= maxChars)
		return s;
	return s.substr(0, maxChars - 3) + "...";
}

std::string DescribeFirmware(const std::string &raw)
{
	if (raw.empty())
		return "-";
	return atoi(raw.c_str()) != 0 ? "External file" : "Built-in";
}

std::string DescribeLanguage(const std::string &raw)
{
	static const char *kLanguages[] = {
		"Japanese", "English", "French", "German", "Italian", "Spanish", "Chinese"
	};
	if (raw.empty())
		return "-";
	const int v = atoi(raw.c_str());
	if (v >= 0 && v < (int)(sizeof(kLanguages) / sizeof(kLanguages[0])))
		return kLanguages[v];
	return raw;
}

} // namespace MovieInfo
