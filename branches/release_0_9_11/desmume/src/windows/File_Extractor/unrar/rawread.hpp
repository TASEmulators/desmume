#ifndef _RAR_RAWREAD_
#define _RAR_RAWREAD_

class RawRead
{
private:
	Array<byte> Data;
	File *SrcFile;
	int DataSize;
	int ReadPos;
	friend class Archive;
public:
	RawRead(File *SrcFile);
	void Reset();
	void Read(int Size);
	void Get(byte &Field);
	void Get(ushort &Field);
	void Get(uint &Field);
	void Get(byte *Field,int Size);
	uint GetCRC(bool ProcessedOnly);
	int Size() {return DataSize;}
    int PaddedSize() {return Data.Size()-DataSize;}
};

#endif
