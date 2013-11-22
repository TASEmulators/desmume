#include "rar.hpp"

RawRead::RawRead(ComprDataIO *SrcFile) : Data( SrcFile )
{
	RawRead::SrcFile=SrcFile;
	ReadPos=0;
	DataSize=0;
}

void RawRead::Reset()
{
	ReadPos=0;
	DataSize=0;
	Data.Reset();
}

void RawRead::Read(int Size)
{
	// (removed decryption)
	if (Size!=0)
	{
		Data.Add(Size);
		DataSize+=SrcFile->Read(&Data[DataSize],Size);
	}
}




void RawRead::Get(byte &Field)
{
	if (ReadPos<DataSize)
	{
		Field=Data[ReadPos];
		ReadPos++;
	}
	else
		Field=0;
}


void RawRead::Get(ushort &Field)
{
	if (ReadPos+1<DataSize)
	{
		Field=Data[ReadPos]+(Data[ReadPos+1]<<8);
		ReadPos+=2;
	}
	else
		Field=0;
}


void RawRead::Get(uint &Field)
{
	if (ReadPos+3<DataSize)
	{
		Field=Data[ReadPos]+(Data[ReadPos+1]<<8)+(Data[ReadPos+2]<<16)+
				(Data[ReadPos+3]<<24);
		ReadPos+=4;
	}
	else
		Field=0;
}




void RawRead::Get(byte *Field,int Size)
{
	if (ReadPos+Size-1<DataSize)
	{
		memcpy(Field,&Data[ReadPos],Size);
		ReadPos+=Size;
	}
	else
	    memset(Field,0,Size);
}




uint RawRead::GetCRC(bool ProcessedOnly)
{
	return(DataSize>2 ? CRC(0xffffffff,&Data[2],(ProcessedOnly ? ReadPos:DataSize)-2):0xffffffff);
}
