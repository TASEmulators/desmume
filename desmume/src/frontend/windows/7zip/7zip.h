#ifndef _7ZIP_DEC_HEADER
#define _7ZIP_DEC_HEADER

// 7zip file extraction, adapted from vba-rr-modernized (GPL-2.0).
// NOTE: if you want to add support for opening files within archives to something,
// consider using the higher-level interface provided by OpenArchive.h instead

#include <string>

void InitDecoder();
void CleanupDecoder();
const char* GetSupportedFormatsFilter();
bool IsArchiveSupportAvailable();

// simplest way of extracting a file after calling InitDecoder():
// int size = ArchiveFile(filename).ExtractItem(0, buf, sizeof(buf));

class ZipArchive;

struct ArchiveFile
{
	ArchiveFile(const char* filename, const char* displayFilename=0);
	virtual ~ArchiveFile();

	int GetNumItems();
	int GetItemSize(int item);
	const char* GetItemName(int item);
	const wchar_t* GetItemNameW(int item) const;
	int ExtractItem(int item, unsigned char* outBuffer, int bufSize) const; // returns size, or 0 if failed
	int ExtractItem(int item, const char* outFilename) const;

	bool IsCompressed();
	const char* GetArchiveTypeName();
	const char* GetArchiveFileName() { return m_displayFilename ? m_displayFilename : m_filename; }
	// True when the name looks like an archive but archive support is not
	// available, so the caller can report that instead of loading the raw file.
	bool IsUnsupportedArchive() const { return m_unsupportedArchive; }

	bool m_userMadeSelection;

protected:
	struct ArchiveItem
	{
		int size;
		char* name;
		std::wstring wname;
	};
	ArchiveItem* m_items;
	int m_numItems;
	int m_typeIndex;
	bool m_unsupportedArchive;
	ZipArchive* m_zip; // zlib fallback when 7z.dll is unavailable
	char* m_filename;
	char* m_displayFilename;
};

#endif
