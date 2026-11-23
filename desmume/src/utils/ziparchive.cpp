// Minimal read-only ZIP reader driven by zlib's inflate().
// Adapted from vba-rr-modernized (GPL-2.0), which replaced an older minizip
// copy with this central-directory reader.

#include <cstring>
#include <new>
#include <zlib.h>

#include "ziparchive.h"

#ifdef _WIN32
#include <windows.h>
static FILE* ZipOpenFile(const char* filename)
{
	if (!filename || !*filename) return NULL;
	const int length = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
	if (length <= 0) return NULL;
	wchar_t* wide = new wchar_t[length];
	if (MultiByteToWideChar(CP_UTF8, 0, filename, -1, wide, length) <= 0)
	{
		delete[] wide;
		return NULL;
	}
	FILE* file = _wfopen(wide, L"rb");
	delete[] wide;
	return file;
}
#else
static FILE* ZipOpenFile(const char* filename)
{
	return filename ? fopen(filename, "rb") : NULL;
}
#endif

// ZIP on-disk structures (little endian).
#define ZIP_LOCAL_SIG    0x04034b50
#define ZIP_CENTRAL_SIG  0x02014b50
#define ZIP_EOCD_SIG     0x06054b50

#define ZIP_EOCD_SIZE    22
#define ZIP_CENTRAL_SIZE 46
#define ZIP_LOCAL_SIZE   30
#define ZIP_MAX_COMMENT  65535

static unsigned int zipRead16(const unsigned char* p)
{
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int zipRead32(const unsigned char* p)
{
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
	       ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

ZipArchive::ZipArchive(const char* filename)
	: m_file(NULL), m_fileSize(0), m_entries(NULL), m_numEntries(0), m_valid(false)
{
	m_file = ZipOpenFile(filename);
	if (!m_file)
		return;

	if (fseek(m_file, 0, SEEK_END) != 0)
	{
		fclose(m_file);
		m_file = NULL;
		return;
	}

	long fileSize = ftell(m_file);
	if (fileSize < ZIP_EOCD_SIZE)
	{
		fclose(m_file);
		m_file = NULL;
		return;
	}
	m_fileSize = fileSize;

	m_valid = ReadCentralDirectory();
	if (!m_valid)
	{
		delete[] m_entries;
		m_entries = NULL;
		m_numEntries = 0;
		fclose(m_file);
		m_file = NULL;
	}
}

ZipArchive::~ZipArchive()
{
	delete[] m_entries;
	if (m_file)
		fclose(m_file);
}

bool ZipArchive::ReadCentralDirectory()
{
	// The end-of-central-directory record is at the very end, but may be
	// followed by a comment of up to 64 KB, so scan backwards for it.
	long tailSize = m_fileSize < (long)(ZIP_MAX_COMMENT + ZIP_EOCD_SIZE)
	              ? m_fileSize : (long)(ZIP_MAX_COMMENT + ZIP_EOCD_SIZE);

	unsigned char* tail = new unsigned char[tailSize];
	if (fseek(m_file, m_fileSize - tailSize, SEEK_SET) != 0 ||
	    fread(tail, 1, (size_t)tailSize, m_file) != (size_t)tailSize)
	{
		delete[] tail;
		return false;
	}

	long eocd = -1;
	for (long i = tailSize - ZIP_EOCD_SIZE; i >= 0; i--)
	{
		if (zipRead32(tail + i) != ZIP_EOCD_SIG)
			continue;

		unsigned int commentLen = zipRead16(tail + i + 20);
		if (i + ZIP_EOCD_SIZE + (long)commentLen <= tailSize)
		{
			eocd = i;
			break;
		}
	}

	if (eocd < 0)
	{
		delete[] tail;
		return false;
	}

	const unsigned char* e = tail + eocd;
	unsigned int diskNumber     = zipRead16(e + 4);
	unsigned int centralDisk    = zipRead16(e + 6);
	unsigned int entriesOnDisk  = zipRead16(e + 8);
	unsigned int totalEntries   = zipRead16(e + 10);
	unsigned int centralSize    = zipRead32(e + 12);
	unsigned int centralOffset  = zipRead32(e + 16);
	delete[] tail;

	if (diskNumber != 0 || centralDisk != 0 || entriesOnDisk != totalEntries)
		return false; // multi-disk archive
	if (totalEntries == 0xFFFF || centralSize == 0xFFFFFFFF || centralOffset == 0xFFFFFFFF)
		return false; // ZIP64, not supported here
	if ((long)centralOffset + (long)centralSize > m_fileSize)
		return false;

	// Every central directory entry needs at least its fixed-size header.
	if ((unsigned long long)totalEntries * ZIP_CENTRAL_SIZE > (unsigned long long)centralSize)
		return false;

	if (totalEntries == 0)
	{
		m_numEntries = 0;
		return true; // empty but valid archive
	}

	m_entries = new (std::nothrow) Entry[totalEntries];
	if (!m_entries)
		return false;

	if (fseek(m_file, (long)centralOffset, SEEK_SET) != 0)
		return false;

	for (unsigned int i = 0; i < totalEntries; i++)
	{
		unsigned char header[ZIP_CENTRAL_SIZE];

		if (fread(header, 1, ZIP_CENTRAL_SIZE, m_file) != ZIP_CENTRAL_SIZE)
			return false;
		if (zipRead32(header) != ZIP_CENTRAL_SIG)
			return false;

		Entry& entry = m_entries[i];
		unsigned int nameLen    = zipRead16(header + 28);
		unsigned int extraLen   = zipRead16(header + 30);
		unsigned int commentLen = zipRead16(header + 32);

		entry.method             = (unsigned short)zipRead16(header + 10);
		entry.compressedSize     = zipRead32(header + 20);
		entry.uncompressedSize   = zipRead32(header + 24);
		entry.localHeaderOffset  = zipRead32(header + 42);

		if ((long)entry.localHeaderOffset + ZIP_LOCAL_SIZE > m_fileSize)
			return false;

		size_t keep = nameLen;
		if (keep > sizeof(entry.name) - 1)
			keep = sizeof(entry.name) - 1;
		if (keep && fread(entry.name, 1, keep, m_file) != keep)
			return false;
		entry.name[keep] = '\0';

		long skip = (long)(nameLen - (unsigned int)keep) + (long)extraLen + (long)commentLen;
		if (skip && fseek(m_file, skip, SEEK_CUR) != 0)
			return false;
	}

	m_numEntries = (int)totalEntries;
	return true;
}

bool ZipArchive::GetEntryInfo(int index, ZipEntryInfo* info) const
{
	if (!m_valid || !info || index < 0 || index >= m_numEntries)
		return false;

	const Entry& entry = m_entries[index];
	info->compressedSize    = entry.compressedSize;
	info->uncompressedSize  = entry.uncompressedSize;
	info->localHeaderOffset = entry.localHeaderOffset;
	info->method            = entry.method;
	memcpy(info->name, entry.name, sizeof(info->name));
	info->name[sizeof(info->name) - 1] = '\0';
	return true;
}

int ZipArchive::ExtractEntry(int index, void* buffer, unsigned int bufferSize) const
{
	if (!m_valid || !buffer || index < 0 || index >= m_numEntries)
		return 0;

	const Entry& entry = m_entries[index];

	if (bufferSize > entry.uncompressedSize)
		bufferSize = entry.uncompressedSize;
	if (bufferSize == 0)
		return 0;

	unsigned char header[ZIP_LOCAL_SIZE];
	if (fseek(m_file, (long)entry.localHeaderOffset, SEEK_SET) != 0)
		return 0;
	if (fread(header, 1, ZIP_LOCAL_SIZE, m_file) != ZIP_LOCAL_SIZE)
		return 0;
	if (zipRead32(header) != ZIP_LOCAL_SIG)
		return 0;

	unsigned int nameLen  = zipRead16(header + 26);
	unsigned int extraLen = zipRead16(header + 28);
	long dataOffset = (long)entry.localHeaderOffset + ZIP_LOCAL_SIZE +
	                  (long)nameLen + (long)extraLen;
	if (dataOffset > m_fileSize || fseek(m_file, dataOffset, SEEK_SET) != 0)
		return 0;

	if (entry.method == 0) // stored
	{
		size_t got = fread(buffer, 1, bufferSize, m_file);
		return (int)got;
	}

	if (entry.method != 8) // only deflate is supported
		return 0;

	z_stream stream;
	memset(&stream, 0, sizeof(stream));
	if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
		return 0;

	unsigned char input[16384];
	unsigned int remaining = entry.compressedSize;
	bool done = false;

	stream.next_out  = (Bytef*)buffer;
	stream.avail_out = bufferSize;

	while (stream.avail_out > 0)
	{
		if (stream.avail_in == 0)
		{
			unsigned int chunk = remaining < sizeof(input) ? remaining : (unsigned int)sizeof(input);
			if (chunk == 0)
				break;

			size_t got = fread(input, 1, chunk, m_file);
			if (got == 0)
				break;

			remaining -= (unsigned int)got;
			stream.next_in  = input;
			stream.avail_in = (uInt)got;
		}

		int result = inflate(&stream, Z_NO_FLUSH);
		if (result == Z_STREAM_END)
		{
			done = true;
			break;
		}
		if (result != Z_OK)
			break;
	}

	int produced = (int)stream.total_out;
	inflateEnd(&stream);

	// A truncated stream that did not fill the caller's buffer is an error;
	// a buffer that is exactly full is a normal short read.
	if (!done && produced < (int)bufferSize)
		return 0;

	return produced;
}
