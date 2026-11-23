#ifndef ZIPARCHIVE_HEADER
#define ZIPARCHIVE_HEADER

#include <cstdio>

// Minimal read-only ZIP reader built on zlib's inflate().
//
// This replaces the bundled minizip copy (unzip.cpp/unzip.h): it parses the
// central directory itself and inflates raw deflate streams, so the only
// dependency left is zlib.  The supported scope matches what the emulator
// used before:
//   - stored and deflated entries
//   - single-disk archives, no ZIP64, no encryption
// Entries that cannot be decoded (encrypted, bzip2/lzma compressed, corrupt)
// simply fail to extract.

struct ZipEntryInfo
{
	unsigned int   compressedSize;
	unsigned int   uncompressedSize;
	unsigned int   localHeaderOffset;
	unsigned short method;          // 0 = stored, 8 = deflate
	char           name[1024];
};

class ZipArchive
{
public:
	explicit ZipArchive(const char* filename);
	~ZipArchive();

	bool IsValid() const { return m_valid; }
	int  GetEntryCount() const { return m_numEntries; }
	bool GetEntryInfo(int index, ZipEntryInfo* info) const;
	// Returns the number of bytes written to buffer, or 0 on failure.
	int  ExtractEntry(int index, void* buffer, unsigned int bufferSize) const;

private:
	struct Entry
	{
		unsigned int   compressedSize;
		unsigned int   uncompressedSize;
		unsigned int   localHeaderOffset;
		unsigned short method;
		char           name[1024];
	};

	bool ReadCentralDirectory();

	FILE*        m_file;
	long         m_fileSize;
	Entry*       m_entries;
	int          m_numEntries;
	bool         m_valid;
};

#endif
