#ifndef _RAR_ARCHIVE_
#define _RAR_ARCHIVE_

typedef ComprDataIO File;
#include "rawread.hpp"

class Archive:public File
{
private:
	bool IsSignature(byte *D);
	void ConvertUnknownHeader();
	int ReadOldHeader();

	RawRead Raw;

	MarkHeader MarkHead;
	OldMainHeader OldMhd;

	int CurHeaderType;

public:
	Archive();
	unrar_err_t IsArchive();
	unrar_err_t ReadHeader();
	void SeekToNext();
    bool IsArcDir();
    bool IsArcLabel();
	int GetHeaderType() {return(CurHeaderType);};

	BaseBlock ShortBlock;
	MainHeader NewMhd;
	FileHeader NewLhd;
    SubBlockHeader SubBlockHead;
	FileHeader SubHead;
    ProtectHeader ProtectHead;

	Int64 CurBlockPos;
	Int64 NextBlockPos;

	bool Solid;
	enum { SFXSize = 0 }; // self-extracting not supported
	ushort HeaderCRC;
};

#endif
